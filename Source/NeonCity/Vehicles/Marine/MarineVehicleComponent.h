#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Water/TidewaterWaterSample.h"
#include "MarineVehicleComponent.generated.h"
class UPrimitiveComponent;
class UMarineVehicleData;
#if WITH_DEV_AUTOMATION_TESTS
struct FReferenceBoatStepDiagnostics
{
 uint64 Steps=0;
 double LastDelta=0,MaximumDelta=0;
 FVector Acceleration=FVector::ZeroVector,AngularAcceleration=FVector::ZeroVector;
 double MinimumWater=0,MaximumWater=0,MaximumSampleAge=0;
 double MaximumIssuedAge=0,MaximumAngularAcceleration=0;
 int32 ValidWaterSamples=0;
 double MaximumLeaseAge=0,MaximumLeaseFootprint=0;
 int32 UnavailableLeases=0,ExpiredLeases=0,OutsideLeases=0;
};
#endif
/** Forces on the simulated hull; never sets its transform or bypasses Chaos contacts. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UMarineVehicleComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UMarineVehicleComponent();
 void Configure(UPrimitiveComponent* Hull,const UMarineVehicleData* Data);
 bool StartEngine();
 void StopEngine();
 void SetInput(float Throttle,float Steering,float Brake);
 void SetMooringBerth(const FTransform& Berth);
 bool IsMoored() const { return bMoored; }
 // Physical berth eligibility also applies while a driver idles the engine.
 bool IsAtMooringBerth() const;
 bool IsEngineRunning() const { return bEngineRunning; }
 bool IsAfloat() const { return WetPoints>0; }
 float GetSteering() const { return Steer; }
 float GetWakeEngineRPM() const { return bEngineRunning?FMath::Max(.15f,FMath::Max(ReferenceRPM,FMath::Abs(CurrentDrive))):0; }
 FVector GetWaterVelocity() const { return WaterVelocity; }
 UFUNCTION(BlueprintPure,Category="Marine") FMarineDriveTelemetry GetTelemetry() const;
#if WITH_DEV_AUTOMATION_TESTS
 // Read after frozen-game-thread solver callbacks; diagnostic data never drives physics.
 const FReferenceBoatStepDiagnostics& GetReferenceStepDiagnostics() const { return StepDiagnostics; }
#endif
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void AsyncPhysicsTickComponent(float DeltaTime,float SimTime) override;
private:
 void ApplyReferenceHydrodynamics(float Delta);
 // The engine invokes this component's physics tick while the game thread is
 // frozen. Only the captured water planes and the solver's live body are used.
 NeonCityReferenceBoat::FHydroState ReferenceInput;
 TArray<FVector> ReferenceWaterPoints,ReferenceWaterNormals;
 TArray<FTidewaterWaterSample> ReferenceWaterStates;
 TArray<double> ReferenceInitialEffective;
 bool bReferenceInputReady=false;
#if WITH_DEV_AUTOMATION_TESTS
 bool bRecordStepDiagnostics=false;
 FReferenceBoatStepDiagnostics StepDiagnostics;
#endif
 float ReferenceRPM=0,ReferenceSteer=0;
 UPROPERTY() TObjectPtr<UPrimitiveComponent> Body;
 UPROPERTY() TObjectPtr<const UMarineVehicleData> Profile;
 FVector WaterVelocity=FVector::ZeroVector;
 float Drive=0,CurrentDrive=0,Steer=0,BrakeInput=0;
 float LastForwardSpeed=0,ForwardAcceleration=0;
 bool bHadWaterSample=false;
 int32 WetPoints=0;
 bool bEngineRunning=false;
 FTransform MooringBerth;
 bool bHasMooringBerth=false,bMoored=false;
};
