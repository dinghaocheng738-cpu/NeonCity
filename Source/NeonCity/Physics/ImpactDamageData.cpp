#include "Physics/ImpactDamageData.h"
UImpactDamageData::UImpactDamageData()
{
 const float Speeds[]={0,150,300,650,1100,1600,2300,3500,6000};
 const float Cars[]={0,0,1,5,18,42,90,170,300};
 const float People[]={0,0,5,25,65,90,140,200,300};
 const float Props[]={0,0,10,35,100,150,200,250,300};
 for(int32 I=0;I<UE_ARRAY_COUNT(Speeds);++I)
 { VehicleDamage.GetRichCurve()->AddKey(Speeds[I],Cars[I]); CharacterDamage.GetRichCurve()->AddKey(Speeds[I],People[I]); PropDamage.GetRichCurve()->AddKey(Speeds[I],Props[I]); }
}
float UImpactDamageData::Damage(float Speed,float SourceMass,float TargetMass,EImpactTargetKind Kind,float Scale) const
{
 if(!FMath::IsFinite(Speed) || !FMath::IsFinite(SourceMass) || !FMath::IsFinite(TargetMass) || !FMath::IsFinite(Scale) || SourceMass<=0 || TargetMass<0 || Speed<MinimumSpeed) return 0;
 Speed=FMath::Clamp(Speed,0.f,MaximumSpeed); SourceMass=FMath::Clamp(SourceMass,1.f,10000.f);
 if(Kind==EImpactTargetKind::Vehicle)
 {
  if(TargetMass<=0) return 0;
  // Equivalent energy per receiving mass: sqrt(2 * (0.5 * reducedMass * vNormal^2) / receivingMass).
  Speed*=FMath::Sqrt(SourceMass/(SourceMass+TargetMass));
 }
 else if(Kind==EImpactTargetKind::Character || Kind==EImpactTargetKind::Prop) Speed*=FMath::Sqrt(FMath::Clamp(SourceMass/1450.f,.4f,2.f));
 const auto& Curve=Kind==EImpactTargetKind::Character?CharacterDamage:Kind==EImpactTargetKind::Prop?PropDamage:VehicleDamage;
 return FMath::Clamp(Curve.GetRichCurveConst()->Eval(Speed)*FMath::Clamp(Scale,.1f,2.f),0.f,500.f);
}
EImpactSeverity UImpactDamageData::Severity(float Speed) const
{ return Speed>=ExtremeSpeed?EImpactSeverity::Extreme:Speed>=HighSpeed?EImpactSeverity::High:Speed>=MediumSpeed?EImpactSeverity::Medium:EImpactSeverity::Low; }
bool UImpactDamageData::IsValidProfile() const
{
 const float Values[]={MinimumSpeed,MediumSpeed,HighSpeed,ExtremeSpeed,PairCooldown,MaximumSpeed,MaximumPropSpeed};
 for(float V:Values) if(!FMath::IsFinite(V)) return false;
 for(const auto* Curve:{&VehicleDamage,&CharacterDamage,&PropDamage})
 {
  if(Curve->GetRichCurveConst()->GetNumKeys()<2) return false;
  float Previous=-1;
  for(const auto& Key:Curve->GetRichCurveConst()->GetConstRefOfKeys())
  { if(!FMath::IsFinite(Key.Time) || !FMath::IsFinite(Key.Value) || Key.Time<0 || Key.Value<0 || Key.Value<Previous || Key.Value>500) return false; Previous=Key.Value; }
 }
 return MinimumSpeed>=100 && MinimumSpeed<MediumSpeed && MediumSpeed<HighSpeed && HighSpeed<ExtremeSpeed && ExtremeSpeed<=MaximumSpeed
  && MaximumSpeed<=10000 && PairCooldown>=.3f && PairCooldown<=3 && MaximumPropSpeed>=100 && MaximumPropSpeed<=800;
}
