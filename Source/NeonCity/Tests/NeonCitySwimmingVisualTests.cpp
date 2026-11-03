#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Water/SwimmingAnimInstance.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "InputKeyEventArgs.h"

namespace
{
void SwimKey(APlayerController* PC,FKey Key,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,FPlatformTime::Cycles64())); }
class FSwimmingVisualRoute : public IAutomationLatentCommand
{
public:
    explicit FSwimmingVisualRoute(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(!Started) Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>240) { Test->AddError(FString::Printf(TEXT("Swimming route timeout stage %d"),Stage)); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
        if(!P || W->GetTimeSeconds()<2) return false;
        auto* M=P->GetCharacterMovement(); auto* Anim=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance());
        auto* Water=P->FindComponentByClass<UWaterInteractionComponent>(); auto* Oxygen=P->FindComponentByClass<UOxygenComponent>();
        if(W->GetTimeSeconds()-LastDiagnostic>10)
        {
            LastDiagnostic=W->GetTimeSeconds();
            Test->AddInfo(FString::Printf(TEXT("Swimming progress stage=%d pos=%s mode=%d"),Stage,*P->GetActorLocation().ToString(),int32(M->MovementMode)));
        }
        if(Stage==0)
        {
            if(!M->IsMovingOnGround()) return false;
            if(!Test->TestNotNull(TEXT("Real default player uses swimming wrapper"),Anim)) return true;
            for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It) { It->SetCycleEnabled(false); It->SetTimeOfDay(11); }
            P->SetActorLocation(FVector(5200,-1000,142),false,nullptr,ETeleportType::TeleportPhysics);
            M->StopMovementImmediately(); M->SetMovementMode(MOVE_Falling);
            Phase=W->GetTimeSeconds(); Stage=1; return false;
        }
        Recorder.Record(W,PC,Stage);
        const float Elapsed=W->GetTimeSeconds()-Phase;
        auto Next=[&](int32 Value){ Test->AddInfo(FString::Printf(TEXT("Route stage %d -> %d pos=%s mode=%d state=%s mouth=%s oxygen=%.2f"),Stage,Value,*P->GetActorLocation().ToString(),int32(M->MovementMode),*Anim->VisibleSwimState.ToString(),*Water->GetBreathingLocation().ToString(),Oxygen->CurrentOxygen)); Stage=Value; Phase=W->GetTimeSeconds(); return false; };
        auto MoveTo=[&](FVector Target,float Radius=45.f)
        {
            const FVector To=Target-P->GetActorLocation();
            PC->SetControlRotation(FRotator(-12,To.Rotation().Yaw,0));
            if(To.Size2D()<=Radius) return true;
            P->DoMove(0,FMath::Clamp(To.Size2D()/200.f,.15f,1.f)); return false;
        };
        if(Stage==1)
        {
            if(Elapsed<.5f) return false;
            if(MoveTo(FVector(5200,-4500,-190)))
            {
                Test->TestTrue(TEXT("Walked from coast down actual pier steps into deep water"),M->IsSwimming());
                Origin=P->GetActorLocation(); return Next(2);
            }
        }
        else if(Stage==2 && Elapsed>3)
        {
            Test->TestTrue(TEXT("Deep idle visibly selects sampled tread cycle"),M->IsSwimming() && Anim->WaterAlpha>.99f && Anim->MovingAlpha<.1f && Anim->VisibleSwimState==TEXT("Tread"));
            Test->TestTrue(TEXT("Head above surface restores oxygen"),!Water->IsFullySubmerged() && Oxygen->CurrentOxygen>95);
            HandStart=P->GetMesh()->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation();
            return Next(3);
        }
        else if(Stage==3)
        {
            HandTravel=FMath::Max(HandTravel,FVector::Distance(HandStart,P->GetMesh()->GetSocketTransform(TEXT("hand_l"),RTS_Component).GetLocation()));
            if(Elapsed<2.5f) return false;
            Test->TestTrue(TEXT("Final rendered skeleton has a moving sculling hand"),HandTravel>8);
            return Next(4);
        }
        else if(Stage==4)
        {
            PeakForwardAlpha=FMath::Max(PeakForwardAlpha,Anim->MovingAlpha);
            if(MoveTo(FVector(4700,-5300,-190)))
            {
                Test->TestTrue(TEXT("Surface cruise played full forward swimming before slowing for turn"),PeakForwardAlpha>.9f && Anim->WaterAlpha>.99f);
                return Next(5);
            }
        }
        else if(Stage==5)
        {
            if(MoveTo(FVector(4000,-5100,-190)))
            {
                Test->TestTrue(TEXT("Turn preserves swimming animation and collision mode"),M->IsSwimming() && Anim->bNativeSwimming);
                InitialOxygen=Oxygen->CurrentOxygen; SwimKey(PC,EKeys::C,IE_Pressed); return Next(6);
            }
        }
        else if(Stage==6 && Elapsed>2.5f)
        {
            SwimKey(PC,EKeys::C,IE_Released);
            Test->TestTrue(TEXT("Dive uses directional swimming clip"),Anim->DescendAlpha>.5f && Water->IsFullySubmerged());
            Test->TestTrue(TEXT("Animated mouth underwater consumes oxygen"),Oxygen->CurrentOxygen<InitialOxygen);
            return Next(7);
        }
        else if(Stage==7)
        {
            P->DoMove(0,1);
            if(Elapsed<2) return false;
            Test->TestTrue(TEXT("Underwater uses actual underwater pose branch"),Anim->UnderwaterAlpha>.95f);
            Test->TestEqual(TEXT("Ordinary firearms remain disabled"),P->GetCombatComponent()->GetCombatState(),ENeonCityCombatState::Disabled);
            SwimKey(PC,EKeys::SpaceBar,IE_Pressed); return Next(8);
        }
        else if(Stage==8)
        {
            if(Elapsed>.5f && !Water->IsFullySubmerged())
            {
                SwimKey(PC,EKeys::SpaceBar,IE_Released); return Next(9);
            }
        }
        else if(Stage==9 && Elapsed>3)
        {
            Test->TestTrue(TEXT("Returned to stable water surface and recovered oxygen"),M->IsSwimming() && !Water->IsFullySubmerged() && Oxygen->CurrentOxygen>95);
            return Next(10);
        }
        else if(Stage==10)
        {
            if(MoveTo(FVector(5200,-4650,-190),20)) return Next(101);
        }
        else if(Stage==101 && Elapsed>1) return Next(11);
        else if(Stage==11)
        {
            if(MoveTo(FVector(5200,-3150,0)))
            {
                Test->TestTrue(TEXT("Native walking restored on real shore without a teleport"),M->IsMovingOnGround());
                return Next(12);
            }
        }
        else if(Stage==12)
        {
            MoveTo(FVector(5200,-2600,0));
            if(Elapsed<1) return false;
            Test->TestTrue(TEXT("Land graph regains full output after shore transition"),Anim->WaterAlpha<.01f && !Anim->bNativeSwimming);
            Test->TestTrue(TEXT("Normal land speed and alive state preserved"),M->MaxWalkSpeed==500 && P->GetHealthComponent()->GetCurrentHealth()==100);
            P->DoJumpStart(); return Next(13);
        }
        else if(Stage==13 && Elapsed>.15f)
        {
            Test->TestTrue(TEXT("Template jump works after swim"),M->IsFalling()); P->DoJumpEnd(); return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test; FNeonCityVisualEvidence Recorder{TEXT("A-Swimming")};
    double Started=0; int32 Stage=0; float Phase=0,HandTravel=0,InitialOxygen=0,LastDiagnostic=0,PeakForwardAlpha=0; FVector Origin,HandStart;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwimmingVisualRouteTest,"NeonCity.Visual.SwimmingRoutePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwimmingVisualRouteTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
    ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSwimmingVisualRoute>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
