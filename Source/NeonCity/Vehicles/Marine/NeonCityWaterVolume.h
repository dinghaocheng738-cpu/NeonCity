#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PhysicsVolume.h"
#include "Water/WaterStateInterface.h"
#include "NeonCityWaterVolume.generated.h"
class ACoastalOceanSurface;
/** Bounded native swim volume with an optional shared displaced ocean surface. */
UCLASS()
class NEONCITY_API ANeonCityWaterVolume : public APhysicsVolume, public IWaterStateInterface
{
 GENERATED_BODY()
public:
 ANeonCityWaterVolume();
 UPROPERTY(EditAnywhere) FVector Extent=FVector(8000,12000,2000);
 UPROPERTY(EditAnywhere) FVector CurrentVelocity=FVector::ZeroVector;
 /** Finite architectural pool: no displaced-crest envelope or water below its basin. */
 UPROPERTY(EditAnywhere,Category="Water") bool bContainedPoolWater=false;
 /** Overlapping contained basins: the highest priority answers (a lock chamber inside the lake-wide basin). */
 UPROPERTY(EditAnywhere,Category="Water") int32 BasinPriority=0;
 UPROPERTY() TObjectPtr<ACoastalOceanSurface> OceanSurface;
 bool Sample(FVector Point,float& SurfaceZ,FVector& Velocity) const;
 virtual bool QueryWaterSurface_Implementation(FVector Point,FWaterSurfaceSample& Result) const override;
 virtual bool IsOverlapInVolume(const USceneComponent& TestComponent) const override;
protected:
 virtual void PostInitializeComponents() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
};
