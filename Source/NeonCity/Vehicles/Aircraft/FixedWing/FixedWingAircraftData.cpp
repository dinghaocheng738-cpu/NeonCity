#include "Vehicles/Aircraft/FixedWing/FixedWingAircraftData.h"
UFixedWingAircraftData::UFixedWingAircraftData()
{
 AircraftType=TEXT("NeonCity.LightAircraftPrototype"); Mass=900; MaxSpeed=198; Durability=700;
 BodyExtent=FVector(320,85,65); CenterOfMass=FVector(0,0,-20); SafeGroundImpactSpeed=300;
}
bool UFixedWingAircraftData::IsValidProfile() const
{
 const float Values[]={EngineThrust,CruiseSpeed,TakeoffSpeed,StallSpeed,WingArea,LiftCoefficient,LiftSlope,MaximumLiftCoefficient,
  DragCoefficient,InducedDrag,AirDensity,PitchAuthority,RollAuthority,YawAuthority,ControlResponse,ThrottleRate,GroundBrake,
  GroundSteering,StallNoseDown,StallControlFraction,SideSlipDamping,OverspeedDrag,CameraDistance,CameraSpeedExtension,CameraFOV,CameraSpeedFOV,CameraBankFraction};
 for(float V:Values) if(!FMath::IsFinite(V) || V<=0) return false;
 return IsValidAirframe() && FixedLandingGear && StallSpeed<TakeoffSpeed && TakeoffSpeed<CruiseSpeed && CruiseSpeed<MaxSpeed
  && EngineThrust<Mass*30 && WingArea<=100 && LiftCoefficient<MaximumLiftCoefficient && MaximumLiftCoefficient<=4
  && DragCoefficient<=1 && InducedDrag<=1 && AirDensity<=2 && PitchAuthority<=90 && RollAuthority<=120 && YawAuthority<=45
  && ControlResponse<=10 && ThrottleRate<=2 && GroundBrake<=15 && StallControlFraction<1 && CameraBankFraction<=.5f
  && CameraFOV>=40 && CameraFOV+CameraSpeedFOV<=110 && CameraDistance<=5000 && CameraSpeedExtension<=2000
  && LiftSlope<=12 && GroundSteering<=60 && StallNoseDown<=30 && SideSlipDamping<=6 && OverspeedDrag<=10;
}
