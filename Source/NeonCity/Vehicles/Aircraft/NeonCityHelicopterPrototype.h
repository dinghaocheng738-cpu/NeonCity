#pragma once
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "NeonCityHelicopterPrototype.generated.h"
/** Graybox presentation belongs to this prototype, not to future planes/drones. */
UCLASS()
class NEONCITY_API ANeonCityHelicopterPrototype : public ANeonCityAircraftVehicleBase
{
 GENERATED_BODY()
public:
 ANeonCityHelicopterPrototype();
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rotor;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TailRotor;
};
