#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatInterface.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "CombatComponent.generated.h"
class AWeaponBase;
class USceneComponent;
/** Generic weapon host for any Actor/platform. Input and aiming cameras belong to adapters. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
 TArray<FNeonCityWeaponSaveData> BuildPersistenceSnapshot() const;
 static void NormalizePersistenceSnapshot(TArray<FNeonCityWeaponSaveData>& Snapshot,TArray<FString>& Warnings);
 bool ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot,USceneComponent* Mount,FName Socket);
 bool RestoreMountedWeapon(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,const FNeonCityWeaponSaveData& Snapshot);
 bool HasRestoredInventory() const { return bHasRestoredInventory; }

	UCombatComponent();
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") bool EquipWeapon(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,FName Socket=NAME_None);
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") void UnequipWeapon();
	/** Injury stow keeps the weapon actor, its magazine and its ID; recovery re-equips that same actor. */
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") void StowForIncapacitation();
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") bool RestoreAfterIncapacitation();
	UFUNCTION(BlueprintPure,Category="NeonCity|Combat") bool IsInjuryStowed() const { return bInjuryStowed; }
	int32 GetInjuryStowCount() const { return InjuryStowCount; }
	int32 GetInjuryRestoreCount() const { return InjuryRestoreCount; }
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") bool Attack(const FCombatAttackRequest& Request);
	UFUNCTION(BlueprintCallable,Category="NeonCity|Combat") bool ReloadWeapon();
	UFUNCTION(BlueprintPure,Category="NeonCity|Combat") AWeaponBase* GetWeapon() const { return Weapon; }
	UFUNCTION(BlueprintPure,Category="NeonCity|Combat") ENeonCityCombatState GetCombatState() const;
	void SetAiming(bool Value) { bAiming=Value; }
private:
 bool EquipWeaponInternal(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,FName Socket,bool bRestoring);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY() TObjectPtr<AWeaponBase> Weapon;
	bool bAiming=false;
 bool bHasRestoredInventory=false;
 bool bInjuryStowed=false;
 int32 InjuryStowCount=0,InjuryRestoreCount=0;
 TWeakObjectPtr<USceneComponent> InjuryMount;
 FName InjurySocket=NAME_None;
 UPROPERTY() TArray<FNeonCityWeaponSaveData> RestoredInventory;
};
