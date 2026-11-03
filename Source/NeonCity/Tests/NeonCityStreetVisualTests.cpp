#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractiveBuildingDoor.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/VehicleComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "InputKeyEventArgs.h"

namespace
{
void StreetKey(APlayerController* PC,FKey Key,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,FPlatformTime::Cycles64())); }
class FStreetVisualRoute : public IAutomationLatentCommand
{
public:
    explicit FStreetVisualRoute(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(!Started) Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Street route timeout stage=%d pawn=%s"),Stage,Player?*Player->GetActorLocation().ToString():TEXT("none"))); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
        if(!PC || W->GetTimeSeconds()<2) return false;
        if(Stage==0)
        {
            Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
            for(TActorIterator<AInteractiveBuildingDoor> It(W);It;++It) Door=*It;
            for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It) Cycle=*It;
            for(TActorIterator<ANeonCityStarterSedan> It(W);It;++It) if(It->GetClass()==ANeonCityStarterSedan::StaticClass()) Car=*It;
            if(!Test->TestTrue(TEXT("Normal map retains entrance, clock and original sedan"),Door && Cycle && Car)) return true;
            int32 Visuals=0;
            for(TActorIterator<AStaticMeshActor> It(W);It;++It) if(It->ActorHasTag(TEXT("NeonCity.Visual205")))
            {
                ++Visuals; Test->TestEqual(TEXT("Presentation mesh adds no duplicate collision"),It->GetStaticMeshComponent()->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
                Test->TestTrue(TEXT("Placed visual has a real asset"),It->GetStaticMeshComponent()->GetStaticMesh()!=nullptr);
                Test->TestTrue(TEXT("Authored assets remain upright after FBX import"),It->GetActorUpVector().Z>.99f);
            }
            Test->TestTrue(TEXT("Reference block is integrated in playable South Beach"),Visuals>=20);
            Cycle->SetCycleEnabled(false); Cycle->SetTimeOfDay(10);
            // Initial test fixtures only. Everything after recording starts walks
            // or drives through native movement, interaction and Chaos physics.
            Player->SetActorLocation(FVector(-5200,1050,142),false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Car->SetActorLocationAndRotation(FVector(-800,280,110),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
            Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            Phase=W->GetTimeSeconds(); Stage=1; return false;
        }
        Recorder.Record(W,PC,Stage);
        if(W->GetTimeSeconds()-LastProgress>10) { LastProgress=W->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("Street visual stage=%d player=%s"),Stage,*Player->GetActorLocation().ToString()); }
        const float Elapsed=W->GetTimeSeconds()-Phase;
        auto Next=[&](int32 Value){ Test->AddInfo(FString::Printf(TEXT("Street stage %d -> %d player=%s vehicle=%s"),Stage,Value,*Player->GetActorLocation().ToString(),*Car->GetActorLocation().ToString())); Stage=Value;Phase=W->GetTimeSeconds();return false; };
        auto Walk=[&](FVector Target,float Radius=35.f){ const FVector To=Target-Player->GetActorLocation(); PC->SetControlRotation(FRotator(-10,To.Rotation().Yaw,0)); if(To.Size2D()<Radius) return true; Player->DoMove(0,1); return false; };
        if(Stage==1)
        {
            if(Elapsed<.5f) return false;
            if(Walk(FVector(-1500,1180,0),10)) return Next(2);
        }
        else if(Stage==2)
        {
            PC->SetControlRotation(FRotator(0,90,0));
            if(Elapsed<.5f) return false;
            if(!Test->TestTrue(TEXT("New entry mesh leaves original F focus unobstructed"),Player->GetInteractionComponent()->RefreshFocusedInteractable()==Door)) return true;
            StreetKey(PC,EKeys::F,IE_Pressed); return Next(3);
        }
        else if(Stage==3 && Elapsed>.2f)
        { StreetKey(PC,EKeys::F,IE_Released); Test->TestTrue(TEXT("Original entrance opens"),Door->IsOpen()); return Next(4); }
        else if(Stage==4)
        {
            if(Walk(FVector(-1500,1770,0))) { Test->TestTrue(TEXT("Walked through refined doorway into original lobby"),Player->GetCharacterMovement()->IsMovingOnGround()); return Next(5); }
        }
        else if(Stage==5)
        { if(Elapsed<2) return false; if(Walk(FVector(-1500,1050,0))) return Next(6); }
        else if(Stage==6)
        { if(Walk(FVector(-800,1000,0))) return Next(7); }
        else if(Stage==7)
        { if(Walk(Car->GetActorLocation()-Car->GetActorRightVector()*210,20)) return Next(8); }
        else if(Stage==8)
        {
            FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);
            PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,90)-Eye).Rotation());
            if(Elapsed<.5f) return false;
            if(!Test->TestTrue(TEXT("Sedan reached on foot and focused"),Player->GetInteractionComponent()->RefreshFocusedInteractable()==Car)) return true;
            StreetKey(PC,EKeys::F,IE_Pressed);return Next(9);
        }
        else if(Stage==9 && Elapsed>.3f)
        {
            StreetKey(PC,EKeys::F,IE_Released);
            if(!Test->TestTrue(TEXT("F possesses same sedan"),PC->GetPawn()==Car)) return true;
            DriveOrigin=Car->GetActorLocation();StreetKey(PC,EKeys::W,IE_Pressed);return Next(10);
        }
        else if(Stage==10)
        {
            Car->GetDrivingComponent()->SetSteerInput(FMath::Clamp(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,180.f)*.04f,-.4f,.4f));
            if(Elapsed<4) return false;
            Test->TestTrue(TEXT("Drives past hero facade on original roadway"),FVector::Dist2D(DriveOrigin,Car->GetActorLocation())>1000);
            Test->TestTrue(TEXT("Road skins introduce no launch or roll"),FMath::Abs(Car->GetActorRotation().Roll)<15 && Car->GetActorLocation().Z<180);
            StreetKey(PC,EKeys::W,IE_Released);StreetKey(PC,EKeys::SpaceBar,IE_Pressed);
            Cycle->SetTimeOfDay(21);return Next(11);
        }
        else if(Stage==11 && Elapsed>2 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<2)
        { StreetKey(PC,EKeys::F,IE_Pressed);return Next(12); }
        else if(Stage==12 && Elapsed>.3f)
        {
            StreetKey(PC,EKeys::F,IE_Released);
            if(!Test->TestTrue(TEXT("Night observation returns to the original on-foot camera"),PC->GetPawn()==Player)) return true;
            FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);
            PC->SetControlRotation((FVector(-4400,2300,620)-Eye).Rotation());
            return Next(13);
        }
        else if(Stage==13 && Elapsed>20)
        {
            Test->TestTrue(TEXT("Night pass preserves original nine street lights"),Cycle->NightLights.Num()==9);
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;FNeonCityVisualEvidence Recorder{TEXT("C-Street")};
    ANeonCityPlayerCharacter* Player=nullptr;ANeonCityStarterSedan* Car=nullptr;AInteractiveBuildingDoor* Door=nullptr;ANeonCityDayNightCycle* Cycle=nullptr;
    double Started=0;float Phase=0,LastProgress=0;int32 Stage=0;FVector DriveOrigin;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetVisualRouteTest,"NeonCity.Visual.StreetRoutePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStreetVisualRouteTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
    ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStreetVisualRoute>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
