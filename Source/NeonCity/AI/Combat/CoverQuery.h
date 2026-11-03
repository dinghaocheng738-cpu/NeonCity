#pragma once
#include "CoreMinimal.h"
class ACharacter;
class ACoverPoint;
/** Bounded, throttled caller-side queries. Ground and capsule paths are checked, not teleported. */
namespace NeonCityCover
{
 bool SafeSegment(ACharacter* User,FVector Start,FVector End);
 bool ClearSight(UWorld* World,AActor* Observer,FVector Start,FVector End,AActor* Target=nullptr);
 ACoverPoint* FindAndReserve(ACharacter* User,FVector ThreatAim,float Radius,ACoverPoint* Exclude=nullptr);
}
