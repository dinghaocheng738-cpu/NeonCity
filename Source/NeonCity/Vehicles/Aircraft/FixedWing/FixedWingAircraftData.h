#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "FixedWingAircraftData.generated.h"
UENUM(BlueprintType)
enum class EFixedWingFlightState : uint8 { Grounded,Taxiing,TakeoffRoll,Airborne,Cruising,Stalling,LandingApproach,Landing,Disabled };
/** SI aerodynamic parameters; inherited MaxSpeed and the three design speeds are km/h. */
UCLASS(BlueprintType)
class NEONCITY_API UFixedWingAircraftData : public UAircraftVehicleData
{
 GENERATED_BODY()
public:
 UFixedWingAircraftData();
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float EngineThrust=6500;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CruiseSpeed=126;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float TakeoffSpeed=79.2f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float StallSpeed=61.2f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float WingArea=16;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float LiftCoefficient=.75f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float LiftSlope=4.5f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float MaximumLiftCoefficient=1.8f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float DragCoefficient=.055f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float InducedDrag=.065f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float AirDensity=1.225f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float PitchAuthority=28; // degrees / second
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float RollAuthority=65;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float YawAuthority=12;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ControlResponse=4;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ThrottleRate=.35f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float GroundBrake=7; // m/s^2
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float GroundSteering=22;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float StallNoseDown=9;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float StallControlFraction=.3f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float SideSlipDamping=1.5f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float OverspeedDrag=3;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CameraDistance=1500;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CameraSpeedExtension=450;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CameraFOV=85;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CameraSpeedFOV=8;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float CameraBankFraction=.2f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool FixedLandingGear=true;
 virtual bool IsValidProfile() const override;
};
