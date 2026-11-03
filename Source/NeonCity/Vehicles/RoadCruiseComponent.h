#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadCruiseComponent.generated.h"
class ANeonCityBaseVehicle;

/** Authored-lane assistance. Produces only throttle/brake/steering inputs for Chaos. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API URoadCruiseComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URoadCruiseComponent();
 bool IsAvailable() const;
 bool IsActive() const { return bActive; }
 float GetRequestedSpeedKmh() const { return RequestedSpeedKmh; }
 float GetSafeSpeedKmh() const { return SafeSpeedKmh; }
 float GetAppliedSteering() const { return AppliedSteering; }
 float GetLaneErrorCm() const { return LaneErrorCm; }
 const FString& GetReason() const { return Reason; }
 const TArray<FVector>& GetRoute() const { return Route; }
 void Toggle();
 void Cancel(const FString& Cause);
 bool HandlePlayerDrive(float Axis);
 bool HandlePlayerBrake(float Axis);
 void PlayerSteeringTakeover();
 FText GetStatusText() const;
 /** Preferred open-road speed; authored approaches and bends still cap it. */
 UPROPERTY(EditAnywhere,Category="Road|Cruise",meta=(ClampMin="5",ClampMax="100")) float PreferredSpeedKmh=65;
protected:
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 ANeonCityBaseVehicle* Vehicle() const;
 void Notify(const FString& Text);
 FVector PointAt(float Distance) const;
 bool UpdateProgress(const FVector& Position);
 bool MeasureAxles();
 float PreviewSafeSpeed(float SpeedCm) const;
 float FindHazardDistance(float HorizonCm);
 void Hold(const FString& Cause,float Delta);
 TArray<FVector> Route;
 TArray<float> Distances;
 TArray<float> PointSpeedLimits;
 int32 Segment=0;
 float ProgressCm=0,RoadSpeedLimitKmh=45,RequestedSpeedKmh=0,SafeSpeedKmh=0;
 float PlayerDrive=0,PlayerBrake=0,AppliedSteering=0,SpeedIntegral=0,LaneErrorCm=0;
 float HazardCountdown=0,CachedHazardDistance=TNumericLimits<float>::Max();
 float LastHazardLog=-10;
 float LastControlLog=-10;
 FVector RearAxleLocal=FVector::ZeroVector;
 float WheelbaseCm=280;
 float SafetyHalfWidthCm=110,SafetyHalfLengthCm=220;
 bool bActive=false,bLooped=false,bLaneFault=false;
 FString Reason=TEXT("Off");
 uint64 NavigationRevision=0;
};
