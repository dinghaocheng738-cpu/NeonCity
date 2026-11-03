#include "Fishing/FishData.h"
bool UFishData::IsValidProfile() const
{
 return !SpeciesID.IsNone() && !Name.IsEmpty() && !Habitat.IsNone() && uint8(Rarity)<=2 && uint8(TimePreference)<=3
  && FMath::IsFinite(WeightKg.X) && FMath::IsFinite(WeightKg.Y) && WeightKg.X>0 && WeightKg.Y>=WeightKg.X && WeightKg.Y<=250
  && FMath::IsFinite(SizeCm.X) && FMath::IsFinite(SizeCm.Y) && SizeCm.X>0 && SizeCm.Y>=SizeCm.X && SizeCm.Y<=450
  && ValuePerKg>0 && ValuePerKg<=10000
  && FMath::IsFinite(FightStrength) && FightStrength>=0 && FightStrength<=1
  && FMath::IsFinite(FightStaminaSeconds) && FightStaminaSeconds>0 && FightStaminaSeconds<=600;
}
float UFishData::EncounterWeight(FName AtHabitat,float TimeOfDay,FName Weather) const
{
 if(!IsValidProfile() || AtHabitat!=Habitat || !FMath::IsFinite(TimeOfDay))return 0;
 float Weight=Rarity==ENeonCityFishRarity::Common?6.f:(Rarity==ENeonCityFishRarity::Uncommon?3.f:1.f);
 const float Hour=FMath::Fmod(FMath::Max(0.f,TimeOfDay),24.f);
 const bool PreferredTime=TimePreference==ENeonCityFishTime::Any
  || (TimePreference==ENeonCityFishTime::Day && Hour>=7 && Hour<18)
  || (TimePreference==ENeonCityFishTime::Dusk && Hour>=17 && Hour<21)
  || (TimePreference==ENeonCityFishTime::Night && (Hour>=20 || Hour<6));
 if(PreferredTime)Weight*=1.5f;
 if(WeatherPreference!=TEXT("Any") && WeatherPreference==Weather)Weight*=1.4f;
 return Weight;
}
