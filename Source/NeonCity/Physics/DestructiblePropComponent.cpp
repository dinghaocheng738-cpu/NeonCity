#include "Physics/DestructiblePropComponent.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Physics/ImpactDamageData.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Destruction/BreakableSurfaceData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
UDestructiblePropComponent::UDestructiblePropComponent() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
EPhysicsPropState UDestructiblePropComponent::GetState() const { return StateRegister?StateRegister->GetState():State; }
void UDestructiblePropComponent::BeginPlay()
{
 Super::BeginPlay(); Body=Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());
 StateRegister=GetOwner()->FindComponentByClass<UDestructibleStateComponent>();
 if(auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>()) { InitialHealth=Health->GetCurrentHealth(); InitialMaxHealth=Health->GetMaxHealth(); Health->OnDamageTaken.AddDynamic(this,&UDestructiblePropComponent::Damaged); }
 TInlineComponentArray<ULightComponent*> Lights(GetOwner()); for(auto* Light:Lights) InitialLights.Add(Light,Light->IsVisible());
 if(Body)
 {
  Body->BodyInstance.bGenerateWakeEvents=true;
  Body->OnComponentWake.AddDynamic(this,&UDestructiblePropComponent::Woke); Body->OnComponentSleep.AddDynamic(this,&UDestructiblePropComponent::Slept);
  Body->SetMassOverrideInKg(NAME_None,FMath::Clamp(Mass,2.f,300.f)); Body->SetLinearDamping(1.2f); Body->SetAngularDamping(3.f); Body->SetUseCCD(true);
  if(Mode==EPhysicsPropMode::Movable) { Body->SetSimulatePhysics(true); Body->PutAllRigidBodiesToSleep(); }
 }
}
bool UDestructiblePropComponent::SetState(EPhysicsPropState Value)
{
 if(Mode==EPhysicsPropMode::Static) return false;
 const EPhysicsPropState Current=GetState();
 if(Current==Value || Current==EPhysicsPropState::Broken) return false;
 State=Value;
 if(StateRegister) StateRegister->ApplyState(Value,GetOwner()->GetActorLocation());
 if(State==EPhysicsPropState::Broken)
 {
  TInlineComponentArray<ULightComponent*> Lights(GetOwner()); for(auto* Light:Lights) Light->SetVisibility(false);
  if(bGlass) { Body->SetVisibility(false); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 }
 OnStateChanged.Broadcast(State);
 return true;
}
bool UDestructiblePropComponent::ApplyExternalState(EPhysicsPropState Value)
{
 const bool bChanged=SetState(Value);
 if(Value==EPhysicsPropState::Broken && GetState()==EPhysicsPropState::Broken && !bGlass && Body && bPhysicsLOD)
 {
  bReleased=!Body->IsSimulatingPhysics(); Body->SetSimulatePhysics(true);
  ActiveUntil=GetWorld()->GetTimeSeconds()+4; GetWorld()->GetTimerManager().SetTimer(Timer,this,&UDestructiblePropComponent::Monitor,.1f,true);
 }
 return bChanged;
}
void UDestructiblePropComponent::Damaged(const FNeonCityDamageEvent& Damage)
{
 if(Mode==EPhysicsPropMode::Static || Damage.DamageType==ENeonCityDamageType::Administrative) return;
 LastDamageTime=GetWorld()->GetTimeSeconds();
 const auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 // Released policy: 30% health breaks the prop, any lesser damage only damages it.
 SetState(Health && Health->GetCurrentHealth()<=Health->GetMaxHealth()*.3f?EPhysicsPropState::Broken:EPhysicsPropState::Damaged);
 if(GetState()==EPhysicsPropState::Broken && !bGlass && Body && bPhysicsLOD) { bReleased=!Body->IsSimulatingPhysics(); Body->SetSimulatePhysics(true); ActiveUntil=GetWorld()->GetTimeSeconds()+4; GetWorld()->GetTimerManager().SetTimer(Timer,this,&UDestructiblePropComponent::Monitor,.1f,true); }
}
void UDestructiblePropComponent::ReceiveImpact(const FPhysicalImpactEvent& Event)
{
 if(!Body || Mode==EPhysicsPropMode::Static) return;
 // A high-speed contact may shatter a surface outright; the state register owns that threshold.
 if(StateRegister) StateRegister->ApplyImpact(Event.ImpactSpeed,Event.ImpactLocation);
 if(bGlass || !bPhysicsLOD) return;
 const bool WasSimulating=Body->IsSimulatingPhysics();
 if(Mode==EPhysicsPropMode::Movable || GetState()==EPhysicsPropState::Broken)
 {
  Body->SetSimulatePhysics(true);
  // A newly released support had no dynamic response in the contact solver; only this transition receives an impulse.
  if(!WasSimulating || bReleased) Body->AddImpulse(Event.ImpactDirection*FMath::Min(Event.ImpactSpeed*.3f,400.f)*Mass);
  bReleased=false; Body->WakeAllRigidBodies(); ActiveUntil=GetWorld()->GetTimeSeconds()+4;
  GetWorld()->GetTimerManager().SetTimer(Timer,this,&UDestructiblePropComponent::Monitor,.1f,true);
 }
}
void UDestructiblePropComponent::Monitor()
{
 if(!Body) return;
 const float Limit=GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->Tuning->MaximumPropSpeed;
 const FVector V=Body->GetPhysicsLinearVelocity(); if(V.SizeSquared()>FMath::Square(Limit)) Body->SetPhysicsLinearVelocity(V.GetClampedToMaxSize(Limit));
 const FVector A=Body->GetPhysicsAngularVelocityInRadians(); if(A.SizeSquared()>16) Body->SetPhysicsAngularVelocityInRadians(A.GetClampedToMaxSize(4));
 if(GetWorld()->GetTimeSeconds()>ActiveUntil || !Body->IsAnyRigidBodyAwake()) { Body->PutAllRigidBodiesToSleep(); Slept(Body,NAME_None); GetWorld()->GetTimerManager().ClearTimer(Timer); }
}
void UDestructiblePropComponent::Woke(UPrimitiveComponent* Component,FName Bone) { if(bPhysicsLOD) SetComponentTickEnabled(true); }
void UDestructiblePropComponent::Slept(UPrimitiveComponent* Component,FName Bone) { PrePhysicsVelocity=FVector::ZeroVector; SetComponentTickEnabled(false); }
void UDestructiblePropComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{ Super::TickComponent(Delta,Type,Function); if(Body) PrePhysicsVelocity=Body->GetPhysicsLinearVelocity().GetClampedToMaxSize(600); }
void UDestructiblePropComponent::SetPhysicsLOD(bool Active)
{ bPhysicsLOD=Active; if(!Active && Body) { Body->PutAllRigidBodiesToSleep(); Slept(Body,NAME_None); GetWorld()->GetTimerManager().ClearTimer(Timer); } }
void UDestructiblePropComponent::ResetForPool()
{
 GetWorld()->GetTimerManager().ClearTimer(Timer); State=EPhysicsPropState::Normal; bReleased=false; LastDamageTime=-1000; Slept(Body,NAME_None);
 if(Body) { Body->SetSimulatePhysics(false); Body->SetVisibility(true); Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
 if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->ApplyPersistenceHealth(InitialHealth,InitialMaxHealth);
 for(const auto& Pair:InitialLights) if(Pair.Key.IsValid()) Pair.Key->SetVisibility(Pair.Value);
 if(Body && Mode==EPhysicsPropMode::Movable) { Body->SetSimulatePhysics(true); Body->PutAllRigidBodiesToSleep(); }
 if(StateRegister) StateRegister->ResetDestruction();
}
void UDestructiblePropComponent::EndPlay(EEndPlayReason::Type Reason)
{
 GetWorld()->GetTimerManager().ClearTimer(Timer); if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->OnDamageTaken.RemoveAll(this);
 if(Body) { Body->OnComponentWake.RemoveAll(this); Body->OnComponentSleep.RemoveAll(this); }
 Super::EndPlay(Reason);
}