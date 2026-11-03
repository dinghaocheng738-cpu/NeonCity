#include "Property/PropertyData.h"
bool UNeonCityPropertyData::IsValidData() const
{
 return !PropertyID.IsNone() && !DisplayName.IsEmpty() && uint8(Type)<=uint8(ENeonCityPropertyType::TeaEstate)
  && Price>=0 && Price<=1000000000000LL && !Location.ContainsNaN()
  && GarageSlotCount>=0 && GarageSlotCount<=8 && DailyIncome>=0 && DailyIncome<=1000000 && (DailyIncome==0 || IsBusiness());
}
