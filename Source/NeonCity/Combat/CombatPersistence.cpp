#include "Combat/CombatComponent.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Combat/HealthComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
void UHealthComponent::ApplyPersistenceHealth(float Current,float Maximum)
{
 if(!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Current) || !FMath::IsFinite(Maximum) || Maximum<=0 || Current<0 || Current>Maximum) return;
 MaxHealth=Maximum; CurrentHealth=Current; bDead=Current<=0; OnHealthChanged.Broadcast(CurrentHealth,MaxHealth);
}
FNeonCityWeaponSaveData AWeaponBase::BuildDefaultSaveData() const
{
 const auto* Profile=Data ? Data.Get() : GetDefaultData();
 FNeonCityWeaponSaveData Result;
 if(Profile) { Result.WeaponID=Profile->WeaponId; Result.DataAssetID=NeonCityWeapons::AssetID(Profile->WeaponId); Result.Ammo=Profile->MagazineSize; Result.ReserveAmmo=Profile->StartingReserve; }
 return Result;
}
FNeonCityWeaponSaveData AWeaponBase::BuildPersistenceSnapshot() const
{
 auto Result=BuildDefaultSaveData(); Result.Ammo=Ammo; Result.ReserveAmmo=Reserve; Result.Equipped=bEquipped; return Result;
}
void AWeaponBase::ApplyPersistenceAmmo(const FNeonCityWeaponSaveData& Snapshot)
{ CancelReload(); Ammo=FMath::Clamp(Snapshot.Ammo,0,Data?Data->MagazineSize:100); Reserve=FMath::Clamp(Snapshot.ReserveAmmo,0,1000); NextFireTime=0; LastShotTime=-100; SustainedFire=0; LastHit=FHitResult(); LastAppliedDamage=0; }
TArray<FNeonCityWeaponSaveData> UCombatComponent::BuildPersistenceSnapshot() const
{
 if(auto* I=GetOwner()->FindComponentByClass<UWeaponInventoryComponent>()) return I->BuildPersistenceSnapshot();
 if(IsValid(Weapon)) return {Weapon->BuildPersistenceSnapshot()};
 return RestoredInventory;
}
void UCombatComponent::NormalizePersistenceSnapshot(TArray<FNeonCityWeaponSaveData>& Snapshot,TArray<FString>& Warnings)
{
 TSet<FName> Seen; bool HasEquipped=false;
 Snapshot.RemoveAll([&](auto& Entry)
 {
  const auto* Profile=NeonCityWeapons::Profile(Entry.WeaponID);
  if(!Profile || Seen.Contains(Entry.WeaponID) || (!Entry.DataAssetID.IsNone() && Entry.DataAssetID!=NeonCityWeapons::AssetID(Entry.WeaponID))
   || Entry.Ammo<0 || Entry.Ammo>Profile->MagazineSize || Entry.ReserveAmmo<0 || Entry.ReserveAmmo>1000)
  { Warnings.Add(TEXT("Unknown/invalid weapon entry skipped.")); return true; }
  Seen.Add(Entry.WeaponID); Entry.DataAssetID=NeonCityWeapons::AssetID(Entry.WeaponID);
  if(!Entry.Owned) { Entry.Equipped=false; Entry.Ammo=Entry.ReserveAmmo=0; }
  if(Entry.Equipped && HasEquipped) { Entry.Equipped=false; Warnings.Add(TEXT("Extra equipped weapon reset to stowed.")); }
  HasEquipped|=Entry.Equipped; return false;
 });
}

bool UCombatComponent::ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot,USceneComponent* Mount,FName Socket)
{
 UnequipWeapon(); if(IsValid(Weapon)) Weapon->Destroy(); Weapon=nullptr;
 RestoredInventory=Snapshot; TArray<FString> Warnings; NormalizePersistenceSnapshot(RestoredInventory,Warnings); bHasRestoredInventory=true;
 if(auto* I=GetOwner()->FindComponentByClass<UWeaponInventoryComponent>()) I->ApplyPersistenceSnapshot(RestoredInventory);
 for(const auto& Data:RestoredInventory)
 {
  if(!Data.Owned || !Data.Equipped) continue;
  // Explicit native factory. Never resolve an arbitrary Actor class supplied by a save file.
  FActorSpawnParameters Params; Params.Owner=GetOwner(); Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Weapon=GetWorld()->SpawnActor<AWeaponBase>(NeonCityWeapons::Resolve(Data.WeaponID),FTransform::Identity,Params);
  if(!Weapon) return false;
  Weapon->ApplyPersistenceAmmo(Data);
  if(Data.Equipped && GetCombatState()!=ENeonCityCombatState::Dead && GetCombatState()!=ENeonCityCombatState::Disabled) Weapon->Equip(Mount,Socket);
  break;
 }
 return true;
}
