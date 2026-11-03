#pragma once
#include "CoreMinimal.h"

/** Court coordinates are centimetres: X along court, negative X is the player.
 * A small public rules seam; presentation and input cannot award arbitrary points. */
struct NEONCITY_API FIslandTennisRules
{
 int32 Points[2]={0,0},Games[2]={0,0};
 int32 LastHitter=-1,Bounces=0,Winner=-1,Faults=0;
 bool Rally=false,Service=false,FirstLanding=true;
 static constexpr float HalfLength=1188.5f;
 static constexpr float HalfWidth=411.5f;
 static constexpr float ServiceLength=640.f;
 bool Strike(int32 Side,bool Serve=false);
 bool CanServeFrom(const FVector& Local)const;
 bool Bounce(const FVector& Local);
 void Net();
 FString Score()const;
 void Reset();
private:
 void Award(int32 Side);
 void Fault();
};
