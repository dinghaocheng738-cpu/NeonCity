#include "Aircraft/Lighting/SearchLightData.h"
bool USearchLightData::IsValidProfile() const
{
 for(float V:{Intensity,Range,InnerCone,OuterCone,RotationSpeed,DayMultiplier,ShadowDistance,CullDistance,VolumetricScattering}) if(!FMath::IsFinite(V)) return false;
 return Intensity>0 && Intensity<=1000000 && Range>=100 && Range<=15000 && InnerCone>0 && InnerCone<OuterCone && OuterCone<=60
  && RotationSpeed>0 && RotationSpeed<=180 && DayMultiplier>=0 && DayMultiplier<=1 && ShadowDistance>0 && CullDistance>ShadowDistance && CullDistance<=50000
  && VolumetricScattering>=0 && VolumetricScattering<=16;
}
