#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/NeonCityMainlandSite.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Player/CombatFeedbackWidget.h"
#include "Combat/WeaponStoreActor.h"
#include "Interaction/InteractionComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandCommerce920
{
enum class EStage : uint8
{
    Initialize, SettleStart, WalkOutfitter, FocusStore,
    PressOpen, ObserveOpen, PressClose, ObserveClosed, WalkCafe, ObserveCafe
};

FString FileSHA1(const FString& Path)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return TEXT("unavailable");
    FSHAHash Hash;
    FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash);
    return Hash.ToString();
}

/** Actual normal PlayerStart -> public promenade -> open arch -> lobby side
 * doorway -> outfitter terminal, mapped F browsing/closing, then the opposite
 * cafe doorway. No actor spawn, teleport, pose fixture, ownership, purchase,
 * save, collision or movement-mode writes. Existing character input consumes
 * the submitted movement vectors; F uses the normal Enhanced Input mapping. */
class FAccess : public IAutomationLatentCommand
{
public:
    explicit FAccess(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        if (!Started)
        {
            Started = FPlatformTime::Seconds();
            ConfigureOutput();
        }
        W = NeonCityValidationWorld();
        PC = W ? W->GetFirstPlayerController() : nullptr;
        if (FPlatformTime::Seconds() - Started > 220.)
            return End(TEXT("Commerce access exceeded its finite 220-second wall-time bound"));
        if (!PC || W->GetTimeSeconds() < 8.f) return false;
        if (Stage == EStage::Initialize)
        {
            P = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            if (!Initialize()) return End(Failure);
            return Next(EStage::SettleStart);
        }
        if (!IsValid(P) || !IsValid(Store) || !IsValid(Combat)
            || PC->GetPawn() != P || P->GetAttachParentActor()
            || P->IsHidden() || !P->GetActorEnableCollision() || PC->IsMoveInputIgnored())
            return End(TEXT("Normal visible unseated player, intact collision, store or possession was lost"));
        RecordTrajectory();
        const float Age = W->GetTimeSeconds() - At;

        if (Stage == EStage::SettleStart)
        {
            if (Age < 1.f) return false;
            if (!Grounded())
                return Age > 4.f ? End(TEXT("Normal PlayerStart did not settle onto actual C920 static support")) : false;
            if (FVector::Dist2D(P->GetActorLocation(), FVector(-7000, -2400, 0)) > 150)
                return End(TEXT("Player did not spawn at the normal authored mainland PlayerStart; no test repositioning is allowed"));
            NormalStart = true;
            InitialPlayer = P->GetActorTransform();
            PreviousPlayer = P->GetActorLocation();
            Still(TEXT("initial-normal-PlayerStart"));
            BeginWalk(false);
            return Next(EStage::WalkOutfitter);
        }
        if (Stage == EStage::WalkOutfitter || Stage == EStage::WalkCafe)
        {
            const int32 Result = WalkStep();
            if (Result < 0) return End(Failure);
            if (!Result) return false;
            if (Stage == EStage::WalkOutfitter) return Next(EStage::FocusStore);
            if (Combat->IsStoreOpen()) return End(TEXT("Store remained open after normal F close and walking into the cafe"));
            PC->SetControlRotation((FVector(-2452, -985, 190)-P->GetPawnViewLocation()).Rotation());
            return Next(EStage::ObserveCafe);
        }
        if (Stage == EStage::FocusStore)
        {
            if (!Grounded()) return Age > 3.f ? End(TEXT("Reached outfitter counter approach is not supported normal walking")) : false;
            if (!FocusStore()) return Age > 3.f ? End(FocusFailure()) : false;
            if (Age < .6f) return false;
            PromptSeen = true;
            Still(TEXT("reached-outfitter-focused"));
            Key(EKeys::F, true);
            return Next(EStage::PressOpen);
        }
        if (Stage == EStage::PressOpen)
        {
            if (Age < .3f) return false;
            Key(EKeys::F, false);
            return Next(EStage::ObserveOpen);
        }
        if (Stage == EStage::ObserveOpen)
        {
            if (Age < .5f) return false;
            if (!Combat->IsStoreOpen())
                return Age > 3.f ? End(TEXT("Mapped F did not open the actual reached outfitter store")) : false;
            if (!ReadableStoreWidget())
                return Age > 3.f ? End(TEXT("Actual opened store has no visible Marina Outfitters price/cash/close UI")) : false;
            if (!Grounded() || !FocusStore()) return End(TEXT("Opened store lost supported approach or normal focus/LOS"));
            if (Age < 1.f) return false;
            Opened = true;
            UISeen = true;
            Still(TEXT("normal-F-store-open"));
            Key(EKeys::F, true);
            return Next(EStage::PressClose);
        }
        if (Stage == EStage::PressClose)
        {
            if (Age < .3f) return false;
            Key(EKeys::F, false);
            return Next(EStage::ObserveClosed);
        }
        if (Stage == EStage::ObserveClosed)
        {
            if (Age < .5f) return false;
            if (Combat->IsStoreOpen())
                return Age > 3.f ? End(TEXT("Second normal mapped F failed to close the actual store")) : false;
            if (ReadableStoreWidget())
                return Age > 3.f ? End(TEXT("Closed store left its shop panel visibly open")) : false;
            Closed = true;
            BeginWalk(true);
            return Next(EStage::WalkCafe);
        }
        if (Stage == EStage::ObserveCafe)
        {
            if (!Grounded()) return Age > 3.f ? End(TEXT("Reached right cafe doorway/counter approach has no normal static walking support")) : false;
            if (Age < .6f) return false;
            CafeReached = FVector::Dist2D(P->GetActorLocation(), FVector(-2452, -1120, 0)) < 35;
            if (!CafeReached) return End(TEXT("Normal movement did not reach the actual opposite cafe interior"));
            Still(TEXT("right-cafe-normal-walk"));
            return End(TEXT(""));
        }
        return false;
    }

private:
    bool Initialize()
    {
        if (!P || !ANeonCityMainlandSite::Find(W))
        { Failure = TEXT("Normal mainland player and adopted C920 site are required"); return false; }
        int32 StoreCount = 0;
        for (TActorIterator<AWeaponStoreActor> It(W); It; ++It)
            if (It->GetLevel() == W->PersistentLevel) { Store = *It; ++StoreCount; }
        Combat = P->FindComponentByClass<UPlayerCombatAdapter>();
        if (StoreCount != 1 || !Combat || !P->GetInteractionComponent())
        { Failure = TEXT("Exactly one actual persistent outfitter store and normal player interaction/combat adapter are required"); return false; }
        const auto* Body = Cast<UStaticMeshComponent>(Store->GetRootComponent());
        if (!Body || !Body->GetStaticMesh() || Body->GetStaticMesh()->GetName() != TEXT("SM_C920_LightingPanel")
            || FVector::Dist2D(Store->GetActorLocation(), FVector(-3748, -985, 0)) > 35
            || !Body->GetComponentScale().Equals(FVector::OneVector, .001))
        { Failure = TEXT("Actual store is not the authored unit-scale physical outfitter terminal on the real counter"); return false; }
        if (Combat->IsStoreOpen())
        { Failure = TEXT("Fresh isolated normal player unexpectedly started with an open store"); return false; }
        if (PC->IsMoveInputIgnored() || P->GetAttachParentActor() || !P->GetActorEnableCollision())
        { Failure = TEXT("Normal PlayerStart controls/collision are unavailable; no test fixture will bypass them"); return false; }
        return true;
    }

