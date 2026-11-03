#include "Explosion/ExplosionData.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Physics/DestructiblePropComponent.h"
#include "UObject/StrongObjectPtr.h"
UNeonCityExplosionData::UNeonCityExplosionData()
{
 ExplosionID=TEXT("NeonCity.SmallExplosion"); Damage=60; InnerRadius=100; OuterRadius=380; DamageFalloff=1;
 ImpulseStrength=.8f; ImpulseRadius=380; CooldownSeconds=.25f; SourceType=ENeonCityExplosionSourceType::Generic;
 bRequiresLineOfSight=true; OccludedDamageFraction=0; OccludedImpulseFraction=0; FireDuration=4; SmokeDuration=8;
 Impulse.VehicleDeltaVelocity=250; Impulse.PropDeltaVelocity=900; Impulse.MaximumImpulse=90000;
}
bool UNeonCityExplosionData::IsValidProfile() const
{
 for(float V:{Damage,InnerRadius,OuterRadius,DamageFalloff,ImpulseStrength,ImpulseRadius,CooldownSeconds,
  OccludedDamageFraction,OccludedImpulseFraction,FireDuration,SmokeDuration,EnvironmentDamageMultiplier,
  VehicleDamageMultiplier,CharacterDamageMultiplier,DangerRadiusScale})
  if(!FMath::IsFinite(V) || V<0) return false;
 if(ExplosionID.IsNone() || uint8(DamageType)>uint8(ENeonCityDamageType::Explosion) || uint8(SourceType)>uint8(ENeonCityExplosionSourceType::Environmental)) return false;
 if(Damage<1 || Damage>5000 || OuterRadius<1 || OuterRadius>800 || InnerRadius>OuterRadius || DamageFalloff<.25f || DamageFalloff>6) return false;
 if(CooldownSeconds<.2f || CooldownSeconds>10 || MaximumTargets<1 || MaximumTargets>64 || DangerRadiusScale<.2f || DangerRadiusScale>4) return false;
 if(OccludedDamageFraction>1 || OccludedImpulseFraction>1 || EnvironmentDamageMultiplier>4 || VehicleDamageMultiplier>4 || CharacterDamageMultiplier>4) return false;
 if(Impulse.CharacterDeltaVelocity<0 || Impulse.CharacterDeltaVelocity>3000 || Impulse.VehicleDeltaVelocity<0 || Impulse.VehicleDeltaVelocity>3000
  || Impulse.PropDeltaVelocity<0 || Impulse.PropDeltaVelocity>3000 || Impulse.WorldDeltaVelocity<0 || Impulse.WorldDeltaVelocity>3000) return false;
 if(Impulse.MaximumImpulse<0 || Impulse.MaximumImpulse>MaximumAuthoredImpulse || Impulse.MaximumDeltaVelocity<0 || Impulse.MaximumDeltaVelocity>3000) return false;
 if(!FMath::IsFinite(Impulse.UpwardBias) || Impulse.UpwardBias<0 || Impulse.UpwardBias>1 || !FMath::IsFinite(Impulse.TumbleOffset) || Impulse.TumbleOffset<0 || Impulse.TumbleOffset>1
  || !FMath::IsFinite(Impulse.LooseBodyDeltaVelocity) || Impulse.LooseBodyDeltaVelocity<0 || Impulse.LooseBodyDeltaVelocity>3000 || Impulse.MaximumLooseBodies<0 || Impulse.MaximumLooseBodies>64
  || !FMath::IsFinite(Impulse.CharacterReactionSpeed) || Impulse.CharacterReactionSpeed<0 || Impulse.CharacterReactionSpeed>2000) return false;
 // Enclosed blast still needs a lethal core; a zero inner radius is a pure sphere falloff.
 return true;
}
float UNeonCityExplosionData::Falloff(float Distance,float InnerRadius,float OuterRadius,float Exponent)
{
 if(!FMath::IsFinite(Distance) || !FMath::IsFinite(InnerRadius) || !FMath::IsFinite(OuterRadius) || !FMath::IsFinite(Exponent) || OuterRadius<=0 || Distance<0) return 0;
 if(Distance<=InnerRadius) return 1;
 if(Distance>=OuterRadius) return 0;
 const float Span=FMath::Max(OuterRadius-InnerRadius,1.f);
 return FMath::Clamp(FMath::Pow(1-(Distance-InnerRadius)/Span,FMath::Clamp(Exponent,.25f,6.f)),0.f,1.f);
}
float UNeonCityExplosionData::DamageAt(float Distance) const { return Damage*Falloff(Distance,InnerRadius,OuterRadius,DamageFalloff); }
float UNeonCityExplosionData::ImpulseAt(float Distance) const { return ImpulseStrength*Falloff(Distance,0,ImpulseRadius,1); }
float UNeonCityExplosionData::DamageMultiplier(EImpactTargetKind Kind) const
{
 switch(Kind)
 {
 case EImpactTargetKind::Character: return CharacterDamageMultiplier;
 case EImpactTargetKind::Vehicle: return VehicleDamageMultiplier;
 default: return EnvironmentDamageMultiplier;
 }
}
float UNeonCityExplosionData::ImpulseDeltaVelocityLimit(EImpactTargetKind Kind) const
{
 switch(Kind)
 {
 case EImpactTargetKind::Character: return Impulse.CharacterDeltaVelocity;
 case EImpactTargetKind::Vehicle: return Impulse.VehicleDeltaVelocity;
 case EImpactTargetKind::Prop: return Impulse.PropDeltaVelocity;
 default: return Impulse.WorldDeltaVelocity;
 }
}
EImpactTargetKind UNeonCityExplosionData::KindOf(const AActor* Actor)
{
 if(!IsValid(Actor)) return EImpactTargetKind::World;
 if(Actor->IsA<ACharacter>()) return EImpactTargetKind::Character;
 if(Actor->IsA<APawn>()) return EImpactTargetKind::Vehicle;
 return Actor->FindComponentByClass<UDestructiblePropComponent>()?EImpactTargetKind::Prop:EImpactTargetKind::World;
}
namespace NeonCityExplosions
{
namespace
{
 TMap<FName,TStrongObjectPtr<UNeonCityExplosionData>>& Registry()
 { static TMap<FName,TStrongObjectPtr<UNeonCityExplosionData>> Map; return Map; }
 UNeonCityExplosionData* Build(FName ID)
 {
  auto* Data=NewObject<UNeonCityExplosionData>(GetTransientPackage(),ID);
  Data->ExplosionID=ID;
  if(ID==TEXT("NeonCity.VehicleExplosion"))
  {
   Data->Damage=240; Data->InnerRadius=180; Data->OuterRadius=620; Data->DamageFalloff=1.15f;
   Data->ImpulseStrength=2.4f; Data->ImpulseRadius=620; Data->CooldownSeconds=1.5f;
   Data->SourceType=ENeonCityExplosionSourceType::Vehicle; Data->OccludedDamageFraction=.15f;
   Data->FireDuration=12; Data->SmokeDuration=18; Data->DangerRadiusScale=1.4f;
   // A car (1.2-2 t) has to visibly jump: the impulse cap is sized for mass * delta velocity, not a token push.
   Data->Impulse.VehicleDeltaVelocity=650; Data->Impulse.PropDeltaVelocity=1800; Data->Impulse.MaximumImpulse=1400000;
   Data->Impulse.UpwardBias=.8f; Data->Impulse.TumbleOffset=.35f; Data->Impulse.LooseBodyDeltaVelocity=1300; Data->Impulse.CharacterReactionSpeed=700;
   Data->OccludedImpulseFraction=.3f;
  }
  else if(ID==TEXT("NeonCity.RocketExplosion"))
  {
   Data->Damage=150; Data->InnerRadius=150; Data->OuterRadius=450; Data->DamageFalloff=1;
   Data->ImpulseStrength=1.6f; Data->ImpulseRadius=450; Data->CooldownSeconds=.4f;
   Data->SourceType=ENeonCityExplosionSourceType::Rocket; Data->OccludedDamageFraction=.1f;
   Data->FireDuration=8; Data->SmokeDuration=12; Data->DangerRadiusScale=1.3f;
   Data->Impulse.VehicleDeltaVelocity=700; Data->Impulse.PropDeltaVelocity=1500; Data->Impulse.MaximumImpulse=1600000;
   Data->Impulse.UpwardBias=.8f; Data->Impulse.TumbleOffset=.35f; Data->Impulse.LooseBodyDeltaVelocity=1200; Data->Impulse.CharacterReactionSpeed=750;
   Data->OccludedImpulseFraction=.3f;
  }
  else if(ID==TEXT("NeonCity.MissileExplosion"))
  {
   Data->Damage=220; Data->InnerRadius=200; Data->OuterRadius=700; Data->DamageFalloff=1.1f;
   Data->ImpulseStrength=2; Data->ImpulseRadius=700; Data->CooldownSeconds=.5f;
   Data->SourceType=ENeonCityExplosionSourceType::Missile; Data->OccludedDamageFraction=.1f;
   Data->FireDuration=10; Data->SmokeDuration=15; Data->DangerRadiusScale=1.35f;
   // Inside the core a 1.5 t car gains ~8.5 m/s, as much up as away: it leaves the ground by a couple of metres,
   // rolls and comes down a few car lengths off. The pressure wave wraps round small cover at a third of its push.
   Data->OccludedImpulseFraction=.35f;
   Data->Impulse.VehicleDeltaVelocity=850; Data->Impulse.PropDeltaVelocity=2000; Data->Impulse.MaximumImpulse=2500000; Data->Impulse.MaximumDeltaVelocity=2400;
   Data->Impulse.UpwardBias=1; Data->Impulse.TumbleOffset=.4f; Data->Impulse.LooseBodyDeltaVelocity=1500; Data->Impulse.CharacterReactionSpeed=900;
  }
  else if(ID==TEXT("NeonCity.SmallExplosion"))
  {
   Data->Damage=60; Data->InnerRadius=100; Data->OuterRadius=380; Data->ImpulseStrength=.8f; Data->ImpulseRadius=380;
  }
  return Data;
 }
}
bool IsNativeID(FName ID)
{ return ID==TEXT("NeonCity.SmallExplosion") || ID==TEXT("NeonCity.VehicleExplosion") || ID==TEXT("NeonCity.RocketExplosion") || ID==TEXT("NeonCity.MissileExplosion"); }
FName NativeID(ENeonCityExplosionSourceType Type)
{
 switch(Type)
 {
 case ENeonCityExplosionSourceType::Vehicle: return TEXT("NeonCity.VehicleExplosion");
 case ENeonCityExplosionSourceType::Rocket: return TEXT("NeonCity.RocketExplosion");
 case ENeonCityExplosionSourceType::Missile: return TEXT("NeonCity.MissileExplosion");
 default: return TEXT("NeonCity.SmallExplosion");
 }
}
const UNeonCityExplosionData& Profile(FName ID)
{
 const FName Key=IsNativeID(ID)?ID:FName(TEXT("NeonCity.SmallExplosion"));
 auto& Map=Registry();
 if(TStrongObjectPtr<UNeonCityExplosionData>* Found=Map.Find(Key); Found && Found->IsValid()) return *Found->Get();
 TStrongObjectPtr<UNeonCityExplosionData> Built(Build(Key)); Map.Add(Key,Built);
 return *Map[Key].Get();
}
}