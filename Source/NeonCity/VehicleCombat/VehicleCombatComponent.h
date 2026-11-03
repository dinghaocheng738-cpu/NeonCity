#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleCombatComponent.generated.h"
class UVehicleWeaponManager;
class UTargetingComponent;
class ULockOnSystem;
class AVehicleLaserMark;
struct FVehicleTargetInfo;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class APlayerController;
struct FInputActionValue;
UENUM()
enum class EVehicleCombatMountPreset : uint8 { Roof, Boat, Helicopter, FixedWing };
/** One input/aim adapter for any possessed platform; no flight/drive logic. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UVehicleCombatComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVehicleCombatComponent();
 static UVehicleCombatComponent* Install(APawn* Host,EVehicleCombatMountPreset Preset);
 UFUNCTION(BlueprintCallable) bool FireWeapon();
 UFUNCTION(BlueprintCallable) bool ReloadWeapon();
 UFUNCTION(BlueprintCallable) void AimWeapon(FVector Direction);
 UFUNCTION(BlueprintCallable) void LockTarget(bool Enabled);
 bool IsLockRequested() const { return bLockRequested; }
 UFUNCTION(BlueprintCallable) void SetArmed(bool Enabled) { bArmed=Enabled; }
 UFUNCTION(BlueprintPure) bool IsArmed() const { return bArmed; }
 void UpdateLock(float Delta);
 void ResetTransientState();
 UVehicleWeaponManager* GetManager() const { return Manager; }
 UTargetingComponent* GetTargeting() const { return Targeting; }
 ULockOnSystem* GetLockSystem() const { return Lock; }
 /** True while the seeker holds the laser spot (ground, building, water) rather than a vehicle or person. */
 bool IsDesignatingPoint() const;
 FVector GetAimDirection() const { return AimDirection; }
 FString GetDebugStatus() const;
 void SetStatusPresented(bool Presented) { bStatusPresented=Presented; }
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 void BindInput(APlayerController* PC);
 void RemoveInput();
 void PressFire(); void ReleaseFire(); void PressLock(); void ReleaseLock(); void ReloadInput(); void CycleInput(); void ToggleInput();
 void LookYaw(const FInputActionValue& Value); void LookPitch(const FInputActionValue& Value);
 bool CanOperate() const;
 bool ArmKeyIsB() const;
 /** Direction from the selected mount's muzzle to the point under the crosshair (view ray). */
 FVector ConvergeAim(const FVector& ViewOrigin,const FVector& ViewDirection) const;
 UPROPERTY() TObjectPtr<UVehicleWeaponManager> Manager;
 UPROPERTY() TObjectPtr<UTargetingComponent> Targeting;
 UPROPERTY() TObjectPtr<ULockOnSystem> Lock;
 UPROPERTY(Transient) TObjectPtr<AVehicleLaserMark> LaserMark;
 FVehicleTargetInfo Designate(const FVector& Origin,const FVector& Direction);
 bool IsTargetVisible(AActor* Target,const FVector& Origin,const FVector& Direction);
 UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY() TObjectPtr<UEnhancedInputComponent> Input;
 UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
 TWeakObjectPtr<APlayerController> InputController;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 FVector AimDirection=FVector::ForwardVector;
 float ScanElapsed=0,DebugElapsed=0;
 bool bArmed=false,bTrigger=false,bLockRequested=false,bAddMouseLook=false;
 bool bStatusPresented=false;
};
