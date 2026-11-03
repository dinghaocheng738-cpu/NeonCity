#include "Vehicles/VehicleDamageComponent.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionSubsystem.h"
#include "World/WorldEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"
UVehicleDamageComponent::UVehicleDamageComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UVehicleDamageComponent::BeginPlay()
{
 Super::BeginPlay(); Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 if(Health)
 {
  Health->OnHealthChanged.AddDynamic(this,&UVehicleDamageComponent::HealthChanged);
  Health->OnDamageTaken.AddDynamic(this,&UVehicleDamageComponent::Damaged);
  HealthChanged(Health->GetCurrentHealth(),Health->GetMaxHealth());
 }
}
void UVehicleDamageComponent::EndPlay(EEndPlayReason::Type Reason)
{
 CancelExplosionFuse();
 if(Health) { Health->OnHealthChanged.RemoveAll(this); Health->OnDamageTaken.RemoveAll(this); }
 Super::EndPlay(Reason);
}
const UVehicleData* UVehicleDamageComponent::GetProfile() const
{ const auto* Vehicle=Cast<ANeonCityBaseVehicle>(GetOwner()); return Vehicle?Vehicle->GetVehicleProfile():nullptr; }
FName UVehicleDamageComponent::GetExplosionDataID() const
{
 if(!OverrideExplosionDataID.IsNone()) return OverrideExplosionDataID;
 const auto* Data=GetProfile(); return Data?Data->ExplosionDataID:FName(TEXT("NeonCity.VehicleExplosion"));
}
bool UVehicleDamageComponent::CanExplode() const
{
 if(ExplosionState==ENeonCityVehicleExplosionState::Detonated) return false;
 // An explicit runtime opt-in arms platforms that carry no authored vehicle profile.
 if(!OverrideExplosionDataID.IsNone() && OverrideDelayMax>0) return true;
 const auto* Data=GetProfile(); return Data && Data->CanExplode;
}
void UVehicleDamageComponent::ResolveExplosionTiming(float& OutMin,float& OutMax) const
{
 const auto* Data=GetProfile();
 OutMin=OverrideDelayMin>0?OverrideDelayMin:Data?Data->ExplosionDelayMin:3.f;
 OutMax=OverrideDelayMax>0?OverrideDelayMax:Data?Data->ExplosionDelayMax:7.f;
 if(OutMax<OutMin) OutMax=OutMin;
}
float UVehicleDamageComponent::GetFuseRemaining() const
{
 if(!GetWorld() || ExplosionState!=ENeonCityVehicleExplosionState::Pending) return 0;
 return FMath::Max(0.f,GetWorld()->GetTimerManager().GetTimerRemaining(FuseTimer));
}
void UVehicleDamageComponent::EnableExplosion(FName DataID,float MinDelay,float MaxDelay)
{
 OverrideExplosionDataID=DataID; OverrideDelayMin=FMath::Clamp(MinDelay,.5f,30); OverrideDelayMax=FMath::Clamp(FMath::Max(MaxDelay,MinDelay),.5f,30);
 // A test area can arm the fuse before or after the wreck reaches a disabled state.
 if((Condition==ENeonCityVehicleCondition::Disabled || Condition==ENeonCityVehicleCondition::Destroyed) && ExplosionState==ENeonCityVehicleExplosionState::None)
  StartFuse(FMath::FRandRange(OverrideDelayMin,OverrideDelayMax),0);
}
void UVehicleDamageComponent::CancelExplosionFuse()
{
 if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FuseTimer);
 if(ExplosionState==ENeonCityVehicleExplosionState::Pending) ExplosionState=ENeonCityVehicleExplosionState::None;
}
void UVehicleDamageComponent::ResetExplosionState()
{
 CancelExplosionFuse(); ExplosionState=ENeonCityVehicleExplosionState::None; LastResult=FNeonCityExplosionResult();
 LastInstigator.Reset(); LastSourceActor.Reset(); FuseSeconds=0; ChainDepth=0;
 ExplosionCount=RefusedExplosionCount=ChainOfferCount=FuseCount=0;
}
void UVehicleDamageComponent::ConditionFeedback(ENeonCityVehicleCondition Previous,ENeonCityVehicleCondition Next)
{
 if(Previous==Next || !CanExplode()) return;
 // Fire and explosion follow the disabled wreck, never the instant health reaches zero.
 if(Next!=ENeonCityVehicleCondition::Disabled && Next!=ENeonCityVehicleCondition::Destroyed) return;
 if(ExplosionState!=ENeonCityVehicleExplosionState::None) return;
 float Min=0,Max=0; ResolveExplosionTiming(Min,Max);
 StartFuse(FMath::FRandRange(Min,Max),0);
}
void UVehicleDamageComponent::StartFuse(float Delay,int32 Depth)
{
 UWorld* World=GetWorld(); if(!World) return;
 ExplosionState=ENeonCityVehicleExplosionState::Pending; ChainDepth=FMath::Clamp(Depth,0,8);
 FuseSeconds=FMath::Clamp(Delay,.1f,30); ++FuseCount;
 if(auto* Events=World->GetSubsystem<UWorldEventSubsystem>())
  Events->Publish(ENeonCityWorldEventType::VehicleFire,GetOwner()->GetActorLocation(),GetOwner(),LastInstigator.Get(),FuseSeconds,GetExplosionDataID());
 World->GetTimerManager().SetTimer(FuseTimer,this,&UVehicleDamageComponent::FuseElapsed,FuseSeconds,false);
}
void UVehicleDamageComponent::FuseElapsed() { DetonateNow(); }
bool UVehicleDamageComponent::DetonateNow()
{
 UWorld* World=GetWorld(); if(!World) return false;
 if(!CanExplode()) { ++RefusedExplosionCount; return false; }
 auto* Explosions=World->GetSubsystem<UNeonCityExplosionSubsystem>();
 if(!Explosions) { ++RefusedExplosionCount; return false; }
 UNeonCityExplosionSubsystem::FRequest Request;
 Request.Data=&NeonCityExplosions::Profile(GetExplosionDataID()); Request.Center=GetOwner()->GetActorLocation();
 // The wreck is the source; the original shooter, driver or crash keeps the attribution for Crime/Wanted.
 Request.Instigator=LastInstigator.Get(); Request.SourceActor=GetOwner(); Request.Ignored.Add(GetOwner());
 Request.ChainDepth=ChainDepth;
 CancelExplosionFuse();
 const FNeonCityExplosionResult Result=Explosions->Detonate(Request); LastResult=Result;
 if(!Result.bApplied) { ++RefusedExplosionCount; return false; }
 ExplosionState=ENeonCityVehicleExplosionState::Detonated; ++ExplosionCount;
 if(auto* Events=World->GetSubsystem<UWorldEventSubsystem>())
  Events->Publish(ENeonCityWorldEventType::VehicleExplosion,Request.Center,GetOwner(),LastInstigator.Get(),Request.Data->Damage,GetExplosionDataID());
 return true;
}
bool UVehicleDamageComponent::OfferChainExplosion(const FNeonCityExplosionEvent& Event)
{
 const auto* Data=GetProfile();
 if(!CanExplode() || ExplosionState!=ENeonCityVehicleExplosionState::None) return false;
 // Bounded cascade: only a wrecked victim may chain, and only one hop deeper than its parent.
 const int32 Depth=FMath::Clamp(Event.ChainDepth+1,0,8);
 const int32 MaxDepth=Data?Data->MaximumChainDepth:1;
 const float Threshold=Data?Data->ChainReactionThreshold:.25f;
 if(Depth>MaxDepth || GetConditionFraction()>Threshold) { ++RefusedExplosionCount; return false; }
 ++ChainOfferCount; LastInstigator=Event.Instigator; LastSourceActor=Event.SourceActor;
 float Min=0,Max=0; ResolveExplosionTiming(Min,Max);
 // A secondary blast reads as a cascade, so its fuse is shorter than a first fire.
 StartFuse(FMath::FRandRange(Min,Max)*.5f,Depth);
 return true;
}

