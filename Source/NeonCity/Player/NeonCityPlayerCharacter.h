#pragma once

#include "CoreMinimal.h"
#include "NeonCityCharacter.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "NeonCityPlayerCharacter.generated.h"
class UHealthComponent;
class UCombatComponent;
class UPlayerCombatAdapter;
class UDamageableCharacterComponent;
class UCharacterDamageFeedbackComponent;

class UInteractionComponent;
class UCurrencyComponent;
class UNeonCityPlayerDataComponent;
class UNeonCityCameraExtensionComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;

/** Extends template input via Super; movement and animation remain inherited. */
UCLASS(Abstract, Blueprintable)
class NEONCITY_API ANeonCityPlayerCharacter : public ANeonCityCharacter, public IDamageInterface, public ICombatInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="NeonCity|Camera") bool IsFirstPerson() const { return bFirstPerson; }
	void SetFirstPerson(bool bEnable) { if (bEnable != bFirstPerson) { ApplyFirstPerson(bEnable); } }
	virtual UHealthComponent* GetDamageHealth_Implementation() const override { return DamageHealth; }
	virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
	virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
	virtual ENeonCityCombatState GetCombatState_Implementation() const override;
	UHealthComponent* GetHealthComponent() const { return DamageHealth; }
	UCombatComponent* GetCombatComponent() const { return Combat; }
	UPlayerCombatAdapter* GetCombatAdapter() const { return CombatAdapter; }
	UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") UCharacterDamageFeedbackComponent* GetDamageFeedbackComponent() const { return DamageFeedback; }
	ANeonCityPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UFUNCTION(BlueprintPure, Category="NeonCity|Economy")
	UCurrencyComponent* GetCurrencyComponent() const { return Currency; }

	UFUNCTION(BlueprintPure, Category="NeonCity|Interaction")
	UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category="NeonCity|Player")
	UNeonCityPlayerDataComponent* GetPlayerDataComponent() const { return PlayerDataComponent; }

	UFUNCTION(BlueprintPure, Category="NeonCity|Camera")
	UNeonCityCameraExtensionComponent* GetCameraExtensionComponent() const { return CameraExtensionComponent; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UHealthComponent> DamageHealth;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UPlayerCombatAdapter> CombatAdapter;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UDamageableCharacterComponent> BodyState;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCharacterDamageFeedbackComponent> DamageFeedback;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Economy") TObjectPtr<UCurrencyComponent> Currency;
	void HandleInteract();
	void ToggleFirstPerson();
	void ApplyFirstPerson(bool bEnable);
	/** On foot: V swaps the over-the-shoulder camera for the player's own eyes (body faces the view, head hidden). */
	bool bFirstPerson=false;
	float SavedBoomLength=400.f;FVector SavedBoomOffset=FVector::ZeroVector,SavedBoomLocation=FVector::ZeroVector;bool bSavedBoomLag=true,bSavedBoomCollision=true;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ViewAction;
	void RemoveInteractionMapping();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NeonCity|Interaction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NeonCity|Player", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNeonCityPlayerDataComponent> PlayerDataComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NeonCity|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNeonCityCameraExtensionComponent> CameraExtensionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InteractionMapping;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InteractionInputSubsystem;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInteractionInput;
	uint32 InteractionBindingHandle = 0;
	bool bHasInteractionBinding = false;
};
