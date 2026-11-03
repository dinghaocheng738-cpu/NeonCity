#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingAircraftData.h"
#include "FixedWingFlightComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFixedWingStateChanged,EFixedWingFlightState,Previous,EFixedWingFlightState,Current);
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UFixedWingFlightComponent : public UAircraftVehicleComponent
{
 GENERATED_BODY()
public:
 virtual void Configure(UPrimitiveComponent* InBody,const UAircraftVehicleData* Data) override;
 virtual void StopEngine() override;
 virtual void ResetAfterLoad() override;
 virtual void SetInput(FVector Translation,float Yaw) override;
 virtual bool SetPowerInput(float Adjustment) override { return false; } // separate throttle model
 virtual FAircraftFlightTelemetry GetTelemetry() const override;
 bool SetControls(float Throttle,float Pitch,float Roll,float Yaw,bool Brake);
 UFUNCTION(BlueprintPure) float GetThrottle() const { return Throttle; }
 UFUNCTION(BlueprintPure) float GetAirspeed() const { return Airspeed; } // m/s
 UFUNCTION(BlueprintPure) float GetLift() const { return Lift; } // newtons
 UFUNCTION(BlueprintPure) EFixedWingFlightState GetFixedWingState() const { return State; }
 UPROPERTY(BlueprintAssignable) FFixedWingStateChanged OnFixedWingStateChanged;
 static float EvaluateLift(const UFixedWingAircraftData& Data,float Speed,float AngleOfAttack,bool Stalled);
protected:
 virtual bool CanPower() const override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 void ChangeState(EFixedWingFlightState Next);
 float Throttle=0,Pitch=0,Roll=0,Rudder=0,Airspeed=0,Lift=0;
 bool bBrake=false,bStalled=false,bPreviouslyAirborne=false;
 EFixedWingFlightState State=EFixedWingFlightState::Grounded;
};