float UVehicleDamageComponent::GetConditionFraction() const { return Health && Health->GetMaxHealth()>0?Health->GetCurrentHealth()/Health->GetMaxHealth():0; }
float UVehicleDamageComponent::GetDrivePowerMultiplier() const
{ return (Condition==ENeonCityVehicleCondition::Destroyed || Condition==ENeonCityVehicleCondition::Disabled)?0:Condition==ENeonCityVehicleCondition::Critical?.45f:Condition==ENeonCityVehicleCondition::Damaged?.8f:1; }
void UVehicleDamageComponent::HealthChanged(float Current,float Maximum)
{
 const float Fraction=Maximum>0?Current/Maximum:0;
 const auto Next=Fraction<=0?ENeonCityVehicleCondition::Destroyed:Fraction<=.1f?ENeonCityVehicleCondition::Disabled:Fraction<=.3f?ENeonCityVehicleCondition::Critical:Fraction<=.7f?ENeonCityVehicleCondition::Damaged:ENeonCityVehicleCondition::Normal;
 if(Next!=Condition)
 { const auto Previous=Condition; Condition=Next; OnConditionChanged.Broadcast(Previous,Condition); ConditionFeedback(Previous,Condition); }
}
void UVehicleDamageComponent::Damaged(const FNeonCityDamageEvent& Event)
{ if(Event.DamageType!=ENeonCityDamageType::Administrative)
 {
  LastDamage=Event.DamageAmount; FeedbackUntil=GetWorld()->GetTimeSeconds()+1.f;
  // Whoever caused the damage also caused any explosion it produces; the driver is not assumed.
  if(Event.Instigator) LastInstigator=Event.Instigator;
  if(Event.SourceActor) LastSourceActor=Event.SourceActor;
 } }
float UVehicleDamageComponent::GetRecentDamage() const { return GetWorld()->GetTimeSeconds()<FeedbackUntil?LastDamage:0; }
void UVehicleDamageComponent::NotifyPhysicalImpact(float Speed,float AppliedDamage,FVector Location)
{
 if(AppliedDamage>0) ++CollisionDamageCount;
 OnCollisionFeedback.Broadcast(Speed,AppliedDamage,Location);
}
