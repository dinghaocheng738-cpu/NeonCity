#pragma once
#include "CoreMinimal.h"

/** Balance responds to a short physical interval, not the derivative of one
 * noisy contact-velocity sample. Real collision impulses remain immediate. */
struct FDynamicSurfaceMotionFilter
{
 FVector Acceleration=FVector::ZeroVector;
 void Reset(){Acceleration=FVector::ZeroVector;}
 FVector Update(const FVector& VelocityChange,double Seconds)
 {
  if(VelocityChange.ContainsNaN() || !FMath::IsFinite(Seconds) || Seconds<=0 || Seconds>=.25)
  {Reset();return Acceleration;}
  const double Alpha=1.-FMath::Exp(-Seconds/.06);
  Acceleration+=Alpha*(VelocityChange/Seconds-Acceleration);
  return Acceleration;
 }
};
