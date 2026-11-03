#pragma once
#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/** Immutable boat snapshot; island-centred X/Z metres, forward X/Z direction. */
struct FNeonCityWakeInput
{
 FVector2f Position=FVector2f::ZeroVector,Forward=FVector2f(0,1);
 FVector2f HullScale=FVector2f(1,1); // beam, length relative to the source fishing hull
 float Speed=0,Throttle=0,RPM=0;
 uint32 BoatID=0;
 bool Driven=false;
};
struct FNeonCityWakeFrame
{
 FRDGBufferRef State=nullptr,Scratch=nullptr,Spec=nullptr,AerA=nullptr,AerB=nullptr,NearBuffer=nullptr;
 FRDGTextureRef Display=nullptr,Aeration=nullptr,Near=nullptr;
 FVector4f Controls=FVector4f(0,0,0,0),Boat=FVector4f(0,0,1,0);
 FVector2f HullScale=FVector2f(1,1);
 bool Stepped=false;
};
/** Tidewater's three-pass dispersive wake: 512², four depth operators, pressure,
 * turbulent foam/aeration and moving-window history. Owned by the shared ocean. */
class NEONCITYOCEAN_API FNeonCityOceanWake final
{
public:
 FNeonCityOceanWake();
 ~FNeonCityOceanWake();
 FNeonCityWakeFrame AddFrame_RenderThread(FRDGBuilder& Graph,const FNeonCityWakeInput& Boat,
  float Time,float Delta,FRDGTextureRef IslandHeight,FRDGTextureRef Hull,FRDGTextureRef Piles);
 void Reset_RenderThread();
private:
 struct FState;
 TUniquePtr<FState> State;
};
