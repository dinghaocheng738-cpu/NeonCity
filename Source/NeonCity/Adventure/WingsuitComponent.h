#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WingsuitComponent.generated.h"
class USkeletalMesh;
class UAnimInstance;
class UStaticMeshComponent;
class UUserWidget;
class UTextBlock;
class UMaterialInterface;
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UWingsuitComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UWingsuitComponent();
 bool Equip();
 UFUNCTION() void Unequip();
 /** Reapply gear after ordinary saved-position floor registration; flight and
  * canopy are transient and never restored as an airborne pose. */
 void RestoreEquipped(bool SavedEquipped);
 bool IsEquipped()const{return bEquipped;}
 bool IsFlying()const{return bFlying;}
 bool IsCanopyOpen()const{return bCanopy;}
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)override;
protected:
 virtual void BeginPlay()override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 bool bEquipped=false,bFlying=false,bCanopy=false,bOldOrient=true;
 bool bRestorePending=false;
 float RestoreWait=0;
 float AirTime=0,JumpLaunchWindow=0;
 FTransform OriginalTransform;
 float OriginalArmLength=0,OriginalFOV=90;
 FVector OriginalSocketOffset=FVector::ZeroVector;
 UPROPERTY() TObjectPtr<USkeletalMesh> OriginalSkin;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
 UPROPERTY() TSubclassOf<UAnimInstance> OriginalAnimation;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Canopy;
 UPROPERTY() TObjectPtr<UUserWidget> HUD;
 UPROPERTY() TObjectPtr<UTextBlock> Status;
};
