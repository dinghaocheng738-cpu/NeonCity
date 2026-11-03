#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftFlightTelemetry.h"
#include "AircraftVehicleComponent.generated.h"
class UPrimitiveComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAircraftFlightChanged,EAircraftFlightState,Previous,EAircraftFlightState,Current);
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UAircraftVehicleComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UAircraftVehicleComponent();
 virtual void Configure(UPrimitiveComponent* InBody,const UAircraftVehicleData* Data);
 bool StartEngine();
 virtual void StopEngine();
 virtual void SetInput(FVector Translation,float Yaw);
 virtual bool SetPowerInput(float Adjustment);
 UFUNCTION(BlueprintPure,Category="Aircraft") virtual FAircraftFlightTelemetry GetTelemetry() const;
 virtual void ResetAfterLoad();
 bool IsEngineRunning() const { return bEngineRunning; }
 float GetCurrentPower() const { return CurrentPower; }
 bool IsGrounded() const { return bGrounded; }
 /** Perched on a post: the way it is hopping off (zero when not), and how long the hop has left. */
 FVector GetSlipDir() const { return SlipDir; } float GetSlipTime() const { return SlipTime; }
 float GetAltitude() const { return Altitude; }
 EAircraftFlightState GetFlightState() const { return FlightState; }
 EAircraftCondition GetCondition() const;
 UFUNCTION(BlueprintPure) float GetFuelFraction() const { return FuelFraction; }
 UFUNCTION(BlueprintCallable) bool SetFuelFraction(float Value);
 UPROPERTY(BlueprintAssignable) FAircraftFlightChanged OnFlightStateChanged;
protected:
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
protected:
 void SampleGround();
 void SetState(EAircraftFlightState Value);
 virtual bool CanPower() const;
 UPROPERTY() TObjectPtr<UPrimitiveComponent> Body;
 UPROPERTY() TObjectPtr<const UAircraftVehicleData> Profile;
 FVector Input=FVector::ZeroVector;
 float YawInput=0,HoverHeight=0,Altitude=0,FuelFraction=1;
 float CurrentPower=0,TargetPower=0,PowerInput=0,HorizontalAcceleration=0,PreviousSpeed=0;
 bool bEngineRunning=false,bGrounded=false,bWasClimbing=false;
 FVector SlipDir=FVector::ZeroVector,SlipAway=FVector::ZeroVector;float SlipTime=0;  // perched on a post or a lamp: the way it slides off to the ground beside it
 float RestTime=0;                 // s settled (still, upright, not climbing): resting on something counts as landed
 EAircraftFlightState FlightState=EAircraftFlightState::Grounded;
};
