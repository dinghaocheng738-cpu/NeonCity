#include "Water/OxygenComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
UOxygenComponent::UOxygenComponent() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.1f; }
void UOxygenComponent::BeginPlay()
{
 Super::BeginPlay(); ResetOxygen();
 if(auto* Water=GetOwner()->FindComponentByClass<UWaterInteractionComponent>()) AddTickPrerequisiteComponent(Water);
}
void UOxygenComponent::ResetOxygen() { CurrentOxygen=FMath::IsFinite(MaxOxygen)?FMath::Clamp(MaxOxygen,1.f,1000.f):100; DamageElapsed=0; }
void UOxygenComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 if(!GetOwner()->HasAuthority() || !Health || Health->IsDead() || !FMath::IsFinite(Delta) || Delta<=0) return;
 const float Max=FMath::IsFinite(MaxOxygen)?FMath::Clamp(MaxOxygen,1.f,1000.f):100;
 if(!FMath::IsFinite(CurrentOxygen)) CurrentOxygen=Max;
 const bool Under=UWaterInteractionComponent::IsSubmergedActor(GetOwner());
 const float Rate=Under?DrainRate:RecoveryRate;
 CurrentOxygen=FMath::Clamp(CurrentOxygen+(Under?-1:1)*FMath::Clamp(FMath::IsFinite(Rate)?Rate:0,0.f,1000.f)*Delta,0.f,Max);
 if(!Under || CurrentOxygen>0) { DamageElapsed=0; return; }
 DamageElapsed+=Delta;
 if(DamageElapsed>=1)
 {
  DamageElapsed=FMath::Fmod(DamageElapsed,1.f);
  FNeonCityDamageEvent Event; Event.DamageType=ENeonCityDamageType::Drowning;
  Event.DamageAmount=FMath::Clamp(FMath::IsFinite(DrowningDamage)?DrowningDamage:0,0.f,100.f);
  Event.HitLocation=GetOwner()->GetActorLocation(); // Environmental: no criminal instigator.
  GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(GetOwner(),Event);
 }
}
