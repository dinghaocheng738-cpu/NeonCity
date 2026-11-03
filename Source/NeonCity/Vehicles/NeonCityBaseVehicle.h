#pragma once
#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Interaction/InteractableInterface.h"
#include "Vehicles/VehicleInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Explosion/ExplosionResponseInterface.h"
#include "NeonCityBaseVehicle.generated.h"
class UHealthComponent;
class UVehicleDamageComponent;
class UVehicleStatusWidget;
class UCombatComponent;
class URoadCruiseComponent;

class UVehicleComponent;
class UVehicleInteraction;
class UVehicleData;
class UChaosWheeledVehicleMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UTextRenderComponent;
class UNeonCityCameraExtensionComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
struct FInputActionValue;

/** Single-player Chaos vehicle. Driver lifecycle, driving policy and tuning are separate components. */
UCLASS()
class NEONCITY_API ANeonCityBaseVehicle : public AWheeledVehiclePawn, public IInteractableInterface, public IVehicleInterface, public IDamageInterface, public ICombatInterface, public IDynamicSurfaceInteractionInterface, public IExplosionResponseInterface
{
	GENERATED_BODY()
public:
 /** A blast that wrecks this vehicle may light its own bounded fuse. */
	virtual bool ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event) override;
	virtual UHealthComponent* GetDamageHealth_Implementation() const override { return DamageHealth; }
	virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
	virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
	virtual ENeonCityCombatState GetCombatState_Implementation() const override;
	UHealthComponent* GetHealthComponent() const { return DamageHealth; }
	ANeonCityBaseVehicle();
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category="Vehicle|Identity") FGuid VehicleID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Vehicle|Ownership") bool bStartsOwned=false;
 const UVehicleData* GetVehicleProfile() const;
 UVehicleDamageComponent* GetDamageState() const { return DamageState; }
#if WITH_EDITOR
 virtual void PostEditImport() override;
#endif
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionName_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FName GetVehicleId_Implementation() const override { return VehicleID.IsValid()?FName(*VehicleID.ToString()):NAME_None; }

	UVehicleComponent* GetDrivingComponent() const { return Driving; }
 URoadCruiseComponent* GetRoadCruise() const { return RoadCruise; }
	UVehicleInteraction* GetVehicleInteraction() const { return VehicleInteraction; }
	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;
	UCameraComponent* GetVehicleCamera() const { return Camera; }
	USpringArmComponent* GetVehicleSpringArm() const { return SpringArm; }
	bool IsEntryPromptVisible() const;
 /** V: driver's-eye view from the seat (the driver's own head hidden, mouse looks around) / chase view. */
 void SetCockpitView(bool bEnabled);
 bool IsCockpitView() const { return bCockpitView; }
 UCameraComponent* GetCockpitCamera() const { return CockpitCamera; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NeonCity|Vehicle")
	TObjectPtr<UVehicleData> VehicleData;

protected:
	virtual TArray<float> GetFittedWheelRadii() const { return {}; }
 /** Subclasses mix their cabin sound or trim for the driver's-eye view. */
 virtual void OnCockpitViewChanged(bool bEnabled) {}
	virtual void BeginPlay() override;
 virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
 UFUNCTION() void CollisionFeedback(float DeltaSpeed,float AppliedDamage,FVector Location);
 UPROPERTY(VisibleAnywhere,Category="Vehicle") TObjectPtr<UVehicleDamageComponent> DamageState;
 UPROPERTY() TObjectPtr<UVehicleStatusWidget> StatusWidget;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UHealthComponent> DamageHealth;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCombatComponent> Combat;
	void Drive(const FInputActionValue& Value);
	void Steer(const FInputActionValue& Value);
	void Brake(const FInputActionValue& Value);
 void ToggleCruise();
 void ToggleCockpitView();
 void LookYaw(const FInputActionValue& Value);
 void LookPitch(const FInputActionValue& Value);
 void UpdateCockpitCamera(float DeltaSeconds);
 void ManualSteeringTakeover();
	void ExitVehicle();
	void RemoveVehicleMapping();

	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<UVehicleComponent> Driving;
 UPROPERTY(VisibleAnywhere,Category="NeonCity|Vehicle") TObjectPtr<URoadCruiseComponent> RoadCruise;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<UVehicleInteraction> VehicleInteraction;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<UNeonCityCameraExtensionComponent> CameraExtension;
 UPROPERTY(VisibleAnywhere,Category="NeonCity|Vehicle") TObjectPtr<UCameraComponent> CockpitCamera;
 UPROPERTY(Transient) TObjectPtr<UInputAction> CameraAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookYawAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookPitchAction;
 bool bCockpitView=false;
 FRotator CockpitLook=FRotator::ZeroRotator;
 float LookIdle=0;
 FVector CockpitHead=FVector::ZeroVector,PrevVelocity=FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Vehicle")
	TObjectPtr<UTextRenderComponent> EntryPrompt;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> VehicleMapping;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DriveAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ExitAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> CruiseAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ManualLeftAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ManualRightAction;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};
