#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WaterStateInterface.generated.h"

UENUM(BlueprintType)
enum class EWaterInteractionState : uint8 { Dry, Shallow, Surface, Underwater };

/** A query is scoped to a provider's horizontal footprint, not a global sea-level test. */
USTRUCT(BlueprintType)
struct NEONCITY_API FWaterSurfaceSample
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FVector SurfacePoint=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector Normal=FVector::UpVector;
 UPROPERTY(BlueprintReadOnly) FVector Flow=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) bool InsideVolume=false;
 /** Finite filtered water uses swimming without open-sea optical particles. */
 UPROPERTY(BlueprintReadOnly) bool ContainedWater=false;
 float DepthAt(FVector Point) const { return FVector::DotProduct(SurfacePoint-Point,Normal); }
};
UINTERFACE(BlueprintType)
class NEONCITY_API UWaterStateInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IWaterStateInterface
{
 GENERATED_BODY()
public:
 /** Future WaterBody adapters can provide their local surface/flow through the same contract. */
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool QueryWaterSurface(FVector Point,FWaterSurfaceSample& Sample) const;
};