    bool Next(EStage Value)
    {
        Stage = Value;
        At = W->GetTimeSeconds();
        UnsupportedSince = 0;
        return false;
    }

    void BeginWalk(bool ToCafe)
    {
        if (ToCafe)
            Goals = {FVector(-3380, -1120, 0), FVector(-3100, -1120, 0),
                     FVector(-2820, -1120, 0), FVector(-2452, -1120, 0)};
        else
            Goals = {FVector(-4600, -2400, 0), FVector(-4600, -2150, 0),
                     FVector(-3100, -2150, 0), FVector(-3100, -1800, 0),
                     FVector(-2900, -1800, 0), FVector(-2900, -1465, 0),
                     FVector(-3100, -1465, 0), FVector(-3100, -1120, 0),
                     FVector(-3380, -1120, 0), FVector(-3748, -1120, 0)};
        // X -31 at the facade is a real central pier. The source through-route
        // enters at arch X -29 before turning behind that pier into the lobby.
        // Y -21.5 also avoids the actual new entrance planters and cafe bases.
        Point = 0;
        LegStarted = LastProgress = W->GetTimeSeconds();
        ProgressLocation = P->GetActorLocation();
    }

    bool Floor(FHitResult& Hit) const
    {
        if (!P || !W) return false;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C920CommerceActualSupport), false, P);
        if (!W->LineTraceSingleByObjectType(Hit, P->GetActorLocation()+FVector(0, 0, 10),
            P->GetActorLocation()-FVector(0, 0, 240), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
            || !Hit.IsValidBlockingHit() || !Hit.GetComponent() || Hit.GetComponent()->IsSimulatingPhysics()
            || Hit.ImpactNormal.Z < P->GetCharacterMovement()->GetWalkableFloorZ()) return false;
        const auto* Actor = Cast<AStaticMeshActor>(Hit.GetActor());
        const auto* Component = Actor ? Actor->GetStaticMeshComponent() : nullptr;
        return Component && Component->GetStaticMesh() && Component->GetStaticMesh()->GetName().StartsWith(TEXT("SM_C920_"));
    }

    bool Grounded() const
    {
        FHitResult Hit;
        return P && P->GetCharacterMovement()->IsMovingOnGround() && Floor(Hit)
            && FMath::Abs(P->GetActorLocation().Z-Hit.ImpactPoint.Z-P->GetSimpleCollisionHalfHeight()) < 12;
    }

    int32 WalkStep()
    {
        if (!Goals.IsValidIndex(Point)) return 1;
        if (Combat->IsStoreOpen() || P->GetCharacterMovement()->IsSwimming())
        { Failure = TEXT("Continuous retail access lost ordinary on-foot locomotion or left the shop session open"); return -1; }
        if (!Grounded())
        {
            if (!UnsupportedSince) UnsupportedSince = W->GetTimeSeconds();
            if (W->GetTimeSeconds()-UnsupportedSince > .5f)
            { Failure = TEXT("Continuous retail access lost real collidable C920 static support"); return -1; }
        }
        else UnsupportedSince = 0;
        const FVector Here = P->GetActorLocation();
        WalkDistance += FVector::Dist2D(Here, PreviousPlayer);
        PreviousPlayer = Here;
        const FVector Delta = Goals[Point]-Here;
        if (Delta.Size2D() < 23)
        {
            P->ConsumeMovementInputVector();
            ++Point;
            ++CompletedPoints;
            LegStarted = LastProgress = W->GetTimeSeconds();
            ProgressLocation = Here;
            return Point == Goals.Num() ? 1 : 0;
        }
        if (FVector::Dist2D(Here, ProgressLocation) > 8)
        { LastProgress = W->GetTimeSeconds(); ProgressLocation = Here; }
        if (W->GetTimeSeconds()-LegStarted > 30.f || W->GetTimeSeconds()-LastProgress > 5.f)
        {
            FHitResult Obstacle;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(C920CommerceStalled), false, P);
            W->SweepSingleByChannel(Obstacle, Here, Here+Delta.GetSafeNormal2D()*130,
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),
                P->GetSimpleCollisionHalfHeight()*.85f), Query);
            Failure = FString::Printf(TEXT("Native walk stalled at point %d/%d, pose %s, goal %s, obstacle %s component %s"),
                Point, Goals.Num(), *Here.ToString(), *Goals[Point].ToString(),
                *GetNameSafe(Obstacle.GetActor()), *GetNameSafe(Obstacle.GetComponent()));
            return -1;
        }
        PC->SetControlRotation(FRotator(0, Delta.Rotation().Yaw, 0));
        P->AddMovementInput(Delta.GetSafeNormal2D(), FMath::Clamp(float(Delta.Size2D()/120.), .15f, 1.f));
        return 0;
    }

    bool FocusStore()
    {
        FVector Eye;
        FRotator Rotation;
        P->GetActorEyesViewPoint(Eye, Rotation);
        PC->SetControlRotation((Store->GetActorLocation()-Eye).Rotation());
        auto* Interaction = P->GetInteractionComponent();
        return Store->CanInteract_Implementation(P) && Interaction->CanInteractWith(Store)
            && Interaction->RefreshFocusedInteractable() == Store
            && !Store->GetInteractionName_Implementation().IsEmpty();
    }

    FString FocusFailure() const
    {
        FVector Eye;
        FRotator Rotation;
        P->GetActorEyesViewPoint(Eye, Rotation);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C920CommerceFocusDiagnostic), false, P);
        W->LineTraceSingleByChannel(Hit, Eye, Store->GetActorLocation(), ECC_Visibility, Query);
        return FString::Printf(TEXT("Normal reached eye focus/prompt unavailable; eye %s target %s blocker %s component %s"),
            *Eye.ToString(), *Store->GetActorLocation().ToString(),
            *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
    }

    bool ReadableStoreWidget() const
    {
        for (TObjectIterator<UCombatFeedbackWidget> It; It; ++It)
        {
            if (It->GetWorld() != W || !It->IsInViewport() || !It->IsVisible()
                || It->GetOwningPlayer() != PC || !It->WidgetTree) continue;
            TArray<UWidget*> Widgets;
            It->WidgetTree->GetAllWidgets(Widgets);
            for (const auto* Widget : Widgets)
                if (const auto* Text = Cast<UTextBlock>(Widget))
                {
                    const FString Content = Text->GetText().ToString();
                    if (Text->IsVisible() && Content.Contains(TEXT("Marina Outfitters"))
                        && Content.Contains(TEXT("Cash:")) && Content.Contains(TEXT("F / Esc: Close"))) return true;
                }
        }
        return false;
    }

    void Key(FKey Key, bool Down)
    {
        if (PC) PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
            Down ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    }

    void ConfigureOutput()
    {
        FParse::Value(FCommandLine::Get(), TEXT("NeonCityArtRun="), Run);
        Run = FPaths::MakeValidFileName(Run.IsEmpty() ? TEXT("CommerceAccess") : Run, TEXT('_'));
        Run.ReplaceInline(TEXT(".."), TEXT("_"));
        Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Directory = FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC920/CommerceAccess")/Run/Token;
        IFileManager::Get().MakeDirectory(*Directory, true);
        bTrajectoryFailed = !FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,walk_point,player_x,player_y,player_z,speed_cm_s,grounded,store_open\n"),
            *(Directory/TEXT("trajectory.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        MapHash = FileSHA1(FPaths::ProjectContentDir()/TEXT("NeonCity/Maps/NeonCity_SouthBeach_Test.umap"));
        SourceHash = FileSHA1(FPaths::ProjectDir()/TEXT("Source/NeonCity/Tests/NeonCityMainlandCommerce920Tests.cpp"));
        bStills = FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtStills"));
    }

    void RecordTrajectory()
    {
        if (W->GetTimeSeconds()-LastRecord < .25f) return;
        LastRecord = W->GetTimeSeconds();
        const FVector Position = P->GetActorLocation();
        const FString Row = FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d\n"), LastRecord,
            int32(Stage), Point, Position.X, Position.Y, Position.Z, P->GetVelocity().Size(), Grounded(), Combat->IsStoreOpen());
        if (!FFileHelper::SaveStringToFile(Row, *(Directory/TEXT("trajectory.csv")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
            bTrajectoryFailed = true;
    }

    void Still(const TCHAR* View)
    {
        if (!bStills) return;
        const FString Name = FString::Printf(TEXT("C920-CommerceAccess-%s-%s.jpg"), *Token, View);
#if WITH_EDITOR
        if (FNeonCityVisualEvidence::CaptureRenderedJPEG(W, Name))
            Images.Add(MakeShared<FJsonValueString>(Name));
        else { ++FailedImages; Test->AddError(TEXT("Requested actual commerce screenshot failed: ")+Name); }
#else
        ++FailedImages;
        Test->AddError(TEXT("Actual commerce stills require the editor-enabled offscreen client"));
#endif
    }

    bool End(const FString& Error)
    {
        if (!Error.IsEmpty())
        {
            Failure = FString::Printf(TEXT("CommerceAccess stage %d point %d: %s; player %s"), int32(Stage), Point,
                *Error, IsValid(P) ? *P->GetActorLocation().ToString() : TEXT("absent"));
            Test->AddError(Failure);
        }
        Key(EKeys::F, false);
        if (IsValid(P))
        {
            P->ConsumeMovementInputVector();
            if (P->GetCharacterMovement()) P->GetCharacterMovement()->StopMovementImmediately();
        }
        // Failure cleanup does not satisfy the normal-F-closed acceptance gate.
        if (IsValid(Combat) && Combat->IsStoreOpen()) Combat->CloseStore();
        const bool Passed = Failure.IsEmpty() && NormalStart && PromptSeen && Opened && UISeen && Closed
            && CafeReached && CompletedPoints == 14 && WalkDistance >= 6500 && !FailedImages
            && (!bStills || Images.Num() == 4) && !bTrajectoryFailed;
        if (Failure.IsEmpty() && !Passed)
        { Failure = TEXT("Sequence ended without every declared normal commerce access/evidence gate"); Test->AddError(Failure); }
        auto Receipt = MakeShared<FJsonObject>();
        Receipt->SetStringField(TEXT("test"), TEXT("CommerceAccess"));
        Receipt->SetBoolField(TEXT("passed"), Passed);
        Receipt->SetStringField(TEXT("failure"), Failure);
        Receipt->SetNumberField(TEXT("final_stage"), int32(Stage));
        Receipt->SetBoolField(TEXT("normal_player_start_observed"), NormalStart);
        Receipt->SetStringField(TEXT("initial_player_transform"), InitialPlayer.ToString());
        Receipt->SetStringField(TEXT("final_player_cm"), IsValid(P) ? P->GetActorLocation().ToString() : TEXT("absent"));
        Receipt->SetStringField(TEXT("actual_store"), GetPathNameSafe(Store));
        Receipt->SetBoolField(TEXT("normal_focused_interaction_available"), PromptSeen);
        Receipt->SetBoolField(TEXT("normal_F_opened"), Opened);
        Receipt->SetBoolField(TEXT("actual_visible_store_UI"), UISeen);
        Receipt->SetBoolField(TEXT("normal_F_closed"), Closed);
        Receipt->SetBoolField(TEXT("opposite_cafe_reached_on_foot"), CafeReached);
        Receipt->SetNumberField(TEXT("completed_walk_waypoints"), CompletedPoints);
        Receipt->SetNumberField(TEXT("continuous_walk_distance_cm"), WalkDistance);
        Receipt->SetNumberField(TEXT("failed_images"), FailedImages);
        Receipt->SetBoolField(TEXT("trajectory_write_failed"), bTrajectoryFailed);
        Receipt->SetArrayField(TEXT("actual_player_images"), Images);
        Receipt->SetStringField(TEXT("map_sha1"), MapHash);
        Receipt->SetStringField(TEXT("test_source_sha1"), SourceHash);
        Receipt->SetBoolField(TEXT("cacheable_success"), Passed);
        Receipt->SetBoolField(TEXT("clean_performance_sample"), false);
        Receipt->SetStringField(TEXT("scope"), TEXT("Actual untouched normal mainland PlayerStart; continuous CharacterMovement around real furniture into source arcade, lobby and two real retail side portals. Eye-focused actual outfitter terminal and native visible price/cash UI open and close through mapped F. No teleport/pose fixture, actor substitute, purchases, ownership/save writes, collision disable or normal input changes. Opposite cafe walking access only; subjective art remains separate."));
        Receipt->SetStringField(TEXT("cache_policy"), TEXT("Reuse only while relevant Marina geometry, portal/counter/retail furniture placement, store collision/interaction/UI and normal player movement/input remain unchanged; failed attempts never replace successful evidence."));
        FString Json;
        FJsonSerializer::Serialize(Receipt, TJsonWriterFactory<>::Create(&Json));
        if (!FFileHelper::SaveStringToFile(Json, *(Directory/TEXT("receipt.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            Test->AddError(TEXT("Could not write unique commerce access receipt"));
        Test->AddInfo(TEXT("Unique actual commerce access receipt: ")+Directory/TEXT("receipt.json"));
        return true;
    }

    FAutomationTestBase* Test;
    EStage Stage = EStage::Initialize;
    UWorld* W = nullptr;
    APlayerController* PC = nullptr;
    ANeonCityPlayerCharacter* P = nullptr;
    AWeaponStoreActor* Store = nullptr;
    UPlayerCombatAdapter* Combat = nullptr;
    TArray<FVector> Goals;
    TArray<TSharedPtr<FJsonValue>> Images;
    FVector PreviousPlayer = FVector::ZeroVector, ProgressLocation = FVector::ZeroVector;
    FTransform InitialPlayer = FTransform::Identity;
    FString Failure, Run, Token, Directory, MapHash, SourceHash;
    double Started = 0;
    float At = 0, LegStarted = 0, LastProgress = 0, UnsupportedSince = 0, LastRecord = 0, WalkDistance = 0;
    int32 Point = 0, CompletedPoints = 0, FailedImages = 0;
    bool NormalStart = false, PromptSeen = false, Opened = false, UISeen = false, Closed = false, CafeReached = false;
    bool bStills = false, bTrajectoryFailed = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCommerceAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.CommerceAccess",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FMainlandCommerceAccess920::RunTest(const FString&)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityDisableAutomaticPersistence")))
    { AddError(TEXT("Commerce access requires isolated automatic-persistence-disabled validation; no live save writes")); return false; }
    if (FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtMetrics")))
    { AddError(TEXT("Commerce gameplay and optional screenshot readback must run separately from clean timing")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(MainlandCommerce920::FAccess(this));
    return true;
}
#endif
