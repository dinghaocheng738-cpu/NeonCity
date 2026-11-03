#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DynamicSurfaceInteractionInterface.generated.h"

class ACharacter;

/** Contact-local contract, in cm/s and cm/s^2. No Actor attachment or persistent references. */
USTRUCT(BlueprintType)
struct NEONCITY_API FDynamicStandingSurface
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FVector PointVelocity=FVector::ZeroVector;
 UPROPERTY(BlueprintReadWrite) float MaxLandingHorizontalSpeed=650.f;
 UPROPERTY(BlueprintReadWrite) float MaxLandingVerticalSpeed=900.f;
 UPROPERTY(BlueprintReadWrite) float ReleaseAcceleration=2200.f;
 UPROPERTY(BlueprintReadWrite) float MaxDepartureSpeed=2200.f;
};

UINTERFACE(BlueprintType)
class NEONCITY_API UDynamicSurfaceInteractionInterface : public UInterface
{ GENERATED_BODY() };

/** Opt-in for vehicles, ships, aircraft or ordinary platforms. Hit normal points OUT of the surface. */
class NEONCITY_API IDynamicSurfaceInteractionInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Physics|Dynamic Surface")
 bool QueryStandingSurface(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const;
};

namespace NeonCityDynamicSurfaces
{
 NEONCITY_API bool IsProvider(const AActor* Actor);
 NEONCITY_API bool Query(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface);
 NEONCITY_API bool IsSafeLanding(const FVector& RelativeVelocity,const FDynamicStandingSurface& Surface);
 /** Converts a source-body hit into a surface hit and distinguishes support from road impacts. */
 NEONCITY_API bool IsSupportContact(AActor* SurfaceActor,ACharacter* Character,const FHitResult& SourceHit);
}
