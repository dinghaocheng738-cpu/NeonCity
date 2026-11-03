#pragma once
#include "CoreMinimal.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "NeonCityStarterSedan.generated.h"
class UStaticMeshComponent;
class UIslandSupercarPresentationData;
class UPhysicsAsset;

/** Fitted luxury fleet presentation over the reusable Chaos vehicle. */
UCLASS()
class NEONCITY_API ANeonCityStarterSedan : public ANeonCityBaseVehicle
{
	GENERATED_BODY()
public:
	ANeonCityStarterSedan();
protected:
	/** Store constructor defaults; update registered runtime bodies normally. */
	void SetChassisPhysicsAsset(UPhysicsAsset* Asset);
	virtual TArray<float> GetFittedWheelRadii() const override;
 virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
private:
 void ConfigureFleetFit();
	void UpdateWheelVisuals(bool bSimulating);
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;
 UPROPERTY(Transient) TObjectPtr<UIslandSupercarPresentationData> FleetPresentation;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FleetCalipers;
};
