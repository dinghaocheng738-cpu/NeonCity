#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;
class UVehicleData;

UENUM(BlueprintType)
enum class ENeonCityEngineState : uint8 { Off, Running, Stalled };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVehicleEngineChanged,ENeonCityEngineState,State);

/** Driving policy over Chaos inputs. Never moves/rotates the pawn directly. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UVehicleComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVehicleComponent();
	void Configure(UChaosWheeledVehicleMovementComponent* Movement, const UVehicleData* Data, TConstArrayView<float> FittedWheelRadii = {});
	void SetDriveInput(float Value);
	void SetSteerInput(float Value);
	void SetBrakeInput(float Value);
	void SetDrivingEnabled(bool bEnabled);
 /** Opt-in to native Chaos parking without a held service
  * brake input that would continuously wake Chaos' stopped suspension. */
 void SetNativeParkingBrake(bool Enabled);
	// Enforced where Chaos consumes input, independent of scene attachment ticks.
	void SetMechanicalMotionInterlock(bool bLocked);
	bool IsMechanicalMotionInterlocked() const { return bMechanicalMotionInterlock; }
 /** 0 idle .. 1 stability control working hard (HUD cue). */
 float GetStabilityLevel() const { return StabilityLevel; }
	float GetSpeedKmh() const;
 UFUNCTION(BlueprintPure,Category="Vehicle|Engine") ENeonCityEngineState GetEngineState() const { return EngineState; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Engine") float GetEngineRPM() const;
 UPROPERTY(BlueprintAssignable) FVehicleEngineChanged OnEngineStateChanged;
	float GetMaxSpeedCmPerSecond() const { return SpeedLimitCmPerSecond; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
 void RefreshEngineState();
 bool CanUseNativeParking() const;
 void ApplyNativeParkingPolicy(bool Parked);
 void RestoreNativeAutoBrake();
 ENeonCityEngineState EngineState=ENeonCityEngineState::Off;
	TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosMovement;
	float DriveInput = 0.f;
	float SteerInput = 0.f;
	float BrakeInput = 0.f;
 float AppliedSteerInput=0.f;
 bool bComfortSteering=false;
 /** Driving assists over Chaos (never move the pawn): steering limited to what the tyres can hold at speed,
  * keyboard steering slew, and stability control (a yaw moment and throttle cut against oversteer / slide). */
 void StabilityAssist(UChaosWheeledVehicleMovementComponent& Movement,float SpeedCm,float DeltaTime,float& Throttle);
 float SteeringLimit(float SpeedCm) const;
 float WheelbaseCm=0.f,MaxSteerDeg=35.f,GripG=.9f;
 float StabilityLevel=0.f;
	float SpeedLimitCmPerSecond = 2777.78f;
	bool bDrivingEnabled = false;
 bool bNativeParkingBrake=false;
 TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> AutoBrakeMovement;
 float OriginalAutoBrakeStopThreshold=0;
 bool bAutoBrakeSuppressed=false;
	bool bMechanicalMotionInterlock = false;
 bool bDeepWaterDisabled=false;
};
