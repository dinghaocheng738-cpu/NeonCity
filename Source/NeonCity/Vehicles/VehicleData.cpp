#include "Vehicles/VehicleData.h"
bool UVehicleData::IsValidProfile() const
{
 auto InRange=[](float V,float Min,float Max){ return FMath::IsFinite(V) && V>=Min && V<=Max; };
 return uint8(VehicleType)<=uint8(ENeonCityVehicleType::Pickup) && !DisplayName.IsEmpty()
  && InRange(MaxSpeed,10,VehicleType==ENeonCityVehicleType::SportsCar?320:160)
  && InRange(Acceleration,.5f,VehicleType==ENeonCityVehicleType::SportsCar?12:8) && InRange(BrakeForce,1000,30000)
  && InRange(Weight,500,3000) && InRange(Handling,5,45) && InRange(Grip,.2f,4) && InRange(Durability,10,1000)
  && !ExplosionDataID.IsNone() && InRange(ExplosionDelayMin,.5f,30) && InRange(ExplosionDelayMax,.5f,30)
  && ExplosionDelayMax>=ExplosionDelayMin && MaximumChainDepth>=0 && MaximumChainDepth<=4
  && InRange(ChainReactionThreshold,0,1);
}
