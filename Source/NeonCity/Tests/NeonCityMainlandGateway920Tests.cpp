#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "World/NeonCityMainlandSite.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Property/NeonCityStarterApartment.h"
#include "Property/PropertyComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/WaterInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandGateway920
{
FString HashFile(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? FMD5::HashBytes(Bytes.GetData(), Bytes.Num()) : FString();
}

/** Shared evidence/input adapter for two bounded installation checks. No
 * gameplay, collision, movement mode, velocity or water state is overridden.
 * Each sequence has exactly one disclosed supported initial on-foot fixture.
 */
class FAccessBase
{
protected:
    FAccessBase(FAutomationTestBase* InTest, const TCHAR* InName) : Test(InTest), Name(InName) {}

    bool Ready(double Timeout)
    {
        if (!Started)
        {
            Started = FPlatformTime::Seconds();
            ConfigureOutput();
        }
        if (FPlatformTime::Seconds() - Started > Timeout)
        { Failure = FString::Printf(TEXT("Finite %.0f-second sequence bound exceeded"), Timeout); return false; }
        W = NeonCityValidationWorld();
        PC = W ? W->GetFirstPlayerController() : nullptr;
        if (!PC || W->GetTimeSeconds() < 8.f) return false;
        if (!P)
        {
            P = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            if (!P || !ANeonCityMainlandSite::Find(W))
            { Failure = TEXT("Normal on-foot player and actual C920 mainland site required"); return false; }
            if (!NormalPlayer())
            { Failure = TEXT("Initial player has attached/hidden/disabled movement or collision"); return false; }
            PreviousView = PC->GetControlRotation();
        }
        if (!NormalPlayer())
        { Failure = TEXT("Sequence lost normal visible, collidable, unseated player possession/input"); return false; }
        Record();
        return true;
    }

    bool NormalPlayer() const
    {
        return IsValid(P) && PC && PC->GetPawn() == P && !PC->IsMoveInputIgnored()
            && !P->GetAttachParentActor() && !P->IsHidden() && P->GetActorEnableCollision()
            && P->GetMesh() && P->GetMesh()->IsVisible();
    }

    bool SampleFloor(FVector Point, FHitResult& Hit) const
    {
        if (!W || !P) return false;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(C920GatewayStaticFloor), false, P);
        if (!W->LineTraceSingleByObjectType(Hit, Point + FVector(0, 0, 45), Point - FVector(0, 0, 160),
            FCollisionObjectQueryParams(ECC_WorldStatic), Q) || Hit.ImpactNormal.Z < .7f) return false;
        const auto* Actor = Cast<AStaticMeshActor>(Hit.GetActor());
        const auto* Component = Actor ? Actor->GetStaticMeshComponent() : nullptr;
        return Component && Component->GetStaticMesh()
            && Component->GetStaticMesh()->GetName().StartsWith(TEXT("SM_C920_"))
            && P->GetCharacterMovement()->IsWalkable(Hit);
    }

    bool FloorGoal(FVector ExpectedFeet, FVector& Stand)
    {
        FHitResult Floor;
        if (!SampleFloor(ExpectedFeet, Floor) || FMath::Abs(Floor.ImpactPoint.Z - ExpectedFeet.Z) > 15.f)
        {
            Failure = FString::Printf(TEXT("Expected native finished floor unsupported/wrong height at%s; hit%s at%s"),
                *ExpectedFeet.ToString(), *GetNameSafe(Floor.GetActor()), *Floor.ImpactPoint.ToString());
            return false;
        }
        Stand = Floor.ImpactPoint + FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 3);
        FCollisionQueryParams Q(SCENE_QUERY_STAT(C920GatewayCapsule), false, P);
        if (W->OverlapBlockingTestByChannel(Stand, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(), P->GetSimpleCollisionHalfHeight()), Q))
        { Failure = TEXT("Complete normal capsule blocked at actual supported stance ") + Stand.ToString(); return false; }
        return true;
    }

    bool Grounded() const
    {
        if (!NormalPlayer() || !P->GetCharacterMovement()->IsMovingOnGround()) return false;
        const FVector Feet = P->GetActorLocation() - FVector(0, 0, P->GetSimpleCollisionHalfHeight());
        FHitResult Floor;
        return SampleFloor(Feet, Floor) && FMath::Abs(Feet.Z - Floor.ImpactPoint.Z) < 12;
    }

    bool InitialFixture(FVector Feet)
    {
        if (FixtureCount != 0) { Failure = TEXT("A second test pose fixture is forbidden"); return false; }
        if (!FloorGoal(Feet, InitialStand)) return false;
        if (!P->SetActorLocation(InitialStand, false, nullptr, ETeleportType::TeleportPhysics))
        { Failure = TEXT("Declared supported initial on-foot fixture failed"); return false; }
        ++FixtureCount;
        LastProgress = W->GetTimeSeconds(); ProgressPosition = P->GetActorLocation();
        return true;
    }

    void Key(FKey Input, bool Down)
    {
        bool& Held = HeldKeys.FindOrAdd(Input);
        if (Held == Down || !PC) return;
        PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Input,
            Down ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
        Held = Down;
    }

    void StartLeg()
    {
        LegStarted = LastProgress = W->GetTimeSeconds(); ProgressPosition = P->GetActorLocation();
    }

    bool MoveToward(FVector Goal, float Tolerance = 22.f)
    {
        const FVector Here = P->GetActorLocation(), Delta = Goal - Here;
        if (Delta.Size2D() < Tolerance)
        { Key(EKeys::W, false); return true; }
        if (FVector::Dist2D(Here, ProgressPosition) > 8)
        { LastProgress = W->GetTimeSeconds(); ProgressPosition = Here; }
        if (W->GetTimeSeconds() - LegStarted > 25.f || W->GetTimeSeconds() - LastProgress > 5.f)
        {
            FHitResult Obstacle;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(C920GatewayBlockedLeg), false, P);
            W->SweepSingleByChannel(Obstacle, Here, Here + Delta.GetSafeNormal2D() * 120,
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(
                    P->GetSimpleCollisionRadius(), P->GetSimpleCollisionHalfHeight()), Q);
            Failure = FString::Printf(TEXT("Normal W movement stalled toward%s at%s, mode%d, velocity%s; blocker%s/%s"),
                *Goal.ToString(), *Here.ToString(), int32(P->GetCharacterMovement()->MovementMode),
                *P->GetVelocity().ToString(), *GetNameSafe(Obstacle.GetActor()), *GetNameSafe(Obstacle.GetComponent()));
            Key(EKeys::W, false); return false;
        }
        PC->SetControlRotation(FRotator(0, Delta.Rotation().Yaw, 0));
        // Pulse the normal digital W mapping near fine physical waypoints.
        // Native braking owns deceleration; no velocity/force is assigned.
        // This avoids charging a 25cm path cell at full walking speed.
        const float DesiredSpeed = FMath::Clamp(float(Delta.Size2D() * 3.), 60.f, 300.f);
        Key(EKeys::W, P->GetVelocity().Size2D() <= DesiredSpeed);
        return false;
    }

    void Capture(const TCHAR* StageName)
    {
        if (!bStills) return;
#if WITH_EDITOR
        const FString ImageName = TEXT("C920-") + Name + TEXT("-") + Token + TEXT("-") + StageName + TEXT(".jpg");
        if (FNeonCityVisualEvidence::CaptureRenderedJPEG(W, ImageName))
            Images.Add(MakeShared<FJsonValueString>(ImageName));
        else { ++FailedImages; Test->AddError(TEXT("Requested normal-player still failed: ") + ImageName); }
#else
        ++FailedImages; Test->AddError(TEXT("Requested gameplay still requires editor-enabled offscreen evidence build"));
#endif
    }

    bool Finish(const FString& Error, bool Completed, const TSharedRef<FJsonObject>& Details)
    {
        if (!Error.IsEmpty()) Failure = Error;
        for (auto& Input : HeldKeys)
            if (Input.Value && PC)
            {
                PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Input.Key,
                    IE_Released, FPlatformTime::Cycles64()));
                Input.Value = false;
            }
        if (PC && P && PC->GetPawn() == P) PC->SetControlRotation(PreviousView);
        const bool Passed = Failure.IsEmpty() && Completed && FixtureCount == 1 && FailedImages == 0;
        if (Failure.IsEmpty() && !Passed) Failure = TEXT("Sequence ended without all declared objective gates");
        if (!Failure.IsEmpty()) Test->AddError(Name + TEXT(": ") + Failure);
        Details->SetStringField(TEXT("test"), Name); Details->SetBoolField(TEXT("passed"), Passed);
        Details->SetStringField(TEXT("failure"), Failure); Details->SetNumberField(TEXT("final_stage"), Stage);
        Details->SetNumberField(TEXT("test_pose_fixtures"), FixtureCount);
        Details->SetStringField(TEXT("initial_disclosed_on_foot_fixture_cm"), InitialStand.ToString());
        Details->SetStringField(TEXT("final_player_cm"), P ? P->GetActorLocation().ToString() : TEXT("absent"));
        Details->SetBoolField(TEXT("final_supported_normal_walking"), Grounded());
        Details->SetBoolField(TEXT("test_movement_mode_velocity_collision_writes"), false);
        Details->SetNumberField(TEXT("failed_requested_stills"), FailedImages);
        Details->SetArrayField(TEXT("actual_player_images"), Images);
        Details->SetStringField(TEXT("map_md5"), MapHash); Details->SetStringField(TEXT("test_source_md5"), SourceHash);
        Details->SetBoolField(TEXT("automatic_player_persistence"), false);
        Details->SetBoolField(TEXT("clean_performance_sample"), false);
        Details->SetBoolField(TEXT("cacheable_success"), Passed);
        FString Json; FJsonSerializer::Serialize(Details, TJsonWriterFactory<>::Create(&Json));
        if (!FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("receipt.json"))))
            Test->AddError(TEXT("Could not preserve unique gateway/pool evidence receipt"));
        Test->AddInfo(TEXT("Actual gateway/pool receipt: ") + Directory / TEXT("receipt.json"));
        return true;
    }

    void ConfigureOutput()
    {
        FString Run; FParse::Value(FCommandLine::Get(), TEXT("NeonCityArtRun="), Run);
        Run = FPaths::MakeValidFileName(Run.IsEmpty() ? TEXT("unlabelled") : Run);
        Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Directory = FPaths::ProjectSavedDir() / TEXT("Phase205cUpgrade/MainlandC920/GatewayAccess") / Run / (Name + TEXT("-") + Token);
        IFileManager::Get().MakeDirectory(*Directory, true);
        bStills = FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtStills"));
        MapHash = HashFile(FPaths::ProjectContentDir() / TEXT("NeonCity/Maps/NeonCity_SouthBeach_Test.umap"));
        SourceHash = HashFile(FPaths::ProjectDir() / TEXT("Source/NeonCity/Tests/NeonCityMainlandGateway920Tests.cpp"));
        FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,player_x,player_y,player_z,movement_mode,water_state,grounded,speed_cm_s\n"),
            *(Directory / TEXT("trajectory.csv")));
    }

    void Record()
    {
        if (!P || W->GetTimeSeconds() - LastRecord < .25f) return;
        LastRecord = W->GetTimeSeconds(); const FVector ActualPosition = P->GetActorLocation();
        const auto* Water = P->FindComponentByClass<UWaterInteractionComponent>();
        if (!FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.3f,%d,%.3f,%.3f,%.3f,%d,%d,%d,%.3f\n"),
            LastRecord, Stage, ActualPosition.X, ActualPosition.Y, ActualPosition.Z, int32(P->GetCharacterMovement()->MovementMode),
            Water ? int32(Water->GetWaterState()) : -1, Grounded() ? 1 : 0, P->GetVelocity().Size()),
            *(Directory / TEXT("trajectory.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(), FILEWRITE_Append)) Failure = TEXT("Could not retain actual gateway/pool trajectory");
    }

    FAutomationTestBase* Test;
    FString Name, Directory, Token, Failure, MapHash, SourceHash;
    UWorld* W = nullptr;
    APlayerController* PC = nullptr;
    ANeonCityPlayerCharacter* P = nullptr;
    TMap<FKey, bool> HeldKeys;
    TArray<TSharedPtr<FJsonValue>> Images;
    FRotator PreviousView;
    FVector InitialStand = FVector::ZeroVector, ProgressPosition = FVector::ZeroVector;
    double Started = 0;
    float At = 0, LegStarted = 0, LastProgress = 0, LastRecord = 0;
    int32 Stage = 0, FixtureCount = 0, FailedImages = 0;
    bool bStills = false;
};

class FStarterHome : public IAutomationLatentCommand, private FAccessBase
{
public:
    explicit FStarterHome(FAutomationTestBase* InTest) : FAccessBase(InTest, TEXT("StarterHomeAccess")) {}
    bool Update() override
    {
        if (!Ready(180)) return Failure.IsEmpty() ? false : End(Failure);
        if (!Failure.IsEmpty()) return End(Failure);
        const float Age = W->GetTimeSeconds() - At;
        if (Stage == 0)
        {
            for (TActorIterator<ANeonCityStarterApartment> I(W); I; ++I)
                if (I->GetLevel() == W->PersistentLevel && I->GetPropertyComponent()
                    && I->GetPropertyComponent()->GetPropertyID() == TEXT("NeonCity.Property.StarterApartment"))
                { if (Home) return End(TEXT("Duplicate retained starter property")); Home = *I; }
            if (!Home) return End(TEXT("Actual retained starter property absent"));
            TInlineComponentArray<UStaticMeshComponent*> Components(Home);
            for (auto* Component : Components)
                if (Component->GetFName() == TEXT("Door")) Door = Component;
            if (!Door || !Door->GetStaticMesh() || Door->GetStaticMesh()->GetFName() != TEXT("SM_C920_BronzeGlazedDoor_Hinge")
                || !Door->IsQueryCollisionEnabled() || Door->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
                return End(TEXT("Original property gateway has no actual fitted collidable bronze leaf"));
            OriginalDoorTransform = Door->GetComponentTransform();
            const FVector Exit = (Home->ExitLocal * Home->GetActorTransform()).GetLocation();
            if (!InitialFixture(Exit - FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 3))) return End(Failure);
            Test->AddInfo(TEXT("One disclosed supported initial outside-leaf stance. F uses the existing production short doorway gateway; subsequent room circulation uses mapped W with intact native collision. No test pose after this fixture."));
            return Next(1);
        }
        if (!IsValid(Home) || !IsValid(Door)) return End(TEXT("Original property or fitted leaf disappeared"));
        if (Stage == 1)
        {
            if (Age < .75f) return false;
            if (!Grounded()) return Age > 3 ? End(TEXT("Outside starter stance did not settle supported")) : false;
            if (Home->IsInsideInterior_Implementation(P) || !Home->CanEnterInterior_Implementation(P) || !Focus())
                return End(FocusFailure(TEXT("Outside normal Enter home prompt unavailable")));
            Capture(TEXT("outside-prompt")); PromptOutside = true;
            Key(EKeys::F, true); return Next(2);
        }
        if (Stage == 2 && Age > .25f)
        {
            Key(EKeys::F, false);
            if (!Home->IsInsideInterior_Implementation(P)) return End(TEXT("Normal F did not enter the actual retained home gateway"));
            const FVector Entry = (Home->EntryLocal * Home->GetActorTransform()).GetLocation();
            if (FVector::Dist(P->GetActorLocation(), Entry) > 20)
                return End(TEXT("Production F entry did not reach its actual fitted EntryLocal"));
            Entered = true; return Next(3);
        }
        if (Stage == 3)
        {
            if (!Grounded()) return Age > 3 ? End(TEXT("Production home entry did not settle on finished room floor")) : false;
            Capture(TEXT("production-F-entered")); PreviousWalkPosition = P->GetActorLocation();
            // New suite lies west of the retained doorway. These are four
            // distinct actual room destinations, not the old +Y home layout.
            const FVector Offsets[] = { FVector(-190, -310, 0), FVector(-650, -285, 0),
                FVector(-190, 360, 0), FVector(-190, 0, 0) };
            for (const auto& Offset : Offsets) RoomFeet.Add(Home->GetActorTransform().TransformPosition(Offset));
            return BeginRoomLeg();
        }
        if (Stage == 4)
        {
            if (!Home->IsInsideInterior_Implementation(P)) return End(TEXT("Room walking left the actual home bounds before F exit"));
            if (P->GetCharacterMovement()->IsSwimming()) return End(TEXT("Dry starter room unexpectedly entered swimming"));
            WalkDistance += FVector::Dist2D(PreviousWalkPosition, P->GetActorLocation()); PreviousWalkPosition = P->GetActorLocation();
            if (!Path.IsValidIndex(PathPoint)) return End(TEXT("Physical room route missing a walking point"));
            if (!MoveToward(Path[PathPoint], 18)) return Failure.IsEmpty() ? false : End(Failure);
            if (FMath::Abs(P->GetActorLocation().Z - Path[PathPoint].Z) > 45)
                return End(TEXT("Room walking reached XY on the wrong vertical floor"));
            if (++PathPoint < Path.Num()) { StartLeg(); return false; }
            if (!Grounded()) return End(TEXT("Room destination lacks supported normal walking"));
            if (RoomPoint == 1) Capture(TEXT("actual-room-walk"));
            if (++RoomPoint < RoomFeet.Num()) return BeginRoomLeg();
            return Next(5);
        }
        if (Stage == 5)
        {
            if (Age < .4f) return false;
            if (!Grounded() || !Home->IsInsideInterior_Implementation(P) || !Home->CanInteract_Implementation(P) || !Focus())
                return End(FocusFailure(TEXT("Reached inside normal Leave home prompt unavailable")));
            PromptInside = true; Key(EKeys::F, true); return Next(6);
        }
        if (Stage == 6 && Age > .25f)
        {
            Key(EKeys::F, false);
            if (Home->IsInsideInterior_Implementation(P)) return End(TEXT("Normal F failed to leave actual home gateway"));
            const FVector Exit = (Home->ExitLocal * Home->GetActorTransform()).GetLocation();
            if (FVector::Dist(P->GetActorLocation(), Exit) > 20)
                return End(TEXT("Production F exit did not reach its actual fitted ExitLocal"));
            Exited = true; return Next(7);
        }
        if (Stage == 7)
        {
            if (!Grounded()) return Age > 3 ? End(TEXT("Home F exit did not restore dry supported normal walking")) : false;
            if (Age < .5f) return false;
            if (!Door->IsQueryCollisionEnabled() || Door->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
                || !Door->GetComponentTransform().Equals(OriginalDoorTransform, .01f))
                return End(TEXT("Home gateway changed the original leaf collision policy"));
            Capture(TEXT("production-F-dry-exit"));
            return End(TEXT(""));
        }
        return false;
    }

private:
    bool Next(int32 Value) { Stage = Value; At = W->GetTimeSeconds(); return false; }
    bool BeginRoomLeg()
    {
        FVector Destination;
        if (!RoomFeet.IsValidIndex(RoomPoint) || !FloorGoal(RoomFeet[RoomPoint], Destination)) return End(Failure);
        FPhase205cWalkSurvey Survey;
        if (!Phase205cSupportedWalkPath(W, P, Destination, Path, 90, 25, &Survey, 5000))
            return End(FString::Printf(TEXT("Actual room%d full-capsule route failed: %s; from%s to%s"),
                RoomPoint, *Survey.Failure, *P->GetActorLocation().ToString(), *Destination.ToString()));
        PathPoint = 0; StartLeg(); return Next(4);
    }
    bool Focus()
    {
        FVector Eye; FRotator Rotation; P->GetActorEyesViewPoint(Eye, Rotation);
        const FVector Centre = Door->Bounds.Origin, Extent = Door->Bounds.BoxExtent;
        for (float Y : {0.f, -.5f, .5f}) for (float Z : {0.f, -.35f, .35f})
        {
            PC->SetControlRotation((Centre + Extent * FVector(0, Y, Z) - Eye).Rotation());
            if (P->GetInteractionComponent()->RefreshFocusedInteractable() == Home
                && Home->CanInteract_Implementation(P) && !Home->GetInteractionName_Implementation().IsEmpty()) return true;
        }
        return false;
    }
    FString FocusFailure(const TCHAR* Reason) const
    {
        FVector Eye; FRotator Rotation; P->GetActorEyesViewPoint(Eye, Rotation);
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(C920HomeGatewayFocus), false, P);
        W->LineTraceSingleByChannel(Hit, Eye, Door->GetComponentLocation(), ECC_Visibility, Q);
        return FString::Printf(TEXT("%s; eye%s leaf pivot%s bounds%s; production pivot-ray hit%s/%s; inside%d canEnter%d canInteract%d"),
            Reason, *Eye.ToString(), *Door->GetComponentLocation().ToString(), *Door->Bounds.Origin.ToString(),
            *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Home->IsInsideInterior_Implementation(P),
            Home->CanEnterInterior_Implementation(P), Home->CanInteract_Implementation(P));
    }
    bool End(const FString& Error)
    {
        auto Details = MakeShared<FJsonObject>();
        Details->SetBoolField(TEXT("outside_normal_prompt"), PromptOutside);
        Details->SetBoolField(TEXT("normal_F_entered"), Entered);
        Details->SetBoolField(TEXT("inside_normal_prompt"), PromptInside);
        Details->SetBoolField(TEXT("normal_F_exited"), Exited);
        Details->SetNumberField(TEXT("completed_room_walk_stops"), RoomPoint);
        Details->SetNumberField(TEXT("actual_room_walk_cm"), WalkDistance);
        Details->SetStringField(TEXT("home_actor"), GetPathNameSafe(Home));
        Details->SetStringField(TEXT("scope"), TEXT("One supported outside-leaf fixture; mapped F invokes the existing production short teleport doorway gateway, four actual mapped-W room stops with full native capsule, normal inside prompt/F exit and dry walking. The production gateway is explicitly not evidence of physically walking through its closed leaf. No user save read/write or test pose after the initial fixture."));
        return Finish(Error, PromptOutside && Entered && PromptInside && Exited && RoomPoint == 4
            && WalkDistance > 1000 && Grounded(), Details);
    }

    ANeonCityStarterApartment* Home = nullptr;
    UStaticMeshComponent* Door = nullptr;
    FTransform OriginalDoorTransform;
    TArray<FVector> RoomFeet, Path;
    FVector PreviousWalkPosition = FVector::ZeroVector;
    int32 RoomPoint = 0, PathPoint = 0;
    float WalkDistance = 0;
    bool PromptOutside = false, PromptInside = false, Entered = false, Exited = false;
};

