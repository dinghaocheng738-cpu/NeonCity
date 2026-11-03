#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "AircraftFlightTelemetry.generated.h"

/** Presentation snapshot only; flight forces and weapon authority stay in their systems. */
USTRUCT(BlueprintType)
struct NEONCITY_API FAircraftFlightTelemetry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float SpeedKmh=0;
 UPROPERTY(BlueprintReadOnly) float AltitudeMeters=0;
 UPROPERTY(BlueprintReadOnly) float CurrentPower=0;
 UPROPERTY(BlueprintReadOnly) float TargetPower=0;
 UPROPERTY(BlueprintReadOnly) bool EngineRunning=false;
 UPROPERTY(BlueprintReadOnly) bool FixedWing=false;
 UPROPERTY(BlueprintReadOnly) FText Phase;
};
