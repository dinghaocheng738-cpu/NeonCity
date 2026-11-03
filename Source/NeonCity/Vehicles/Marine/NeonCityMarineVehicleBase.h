#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/Marine/MarineVehicleInterface.h"
#include "Vehicles/VehicleInterface.h"
#include "Interaction/InteractableInterface.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "NeonCityMarineVehicleBase.generated.h"
class UBoxComponent;
class UMarineVehicleComponent;
class UMarineFeedbackComponent;
class UMarineWaterEffectsComponent;
class UMarineInteractionComponent;
class UMarineVehicleData;
class UHealthComponent;
class UCombatComponent;
class UNeonCityCameraExtensionComponent;
class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;
class UVehicleStatusWidget;
class UTransportOccupantComponent;
struct FInputActionValue;

/** Parallel to the wheeled pawn: composition shares combat, damage, interaction and dynamic surfaces. */
UCLASS()
class NEONCITY_API ANeonCityMarineVehicleBase : public APawn, public IMarineVehicleInterface, public IVehicleInterface,
 public IInteractableInterface, public IDynamicSurfaceInteractionInterface, public IDamageInterface, public ICombatInterface
{
 GENERATED_BODY()
public:
 ANeonCityMarineVehicleBase();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FGuid VehicleID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool Owned=true;
 /** False for boats that exist only while in use (the swan yacht's tender): no registry entry, nothing persisted. */
 bool bPersistent=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UMarineVehicleData> VehicleData;
 const UMarineVehicleData* GetProfile() const;
 UBoxComponent* GetHull() const { return Hull; }
 UMarineVehicleComponent* GetMarineMovement() const { return Movement; }
 UMarineInteractionComponent* GetMarineInteraction() const { return Interaction; }
 UHealthComponent* GetHealthComponent() const { return Health; }
 UCameraComponent* GetMarineCamera() const { return Camera; }
 bool IsHelmCamera() const { return bHelmCamera; }
 void ToggleMarineCamera();
 bool IsEntryPromptVisible() const;
 bool IsDrivingStatusVisible() const;
 FText GetExitFeedback() const { return ExitFeedback; }
 /** Boat-specific place to step ashore (e.g. a low landing beside the seat); false = use the generic search. */
 virtual bool FindShoreLanding(const class ACharacter* Character,FTransform& Out) const { return false; }
 /** Highest point above the waterline (cm): what a bridge's closed span must clear. */
 virtual float GetAirDraft() const;
 /** Helm (first-person) view switched: subclasses hide whatever body would sit on the eye. */
 virtual void OnHelmCameraChanged(bool bHelm) {}
#if WITH_EDITOR
 virtual void PostEditImport() override;
#endif
 virtual FName GetVehicleId_Implementation() const override;
 virtual bool EnterVehicle_Implementation(AActor* Driver) override;
 virtual bool ExitVehicle_Implementation() override;
 virtual bool StartEngine_Implementation() override;
 virtual void StopEngine_Implementation() override;
 virtual float GetSpeed_Implementation() const override;
 virtual FVector GetWaterVelocity_Implementation() const override;
 virtual bool SubmitDriverInput_Implementation(AController* Driver,float Throttle,float Steering,float Brake) override;
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Interactor) override;
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 virtual UHealthComponent* GetDamageHealth_Implementation() const override { return Health; }
 virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
 virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
 virtual ENeonCityCombatState GetCombatState_Implementation() const override;
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float Delta) override;
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 virtual void PossessedBy(AController* Controller) override;
 virtual void UnPossessed() override;
 /** Driver axes (keyboard or SubmitDriverInput). Motor boats feed the marine movement; hand-propelled boats override. */
 virtual void ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue);
 /** Driving HUD text; State is the localized drive state. */
 virtual FText BuildStatusText(const struct FMarineDriveTelemetry& T,const FText& State) const;
 /** Leave request from the driver (F). Returns true when the driver left or a staged exit began. */
 virtual bool RequestDriverExit();
 UTransportOccupantComponent* GetOccupantPresentation() const;
 USpringArmComponent* GetCameraArm() const { return Arm; }
 /** Extra helm keys for a subclass: map them into Context once (it persists) and bind them on Input. */
 virtual void BindExtraHelmKeys(class UEnhancedInputComponent* Input,UInputMappingContext* Context) {}
private:
 void RemoveMapping();
 void UpdateDrivingStatus(float Delta);
 void Throttle(const FInputActionValue& Value);
 void Steering(const FInputActionValue& Value);
 void Brake(const FInputActionValue& Value);
 void Exit();
 void LookYaw(const FInputActionValue& Value);
 void LookPitch(const FInputActionValue& Value);
 bool bHelmCamera=false;
 FRotator CameraLook=FRotator::ZeroRotator;
 float InputThrottle=0,InputSteer=0,InputBrake=0;
 float StatusRefresh=0;
 float ExitFeedbackRemaining=0;
 FText ExitFeedback;
 UPROPERTY(Transient) TObjectPtr<UVehicleStatusWidget> StatusWidget;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Hull;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UMarineVehicleComponent> Movement;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UMarineFeedbackComponent> Feedback;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UMarineWaterEffectsComponent> WaterEffects;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UMarineInteractionComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UNeonCityCameraExtensionComponent> CameraExtension;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Prompt;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> BrakeAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ExitAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> CameraAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookYawAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookPitchAction;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};
