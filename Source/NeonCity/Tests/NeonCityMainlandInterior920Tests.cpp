#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractiveBuildingDoor.h"
#include "Property/NeonCityStarterApartment.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Adventure/IslandHomeSeat.h"
#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Adventure/IslandLandLight.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandInterior920
{
constexpr TCHAR EstateID[] = TEXT("Property.Mainland.RivieraEstate916");
constexpr TCHAR StarterID[] = TEXT("NeonCity.Property.StarterApartment");
constexpr TCHAR ManifestPath[] = TEXT("ArtSource/Phase205cUpgrade/MainlandC920/Architecture/Revision1/architecture.json");

TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key)
{
    return Parent && Parent->HasTypedField<EJson::Object>(Key) ? Parent->GetObjectField(Key) : nullptr;
}

bool VectorField(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key, FVector& Value)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (!Parent || !Parent->TryGetArrayField(Key, Array) || !Array || Array->Num() != 3) return false;
    double V[3];
    for (int32 I = 0; I < 3; ++I)
        if (!(*Array)[I].IsValid() || !(*Array)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I])) return false;
    Value = FVector(V[0], V[1], V[2]);
    return true;
}

bool VectorArray(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key, TArray<FVector>& Values)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (!Parent || !Parent->TryGetArrayField(Key, Array) || !Array) return false;
    for (const auto& Entry : *Array)
    {
        if (!Entry || Entry->Type != EJson::Array || Entry->AsArray().Num() != 3) return false;
        const auto& V = Entry->AsArray();
        double N[3];
        for (int32 I = 0; I < 3; ++I)
            if (!V[I] || !V[I]->TryGetNumber(N[I]) || !FMath::IsFinite(N[I])) return false;
        Values.Add(FVector(N[0], N[1], N[2]));
    }
    return !Values.IsEmpty();
}

enum class EKind : uint8 { Walk, UseChair, FocusResidence };
struct FStop
{
    FString Name;
    EKind Kind = EKind::Walk;
    AActor* Building = nullptr;
    FVector LocalFloor = FVector::ZeroVector; // native metres, actual floor level
    AIslandHomeSeat* Chair = nullptr;
    bool AuthoredStairSegment = false;
};

/** Exactly one disclosed pose fixture at the start. All later corridor/stair/
 * exterior travel uses normal CharacterMovement and intact native collision.
 * Normal F seat attachment and normal W safe unseat remain production behavior.
 * Starter property's short TeleportTo gateway is NOT used to skip its leaf;
 * this access test reaches and validates that real normal focused prompt. */
class FAccess : public IAutomationLatentCommand
{
public:
    explicit FAccess(FAutomationTestBase* InTest, bool InRemainingOnly=false) : Test(InTest), bRemainingOnly(InRemainingOnly) {}

    bool Update() override
    {
        if (!Started)
        {
            Started = FPlatformTime::Seconds();
            ConfigureOutput();
        }
        W = NeonCityValidationWorld();
        PC = W ? W->GetFirstPlayerController() : nullptr;
        if (Cleaning) return Cleanup();
        if (FPlatformTime::Seconds() - Started > (bRemainingOnly?120.:240.)) return End(TEXT("Interior access exceeded its finite route timeout"));
        if (!PC || W->GetTimeSeconds() < 6.f) return false;
        if (!P)
        {
            P = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            if (!P) return End(TEXT("Normal on-foot NeonCity player absent"));
            if (!Initialize()) return End(Failure);
            StageStarted = W->GetTimeSeconds();
            return false;
        }
        if (!IsValid(P) || PC->GetPawn() != P) return End(TEXT("Player possession changed during physical interior route"));
        if (Stage == 0)
        {
            if (W->GetTimeSeconds() - StageStarted < 1.f) return false;
            if (!P->GetCharacterMovement()->IsMovingOnGround()) return End(TEXT("Declared initial fixture did not settle onto native promenade support"));
            Stage = 1;
        }
        if (Stage == 1) return WalkStep();
        const float Age = W->GetTimeSeconds() - StageStarted;
        if (Stage == 2)
        {
            // Settling is observed, not forced by changing movement mode.
            if (!P->GetCharacterMovement()->IsMovingOnGround())
                return Age > 2.f ? End(TEXT("Reached furnishing approach did not settle grounded")) : false;
            if (Age < .4f) return false;
            if (!Focus(ActiveChair)) return End(FString::Printf(TEXT("Normal F prompt occluded at reached%s approach"), *GetNameSafe(ActiveChair)));
            Key(EKeys::F, true);
            Stage = 3; StageStarted = W->GetTimeSeconds();
            return false;
        }
        if (Stage == 3 && Age > .3f)
        {
            Key(EKeys::F, false);
            if (!ActiveChair || !ActiveChair->IsOccupied() || !ActiveChair->Occupant ||
                !ActiveChair->Occupant->GetDriverMesh())
                return End(TEXT("Normal F failed actual fitted chair/cinema entry and visible seated body"));
            Stage = 4; StageStarted = W->GetTimeSeconds();
            SawCinemaFeedback = false;
            return false;
        }
        if (Stage == 4)
        {
            if (!ActiveChair || !ActiveChair->IsOccupied()) return End(TEXT("Furnishing unexpectedly released its normal seated session"));
            if (auto* Leisure = Cast<AIslandLeisureFacility>(ActiveChair))
                SawCinemaFeedback |= Leisure->IsFeedbackPlaying() && Leisure->Screen && Leisure->Screen->IsVisible();
            if (Age < 2.8f) return false;
            if (Cast<AIslandLeisureFacility>(ActiveChair) && !SawCinemaFeedback)
                return End(TEXT("Actual cinema entry has no live screen/audio feedback"));
            auto* Body = ActiveChair->Occupant->GetDriverMesh();
            if (!Body || !Body->IsVisible()) return End(TEXT("Seated animation presentation is not visible"));
            Still(ActiveChair==Cinema?TEXT("Cinema-normal-seated-view"):TEXT("Upstairs-normal-seated-view"));
            Key(EKeys::W, true);
            Stage = 5; StageStarted = W->GetTimeSeconds();
            return false;
        }
        if (Stage == 5 && Age > .3f)
        {
            Key(EKeys::W, false);
            Stage = 6; StageStarted = W->GetTimeSeconds();
            return false;
        }
        if (Stage == 6)
        {
            if (ActiveChair && ActiveChair->IsOccupied())
                return Age > 8.f ? End(TEXT("Normal W-unseat could not find a supported safe exit")) : false;
            if (!P->GetCharacterMovement()->IsMovingOnGround() || PC->IsMoveInputIgnored() ||
                !P->GetActorEnableCollision() || P->GetAttachParentActor())
                return Age > 2.f ? End(TEXT("Unseat did not restore normal grounded movement/collision")) : false;
            if (ActiveChair == Cinema) CinemaUsed = true; else UpstairsChairUsed = true;
            ActiveChair = nullptr;
            return Advance();
        }
        return false;
    }

private:
    static FString FileSHA1(const FString& Relative)
    {
        TArray<uint8> Bytes;
        if(!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/Relative)))return TEXT("unavailable");
        FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);
        return Hash.ToString();
    }
    void ConfigureOutput()
    {
        FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),RunLabel);
        RunLabel=FPaths::MakeValidFileName(RunLabel.IsEmpty()?TEXT("MainlandC920-InteriorAccess"):RunLabel,TEXT('_'));
        RunLabel.ReplaceInline(TEXT(".."),TEXT("_"));
        RunToken=FString::Printf(TEXT("%s-%u-%llu"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S")),
            FPlatformProcess::GetCurrentProcessId(),static_cast<unsigned long long>(FPlatformTime::Cycles64()));
        ReceiptDirectory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/RunLabel/TEXT("InteriorAccess")/RunToken;
        bStills=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"));
        ManifestSHA1=FileSHA1(ManifestPath);
        SourceSHA1=FileSHA1(TEXT("Source/NeonCity/Tests/NeonCityMainlandInterior920Tests.cpp"));
        MapSHA1=FileSHA1(TEXT("Content/NeonCity/Maps/NeonCity_SouthBeach_Test.umap"));
    }
    void Still(const FString& View)
    {
        if(!bStills)return;
        const FString Name=FString::Printf(TEXT("Interior920-%s-stop%03d-%s.jpg"),*RunToken,Stop,*FPaths::MakeValidFileName(View,TEXT('_')));
#if WITH_EDITOR
        if(FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Name))
        {
            CapturedStills.Add(MakeShared<FJsonValueString>(Name));
            Test->AddInfo(FString::Printf(TEXT("Actual normal-player rendered frame retained: %s"),*Name));
        }
        else
        {++FailedStills;Test->AddWarning(FString::Printf(TEXT("Requested normal-player frame unavailable: %s"),*Name));}
