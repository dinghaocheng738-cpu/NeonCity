#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityTrafficRoute.generated.h"
class USplineComponent;
class ANeonCityTrafficVehicle;

/** Editor-authored road nodes; spawn a bounded pair of ambient vehicles once per play world. */
UCLASS()
class NEONCITY_API ANeonCityTrafficRoute : public AActor
{
	GENERATED_BODY()
public:
	ANeonCityTrafficRoute();
	USplineComponent* GetRoadSpline() const { return Road; }
	UPROPERTY(EditAnywhere, Category="Traffic", meta=(ClampMin="0", ClampMax="3"))
	int32 VehicleCount = 2;
 // Road centreline authoring uses an explicit lane offset; existing traffic splines are lane centres.
 UPROPERTY(EditAnywhere,Category="Road|Cruise") bool bCruiseEligible=true;
 UPROPERTY(EditAnywhere,Category="Road|Cruise") bool bBidirectional=false;
 UPROPERTY(EditAnywhere,Category="Road|Cruise",meta=(ClampMin="0",ClampMax="200")) float CruiseLaneOffsetCm=0;
 UPROPERTY(EditAnywhere,Category="Road|Cruise",meta=(ClampMin="5",ClampMax="130")) float CruiseSpeedLimitKmh=45;
 /** Some traffic guides are authored at chassis height. Navigation/cruise
  * safety uses the measured road surface; traffic keeps its original spline. */
 UPROPERTY(EditAnywhere,Category="Road|Cruise",meta=(ClampMin="0",ClampMax="200",Units="cm")) float CruiseSurfaceOffsetCm=0;
 UPROPERTY(EditAnywhere,Category="Road|Cruise") FName RoadID;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(VisibleAnywhere, Category="Traffic")
	TObjectPtr<USplineComponent> Road;
	TArray<TWeakObjectPtr<ANeonCityTrafficVehicle>> Spawned;
};
