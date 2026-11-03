#pragma once
#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "NeonCityVehicleWheel.generated.h"

UCLASS()
class NEONCITY_API UNeonCityVehicleWheel : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UNeonCityVehicleWheel();
};

UCLASS()
class NEONCITY_API UNeonCityVehicleFrontWheel : public UNeonCityVehicleWheel
{
	GENERATED_BODY()
public:
	UNeonCityVehicleFrontWheel();
};
