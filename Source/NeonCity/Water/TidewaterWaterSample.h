// Adapted from Tidewater BoatController.readQueries/step at 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT: Docs/ThirdParty/Tidewater-LICENSE.txt.
#pragma once
#include "CoreMinimal.h"

/** One named sample, in source metres. Query latency predicts the INPUT point;
 * it must not be added again to the blended output on every readback. */
struct FTidewaterWaterSample
{
 FVector2D Point=FVector2D::ZeroVector,Slope=FVector2D::ZeroVector;
 double Height=0,Vertical=0,Offset=0,Effective=0,Time=-1,Age=0;
 bool Valid=false;
 void Advance(double Delta)
 {
  if(!Valid || !FMath::IsFinite(Delta) || Delta<=0)return;
  Age=FMath::Min(Age+Delta,.12);Offset*=FMath::Exp(-Delta/.06);
  Effective=Height+Vertical*Age+Offset;
 }
 bool Accept(FVector4f Value,FVector2D IssuedPoint,double IssuedTime)
 {
  if(Value.ContainsNaN() || IssuedPoint.ContainsNaN() || !FMath::IsFinite(IssuedTime)
   || (Valid && IssuedTime<=Time))return false;
  const double NY=FMath::Sqrt(FMath::Max(1.-Value.Y*Value.Y-Value.Z*Value.Z,.05));
  const FVector2D NewSlope(-Value.Y/NY,-Value.Z/NY);const double DT=IssuedTime-Time;
  // A nearby save/load/fixture teleport is still discontinuous. A fixed 20 m
  // allowance turns a 10 m relocation in one frame into fictitious water speed.
  // The 60 m/s envelope exceeds every current hull speed without limiting boats.
  const bool Continuous=Valid && DT>1e-4 && DT<.5 && (IssuedPoint-Point).Size()<FMath::Max(2.,DT*60.);
  const double W=Continuous?(Value.X-Height-.5*FVector2D::DotProduct(NewSlope+Slope,IssuedPoint-Point))/DT:0;
  Offset=Continuous?Effective-Value.X:0;
  Vertical=Continuous?Vertical+(FMath::Clamp(W,-3.,3.)-Vertical)*.5:0;
  Height=Value.X;Point=IssuedPoint;Slope=NewSlope;Time=IssuedTime;Age=0;
  Effective=Height+Offset;Valid=true;return true;
 }
};
