#pragma once
#include "CoreMinimal.h"

/** Source frame: metres, +X port, +Y up, +Z bow. Pure forces, no actor transforms or contacts. */
namespace NeonCityReferenceBoat
{
 struct FHydroState
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector,Angular=FVector::ZeroVector;
  FQuat Rotation=FQuat::Identity;
  double RPM=0,Throttle=0,Steering=0,Gravity=9.81;
  bool Driven=false;
  bool Moored=false;
  FVector MooringAnchor=FVector::ZeroVector;
  double MooringHeading=0;
  TArray<double> WaterHeight,WaterVerticalVelocity;
 };
 struct FHydroResult
 {
  FVector Acceleration=FVector::ZeroVector,AngularAcceleration=FVector::ZeroVector;
  double WetFraction=0,Thrust=0;
 };
 NEONCITY_API FHydroResult EvaluateHydrodynamics(const FHydroState& State);
 // Polar vectors reflect handedness; rotation/torque are axial vectors.
 inline FVector ToUEPolar(FVector V) { return FVector(V.Z,-V.X,V.Y); }
 inline FVector ToSourcePolar(FVector V) { return FVector(-V.Y,V.Z,V.X); }
 inline FVector ToUEAxial(FVector V) { return -ToUEPolar(V); }
 inline FVector ToSourceAxial(FVector V) { return -ToSourcePolar(V); }
 inline FQuat ToSourceRotation(FQuat Q) { return FQuat(Q.Y,-Q.Z,-Q.X,Q.W); }
}
