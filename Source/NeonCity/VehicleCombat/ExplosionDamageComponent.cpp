#include "VehicleCombat/ExplosionDamageComponent.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Combat/CombatCollision.h"
#include "Engine/World.h"
float UExplosionDamageComponent::Falloff(float Distance,float Radius)
{ return UNeonCityExplosionData::Falloff(Distance,0,Radius,1); }
int32 UExplosionDamageComponent::Detonate(FVector Center,float Radius,const FNeonCityDamageEvent& Payload,AActor* IgnoredHost)
{
 if(bDetonated || !GetOwner() || !GetOwner()->HasAuthority() || Center.ContainsNaN() || !FMath::IsFinite(Radius) || Radius<=0 || Radius>800
  || !FMath::IsFinite(Payload.DamageAmount) || Payload.DamageAmount<=0) return 0;
 auto* System=GetWorld()?GetWorld()->GetSubsystem<UNeonCityExplosionSubsystem>():nullptr; if(!System) return 0;
 bDetonated=true;
 // The released payload contract: linear attenuation on one radius, no impulse, no impulse-free pass-through damage.
 if(!AdapterProfile) AdapterProfile=NewObject<UNeonCityExplosionData>(this);
 AdapterProfile->ExplosionID=TEXT("NeonCity.LegacyPayloadExplosion");
 AdapterProfile->Damage=FMath::Min(500.f,Payload.DamageAmount);
 AdapterProfile->InnerRadius=0; AdapterProfile->OuterRadius=Radius; AdapterProfile->DamageFalloff=1;
 AdapterProfile->ImpulseStrength=0; AdapterProfile->ImpulseRadius=0; AdapterProfile->CooldownSeconds=.2f;
 AdapterProfile->bRequiresLineOfSight=true; AdapterProfile->OccludedDamageFraction=0; AdapterProfile->OccludedImpulseFraction=0;
 AdapterProfile->EnvironmentDamageMultiplier=1; AdapterProfile->VehicleDamageMultiplier=1; AdapterProfile->CharacterDamageMultiplier=1;
 AdapterProfile->bEmitDangerSignal=false; AdapterProfile->MaximumTargets=64;
 UNeonCityExplosionSubsystem::FRequest Request;
 Request.Data=AdapterProfile; Request.Center=Center; Request.Instigator=Payload.Instigator; Request.SourceActor=Payload.SourceActor;
 Request.Ignored.Add(IgnoredHost); Request.DamageOverride=AdapterProfile->Damage; Request.OcclusionChannel=NeonCityCombat::WeaponTrace;
 Request.bApplyImpulse=false; Request.bNotifyResponses=false; Request.bEmitDangerSignal=false; Request.bPublishWorldEvent=false;
 return System->Detonate(Request).DamageTargets;
}