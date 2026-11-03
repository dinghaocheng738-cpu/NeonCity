#pragma once
#include "CoreMinimal.h"
#include "Explosion/ExplosionData.h"
class UPrimitiveComponent;
/** Controlled radial impulse. Mass scaling and hard clamps replace raw AddRadialImpulse. */
class NEONCITY_API FExplosionImpulseUtility
{
public:
 struct FRequest
 {
  FVector Center=FVector::ZeroVector;
  FVector Target=FVector::ZeroVector;
  EImpactTargetKind Kind=EImpactTargetKind::World;
  float Falloff=0;
  float ImpulseStrength=1;
  float TargetMass=0;
  float OcclusionFraction=1;
  float WeatherScale=1;
  /** Loose bodies use the policy's LooseBodyDeltaVelocity instead of the per-kind limit. */
  bool bLooseBody=false;
 };
 /** Push direction: radial from the blast, lifted by the policy's upward bias. */
 static FVector PushDirection(const FVector& Center,const FVector& Target,float UpwardBias);
 /** Bounded delta velocity for one target; characters keep a limit of zero by default. */
 static float VelocityDelta(EImpactTargetKind Kind,float Falloff,float ImpulseStrength,float OcclusionFraction,float WeatherScale,const FNeonCityImpulsePolicy& Policy);
 static float ImpulseMagnitude(float TargetMass,float VelocityDelta,const FNeonCityImpulsePolicy& Policy);
 /** Applies at most one impulse, only to an awake-eligible dynamic body below the runaway speed gate. */
 static bool Apply(UPrimitiveComponent* Body,const FRequest& Request,const FNeonCityImpulsePolicy& Policy);
 static constexpr float RunawaySpeed=3000.f;
};