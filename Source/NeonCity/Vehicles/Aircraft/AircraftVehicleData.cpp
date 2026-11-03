#include "Vehicles/Aircraft/AircraftVehicleData.h"
bool UAircraftVehicleData::IsValidAirframe() const
{
 const float Values[]={Mass,MaxSpeed,Durability,CriticalFraction,DisabledFraction,SeriousImpactSpeed,SafeGroundImpactSpeed,FuelCapacity};
 for(float V:Values) if(!FMath::IsFinite(V) || V<=0) return false;
 return !AircraftType.IsNone() && Mass>=100 && Mass<=10000 && MaxSpeed<=300 && Durability<=100000
  && DisabledFraction<CriticalFraction && CriticalFraction<.5f && SeriousImpactSpeed>=1100 && SeriousImpactSpeed<=6000
  && SafeGroundImpactSpeed>=150 && SafeGroundImpactSpeed<=500 && FuelCapacity<=10000
  && !BodyExtent.ContainsNaN() && BodyExtent.GetMin()>30 && BodyExtent.GetMax()<1000
  && !CenterOfMass.ContainsNaN() && CenterOfMass.GetAbsMax()<BodyExtent.GetMin();
}
bool UAircraftVehicleData::IsValidProfile() const
{
 const float Values[]={Mass,MaxSpeed,Acceleration,LiftForce,VerticalSpeed,RotationSpeed,AirResistance,HoverBrake,HeightResponse,
  VerticalResponse,AttitudeResponse,AngularResistance,MaximumBank,LandingSpeed,Durability,CriticalFraction,DisabledFraction,
  SeriousImpactSpeed,SafeGroundImpactSpeed,FuelCapacity,HoverPower,PowerResponse,PowerAdjustmentRate,FastFlightAccelerationScale,Deceleration};
 for(float V:Values) if(!FMath::IsFinite(V) || V<=0) return false;
 return !AircraftType.IsNone() && Mass>=100 && Mass<=10000 && MaxSpeed<=300 && Acceleration<=20 && LiftForce>Mass*9.81f && LiftForce<Mass*50
  && HoverPower>=.3f && HoverPower<=.8f && PowerResponse<=8 && PowerAdjustmentRate<=2 && FastFlightAccelerationScale>=1 && FastFlightAccelerationScale<=2.5f && Deceleration<=10
  && VerticalSpeed<=15 && RotationSpeed<=120 && AirResistance<=5 && HoverBrake<=10 && HeightResponse<=5 && VerticalResponse<=8
  && AttitudeResponse<=30 && AngularResistance<=15 && MaximumBank<=25 && LandingSpeed<=2 && Durability<=100000
  && DisabledFraction<CriticalFraction && CriticalFraction<.5f && SeriousImpactSpeed>=1100 && SeriousImpactSpeed<=6000
  && SafeGroundImpactSpeed>=150 && SafeGroundImpactSpeed<=500 && FuelCapacity<=10000
  && !BodyExtent.ContainsNaN() && BodyExtent.GetMin()>30 && BodyExtent.GetMax()<1000
  && !CenterOfMass.ContainsNaN() && CenterOfMass.GetAbsMax()<BodyExtent.GetMin();
}
