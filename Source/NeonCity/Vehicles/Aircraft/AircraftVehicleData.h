#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AircraftVehicleData.generated.h"
UENUM(BlueprintType)
enum class EAircraftFlightState : uint8 { Grounded, TakingOff, Hovering, Flying, Landing, Disabled };
UENUM(BlueprintType)
enum class EAircraftCondition : uint8 { Healthy, Damaged, Critical, Disabled };
UENUM(BlueprintType)
enum class EAircraftWaterState : uint8 { Dry, Contact, Submerged };
/** SI units unless specified; force in newtons, speed in km/h, geometry in cm. */
UCLASS(BlueprintType)
class NEONCITY_API UAircraftVehicleData : public UPrimaryDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FName AircraftType=TEXT("NeonCity.HelicopterPrototype");
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Mass=1200;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float MaxSpeed=240;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Acceleration=12;
 // Rotor power is normalized. Hover assist is the safe lower powered setting;
 // Ctrl reduces power and applies horizontal deceleration, not engine shutdown.
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float HoverPower=.55f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float PowerResponse=2.f; // first-order response, 1/second
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float PowerAdjustmentRate=.3f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float FastFlightAccelerationScale=1.6f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Deceleration=3.5f; // m/s^2
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float LiftForce=28000;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float VerticalSpeed=5;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float RotationSpeed=45;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float AirResistance=.25f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float HoverBrake=1.8f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float HeightResponse=1.5f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float VerticalResponse=3;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float AttitudeResponse=10;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float AngularResistance=5;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float MaximumBank=10;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float LandingSpeed=1.2f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Durability=600;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CriticalFraction=.25f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float DisabledFraction=.1f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float SeriousImpactSpeed=2200; // cm/s; critical damage threshold
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float SafeGroundImpactSpeed=250; // cm/s
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float FuelCapacity=150; // interface only, no consumption gameplay
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector BodyExtent=FVector(200,90,75);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector CenterOfMass=FVector(0,0,-25);
 virtual bool IsValidProfile() const;
 /** Shared airframe constraints; fixed-wing profiles must not depend on rotor lift/hover fields. */
 bool IsValidAirframe() const;
 virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("AircraftVehicle"),AircraftType); }
};