#else
        ++FailedStills;
        Test->AddWarning(TEXT("Normal-player still capture requires the editor-enabled offscreen validation build"));
#endif
    }
    AStaticMeshActor* Building(const TCHAR* MeshName)
    {
        AStaticMeshActor* Found = nullptr;
        for (TActorIterator<AStaticMeshActor> I(W); I; ++I)
            if (I->GetLevel() == W->PersistentLevel && I->GetStaticMeshComponent()->GetStaticMesh() &&
                I->GetStaticMeshComponent()->GetStaticMesh()->GetName() == MeshName)
            {
                if (Found) { Failure = FString::Printf(TEXT("Multiple persistent%s fixtures"), MeshName); return nullptr; }
                Found = *I;
            }
        return Found;
    }

    bool Initialize()
    {
        FString Text;
        TSharedPtr<FJsonObject> Manifest;
        if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / ManifestPath)) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest))
        { Failure = TEXT("Immutable Revision1 architecture metadata missing/unreadable"); return false; }
        const auto Addresses = Object(Manifest, TEXT("addresses"));
        CourtData = Object(Addresses, TEXT("SM_C920_Courtyard21"));
        GalleryData = Object(Addresses, TEXT("SM_C920_Gallery27"));
        if (!CourtData || !GalleryData || !VectorArray(CourtData, TEXT("through_route"), Through) ||
            !VectorArray(CourtData, TEXT("stair_route"), Stairs) || Stairs.Num() != 10)
        { Failure = TEXT("Revision1 through-route/two dog-leg stair contracts absent"); return false; }
        Court = Building(TEXT("SM_C920_Courtyard21_Opaque"));
        Gallery = Building(TEXT("SM_C920_Gallery27_Opaque"));
        if (!Court || !Gallery) { if (Failure.IsEmpty()) Failure = TEXT("Actual native Courtyard21/Gallery27 opaque fixtures absent"); return false; }
        if (!Court->GetActorScale3D().Equals(FVector::OneVector,.001) ||
            !Gallery->GetActorScale3D().Equals(FVector::OneVector,.001))
        { Failure = TEXT("Native building scale differs from metre metadata"); return false; }
        if (PC->IsMoveInputIgnored() || P->GetAttachParentActor() || !P->GetActorEnableCollision())
        { Failure = TEXT("Isolated start requires normal unseated player controls/collision"); return false; }

        int32 Seats = 0, Beds = 0, Wardrobes = 0;
        for (TActorIterator<AIslandHomeSeat> I(W); I; ++I)
        {
            if (I->GetLevel() != W->PersistentLevel || I->PropertyID != EstateID) continue;
            ++Seats; if (I->bSleepBed) ++Beds;
            if (I->GetFName() == TEXT("IslandHomeSeat_1")) UpstairsChair = *I;
            if (auto* L = Cast<AIslandLeisureFacility>(*I); L && L->Kind == EIslandLeisureKind::Cinema) Cinema = L;
        }
        for (TActorIterator<AIslandHomeWardrobe> I(W); I; ++I)
            if (I->GetLevel() == W->PersistentLevel && I->PropertyID == EstateID) ++Wardrobes;
        for (TActorIterator<ANeonCityStarterApartment> I(W); I; ++I)
            if (I->GetLevel() == W->PersistentLevel && I->GetPropertyComponent() &&
                I->GetPropertyComponent()->GetPropertyID() == StarterID) Home = *I;
        for (TActorIterator<AInteractiveBuildingDoor> I(W); I; ++I)
            if (I->GetLevel() == W->PersistentLevel && I->GetFName() == TEXT("InteractiveBuildingDoor_0")) ServiceDoor = *I;
        auto Require = [this](bool Condition, const FString& Description)
        {
            Test->AddInfo(FString::Printf(TEXT("C920 initialize %s: %s"),Condition?TEXT("PASS"):TEXT("FAIL"),*Description));
            if (!Condition)
            {
                if (!Failure.IsEmpty()) Failure += TEXT("; ");
                Failure += Description;
            }
            return Condition;
        };
        bool Ready = true;
        Ready &= Require(Seats >= 35,FString::Printf(TEXT("Retained estate seats >=35, actual%d"),Seats));
        Ready &= Require(Beds == 6,FString::Printf(TEXT("Retained estate beds ==6, actual%d"),Beds));
        Ready &= Require(Wardrobes == 6,FString::Printf(TEXT("Retained estate wardrobes ==6, actual%d"),Wardrobes));
        Ready &= Require(UpstairsChair && !UpstairsChair->bSleepBed,
            FString::Printf(TEXT("Representative upstairs chair exists and is not a bed: %s"),*GetPathNameSafe(UpstairsChair)));
        Ready &= Require(UpstairsChair && UpstairsChair->Occupant && UpstairsChair->Occupant->SeatedAnimation,
            TEXT("Representative upstairs chair has its original occupant component and seated animation"));
        Ready &= Require(Cinema != nullptr,TEXT("Existing estate Cinema gameplay actor exists"));
        Ready &= Require(Cinema && Cinema->Screen && Cinema->Screen->GetStaticMesh(),TEXT("Cinema has its real native film screen"));
        Ready &= Require(Cinema && Cinema->ScenePanel,TEXT("Cinema retains its scene panel binding"));
        Ready &= Require(Cinema && Cinema->ScenePanel && Cinema->ScenePanel->ZoneID == TEXT("Mainland916.Estate.Lighting"),
            TEXT("Cinema scene panel has the exact retained estate lighting zone"));
        Ready &= Require(Cinema && Cinema->ScenePanel && !Cinema->ScenePanel->Lights.IsEmpty(),
            FString::Printf(TEXT("Cinema scene panel has live lamp bindings, count%d"),Cinema && Cinema->ScenePanel?Cinema->ScenePanel->Lights.Num():0));
        Ready &= Require(Home != nullptr,TEXT("Original starter apartment property actor exists"));
        Ready &= Require(ServiceDoor != nullptr,TEXT("Original service door exists"));

        // Rebind keeps the licensed chair as a separate fitted StaticMeshActor.
        // Cinema.Furniture intentionally has no mesh; duplicating it would put
        // a second collidable chair onto the same normal player seat anchor.
        const FName CinemaFixtureTag(TEXT("NeonCity.MainlandC920.Fixture.IslandLeisureFacility_0"));
        AStaticMeshActor* CinemaFixture = nullptr;
        int32 CinemaFixtures = 0;
        for (TActorIterator<AStaticMeshActor> I(W); I; ++I)
            if (I->GetLevel() == W->PersistentLevel && I->ActorHasTag(CinemaFixtureTag))
            { CinemaFixture = *I; ++CinemaFixtures; }
        Ready &= Require(CinemaFixtures == 1,FString::Printf(TEXT("Exactly one persistent tagged fitted cinema chair, actual%d"),CinemaFixtures));
        auto* ChairMesh = CinemaFixture?CinemaFixture->GetStaticMeshComponent():nullptr;
        UStaticMesh* NativeChair = ChairMesh?ChairMesh->GetStaticMesh():nullptr;
        Ready &= Require(NativeChair && NativeChair->GetFName() == TEXT("SM_NC_Interior_Armchair"),
            FString::Printf(TEXT("Fitted cinema has its actual native armchair mesh: %s"),*GetPathNameSafe(NativeChair)));
        Ready &= Require(CinemaFixture && ChairMesh && ChairMesh->IsRegistered() && ChairMesh->IsVisible() && !CinemaFixture->IsHidden(),
            TEXT("Fitted cinema chair is registered and rendered"));
        Ready &= Require(CinemaFixture && CinemaFixture->GetActorEnableCollision() && ChairMesh && ChairMesh->IsQueryCollisionEnabled() &&
            ChairMesh->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block,TEXT("Fitted cinema chair retains actual Pawn-blocking query collision"));
        if (Cinema && ChairMesh && NativeChair)
        {
            const FBox Bounds = NativeChair->GetBoundingBox();
            const FVector LocalAnchor = ChairMesh->GetComponentTransform().InverseTransformPosition(Cinema->GetActorLocation());
            const FVector Hip = Cinema->GetActorTransform().TransformPosition(Cinema->SeatHipLocal);
            const FVector LocalHip = ChairMesh->GetComponentTransform().InverseTransformPosition(Hip);
            constexpr double AlignmentTolerance = 10.; // centimetres, native fitted furniture units
            Test->AddInfo(FString::Printf(TEXT("C920 cinema fitted actor=%s mesh=%s world_bounds_origin=%s world_bounds_extent=%s local_bounds_min=%s max=%s anchor=%s local_anchor=%s hip=%s local_hip=%s collision=%d pawn_response=%d"),
                *CinemaFixture->GetPathName(),*NativeChair->GetPathName(),*ChairMesh->Bounds.Origin.ToString(),*ChairMesh->Bounds.BoxExtent.ToString(),
                *Bounds.Min.ToString(),*Bounds.Max.ToString(),*Cinema->GetActorLocation().ToString(),*LocalAnchor.ToString(),*Hip.ToString(),*LocalHip.ToString(),
                static_cast<int32>(ChairMesh->GetCollisionEnabled()),static_cast<int32>(ChairMesh->GetCollisionResponseToChannel(ECC_Pawn))));
            Ready &= Require(Bounds.IsValid && Bounds.ExpandBy(AlignmentTolerance).IsInsideOrOn(LocalHip),
                TEXT("Cinema seated hip lies within the real fitted chair bounds (10cm tolerance)"));
            Ready &= Require(Bounds.IsValid && FMath::Abs(LocalAnchor.Z-Bounds.Min.Z) <= AlignmentTolerance &&
                LocalAnchor.X >= Bounds.Min.X-AlignmentTolerance && LocalAnchor.X <= Bounds.Max.X+AlignmentTolerance &&
                LocalAnchor.Y >= Bounds.Min.Y-AlignmentTolerance && LocalAnchor.Y <= Bounds.Max.Y+AlignmentTolerance,
                TEXT("Cinema seat anchor aligns with the actual chair footprint and bottom (10cm tolerance)"));
        }
        else Ready &= Require(false,TEXT("Cinema fitted-chair bounds cannot be validated without both real fixture and gameplay anchor"));
        if (!Ready) return false;
        auto* Properties = W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
        if (!Require(Properties != nullptr,TEXT("Existing property subsystem is available; test will not grant or rewrite ownership"))) return false;
        Ready &= Require(Properties->IsOwned(EstateID),TEXT("Existing estate ownership is available"));
        Ready &= Require(Properties->IsOwned(StarterID),TEXT("Existing starter ownership is available"));
        if (!Ready) return false;
        int32 LampIndex = 0;
        for (const auto& Lamp : Cinema->ScenePanel->Lights)
        {
            Ready &= Require(IsValid(Lamp),FString::Printf(TEXT("Cinema lighting panel lamp%d is live: %s"),LampIndex++,*GetPathNameSafe(Lamp.Get())));
        }
        if (!Ready) return false;
        Ready &= Require(!UpstairsChair->IsOccupied(),TEXT("Actual representative upstairs chair is vacant at isolated start"));
        Ready &= Require(!Cinema->IsOccupied(),TEXT("Actual cinema is vacant at isolated start"));
        if (!Ready) return false;
        Test->AddInfo(FString::Printf(TEXT("Native retained fixtures: %d seats including6 beds;6 wardrobes; actual cinema screen/panel/lamps and original starter/service door"), Seats));

        // Disclosed initial placement ONLY. No later test code sets a pose.
        const FVector StartFloor = Court->GetActorTransform().TransformPosition((bRemainingOnly?Through.Last():Through[0])*100.);
        FVector Stand;
        if (!FloorGoal(StartFloor, Stand)) { Failure = TEXT("Initial promenade fixture is unsupported/capsule blocked: ")+Failure; return false; }
        InitialStand = Stand;
        P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->StopMovementImmediately();
        Test->AddInfo(FString::Printf(TEXT("Declared isolated supported on-foot start%s; no post-start pose/collision writes"),*Stand.ToString()));
        BuildStops();
        if(bRemainingOnly)Still(TEXT("Initial-supported-physical-rear-exit"));
        return Failure.IsEmpty();
    }

    void Add(AActor* B, FVector Floor, const FString& Name, bool AuthoredStairSegment=false)
    { FStop S; S.Name=Name; S.Building=B; S.LocalFloor=Floor; S.AuthoredStairSegment=AuthoredStairSegment; Stops.Add(S); }
    void SeatStop(AIslandHomeSeat* Chair, const TCHAR* Name)
    { FStop S; S.Name=Name; S.Kind=EKind::UseChair; S.Building=Court; S.Chair=Chair; Stops.Add(S); }

    void BuildStops()
    {
        if(bRemainingOnly){BuildRemainingStops();return;}
        Add(Court,Through[1],TEXT("Arcade inside approach"));
        Add(Court,Through[2],TEXT("Arcade to centre lobby"));
        Add(Court,Through[3],TEXT("Actual open ground lobby"));
        Add(Court,FVector(0,-2.7,0),TEXT("Ground cinema/stair hall"));
        Add(Court,FVector(-3.3,-2.7,0),TEXT("Actual west hall doorway"));
        Add(Court,FVector(-3.3,-.4,0),TEXT("Ground stair approach"));
        for (int32 Floor=0; Floor<2; ++Floor)
        {
            const int32 Start=Floor*5;
            Add(Court,Stairs[Start],FString::Printf(TEXT("Stair%d bottom"),Floor+1));
            Add(Court,Stairs[Start+1],FString::Printf(TEXT("Stair%d lower flight"),Floor+1),true);
            Add(Court,FVector(Stairs[Start+1].X,Stairs[Start+2].Y,Stairs[Start+2].Z),
                FString::Printf(TEXT("Stair%d full turning landing"),Floor+1),true);
            Add(Court,Stairs[Start+2],FString::Printf(TEXT("Stair%d return flight entry"),Floor+1),true);
            Add(Court,Stairs[Start+3],FString::Printf(TEXT("Stair%d upper landing"),Floor+1),true);
            Add(Court,Stairs[Start+4],FString::Printf(TEXT("Stair%d hall connection"),Floor+1),true);
            const float Z=Stairs[Start+4].Z;
            Add(Court,FVector(0,-.28,Z),TEXT("Upper central hall"));
            Add(Court,FVector(0,-3.7,Z),TEXT("Upper west room door approach"));
            Add(Court,FVector(-3.0,-3.7,Z),TEXT("Actual front lounge doorway"));
            if (Floor==0) SeatStop(UpstairsChair,TEXT("Normal upstairs chair F/use/W-unseat"));
            Add(Court,FVector(0,-3.7,Z),TEXT("Return to upper hall"));
            Add(Court,FVector(0,-3.5,Z),TEXT("Upper front suite approach"));
            Add(Court,FVector(3.0,-3.5,Z),TEXT("Actual front suite doorway"));
            Add(Court,FVector(0,-3.5,Z),TEXT("Upper front suite return"));
            Add(Court,FVector(0,4.5,Z),TEXT("Upper rear suite approach"));
            Add(Court,FVector(3.0,4.5,Z),TEXT("Actual rear suite doorway"));
            Add(Court,FVector(0,4.5,Z),TEXT("Upper rear suite return"));
            Add(Court,FVector(0,6.1,Z),TEXT("Upper rear study approach"));
            Add(Court,FVector(-3.0,6.1,Z),TEXT("Actual rear study doorway"));
            Add(Court,FVector(0,6.1,Z),TEXT("Upper rear study return"));
            Add(Court,FVector(0,-.28,Z),TEXT("Upper return to stair hall"));
            Add(Court,Stairs[Start+4],TEXT("Upper stair hall"));
        }
        for (int32 Floor=1; Floor>=0; --Floor)
        {
            const int32 Start=Floor*5;
            Add(Court,Stairs[Start+3],TEXT("Descent upper landing"),true);
            Add(Court,Stairs[Start+2],TEXT("Descent return flight"),true);
            Add(Court,FVector(Stairs[Start+1].X,Stairs[Start+2].Y,Stairs[Start+2].Z),TEXT("Descent full turning landing"),true);
            Add(Court,Stairs[Start+1],TEXT("Descent lower flight entry"),true);
            Add(Court,Stairs[Start],TEXT("Descent bottom"),true);
            if(Floor==1)Add(Court,Stairs[4],TEXT("First floor connected stair transfer"),true);
        }
        Add(Court,FVector(-3.3,-.4,0),TEXT("Ground stair return"));
        Add(Court,FVector(-3.3,-2.7,0),TEXT("Cinema actual room access"));
        SeatStop(Cinema,TEXT("Normal cinema F/use/W-unseat"));
        Add(Court,FVector(0,-2.7,0),TEXT("Ground lobby after cinema"));
        Add(Court,Through.Last(),TEXT("Physical rear residence exit"));
        BuildRemainingStops();
    }

    void BuildRemainingStops()
    {
        // Keep the first leg on the real raised connector before travelling
        // east. No unsupported diagonal shortcut across the lower lawn.
        Add(Court,FVector(0,11.5,0),TEXT("Rear garden west stone landing"));
        // Pool remains intact. The short connection follows the rear court
        // between the buildings and the pool's south edge, not through water.
        FVector GaragePortal;
        if (!VectorField(Object(GalleryData,TEXT("garage_rear")),TEXT("centre"),GaragePortal))
        { Failure=TEXT("Revision1 actual Gallery vehicle portal metadata absent");return; }
        Add(Gallery,FVector(0,GaragePortal.Y+1.5,0),TEXT("Continuous rear court connection"));
        Add(Gallery,GaragePortal+FVector(0,2.5,0),TEXT("Actual garage exterior approach"));
        Add(Gallery,GaragePortal-FVector(0,4.,0),TEXT("Through actual6.2m vehicle portal"));
        Add(Gallery,GaragePortal+FVector(0,2.5,0),TEXT("Garage physical exit"));
        Add(Gallery,FVector(0,GaragePortal.Y+1.0,0),TEXT("Rear court to centre lobby"));
        FVector RearPortal;
        if (!VectorField(Object(GalleryData,TEXT("rear_passage")),TEXT("centre"),RearPortal))
        { Failure=TEXT("Gallery rear pedestrian portal metadata absent");return; }
        Add(Gallery,RearPortal-FVector(0,1.7,0),TEXT("Actual rear pedestrian doorway"));
        FVector ResidenceDoor;
        if (!VectorField(Object(GalleryData,TEXT("starter_residence")),TEXT("door"),ResidenceDoor))
        { Failure=TEXT("Gallery starter residence doorway metadata absent");return; }
        Add(Gallery,FVector(0,ResidenceDoor.Y,0),TEXT("Gallery lobby to original residence prompt"));
        FStop Prompt;Prompt.Name=TEXT("Original starter home normal focused prompt");Prompt.Kind=EKind::FocusResidence;Stops.Add(Prompt);
    }

    bool FloorGoal(const FVector& Feet, FVector& Goal)
    {
        FHitResult Floor;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(C920InteriorFloorGoal),false,P);
        if (!W->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,40),Feet-FVector(0,0,55),ECC_Pawn,Q) ||
            Floor.ImpactNormal.Z<.7 || !P->GetCharacterMovement()->IsWalkable(Floor) ||
            FMath::Abs(Floor.ImpactPoint.Z-Feet.Z)>15)
        { Failure=FString::Printf(TEXT("Expected floor%s unsupported or wrong level; hit%s at%s"),*Feet.ToString(),*GetNameSafe(Floor.GetActor()),*Floor.ImpactPoint.ToString());return false; }
        Goal=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
        if(W->OverlapBlockingTestByChannel(Goal,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))
        {Failure=FString::Printf(TEXT("Original capsule blocked at supported goal%s"),*Goal.ToString());return false;}
        return true;
    }

    bool Focus(AActor* Target)
    {
        if(!IsValid(Target))return false;
        FVector Eye;FRotator Rotation;P->GetActorEyesViewPoint(Eye,Rotation);
        FVector Origin,Extent;Target->GetActorBounds(true,Origin,Extent);
        auto* Interaction=P->GetInteractionComponent();
        if(!Interaction)return false;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(C920ActualFocus),false,P);
        for(float X:{-.75f,0.f,.75f})for(float Y:{-.75f,0.f,.75f})for(float Z:{-.4f,0.f,.75f})
        {
            const FVector D=(Origin+Extent*FVector(X,Y,Z)-Eye).GetSafeNormal();FHitResult Hit;
            if(W->LineTraceSingleByChannel(Hit,Eye,Eye+D*Interaction->InteractionDistance,Interaction->TraceChannel,Q) && Hit.GetActor()==Target)
            {PC->SetControlRotation(D.Rotation());if(Interaction->RefreshFocusedInteractable()==Target)return true;}
        }
        return false;
    }

    bool ChairApproach(AIslandHomeSeat* Chair)
    {
        const FVector Center=Chair->GetActorLocation();
        TArray<FVector> Candidates;
        // The fitted cinema has an aisle approached diagonally between seats.
        // Cardinal-only fixtures would incorrectly reject that real approach.
        for(float Radius:{140.f,175.f,205.f})for(int32 Direction=0;Direction<8;++Direction)
        {
            const float A=Direction*PI/4.f;
            Candidates.Add(Center+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius);
        }
        Candidates.Sort([this](const FVector& A,const FVector& B){return FVector::DistSquared2D(A,P->GetActorLocation())<FVector::DistSquared2D(B,P->GetActorLocation());});
        FCollisionQueryParams Q(SCENE_QUERY_STAT(C920ChairApproachSight),false,P);
        for(const FVector& Feet:Candidates)
        {
            FVector Goal;
            if(!FloorGoal(Feet,Goal))continue;
            FHitResult Hit;
            const FVector Eye=Goal+FVector(0,0,P->BaseEyeHeight);
            bool Visible=false;
            for(float Height:{85.f,120.f,155.f})
                if(W->LineTraceSingleByChannel(Hit,Eye,Center+FVector(0,0,Height),ECC_Visibility,Q) && Hit.GetActor()==Chair)
                {Visible=true;break;}
            if(!Visible)continue;
            FPhase205cWalkSurvey Survey;
            if(!Phase205cSupportedWalkPath(W,P,Goal,Path,90,25,&Survey,5000))continue;
            Destination=Goal;PathPoint=0;Failure.Reset();return true;
        }
        Failure=FString::Printf(TEXT("No supported, visibly focused, physically connected chair approach for%s"),*GetNameSafe(Chair));
        return false;
    }

    bool WalkStep()
    {
        if(Stop>=Stops.Num())return End(TEXT(""));
        const auto& S=Stops[Stop];
        if(S.Kind==EKind::FocusResidence)
        {
            if(!Home || !Home->CanEnterInterior_Implementation(P) || !Focus(Home))
                return End(TEXT("Reached Gallery lobby has no eligible normal original residence F prompt"));
            ResidencePrompt=true;
            if(bRemainingOnly)Still(TEXT("Original-starter-home-reached-normal-prompt"));
            Test->AddInfo(TEXT("Actual garage→rear court→lobby→starter residence prompt reached. Existing short home gateway was not used to bypass its closed leaf."));
            return Advance();
        }
        if(Path.IsEmpty())
        {
            if(S.Kind==EKind::UseChair)
            {
                ActiveChair=S.Chair;
                if(!ChairApproach(ActiveChair))return End(Failure);
            }
            else
            {
                const FVector Feet=S.Building->GetActorTransform().TransformPosition(S.LocalFloor*100.);
                if(!FloorGoal(Feet,Destination))return End(S.Name+TEXT(": ")+Failure);
                if(S.AuthoredStairSegment)
                {
                    // The measured dog-leg uses separate flight/landing stops.
                    // Point-floor grid nodes falsely overlap the next riser with
                    // the capsule's rounded base. Normal CharacterMovement owns
                    // actual StepUp, floor sweeps and intact collision here.
                    Path.Add(Destination);
                }
                else
                {
                    FPhase205cWalkSurvey Survey;
                    if(!Phase205cSupportedWalkPath(W,P,Destination,Path,90,25,&Survey,5000))
                    {
                        for(const auto& R:Survey.Rejections)Test->AddInfo(FString::Printf(TEXT("C920 path rejection%s count%d"),*R.Key,R.Value));
                        return End(FString::Printf(TEXT("Physical path failed stop%d %s: %s; from%s to%s"),Stop,*S.Name,*Survey.Failure,*P->GetActorLocation().ToString(),*Destination.ToString()));
                    }
                }
                PathPoint=0;
            }
            LegPathLengthCm=0.f;LegTimeoutSeconds=22.f;
            if(bRemainingOnly)
            {
                FVector PreviousPoint=P->GetActorLocation();
                for(const FVector& NextPoint:Path)
                {LegPathLengthCm+=FVector::Distance(PreviousPoint,NextPoint);PreviousPoint=NextPoint;}
                const float NormalWalkSpeed=P->GetCharacterMovement()->MaxWalkSpeed;
                if(!FMath::IsFinite(LegPathLengthCm) || !FMath::IsFinite(NormalWalkSpeed) || NormalWalkSpeed<=0.f)
                    return End(TEXT("Remaining physical route has invalid path length or normal walking speed"));
                // The 25 cm surveyed waypoints use the existing low-input
                // approach. Bound a long genuine leg by its measured length,
                // while preserving the independent five-second stuck watchdog.
                LegTimeoutSeconds=FMath::Max(22.f,LegPathLengthCm/(NormalWalkSpeed*.15f)+10.f);
                if(!FMath::IsFinite(LegTimeoutSeconds))return End(TEXT("Remaining physical route leg timeout became non-finite"));
                Test->AddInfo(FString::Printf(TEXT("C920 remaining leg%d planned %.1fcm normal_max_speed %.1fcm/s finite_timeout %.2fs stuck_watchdog 5s"),
                    Stop,LegPathLengthCm,NormalWalkSpeed,LegTimeoutSeconds));
            }
            LegStarted=W->GetTimeSeconds();LastProgress=LegStarted;BestDistance=BIG_NUMBER;StairSettleStarted=-1.f;
        }
        const FVector Delta=Path[PathPoint]-P->GetActorLocation();
        const float Distance=Delta.Size2D();
        if(Delta.ContainsNaN() || !FMath::IsFinite(Distance))return End(TEXT("Interior walking path or player position became non-finite"));
        if(Distance<18.f && FMath::Abs(Delta.Z)<35.f)
        {
            if(S.AuthoredStairSegment && !P->GetCharacterMovement()->IsMovingOnGround())
            {
                if(StairSettleStarted<0.f)StairSettleStarted=W->GetTimeSeconds();
                if(W->GetTimeSeconds()-StairSettleStarted>.75f)
                    return End(FString::Printf(TEXT("Actual stair stop%d %s did not settle onto native support"),Stop,*S.Name));
                return false;
            }
            StairSettleStarted=-1.f;
            if(++PathPoint<Path.Num()){BestDistance=BIG_NUMBER;LastProgress=W->GetTimeSeconds();return false;}
            Path.Reset();P->GetCharacterMovement()->StopMovementImmediately();
            Test->AddInfo(FString::Printf(TEXT("C920 actual walk stop%d %s at%s"),Stop,*S.Name,*P->GetActorLocation().ToString()));
            // Opt-in readback only. Eight upper room door views plus ground
            // lobby, cinema access and Gallery garage make ten-plus real views.
            // This validation never reports these readback frames as clean perf.
            if(S.Name==TEXT("Actual open ground lobby") || S.Name==TEXT("Actual front lounge doorway") ||
                S.Name==TEXT("Actual front suite doorway") || S.Name==TEXT("Actual rear suite doorway") ||
                S.Name==TEXT("Actual rear study doorway") || S.Name==TEXT("Cinema actual room access") ||
                S.Name==TEXT("Through actual6.2m vehicle portal") ||
                (bRemainingOnly && (S.Name==TEXT("Continuous rear court connection") ||
                    S.Name==TEXT("Garage physical exit") || S.Name==TEXT("Actual rear pedestrian doorway"))))Still(S.Name);
            if(S.Kind==EKind::UseChair){Stage=2;StageStarted=W->GetTimeSeconds();return false;}
            if(S.Name.Contains(TEXT("upper landing")) && !S.Name.Contains(TEXT("Descent")))++UpperLandings;
            if(S.Name==TEXT("Descent bottom"))++Descents;
            return Advance();
        }
        if(Distance<BestDistance-3.f){BestDistance=Distance;LastProgress=W->GetTimeSeconds();}
        if(W->GetTimeSeconds()-LegStarted>LegTimeoutSeconds || W->GetTimeSeconds()-LastProgress>5.f)
        {
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(C920ActualWalkBlocker),false,P);
            W->SweepSingleByChannel(Hit,P->GetActorLocation(),P->GetActorLocation()+Delta.GetSafeNormal2D()*100,
                FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
            return End(FString::Printf(TEXT("Normal CharacterMovement %s stop%d %s point%d elapsed%.2fs planned_length%.1fcm timeout%.2fs no_progress%.2fs at%s toward%s blocked by%s/%s normal%s impact%s"),
                W->GetTimeSeconds()-LastProgress>5.f?TEXT("stuck watchdog"):TEXT("finite leg timeout"),
                Stop,*S.Name,PathPoint,W->GetTimeSeconds()-LegStarted,LegPathLengthCm,LegTimeoutSeconds,
                W->GetTimeSeconds()-LastProgress,*P->GetActorLocation().ToString(),*Path[PathPoint].ToString(),*GetNameSafe(Hit.GetActor()),
                *GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToString(),*Hit.ImpactPoint.ToString()));
        }
        P->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(Distance/100.f,.08f,1.f));
        PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));
        return false;
    }

    bool Advance(){++Stop;Path.Reset();Stage=1;StageStarted=W->GetTimeSeconds();return false;}
    void Key(FKey K,bool Down)
    {
        if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));
    }
    bool End(const FString& Error)
    {
        Key(EKeys::F,false);Key(EKeys::W,false);
        if(!Error.IsEmpty()){Failure=Error;Test->AddError(Error);}
        if(P && P->GetCharacterMovement())P->GetCharacterMovement()->StopMovementImmediately();
        Cleaning=true;CleanupStarted=FPlatformTime::Seconds();
        if(ActiveChair && ActiveChair->IsOccupied())Key(EKeys::W,true);
        return false;
    }
    bool Cleanup()
    {
        const double Age=FPlatformTime::Seconds()-CleanupStarted;
        if(Age>.35)Key(EKeys::W,false);
        if(ActiveChair && ActiveChair->IsOccupied())
        {
            if(Age<8.)return false;
            Test->AddError(TEXT("Normal input cleanup could not unseat; isolated test must end without relocating the occupied player"));
            Failure+=TEXT("; cleanup normal unseat timeout");
        }
        Key(EKeys::F,false);Key(EKeys::W,false);
        if(P && P->GetCharacterMovement())P->GetCharacterMovement()->StopMovementImmediately();
        const bool Passed=Failure.IsEmpty() && ResidencePrompt && (bRemainingOnly ? Stop==Stops.Num() :
            UpperLandings==2 && Descents==2 && UpstairsChairUsed && CinemaUsed);
        if(Failure.IsEmpty() && !Passed)
        {Failure=TEXT("Interior route ended without every declared landing/seat/prompt gate");Test->AddError(Failure);}
        IFileManager::Get().MakeDirectory(*ReceiptDirectory,true);
        auto Receipt=MakeShared<FJsonObject>();
        Receipt->SetBoolField(TEXT("passed"),Passed);Receipt->SetStringField(TEXT("failure"),Failure);
        Receipt->SetBoolField(TEXT("remaining_access_only"),bRemainingOnly);
        Receipt->SetStringField(TEXT("automation_test"),bRemainingOnly?TEXT("NeonCity.Phase205c.MainlandC920.Standalone.InteriorRemainingAccess"):TEXT("NeonCity.Phase205c.MainlandC920.Standalone.InteriorAccess"));
        Receipt->SetNumberField(TEXT("completed_stops"),Stop);Receipt->SetNumberField(TEXT("total_stops"),Stops.Num());
        Receipt->SetNumberField(TEXT("upper_landings_reached"),UpperLandings);Receipt->SetNumberField(TEXT("descents_completed"),Descents);
        Receipt->SetBoolField(TEXT("upstairs_chair_normal_F_use_W_unseat"),UpstairsChairUsed);
        Receipt->SetBoolField(TEXT("cinema_normal_F_feedback_W_unseat"),CinemaUsed);
        Receipt->SetBoolField(TEXT("garage_lobby_residence_prompt"),ResidencePrompt);
        Receipt->SetStringField(TEXT("initial_disclosed_fixture_cm"),InitialStand.ToString());
        Receipt->SetStringField(TEXT("final_player_cm"),P?P->GetActorLocation().ToString():TEXT("absent"));
        Receipt->SetStringField(TEXT("metadata"),ManifestPath);
        Receipt->SetStringField(TEXT("run_label"),RunLabel);Receipt->SetStringField(TEXT("unique_attempt"),RunToken);
        Receipt->SetStringField(TEXT("manifest_sha1"),ManifestSHA1);Receipt->SetStringField(TEXT("test_source_sha1"),SourceSHA1);
        Receipt->SetStringField(TEXT("map_sha1"),MapSHA1);
        Receipt->SetBoolField(TEXT("cacheable_success"),Passed);
        Receipt->SetStringField(TEXT("cache_policy"),TEXT("Only successful receipts are reusable. Parent must confirm native architecture/material/furniture/collision, fixture bindings, player/seat/property/door code and metadata unchanged. Failed attempts never overwrite or stand in for a successful receipt. This test does not auto-skip from an old receipt."));
        Receipt->SetBoolField(TEXT("stills_requested"),bStills);
        Receipt->SetArrayField(TEXT("actual_normal_player_stills"),CapturedStills);
        Receipt->SetNumberField(TEXT("failed_requested_stills"),FailedStills);
        Receipt->SetBoolField(TEXT("clean_performance_sample"),false);
        Receipt->SetStringField(TEXT("performance_scope"),TEXT("Gameplay access validation only. Optional still readback is excluded from separate clean performance sampling."));
        Receipt->SetStringField(TEXT("scope"),bRemainingOnly?TEXT("One initial supported fixture at actual Courtyard rear exit; normal CharacterMovement on the finished rear connector, physical garage entry/exit, rear pedestrian lobby and original starter prompt. Existing full room/stair/cinema prefix is not rerun or claimed by this receipt. No later pose writes, collision bypass, ownership writes or home gateway teleport. Not whole-city/visual/performance acceptance."):TEXT("One initial supported on-foot fixture; physical arcade/lobby/two stairs/upper rooms/descent/rear court/garage/lobby travel; normal F chair+cinema and W safe exit; reached original starter prompt. No post-start pose writes, fixture moves, collision bypass, ownership writes or home gateway teleport. Not whole-city/visual/performance acceptance."));
        FString Text;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
        if(!FFileHelper::SaveStringToFile(Text,*(ReceiptDirectory/TEXT("receipt.json"))))Test->AddError(TEXT("Could not save interior access receipt"));
        Test->AddInfo(FString::Printf(TEXT("Unique interior receipt: %s"),*(ReceiptDirectory/TEXT("receipt.json"))));
        return true;
    }

    FAutomationTestBase* Test;
    UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
    AStaticMeshActor* Court=nullptr;AStaticMeshActor* Gallery=nullptr;
    AIslandHomeSeat* UpstairsChair=nullptr;AIslandHomeSeat* ActiveChair=nullptr;
    AIslandLeisureFacility* Cinema=nullptr;ANeonCityStarterApartment* Home=nullptr;AInteractiveBuildingDoor* ServiceDoor=nullptr;
    TSharedPtr<FJsonObject> CourtData,GalleryData;
    TArray<FVector> Through,Stairs,Path;TArray<FStop> Stops;
    FVector Destination=FVector::ZeroVector,InitialStand=FVector::ZeroVector;
    FString Failure,RunLabel,RunToken,ReceiptDirectory,ManifestSHA1,SourceSHA1,MapSHA1;
    TArray<TSharedPtr<FJsonValue>> CapturedStills;
    int32 Stage=0,Stop=0,PathPoint=0,UpperLandings=0,Descents=0;
    double Started=0,CleanupStarted=0;
    float StageStarted=0,LegStarted=0,LastProgress=0,BestDistance=BIG_NUMBER,StairSettleStarted=-1.f;
    float LegPathLengthCm=0.f,LegTimeoutSeconds=22.f;
    int32 FailedStills=0;
    bool bRemainingOnly=false;
    bool Cleaning=false,SawCinemaFeedback=false,UpstairsChairUsed=false,CinemaUsed=false,ResidencePrompt=false,bStills=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityMainlandInteriorAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.InteriorAccess",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityMainlandInteriorAccess920::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
    {AddError(TEXT("InteriorAccess requires isolated automatic-persistence-disabled validation; no live save writes"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(MainlandInterior920::FAccess(this));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityMainlandInteriorRemainingAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.InteriorRemainingAccess",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityMainlandInteriorRemainingAccess920::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
    {AddError(TEXT("InteriorRemainingAccess requires isolated automatic-persistence-disabled validation; no live save writes"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(MainlandInterior920::FAccess(this,true));
    return true;
}
#endif
