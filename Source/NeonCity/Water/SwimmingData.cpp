#include "Water/SwimmingData.h"
bool USwimmingData::IsValidProfile() const
{
 for(float Value:{SwimSpeed,UnderwaterSpeed,Acceleration,AscendSpeed,DescendSpeed})
  if(!FMath::IsFinite(Value) || Value<=0 || Value>3000) return false;
 return FMath::IsFinite(WaterDrag) && WaterDrag>=0 && WaterDrag<=10;
}
