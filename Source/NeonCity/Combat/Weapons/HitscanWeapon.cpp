#include "Combat/Weapons/HitscanWeapon.h"
#include "Combat/CombatCollision.h"
#include "Combat/CombatSurface.h"
#include "Combat/Weapons/WeaponData.h"
#include "Combat/DamageSystem.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
bool AHitscanWeapon::FireShot(const FCombatAttackRequest& Request)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(WeaponShot),false,this); Query.AddIgnoredActor(GetOwner()); Query.bReturnPhysicalMaterial=true;
 const float Range=FMath::Clamp(Data->Range,100.f,100000.f);
 const FVector Start=GetMuzzleLocation(); const float Cone=FMath::DegreesToRadians(GetSpreadDegrees(Request));
 const int32 Count=FMath::Clamp(Data->PelletCount,1,12); LastPelletDirections.Reset();
 float Remaining=Count>1?FMath::Clamp(Data->MaxShotDamage,1.f,1000.f):Data->Damage;
 FRandomStream Pattern(0x4e43+GetShotCount()*7919);
 for(int32 I=0;I<Count;++I)
 {
  // Bounded, repeatable cone samples. Pellets spread in angle, so their footprint grows with distance.
  const FVector Ray=Pattern.VRandCone(Request.Direction.GetSafeNormal(),Cone); LastPelletDirections.Add(Ray);
  FHitResult ViewHit; FVector AimPoint=Request.ViewOrigin+Ray*Range;
  if(GetWorld()->LineTraceSingleByChannel(ViewHit,Request.ViewOrigin,AimPoint,NeonCityCombat::WeaponTrace,Query)) AimPoint=ViewHit.ImpactPoint;
  FVector Direction=(AimPoint-Start).GetSafeNormal(); if(Direction.IsNearlyZero()) Direction=Ray;
  const float Distance=FMath::Min(FVector::Distance(Start,AimPoint)+5.f,Range);
  FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Direction*Distance,NeonCityCombat::WeaponTrace,Query)) continue;
  LastHit=Hit;
  const bool Friendly=Request.AvoidFriendlyFire && UCombatTargetComponent::AreFriendly(GetOwner(),Hit.GetActor());
  FNeonCityDamageEvent Event; Event.DamageAmount=FMath::Min(Remaining,Count>1?Data->DamagePerPellet:Data->Damage);
  // Shotgun falloff is additional to its hard maximum range. Other classes use a hard cutoff.
  if(Count>1) Event.DamageAmount*=FMath::GetMappedRangeValueClamped(FVector2D(.3f*Range,Range),FVector2D(1,.2f),Hit.Distance);
  Event.DamageType=ENeonCityDamageType::Ballistic; Event.Instigator=Request.Instigator; Event.SourceActor=this; Event.HitLocation=Hit.ImpactPoint;
  if(!Friendly) { Remaining=FMath::Max(0.f,Remaining-Event.DamageAmount); LastAppliedDamage+=GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(Hit.GetActor(),Event); }
  const ENeonCityImpactSurface Surface=NeonCityCombat::ResolveImpactSurface(Hit);
  OnSurfaceImpact.Broadcast(Surface,Hit); OnImpactEffect.Broadcast(Hit);
  // Round strike presentation and momentum; a rifle round ~0.4 of the heavy machine-gun round.
  const float Calibre=Data->WeaponClass==ENeonCityWeaponClass::Sniper?.8f:Data->WeaponClass==ENeonCityWeaponClass::Heavy?.6f:Data->WeaponClass==ENeonCityWeaponClass::Rifle?.4f:.25f;
  if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>()) Effects->Impact(Hit,Direction*40000.f,Calibre,Cast<AController>(Request.Instigator));
 }
 return true;
}
