#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeonCityFlashlightComponent.generated.h"
class UInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UStaticMesh;
class UStaticMeshComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityFlashlightSwitch,bool,bOn);

/** On-foot utility item. It owns no movement, combat or camera implementation. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityFlashlightComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityFlashlightComponent();
 void SetupInput(UInputComponent* Input);
 void Suspend();
 UFUNCTION(BlueprintCallable, Category="NeonCity|Flashlight") bool SetEquipped(bool bEquip);
 UFUNCTION(BlueprintCallable, Category="NeonCity|Flashlight") void Toggle();
 UFUNCTION(BlueprintPure, Category="NeonCity|Flashlight") bool IsEquipped() const { return bEquipped; }
 bool IsDiveMounted() const { return bDiveMounted; }
 UFUNCTION(BlueprintPure, Category="NeonCity|Flashlight") float GetBatteryCharge() const { return BatteryCharge; }
 /** Battery integration boundary: no drain, pickups or battery economy in this pass. */
 UFUNCTION(BlueprintCallable, Category="NeonCity|Flashlight") bool SetBatteryCharge(float Charge);
 /** Presentation/audio hook, emitted once for each actual switch. */
 UPROPERTY(BlueprintAssignable, Category="NeonCity|Flashlight") FNeonCityFlashlightSwitch OnLightSwitched;
 UStaticMeshComponent* GetVisual() const { return Visual; }
 USpotLightComponent* GetBeam() const { return Beam; }
protected:
 virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 bool CanUse() const;
 bool CreateVisuals();
 void UpdateMount();
 void RemoveInput();
 UPROPERTY(EditDefaultsOnly, Category="NeonCity|Flashlight") TSoftObjectPtr<UStaticMesh> MeshAsset;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(Transient) TObjectPtr<USpotLightComponent> Beam;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Lens;
 UPROPERTY(Transient) TObjectPtr<UInputAction> Action;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 uint32 BindingHandle=0;
 bool bBound=false;
 bool bEquipped=false;
 bool bDiveMounted=false;
 float BatteryCharge=1.f;
};
