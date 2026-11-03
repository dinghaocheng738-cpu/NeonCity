#include "AI/Combat/CombatAIData.h"
#include "Combat/Weapons/NeonCityPistol.h"
UCombatAIData::UCombatAIData() { WeaponClass=ANeonCityPistol::StaticClass(); }
bool UCombatAIData::IsValidProfile() const
{
 const float Values[]={RetreatHealthFraction,Accuracy,AimError,ReactionTime,BurstPause,EngagementRange,SightRange,SightHalfAngle,SearchSeconds,ChaseLeash,MaximumCombatSeconds,MoveSpeed,CoverRadius,PerceptionInterval,CoverQueryInterval,FarUpdateInterval};
 for(float V:Values) if(!FMath::IsFinite(V)) return false;
 return uint8(Role)<=uint8(ENeonCityCombatRole::Hostile) && uint8(Faction)<=uint8(ENeonCityFaction::Police)
  && Accuracy>=0 && Accuracy<=.95f && AimError>=.5f && AimError<=20 && ReactionTime>=.1f && ReactionTime<=5
  && BurstLength>=1 && BurstLength<=6 && BurstPause>=.2f && BurstPause<=5 && EngagementRange>=200 && EngagementRange<=5000
  && SightRange>=EngagementRange && SightRange<=6000 && SightHalfAngle>=15 && SightHalfAngle<=100
  && SearchSeconds>=1 && SearchSeconds<=20 && ChaseLeash>=200 && ChaseLeash<=5000 && MaximumCombatSeconds>=5 && MaximumCombatSeconds<=120
  && MoveSpeed>=100 && MoveSpeed<=400 && CoverRadius>=100 && CoverRadius<=2000 && PerceptionInterval>=.1f && PerceptionInterval<=1
  && CoverQueryInterval>=.5f && CoverQueryInterval<=5 && FarUpdateInterval>=.5f && FarUpdateInterval<=3
  && RetreatHealthFraction>=.21f && RetreatHealthFraction<=.5f && (!CanFight || (Role!=ENeonCityCombatRole::Civilian && WeaponClass));
}
