#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/Aircraft/AircraftVehicleInterface.h"
#include "Vehicles/VehicleInterface.h"
#include "Interaction/InteractableInterface.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "Physics/ImpactResponseInterface.h"
#include "NeonCityAircraftVehicleBase.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UAircraftVehicleComponent;
class UAircraftFlightStatusComponent;
class UAircraftInteractionComponent;
class UAircraftVehicleData;
class UHealthComponent;
class UCombatComponent;
class UNeonCityCameraExtensionComponent;
class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;
struct FInputActionValue;

/** Parallel to the wheeled pawn: composition shares combat, damage, interaction and dynamic surfaces. */
UCLASS()
class NEONCITY_API ANeonCityAircraftVehicleBase : public APawn, public IAircraftVehicleInterface, public IVehicleInterface,
 public IAircraftDriverInterface, public IImpactResponseInterface, public IInteractableInterface, public IDynamicSurfaceInteractionInterface, public IDamageInterface, public ICombatInterface
{
 GENERATED_BODY()
public:
 ANeonCityAircraftVehicleBase(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FGuid AircraftID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool Owned=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UAircraftVehicleData> VehicleData;
 virtual const UAircraftVehicleData* GetProfile() const;
 virtual bool IsSafeRestoreTransform(const FTransform& Transform) const { return true; }
 virtual bool ResolveRestoreTransform(FTransform& Transform,const FTransform& Stand) const
 { if(IsSafeRestoreTransform(Transform)) return true; Transform=Stand; return IsSafeRestoreTransform(Transform); }
 UBoxComponent* GetHull() const { return Hull; }
 UAircraftVehicleComponent* GetAircraftMovement() const { return Movement; }
 UAircraftFlightStatusComponent* GetFlightStatus() const { return FlightStatus; }
 bool SubmitPowerInput(AController* Driver,float Adjustment);
 UAircraftInteractionComponent* GetAircraftInteraction() const { return Interaction; }
 UHealthComponent* GetHealthComponent() const { return Health; }
 UCameraComponent* GetAircraftCamera() const { return Camera; }
 bool IsEntryPromptVisible() const;
#if WITH_EDITOR
 virtual void PostEditImport() override;
#endif
 virtual FName GetVehicleId_Implementation() const override;
 virtual bool EnterAircraft_Implementation(AActor* Driver) override;
 virtual bool ExitAircraft_Implementation() override;
 virtual bool StartEngine_Implementation() override;
 virtual void StopEngine_Implementation() override;
 virtual FVector GetVelocity() const override;
 virtual float GetAltitude_Implementation() const override;
 virtual EAircraftFlightState GetFlightState_Implementation() const override;
 virtual bool SubmitFlightInput_Implementation(AController* Driver,FVector Translation,float Yaw) override;
 virtual void ReceivePhysicalImpact_Implementation(const FPhysicalImpactEvent& Event) override;
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
protected:
 virtual void UpdateAircraftCamera(float Delta);
 virtual bool CanSupportStandingCharacter() const;
 void RemoveMapping();
 void Forward(const FInputActionValue& Value);
 void Strafe(const FInputActionValue& Value);
 void Vertical(const FInputActionValue& Value);
 void Yaw(const FInputActionValue& Value);
 void Power(const FInputActionValue& Value);
 void LookYaw(const FInputActionValue& Value);
 void LookPitch(const FInputActionValue& Value);
 void UpdateFlightInput();
 void Exit();
 FVector TranslationInput=FVector::ZeroVector;
 float YawInput=0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Hull;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAircraftVehicleComponent> Movement;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAircraftFlightStatusComponent> FlightStatus;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAircraftInteractionComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UNeonCityCameraExtensionComponent> CameraExtension;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Prompt;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ForwardAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> StrafeAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> VerticalAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> YawAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> PowerAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookYawAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LookPitchAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ExitAction;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
};
