#pragma once
#include "CoreMinimal.h"
#include "MarineDriveTelemetry.generated.h"

UENUM(BlueprintType)
enum class EMarineDriveState : uint8
{
 EngineOff, Idle, Accelerating, Cruise, Coasting, Braking, Reverse
};

/** Presentation reads actual propulsion/water motion; it never commands physics. */
USTRUCT(BlueprintType)
struct NEONCITY_API FMarineDriveTelemetry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float SpeedKmh=0;
 UPROPERTY(BlueprintReadOnly) float ForwardSpeedKmh=0;
 UPROPERTY(BlueprintReadOnly) float Throttle=0; // Magnitude, 0..1.
 UPROPERTY(BlueprintReadOnly) float TargetThrottle=0;
 UPROPERTY(BlueprintReadOnly) float SignedThrust=0; // Direction is separate from throttle magnitude.
 UPROPERTY(BlueprintReadOnly) float Steering=0;
 UPROPERTY(BlueprintReadOnly) bool EngineRunning=false;
 UPROPERTY(BlueprintReadOnly) bool Afloat=false;
 UPROPERTY(BlueprintReadOnly) EMarineDriveState State=EMarineDriveState::EngineOff;
};
