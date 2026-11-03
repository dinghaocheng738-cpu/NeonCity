#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "DamageFeedback/DamageFeedbackTypes.h"
#include "PlayerCombatAdapter.generated.h"
class ANeonCityPlayerCharacter;
class UCombatFeedbackWidget;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class AWeaponStoreActor;
class AWeaponBase;
class UCharacterDamageFeedbackComponent;
/** Only player-specific input, view aiming and feedback live here. Weapons remain platform-agnostic. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UPlayerCombatAdapter : public UActorComponent
{
	GENERATED_BODY()
public:
 TArray<FNeonCityWeaponSaveData> BuildPersistenceSnapshot(bool Defaults=false) const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot);
 void PrepareForRestore();

	UPlayerCombatAdapter();
	void SetupInput(UInputComponent* Input);
	void Suspend();
	void OpenStore(AWeaponStoreActor* Store);
	void CloseStore();
	bool IsStoreOpen() const { return ActiveStore.IsValid(); }
	void StartFire();
	void StopFire();
 /** Holster for a held utility without removing input or losing inventory. */
 void StowForUtility();
	bool IsAiming() const { return bAiming; }
	int32 GetMuzzleFeedbackCount() const { return MuzzleCount; }
	/** Damage-taken feedback is separate from "my shot hit" so the two never get confused. */
	float GetDamageIndicatorSeconds() const { return DamageSeconds; }
	float GetLastDamageYaw() const { return LastDamageYaw; }
	int32 GetDamageFeedbackCount() const { return DamageFeedbackCount; }
	ENeonCityFeedbackTier GetVitalsTier() const;
	int32 GetImpactFeedbackCount() const { return ImpactCount; }
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void ToggleEquip();
	void SelectSlot(int32 Slot);
	void Slot2() { SelectSlot(2); }
	void Slot3() { SelectSlot(3); }
	void Slot4() { SelectSlot(4); }
	void BuyAmmo(int32 Slot);
	void Ammo1() { BuyAmmo(1); }
	void Ammo2() { BuyAmmo(2); }
	void Ammo3() { BuyAmmo(3); }
	void Ammo4() { BuyAmmo(4); }
 void AttachWeaponFeedback();
	void Aim();
	void StopAim();
	void Fire();
	void Reload();
	bool CanUse() const;
	UFUNCTION() void MuzzleFlash(FTransform Transform);
	UFUNCTION() void Impact(const FHitResult& Hit);
	UFUNCTION() void Recoil(float Pitch);
	void RemoveInput();
	void BindDamageFeedback();
	UFUNCTION() void DamagedFeedback(const FNeonCityDamageFeedbackEvent& Event);
	UCombatFeedbackWidget* EnsureFeedback();
	FText BuildDamageIndicator() const;
	ANeonCityPlayerCharacter* Player() const;
	UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UCombatFeedbackWidget> Feedback;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	TArray<uint32> Bindings;
	bool bAiming=false,bSavedOrient=true,bSavedYaw=false;
	float HitSeconds=0;
	int32 MuzzleCount=0,ImpactCount=0;
	FTimerHandle FireTimer;
	TWeakObjectPtr<AWeaponBase> TriggerWeapon;
	bool bTriggerHeld=false;
	FRotator RecoilOffset=FRotator::ZeroRotator;
	float LastRecoilTime=-100,RecoilRecovery=4;
	TWeakObjectPtr<UCharacterDamageFeedbackComponent> DamageSource;
	float DamageSeconds=0,LastDamageAmount=0,LastDamageYaw=0;
	int32 DamageFeedbackCount=0;
	TWeakObjectPtr<AWeaponStoreActor> ActiveStore;
	FString ShopResult;
};
