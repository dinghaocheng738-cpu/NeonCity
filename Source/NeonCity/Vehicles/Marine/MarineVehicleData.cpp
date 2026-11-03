#include "Vehicles/Marine/MarineVehicleData.h"
bool UMarineVehicleData::IsValidProfile() const
{
 auto In=[](float V,float Low,float High){return FMath::IsFinite(V) && V>=Low && V<=High;};
 return CameraRig.IsValid() && !VehicleType.IsNone() && In(MaxSpeed,5,120) && In(Acceleration,.1f,10) && In(TurningRate,1,90)
  && In(Mass,300,400000) && In(Buoyancy,1.1f,4) && In(WaterDrag,.05f,5) && In(LateralDrag,.1f,10)
  && In(TurningResistance,1,10) && In(VerticalDamping,1,10) && In(EnginePower,1,10000) && In(BrakeDrag,.1f,10)
  && In(ThrottleRiseRate,.1f,10) && In(ThrottleFallRate,.1f,20)
  && In(Durability,1,100000) && !HullExtent.ContainsNaN() && HullExtent.GetMin()>=20 && HullExtent.GetMax()<4000
  && !CenterOfMass.ContainsNaN() && FMath::Abs(CenterOfMass.X)<HullExtent.X && FMath::Abs(CenterOfMass.Y)<HullExtent.Y && FMath::Abs(CenterOfMass.Z)<HullExtent.Z
  && !DeckExit.ContainsNaN() && !BoardingPoint.ContainsNaN() && In(InteractionReachCm,300,1000);
}

UReferenceFishingBoatData::UReferenceFishingBoatData()
{
 VehicleType=TEXT("NeonCity.HarborFishingBoat"); DisplayName=NSLOCTEXT("NeonCity","HarborFishingBoat","Harborline 82");
 MaxSpeed=45;Mass=3204;Durability=1400;HullExtent=FVector(390,125,34);
 CenterOfMass=FVector(-73.767883967,0,30);bReferenceDisplacementHull=true;bWalkableDeck=true;
 BoardingPoint=FVector(-145,10,35);
 CameraRig.HelmEye=FVector(55,55,180);CameraRig.ChaseAnchor=FVector(-40,0,230);CameraRig.ChaseDistance=1150;
 // Canonical 62-degree vertical lens is approximately 94 degrees horizontally at 16:9.
 CameraRig.HelmFieldOfView=94;
 ThrottleRiseRate=2.2f;ThrottleFallRate=2.2f;
}
