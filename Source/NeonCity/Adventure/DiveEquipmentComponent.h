#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimInstance.h"
#include "Fishing/FishData.h"
#include "DiveEquipmentComponent.generated.h"
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USplineMeshComponent;
class UAudioComponent;
class UCharacterMovementComponent;
class AMarineSchool;
class ACoastalWildlife;
class UNiagaraSystem;
class UNiagaraComponent;

enum class ESpearPhase : uint8 { Ready, Flying, Tethered, Retrieving, Reloading };

/** Manufactured equipment, physical shaft and retained visible capture. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UDiveEquipmentComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDiveEquipmentComponent();
 bool Equip();
 void Stow();
 void Operate(bool Aim,bool Trigger,float Delta,bool ControlsAvailable=true);
 bool ToggleDPV();
 bool ToggleSpeargun();
 void StowDPV();
 bool IsDPVDeployed() const { return bEquipped && bDPV; }
 bool IsSpeargunDeployed() const { return bEquipped && bSpearDeployed; }
 bool IsAiming() const { return bAim && bSpearDeployed; }
 float GetMotorSpool() const { return MotorSpool; }
 void ApplyPropulsion(UCharacterMovementComponent* Movement,float Delta);
 ESpearPhase GetSpearPhase() const { return Phase; }
 float GetActionAge() const;
 int32 GetHandPose() const;
 FString GetStatus() const { return Notice; }
 int32 GetFiredCount() const { return Fired; }
 int32 GetHitCount() const { return Hits; }
 int32 GetRetrievedCount() const { return Retrieved; }
 FVector GetShaftTip() const { return Tip; }
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 void SetPhase(ESpearPhase Value);
 void StepShaft(float Delta,bool Reel);
 void UpdatePresentation(float Delta);
 void ReleaseTarget();
 bool ReserveTarget(const FHitResult& Hit);
 bool MoveTarget(FVector Position,bool Landed=false);
 bool ReadTargetAttachment(bool& Collectible);
 void StartShot();
 UPROPERTY(Transient) TObjectPtr<USceneComponent> EquipmentRoot;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Speargun;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Shaft;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Bands;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Reel;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> DPV;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Rotor;
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Hands;
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> MotorAudio;
 UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> WashSystem;
 UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Wash;
 UPROPERTY(Transient) TArray<TObjectPtr<USplineMeshComponent>> Line;
 TWeakObjectPtr<AMarineSchool> School;
 TWeakObjectPtr<ACoastalWildlife> Individual;
 FNeonCityCaughtFish PendingCatch;
 FGuid Reservation;
 int32 Instance=INDEX_NONE;
 ESpearPhase Phase=ESpearPhase::Ready;
 FVector Tip=FVector::ZeroVector,ShaftVelocity=FVector::ZeroVector,ShotDirection=FVector::ForwardVector,TargetOffset=FVector::ZeroVector;
 bool bEquipped=false,bDPV=false,bSpearDeployed=false,bAim=false,bTriggerHeld=false,bMotorHeld=false;
 float PhaseAt=0,MotorSpool=0,RotorAngle=0,ReelAngle=0,Traveled=0,AimBlend=0,DeployBlend=0,WashClock=0;
 int32 Fired=0,Hits=0,Retrieved=0;
 FString Notice;
 float OriginalFOV=90,SpeedFOV=0;
};

UCLASS(Transient,Blueprintable)
class NEONCITY_API UDiveEquipmentAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly,Category="Dive") int32 HandPose=0;
 UPROPERTY(BlueprintReadOnly,Category="Dive") float ActionTime=0;
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
