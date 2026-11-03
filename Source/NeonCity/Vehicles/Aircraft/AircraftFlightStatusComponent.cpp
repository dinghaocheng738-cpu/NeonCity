#include "Vehicles/Aircraft/AircraftFlightStatusComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/VehicleStatusWidget.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/LockOnSystem.h"
#include "Aircraft/Lighting/HelicopterSearchLightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UAircraftFlightStatusComponent::UAircraftFlightStatusComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool UAircraftFlightStatusComponent::IsVisible() const { return Widget && Widget->IsInViewport(); }
void UAircraftFlightStatusComponent::NotifyExitDenied(bool Airborne)
{
 ExitFeedback=Airborne?NSLOCTEXT("NeonCity","AircraftLandToExit","Land and stop before leaving the aircraft"):
  NSLOCTEXT("NeonCity","AircraftClearExit","Stop on level ground with a clear exit beside the cabin");
 FeedbackRemaining=3;Elapsed=.1f;
}
void UAircraftFlightStatusComponent::Clear()
{
 if(Widget)Widget->RemoveFromParent();Widget=nullptr;
 if(Combat.IsValid())Combat->SetStatusPresented(false);
 Combat.Reset();DisplayedStatus=ExitFeedback=FText::GetEmpty();Elapsed=FeedbackRemaining=0;
}
void UAircraftFlightStatusComponent::UpdateStatus(float Delta)
{
 auto* Pawn=Cast<APawn>(GetOwner());auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 if(!PC || !PC->IsLocalController()) { if(Widget)Clear();return; }
 auto* Flight=GetOwner()->FindComponentByClass<UAircraftVehicleComponent>();if(!Flight)return;
 if(!Widget)
 {
  Widget=CreateWidget<UVehicleStatusWidget>(PC);if(!Widget)return;
  Widget->SetVisibility(ESlateVisibility::HitTestInvisible);Widget->AddToViewport(5);Elapsed=.1f;
 }
 if(!Combat.IsValid())Combat=GetOwner()->FindComponentByClass<UVehicleCombatComponent>();
 if(Combat.IsValid())Combat->SetStatusPresented(true);
 FeedbackRemaining=FMath::Max(0.f,FeedbackRemaining-Delta);
 if(FeedbackRemaining<=0)ExitFeedback=FText::GetEmpty();
 Elapsed+=Delta;if(Elapsed<.1f)return;Elapsed=0;
 const auto T=Flight->GetTelemetry();
 const FString State=ExitFeedback.IsEmpty()?T.Phase.ToString():ExitFeedback.ToString();
 FString Text=FString::Printf(TEXT("%s | %.0f km/h | %.0f m AGL | %s %.0f%%\n%s"),
  T.FixedWing?TEXT("NeonCity Light Aircraft"):TEXT("NeonCity Helicopter"),T.SpeedKmh,T.AltitudeMeters,
  T.FixedWing?TEXT("Throttle"):TEXT("Power"),T.CurrentPower*100,*State);
 if(Combat.IsValid())
 {
  auto* Manager=Combat->GetManager();auto* Weapon=Manager?Manager->GetWeapon():nullptr;
  auto* Lock=Combat->GetLockSystem();
  if(Weapon && Lock)
  {
   FString LockText;
   switch(Lock->State)
   {
    case EVehicleLockState::Locking:LockText=FString::Printf(TEXT("LOCKING %.0f%%"),Lock->GetProgress()*100);break;
    case EVehicleLockState::Locked:LockText=TEXT("LOCKED");break;
    case EVehicleLockState::Lost:LockText=TEXT("LOST");break;
   default:LockText=TEXT("SEARCHING");break;
   }
   if(!Combat->IsArmed() || !Combat->IsLockRequested())LockText=TEXT("Lock standby");
   Text+=FString::Printf(TEXT("\n%s | Ammo %d / %d | %s | %s"),*Weapon->GetData()->DisplayName.ToString(),
    Weapon->GetAmmo(),Weapon->GetReserveAmmo(),Combat->IsArmed()?TEXT("ARMED"):TEXT("SAFE"),*LockText);
  }
 }
 if(const auto* Light=GetOwner()->FindComponentByClass<UHelicopterSearchLightComponent>())
  Text+=Light->IsEnabled()?TEXT("\nSearchlight ON | Mouse aim"):TEXT("\nSearchlight OFF");
 Text+=T.FixedWing?TEXT("\nW/S Throttle  A/D Roll  Q/E Yaw  Arrows Pitch  Space Brake  F Leave"):
  TEXT("\nW/S Fly  A/D Yaw  Q/E Strafe  Space/C Height\nShift Power up  Ctrl Slow  F Leave  L Searchlight");
 if(Combat.IsValid())Text+=TEXT("\nV Arm  Tab Weapon  LMB Fire  RMB Lock  R Reload");
 DisplayedStatus=FText::FromString(Text);
 Widget->UpdateStatus(DisplayedStatus,Flight->GetCondition()==EAircraftCondition::Critical || Flight->GetCondition()==EAircraftCondition::Disabled);
}
void UAircraftFlightStatusComponent::EndPlay(EEndPlayReason::Type Reason) { Clear();Super::EndPlay(Reason); }
