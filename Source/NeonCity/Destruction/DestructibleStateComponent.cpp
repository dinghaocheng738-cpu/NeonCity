#include "Destruction/DestructibleStateComponent.h"
#include "Destruction/DestructionSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Physics/DestructiblePropComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
UDestructibleStateComponent::UDestructibleStateComponent() { PrimaryComponentTick.bCanEverTick=false; }
const UBreakableSurfaceData& UDestructibleStateComponent::GetSurface() const
{ return SurfaceProfile?*SurfaceProfile.Get():NeonCitySurfaces::Profile(SurfaceKind); }
ENeonCityGlassState UDestructibleStateComponent::GetGlassState() const { return GetSurface().GlassStateFor(State); }
void UDestructibleStateComponent::BeginPlay()
{
 Super::BeginPlay();
 Health=GetOwner()?GetOwner()->FindComponentByClass<UHealthComponent>():nullptr;
 Surface=Cast<UPrimitiveComponent>(GetOwner()?GetOwner()->GetRootComponent():nullptr);
 if(Surface)
 {
  // Capture the effective materials: override-slot introspection is not portable across mesh types.
  IntactMaterials.Reset();
  UPrimitiveComponent* Source=Surface;
  if(auto* Body=GetOwner()->FindComponentByClass<UPrimitiveComponent>(); Body && Body->GetNumMaterials()>0) Source=Body;
  for(int32 I=0;I<Source->GetNumMaterials();++I) IntactMaterials.Add(Source->GetMaterial(I));
 }
 if(Health)
 {
  Health->OnHealthChanged.AddDynamic(this,&UDestructibleStateComponent::HealthChanged);
  Health->OnDamageTaken.AddDynamic(this,&UDestructibleStateComponent::Damaged);
  Evaluate(GetOwner()->GetActorLocation());
 }
 bAdapterDriven=GetOwner()->FindComponentByClass<UDestructiblePropComponent>()!=nullptr;
 if(auto* Subsystem=GetWorld()?GetWorld()->GetSubsystem<UDestructionSubsystem>():nullptr) Subsystem->Register(this);
}
void UDestructibleStateComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* Subsystem=GetWorld()?GetWorld()->GetSubsystem<UDestructionSubsystem>():nullptr) Subsystem->Unregister(this);
 if(Health) { Health->OnHealthChanged.RemoveAll(this); Health->OnDamageTaken.RemoveAll(this); }
 Super::EndPlay(Reason);
}
void UDestructibleStateComponent::HealthChanged(float Current,float Maximum)
{ if(!bAdapterDriven && State!=EPhysicsPropState::Broken) Evaluate(GetOwner()?GetOwner()->GetActorLocation():FVector::ZeroVector); }
void UDestructibleStateComponent::Damaged(const FNeonCityDamageEvent& Event)
{
 if(Event.DamageType==ENeonCityDamageType::Administrative) return;
 LastDamageTime=GetWorld()?GetWorld()->GetTimeSeconds():0;
 if(!bAdapterDriven) ApplyDamageEvent(Event);
}
bool UDestructibleStateComponent::ApplyState(EPhysicsPropState Value,FVector Origin)
{ return SetState(Value,Origin); }
bool UDestructibleStateComponent::ApplyDamageEvent(const FNeonCityDamageEvent& Event)
{
 if(bAdapterDriven || State==EPhysicsPropState::Broken || !Health || Event.DamageType==ENeonCityDamageType::Administrative) return false;
 const float Fraction=Health->GetMaxHealth()>0?Health->GetCurrentHealth()/Health->GetMaxHealth():0;
 const auto Next=GetSurface().StateFor(Fraction,Event.DamageAmount);
 return Next!=EPhysicsPropState::Normal && SetState(Next,Event.HitLocation);
}
bool UDestructibleStateComponent::ApplyImpact(float ContactSpeed,FVector Location)
{
 if(State==EPhysicsPropState::Normal && ContactSpeed<GetSurface().MinimumImpactSpeed) return false;
 if(!FMath::IsFinite(ContactSpeed) || ContactSpeed<GetSurface().ShatterSpeed) return false;
 return SetState(EPhysicsPropState::Broken,Location);
}
bool UDestructibleStateComponent::ApplyExplosion(const FNeonCityExplosionEvent& Event)
{
 if(State==EPhysicsPropState::Broken || !Health || !Event.IsValidEvent() || !GetOwner()) return false;
 const float Distance=FVector::Distance(Event.Center,GetOwner()->GetActorLocation());
 // A blast inside the lethal core shatters glass outright; other surfaces still obey their health thresholds.
 if(GetSurface().bGlass && Distance<=Event.InnerRadius) return SetState(EPhysicsPropState::Broken,Event.Center);
 if(bAdapterDriven) return false;
 const float Fraction=Health->GetMaxHealth()>0?Health->GetCurrentHealth()/Health->GetMaxHealth():0;
 const auto Next=GetSurface().StateFor(Fraction,Event.Damage);
 return Next!=EPhysicsPropState::Normal && SetState(Next,Event.Center);
}
bool UDestructibleStateComponent::Break(FVector Origin,float ImpulseScale)
{
 if(State==EPhysicsPropState::Broken) return false;
 if(ImpulseScale<0 || !FMath::IsFinite(ImpulseScale)) return false;
 return SetState(EPhysicsPropState::Broken,Origin);
}
bool UDestructibleStateComponent::SetState(EPhysicsPropState Value,FVector Origin)
{
 if(Value==State || State==EPhysicsPropState::Broken) return false;
 const auto Previous=State; State=Value; ApplyStateVisuals();
 if(State==EPhysicsPropState::Broken)
 {
  ++BreakCount;
  RequestFragmentEffect(Origin,GetSurface().FragmentBudget);
  OnSurfaceBroken.Broadcast(GetSurface().SurfaceID,Origin,BreakCount);
 }
 OnStateChanged.Broadcast(Previous,State);
 OnDestructionFeedback(State,Origin);
 return true;
}
void UDestructibleStateComponent::ApplyStateVisuals()
{
 const UBreakableSurfaceData& Data=GetSurface();
 UPrimitiveComponent* Body=Surface;
 if(!Body && GetOwner()) Body=GetOwner()->FindComponentByClass<UPrimitiveComponent>();
 if(!Body) return;
 if(State==EPhysicsPropState::Normal)
 {
  Body->SetVisibility(true);
  for(int32 Index=0;Index<IntactMaterials.Num();++Index) Body->SetMaterial(Index,IntactMaterials[Index]);
  return;
 }
 if(State==EPhysicsPropState::Damaged)
 {
  Body->SetVisibility(!Data.bHiddenWhenCracked);
  if(CrackedMaterial) Body->SetMaterial(0,CrackedMaterial);
  return;
 }
 Body->SetVisibility(!Data.bHiddenWhenBroken);
 if(BrokenMaterial) Body->SetMaterial(0,BrokenMaterial);
}
bool UDestructibleStateComponent::RequestFragmentEffect(FVector Origin,float BudgetCost)
{
 auto* Subsystem=GetWorld()?GetWorld()->GetSubsystem<UDestructionSubsystem>():nullptr;
 if(!Subsystem || Origin.ContainsNaN()) { ++RefusedFragments; return false; }
 // Distant or over-budget surfaces still break; they simply skip the fragment simulation.
 if(!Subsystem->RequestFragmentSlot(Origin,BudgetCost)) { ++RefusedFragments; return false; }
 ++FragmentEffects;
 OnFragmentEffectRequested.Broadcast(Origin,GetSurface().Kind,BudgetCost);
 return true;
}
bool UDestructibleStateComponent::ResetDestruction()
{
 if(State==EPhysicsPropState::Normal) return false;
 const auto Previous=State; State=EPhysicsPropState::Normal;
 if(Surface) { Surface->SetVisibility(true); for(int32 Index=0;Index<IntactMaterials.Num();++Index) Surface->SetMaterial(Index,IntactMaterials[Index]); }
 OnStateChanged.Broadcast(Previous,State);
 return true;
}
void UDestructibleStateComponent::Evaluate(FVector Origin)
{
 if(!Health || State==EPhysicsPropState::Broken) return;
 const float Fraction=Health->GetMaxHealth()>0?Health->GetCurrentHealth()/Health->GetMaxHealth():0;
 SetState(GetSurface().StateFor(Fraction,0),Origin);
}