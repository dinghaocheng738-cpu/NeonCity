#pragma once
#include "CoreMinimal.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionEvent.h"
#include "ExplosionFutureInterfaces.generated.h"
/** Reserved interfaces for later water, weather and city-event work. No gameplay logic in this phase. */
UENUM(BlueprintType)
enum class ENeonCityExplosionMedium : uint8 { Air, WaterSurface, Underwater };
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityWaterExplosionRequest
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FVector SurfacePoint=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FName WaterVolumeID;
 UPROPERTY(BlueprintReadOnly) float Depth=0;
 UPROPERTY(BlueprintReadOnly) float WaveRadius=0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> SourceActor;
};
/** Weather scales fire, smoke and impulse through one authored modifier instead of hard-coded clear-weather math. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityWeatherModifier
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float FireScale=1;
 UPROPERTY(BlueprintReadOnly) float SmokeScale=1;
 UPROPERTY(BlueprintReadOnly) float ImpulseScale=1;
 UPROPERTY(BlueprintReadOnly) float DamageScale=1;
 UPROPERTY(BlueprintReadOnly) FName WeatherID;
 bool IsValid() const
 { return FMath::IsFinite(FireScale) && FMath::IsFinite(SmokeScale) && FMath::IsFinite(ImpulseScale) && FMath::IsFinite(DamageScale)
  && FireScale>=0 && FireScale<=2 && SmokeScale>=0 && SmokeScale<=2 && ImpulseScale>=0 && ImpulseScale<=2 && DamageScale>=0 && DamageScale<=2; }
};