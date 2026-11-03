#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleLaserMark.generated.h"
class UHealthComponent;

/** The spot a vehicle's laser designator is holding on ground, buildings or water: what a guided missile flies to
 * when no vehicle or person is in the seeker. Invisible, no collision, never damaged (it has health only so the
 * missile's guidance treats it as a live aim point); one per combat component, moved as the laser moves. */
UCLASS(NotPlaceable,Transient)
class NEONCITY_API AVehicleLaserMark : public AActor
{
 GENERATED_BODY()
public:
 AVehicleLaserMark();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
};
