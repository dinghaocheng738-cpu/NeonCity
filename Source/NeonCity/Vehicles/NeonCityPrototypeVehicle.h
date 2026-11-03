#pragma once
#include "CoreMinimal.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "NeonCityPrototypeVehicle.generated.h"
/** Three data-driven test body styles over the proven four-wheel prototype chassis. */
UCLASS()
class NEONCITY_API ANeonCityPrototypeVehicle : public ANeonCityStarterSedan
{
 GENERATED_BODY()
public: ANeonCityPrototypeVehicle();
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
private: void ApplyBodyStyle();
};
