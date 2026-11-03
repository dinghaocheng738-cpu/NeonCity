#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Presentation/RowerPoseComponent.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "NeonCityRowboat.generated.h"
class UStaticMeshComponent;
class URowerPoseComponent;
class ACharacter;

UENUM()
enum class ERowboatService : uint8 { SelfRow, Hired };

/** West Lake hand-row boat (7.2 m, canopy, two thole-pin oarlocks at the stern). */
UCLASS()
class NEONCITY_API URowboatData : public UMarineVehicleData
{
 GENERATED_BODY()
public:
 URowboatData();
};

/** Hand-rowed lake boat. The boat moves only by oar blades working the water: each blade's
 * hydrodynamic force (from its real velocity through the water) acts on the simulated hull,
 * so strokes surge the boat, it glides between strokes and differential strokes turn it.
 * The rower sits on the stern seat facing the bow and pushes two oars (West Lake style);
 * the person is posed procedurally from the same oar kinematics, so hands never leave the grips.
 * SelfRow: the player rows (W row, S back water, A/D turn, Space hold water).
 * Hired: a boatman rows a scenic loop and returns to the pier; the player rides on the bench. */
UCLASS()
class NEONCITY_API ANeonCityRowboat : public ANeonCityMarineVehicleBase
{
 GENERATED_BODY()
public:
 ANeonCityRowboat();
 UPROPERTY(EditAnywhere,Category="Rowboat") ERowboatService Service=ERowboatService::SelfRow;
 /** Hired boats: world waypoints of the tour; the last one is the approach to the berth. */
 UPROPERTY(EditAnywhere,Category="Rowboat") TArray<FVector> TourRoute;
 /** Hired boats: boatman body (mannequin skeleton). */
 UPROPERTY(EditAnywhere,Category="Rowboat") TSoftObjectPtr<USkeletalMesh> BoatmanMesh;

 enum class EPhase : uint8 { Idle, Boarding, Seated, StandingUp, Stepping };
 enum class EOarMode : uint8 { Rest, Forward, Back, Hold, Shipped };
 struct FOar
 {
  EOarMode Mode=EOarMode::Rest;
  float Sweep=8,Height=6,Feather=1,Amplitude=1;   // deg, cm above water at blade centre, 0 square .. 1 flat
  FVector Blade=FVector::ZeroVector,PrevBlade=FVector::ZeroVector; bool bPrev=false;
  float Immersion=0; FVector LastForce=FVector::ZeroVector;
  bool bBlocked=false;   // a pier / bank lies in the oar's sweep: keep it shipped
 };
 EPhase GetPhase() const { return Phase; }
 const FOar& GetOar(int32 Side) const { return Oars[Side]; }
 float GetStrokeRate() const { return Rate; }
 float GetStrokePhase() const { return Stroke; }
 bool IsTourActive() const { return bTourActive; }
 bool IsDocked() const;
 /** Seated, slow enough, and a bank / pier / stair lies within a step of the boat (F steps ashore). */
 bool HasLandingInReach() const;
 int32 GetTourWaypoint() const { return Waypoint; }
 URowerPoseComponent* GetRower() const { return Rower; }
 URowerPoseComponent* GetPassenger() const { return Passenger; }
 /** Boat-local oar pivot and grip for diagnostics. */
 FVector GetOarlock(int32 Side) const;
 FVector GetGrip(int32 Side) const;

 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Interactor) override;
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue) override;
 virtual FText BuildStatusText(const FMarineDriveTelemetry& T,const FText& State) const override;
 virtual bool RequestDriverExit() override;
 virtual bool FindShoreLanding(const ACharacter* Character,FTransform& Out) const override;
 virtual void PossessedBy(AController* NewController) override;
 virtual void OnHelmCameraChanged(bool bHelm) override;
private:
 void UpdateAutopilot(float Delta);
 void UpdateOars(float Delta);
 void UpdateClearance();
 void UpdatePushOff(float Delta);
 void ApplyBladeForces(float Delta);
 void UpdatePeople();
 void UpdateBoarding(float Delta);
 FRowerTargets RowingTargets() const;
 FRowerTargets PassengerTargets() const;
 FVector OarDirection(int32 Side) const;
 FVector StandSpot(bool bRowerSeat) const;
 void Feedback(const FText& Text);
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HullMesh;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> OarMesh0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> OarMesh1;
 UPROPERTY(VisibleAnywhere) TObjectPtr<URowerPoseComponent> Rower;
 UPROPERTY(VisibleAnywhere) TObjectPtr<URowerPoseComponent> Passenger;
 TWeakObjectPtr<ACharacter> Person;
 EPhase Phase=EPhase::Idle;
 float PhaseTime=0;
 FVector StepFrom=FVector::ZeroVector,StepTo=FVector::ZeroVector;
 FOar Oars[2];
 float Stroke=0,Rate=0,Lean=2;
 float InForward=0,InSteer=0,InBrake=0;
 bool bTourActive=false,bReturning=false,bDocking=false;
 int32 Waypoint=0; float WaypointTime=0,BestDistance=0,ProgressTime=0;
 FTransform Berth;
 FText Message; float MessageTime=0;
 float ClearanceTime=0,PushTime=0,PushSide=0,StuckTime=0,BackOffTime=0;
 bool bUnberthing=false;
 float LandingProbeTime=0; bool bLandingInReach=false;
};
