#pragma once

#include "CoreMinimal.h"

/** Resting curved coconut stem, adapted from Tidewater PlantGeometry/VegNodes
 *  (MIT, Copyright (c) 2026 DRG Software Solutions LLC).
 *  Notice: Docs/ThirdParty/Tidewater-LICENSE.txt. No leaf/crown collision.
 *  Shader height is independent of the instance's radial scale.
 */
namespace NeonCityPalmContact
{
 inline constexpr int32 SegmentCount=10;
 inline constexpr float Rows[SegmentCount+1]={-.03f,0,.025f,.06f,.12f,.24f,.40f,.56f,.72f,.86f,1.f};
 inline FVector Center(const FVector& Root,float HeightCm,float Lean,float Azimuth,float Seed,float U)
 {
  const float Curve=FMath::Frac(Seed*7.31f);
  const float F=FMath::Lerp(U,U*(2-U),Curve);
  return Root+FVector(FMath::Cos(Azimuth),-FMath::Sin(Azimuth),0)*(Lean*F*HeightCm)+FVector(0,0,U*HeightCm);
 }
 inline float RadiusCm(float U,float Scale)
 {
  const float Y=U*10;
  float R=.155f+.045f*(1-U)+.19f*FMath::Exp(-FMath::Max(Y,0.f)/.42f);
  R*=1+.05f*FMath::Sin(Y*1.7f)*(1-U);
  R+=.07f*FMath::SmoothStep(.955f,.99f,U)-.1f*FMath::SmoothStep(.995f,1.02f,U);
  // Mean root flare. The tiny asymmetric root lumps remain visual geometry;
  // capsule radii do not envelop the full crown or the bent stem's empty side.
  return FMath::Max(1.f,R*Scale*100);
 }
}
