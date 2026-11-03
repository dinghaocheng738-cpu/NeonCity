#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VehicleData.generated.h"
class UIslandSupercarPresentationData;

UENUM(BlueprintType)
enum class ENeonCityVehicleType : uint8 { Sedan, SportsCar, SUV, Pickup };

/** SI tuning inputs, converted to Chaos torque/centimetres by VehicleComponent. */
UCLASS(BlueprintType)
class NEONCITY_API UVehicleData : public UDataAsset
{
	GENERATED_BODY()
public:
 /** Optional fitted art/mechanics for additional hero cars; unset profiles retain their existing presentation. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Presentation")
 TSoftObjectPtr<UIslandSupercarPresentationData> HeroPresentation;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle") ENeonCityVehicleType VehicleType=ENeonCityVehicleType::Sedan;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle",meta=(ClampMin="10",ClampMax="1000")) float Durability=100.f;
 /** Not every wreck has to explode. The fuse is opt-in per profile. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion") bool CanExplode=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion") FName ExplosionDataID=TEXT("NeonCity.VehicleExplosion");
 /** Escape window between Disabled/fire and the blast. Never zero-length by accident. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion",meta=(ClampMin="0.5",ClampMax="30")) float ExplosionDelayMin=3.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion",meta=(ClampMin="0.5",ClampMax="30")) float ExplosionDelayMax=7.f;
 /** A blast victim may chain one hop deeper at most; a parking lot never cascades. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion",meta=(ClampMin="0",ClampMax="4")) int32 MaximumChainDepth=1;
 /** Health fraction below which a blast victim is wrecked enough to catch fire itself. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Explosion",meta=(ClampMin="0",ClampMax="1")) float ChainReactionThreshold=.25f;
 bool IsValidProfile() const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle")
	FText DisplayName = NSLOCTEXT("NeonCity", "StarterSedan", "NeonCity Starter Sedan");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="10", ClampMax="320", Units="km/h"))
	float MaxSpeed = 100.f;

	/** Target launch acceleration (m/s^2); determines peak engine torque, not a guaranteed acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="0.5", ClampMax="12"))
	float Acceleration = 3.5f;

	/** Total service-brake force in Newtons, distributed equally across four wheels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="1000", ClampMax="30000"))
	float BrakeForce = 14000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="500", ClampMax="3000", Units="kg"))
	float Weight = 1450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="5", ClampMax="45", Units="deg"))
	float Handling = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(ClampMin="0.2", ClampMax="4"))
	float Grip = 2.f;
};
