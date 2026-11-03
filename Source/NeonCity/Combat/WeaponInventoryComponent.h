#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "Combat/Weapons/WeaponData.h"
#include "WeaponInventoryComponent.generated.h"
class USceneComponent;
/** Stable ownership and finite ammo pools. Only CombatComponent's current weapon is a live Actor. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UWeaponInventoryComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UWeaponInventoryComponent();
 UFUNCTION(BlueprintPure,Category="NeonCity|Weapons") bool Owns(FName ID) const;
 bool GrantWeapon(FName ID);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Weapons") bool PurchaseWeapon(FName ID);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Weapons") bool PurchaseAmmo(ENeonCityAmmoType Type);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Weapons") bool EquipSlot(int32 Slot,USceneComponent* Mount,FName Socket=NAME_None);
 UFUNCTION(BlueprintPure,Category="NeonCity|Weapons") int32 GetReserve(ENeonCityAmmoType Type) const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Weapons") FName GetEquippedWeaponID() const;
 TArray<FNeonCityWeaponSaveData> BuildPersistenceSnapshot() const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot);
 void CaptureCurrent();
 bool GetWeaponRecord(FName ID,FNeonCityWeaponSaveData& Out) const;
 static int32 AmmoPackSize(ENeonCityAmmoType Type);
 static int64 AmmoPackPrice(ENeonCityAmmoType Type);
 /** Hook only: no ownership removal or spawned dropped weapon in this phase. */
 UFUNCTION(BlueprintImplementableEvent,Category="Weapon") void OnDropWeaponRequested(FName WeaponID);
protected:
 virtual void BeginPlay() override;
private:
 UPROPERTY() TArray<FNeonCityWeaponSaveData> OwnedWeapons;
 bool bTransaction=false;
};
