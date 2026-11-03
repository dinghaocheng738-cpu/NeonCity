#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "AircraftWaterInteraction.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAircraftWaterChanged,EAircraftWaterState,Previous,EAircraftWaterState,Current);
/** Aircraft-specific immersion state; deliberately independent of car stalling and boat buoyancy. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UAircraftWaterInteraction : public UActorComponent
{
 GENERATED_BODY()
public:
 UAircraftWaterInteraction();
 void UpdateImmersion(float BodyHalfHeight);
 UFUNCTION(BlueprintPure) EAircraftWaterState GetWaterState() const { return State; }
 UPROPERTY(BlueprintAssignable) FAircraftWaterChanged OnWaterStateChanged;
private:
 EAircraftWaterState State=EAircraftWaterState::Dry;
};