class FPool : public IAutomationLatentCommand, private FAccessBase
{
public:
    explicit FPool(FAutomationTestBase* InTest) : FAccessBase(InTest, TEXT("PoolAccess")) {}
    bool Update() override
    {
        if (!Ready(100)) return Failure.IsEmpty() ? false : End(Failure);
        if (!Failure.IsEmpty()) return End(Failure);
        const float Age = W->GetTimeSeconds() - At;
        if (Stage == 0)
        {
            for (TActorIterator<ANeonCityWaterVolume> I(W); I; ++I)
                if (I->GetLevel() == W->PersistentLevel && I->GetFName() == TEXT("NeonCityWaterVolume_0") && I->bContainedPoolWater)
                { if (Pool) return End(TEXT("Duplicate actual C920 finite pool")); Pool = *I; }
            if (!Pool || !Pool->GetActorScale3D().Equals(FVector::OneVector, .001)
                || !Pool->GetActorLocation().Equals(FVector(0, 1100, -17), .1f)
                || !Pool->Extent.Equals(FVector(700, 250, 58), .1f) || Pool->OceanSurface)
                return End(TEXT("Installed C920 contained pool pose/extent/provider differs from its actual shell"));
            if (!CheckWater()) return End(Failure);
            if (!InitialFixture(FVector(-470, 1500, 55))) return End(Failure);
            PreviousSwim = P->GetActorLocation();
            Test->AddInfo(TEXT("One disclosed dry-deck start at(-4.7,15)m. Actual W descends four measured northern pool steps, swims laterally and returns through the same steps. No movement-mode, buoyancy, velocity, collision or post-start pose writes."));
            return Next(1);
        }
        if (!IsValid(Pool)) return End(TEXT("Actual contained pool disappeared"));
        if (P->GetActorLocation().ContainsNaN() || P->GetActorLocation().Z < -140 || P->GetActorLocation().Z > 300)
            return End(TEXT("Pool route produced nonfinite/unsupported player height"));
        if (Stage == 1)
        {
            if (Age < .75f) return false;
            if (!Grounded()) return Age > 3 ? End(TEXT("Actual north dry-deck fixture did not settle walking")) : false;
            Capture(TEXT("supported-dry-deck")); DryStarted = true; StartLeg(); return Next(2);
        }
        if (Stage >= 2 && Stage <= 5)
        {
            if (P->GetCharacterMovement()->IsSwimming())
            {
                Swam = true;
                if (Stage == 3) LateralSwim += FVector::Dist2D(PreviousSwim, P->GetActorLocation());
            }
            PreviousSwim = P->GetActorLocation();
            // X=-4.7 is the actual 2.4m-wide four-step run; other pool edges
            // have vertical shell walls and are deliberately not exit targets.
            const FVector Goals[] = {FVector(-470, 1030, 0), FVector(-150, 1030, 0),
                                     FVector(-470, 1030, 0), FVector(-470, 1500, 55)};
            if (!MoveToward(Goals[Stage - 2], 23)) return Failure.IsEmpty() ? false : End(Failure);
            if (Stage == 2)
            {
                if (!P->GetCharacterMovement()->IsSwimming())
                    return End(FString::Printf(TEXT("Actual four-step descent reached basin but native swimming never engaged; player%s mode%d"),
                        *P->GetActorLocation().ToString(), int32(P->GetCharacterMovement()->MovementMode)));
                EnteredWater = true;
            }
            if (Stage == 3)
            {
                if (!P->GetCharacterMovement()->IsSwimming() || LateralSwim < 120)
                    return End(TEXT("Actual lateral basin travel did not retain native swimming for at least1.2m"));
                Capture(TEXT("actual-native-swimming")); LateralPassed = true;
            }
            if (Stage == 5) return Next(6);
            StartLeg(); return Next(Stage + 1);
        }
        if (Stage == 6)
        {
            if (!Grounded()) return Age > 4 ? End(TEXT("Normal return via pool steps failed dry supported walking")) : false;
            if (Age < .6f) return false;
            if (FMath::Abs(P->GetActorLocation().Z - InitialStand.Z) > 20
                || FVector::Dist2D(P->GetActorLocation(), InitialStand) > 55)
                return End(TEXT("Pool exit did not reach the original dry-deck height and position"));
            const auto* Water = P->FindComponentByClass<UWaterInteractionComponent>();
            if (!Water || Water->GetWaterState() != EWaterInteractionState::Dry)
                return Age > 3 ? End(TEXT("Dry pool exit retained underwater/surface state")) : false;
            ExitedWater = true; Capture(TEXT("normal-steps-dry-exit")); return End(TEXT(""));
        }
        return false;
    }

private:
    bool Next(int32 Value) { Stage = Value; At = W->GetTimeSeconds(); return false; }
    bool CheckWater()
    {
        auto* Marine = W->GetSubsystem<UMarineWorldSubsystem>();
        FWaterSurfaceSample Sample;
        const FVector Centre = Pool->GetActorLocation();
        if (!Marine || !Marine->QueryWater(Centre, Sample) || !Sample.ContainedWater || !Sample.InsideVolume
            || !Sample.Flow.IsNearlyZero(.001f) || Sample.SurfacePoint.ContainsNaN() || Sample.Normal.ContainsNaN()
            || !Sample.Normal.Equals(FVector::UpVector, .001f) || FMath::Abs(Sample.SurfacePoint.Z - 41) > .1f)
        { Failure = TEXT("Native finite pool query is not contained, current-free and aligned with actual41cm water surface"); return false; }
        for (const FVector& Local : {FVector(702, 0, 0), FVector(-702, 0, 0), FVector(0, 252, 0),
                                    FVector(0, -252, 0), FVector(0, 0, -60)})
            if (Pool->QueryWaterSurface_Implementation(Pool->GetActorTransform().TransformPosition(Local), Sample))
            { Failure = TEXT("Contained water query spills beyond its actual basin edge/bottom ") + Local.ToString(); return false; }
        if (!Pool->QueryWaterSurface_Implementation(Centre + FVector(0, 0, 100), Sample) || Sample.InsideVolume)
        { Failure = TEXT("Above-surface pool query incorrectly reports immersion"); return false; }
        WaterBoundsPassed = true; return true;
    }
    bool End(const FString& Error)
    {
        auto Details = MakeShared<FJsonObject>();
        Details->SetStringField(TEXT("pool_actor"), GetPathNameSafe(Pool));
        Details->SetBoolField(TEXT("native_finite_current_free_water_bounds"), WaterBoundsPassed);
        Details->SetBoolField(TEXT("dry_supported_start"), DryStarted);
        Details->SetBoolField(TEXT("normal_steps_entered_native_swimming"), EnteredWater);
        Details->SetBoolField(TEXT("native_swimming_observed"), Swam);
        Details->SetBoolField(TEXT("lateral_swimming_passed"), LateralPassed);
        Details->SetNumberField(TEXT("actual_lateral_swimming_cm"), LateralSwim);
        Details->SetBoolField(TEXT("same_steps_returned_to_dry_walking"), ExitedWater);
        Details->SetStringField(TEXT("scope"), TEXT("One supported dry-deck fixture; actual mapped W descends existing northern four-step run, native CharacterMovement swims laterally over1.2m, then normal return through the same physical steps reaches dry supported walking. Finite current-free provider footprint/bottom checked. No forced swimming, movement mode, velocity, buoyancy, collision, post-start test pose or save writes."));
        return Finish(Error, WaterBoundsPassed && DryStarted && EnteredWater && Swam
            && LateralPassed && ExitedWater && Grounded(), Details);
    }
    ANeonCityWaterVolume* Pool = nullptr;
    FVector PreviousSwim = FVector::ZeroVector;
    float LateralSwim = 0;
    bool WaterBoundsPassed = false, DryStarted = false, EnteredWater = false;
    bool Swam = false, LateralPassed = false, ExitedWater = false;
};

bool ValidateMode(FAutomationTestBase& Test)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityDisableAutomaticPersistence")))
    { Test.AddError(TEXT("Isolated automatic-persistence-disabled gameplay process required")); return false; }
    if (FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtMetrics")))
    { Test.AddError(TEXT("Gateway/pool gameplay and optional still readback are separate from clean performance sampling")); return false; }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandStarterHomeAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.StarterHomeAccess",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FMainlandStarterHomeAccess920::RunTest(const FString&)
{
    if (!MainlandGateway920::ValidateMode(*this)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(MainlandGateway920::FStarterHome(this)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPoolAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.PoolAccess",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FMainlandPoolAccess920::RunTest(const FString&)
{
    if (!MainlandGateway920::ValidateMode(*this)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(MainlandGateway920::FPool(this)); return true;
}
#endif
