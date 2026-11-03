#pragma once
#include "CoreMinimal.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Vehicles/NeonCityVehicleWheel.h"
#include "IslandOffroadVehicle.generated.h"
class USpotLightComponent;

UCLASS()
class NEONCITY_API UIslandTrailWheel : public UNeonCityVehicleWheel
{
 GENERATED_BODY()
public: UIslandTrailWheel();
};
UCLASS()
class NEONCITY_API UIslandTrailFrontWheel : public UIslandTrailWheel
{
 GENERATED_BODY()
public: UIslandTrailFrontWheel();
};
UCLASS()
class NEONCITY_API UIslandPickupWheel : public UIslandTrailWheel
{
 GENERATED_BODY()
public: UIslandPickupWheel();
};
UCLASS()
class NEONCITY_API UIslandPickupFrontWheel : public UIslandPickupWheel
{
 GENERATED_BODY()
public: UIslandPickupFrontWheel();
};
/** Authored expedition bodies on native Chaos AWD, with separate sprung-mass tuning. */
UCLASS()
class NEONCITY_API AIslandOffroadVehicle : public ANeonCityPrototypeVehicle
{
 GENERATED_BODY()
public:
 AIslandOffroadVehicle();
 virtual void Tick(float DeltaSeconds)override;
 UPROPERTY(EditAnywhere,Category="Offroad") bool bPickup=false;
protected:
 virtual void OnConstruction(const FTransform& Transform)override;
 virtual void BeginPlay()override;
private:
 void ConfigureTrailChassis();
 void ApplyExpeditionBody();
 UPROPERTY() TObjectPtr<USpotLightComponent> LeftHeadlight;
 UPROPERTY() TObjectPtr<USpotLightComponent> RightHeadlight;
};
