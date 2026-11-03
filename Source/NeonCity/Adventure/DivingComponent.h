#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "DivingComponent.generated.h"
class ACoastalWildlife;
class UStaticMeshComponent;
class UTextBlock;
class UUserWidget;
class UCameraComponent;
class USkeletalMesh;
class UMaterialInterface;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class APlayerController;
enum class EDiveCaptureStage:uint8 { Searching,Tracking,Sweeping,Securing,Recovering };

UCLASS()
class NEONCITY_API UPolishDivingWidget : public UUserWidget
{
 GENERATED_BODY()
};

/** Accepted drysuit and camera state; equipment owns the physical spear and DPV loop. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UDivingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDivingComponent();
 bool Equip();
 bool BeginGearChange();
 bool CanChangeGear() const;
 bool IsChangingGear() const { return bChanging; }
 float GetGearChangeTime() const;
 bool OwnsSuit() const { return bOwnsSuit; }
 bool OwnsDPV() const { return bOwnsDPV; }
 void RestoreGear(bool SuitOwned,bool DPVOwned,bool Worn);
 UFUNCTION() void Unequip();
 bool IsEquipped() const { return bEquipped; }
 bool IsFirstPerson() const { return bFirstPerson; }
 bool HasBallast() const { return bEquipped && bBallast; }
 void SetFirstPerson(bool Enabled);
 float GetCaptureProgress() const { return FMath::Clamp(CaptureSeconds/1.65f,0.f,1.f); }
 bool CanStartSweep() const { return CaptureStage==EDiveCaptureStage::Tracking && TrackingSeconds>=.45f; }
 EDiveCaptureStage GetCaptureStage() const { return CaptureStage; }
 void Operate(bool Aim,bool Collect,float Delta);
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 bool ControlsAvailable() const;
 void InstallEquipmentInput();
 void RemoveEquipmentInput();
 void TogglePropulsor();
 void ToggleSpeargun();
 void StartEquipmentTrigger();
 void StopEquipmentTrigger();
 void FinishGearChange();
 bool bOwnsSuit=false,bOwnsDPV=false,bChanging=false,bRemoving=false,bChanged=false,bEquipmentTrigger=false;
 float ChangeStarted=0;
 bool bRestoreWorn=false;
 float RestoreUntil=0;
 TWeakObjectPtr<APlayerController> LockedController;
 TWeakObjectPtr<UEnhancedInputComponent> EquipmentInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> EquipmentSubsystem;
 TArray<uint32> EquipmentBindings;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> EquipmentMapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> DPVAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> SpeargunAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> EquipmentTriggerAction;
 bool bEquipped=false;
 bool bFirstPerson=false;
 bool bBallast=true;
 float CaptureSeconds=0,OldDrain=5;
 float TrackingSeconds=0,LostSeconds=0,RecoverySeconds=0;
 bool bTriggerHeld=false;
 EDiveCaptureStage CaptureStage=EDiveCaptureStage::Searching;
 FTransform OldCamera;
 bool OldUseControl=false,OldOwnerNoSee=false;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Net;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Tank;
 UPROPERTY() TObjectPtr<USkeletalMesh> OriginalSkin;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
 UPROPERTY() TObjectPtr<UTextBlock> Crosshair;
 UPROPERTY() TObjectPtr<UUserWidget> HUD;
 UPROPERTY() TObjectPtr<UTextBlock> Status;
 TWeakObjectPtr<AActor> Focus;
 int32 FocusInstance=INDEX_NONE;
 FString Notice;
};
