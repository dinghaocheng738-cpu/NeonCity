#include "Combat/WeaponInventoryComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
UWeaponInventoryComponent::UWeaponInventoryComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
}
void UWeaponInventoryComponent::BeginPlay()
{
 Super::BeginPlay();
 // Preserve the released new-game inventory, reading ammo defaults from the existing pistol asset.
 OwnedWeapons={GetDefault<ANeonCityPistol>()->BuildDefaultSaveData()};
}

TArray<FNeonCityWeaponSaveData> UWeaponInventoryComponent::BuildPersistenceSnapshot() const
{
 auto Result=OwnedWeapons;
 if(auto* C=GetOwner()->FindComponentByClass<UCombatComponent>()) if(auto* W=C->GetWeapon(); IsValid(W))
 {
  auto Live=W->BuildPersistenceSnapshot();
  for(auto& Entry:Result) { Entry.Equipped=false; if(Entry.WeaponID==Live.WeaponID && Entry.Owned) Entry=Live; }
 }
 return Result;
}
void UWeaponInventoryComponent::CaptureCurrent() { OwnedWeapons=BuildPersistenceSnapshot(); }
void UWeaponInventoryComponent::ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot)
{ OwnedWeapons=Snapshot; TArray<FString> Warnings; UCombatComponent::NormalizePersistenceSnapshot(OwnedWeapons,Warnings); }
bool UWeaponInventoryComponent::Owns(FName ID) const
{ return OwnedWeapons.ContainsByPredicate([&](const auto& E){return E.WeaponID==ID && E.Owned;}); }
bool UWeaponInventoryComponent::GetWeaponRecord(FName ID,FNeonCityWeaponSaveData& Out) const
{ for(const auto& E:BuildPersistenceSnapshot()) if(E.WeaponID==ID && E.Owned) { Out=E; return true; } return false; }
FName UWeaponInventoryComponent::GetEquippedWeaponID() const
{ for(const auto& E:BuildPersistenceSnapshot()) if(E.Owned && E.Equipped) return E.WeaponID; return NAME_None; }
int32 UWeaponInventoryComponent::GetReserve(ENeonCityAmmoType Type) const
{
 // Catalog v1 has exactly one weapon per ammo pool. Persist reserve with its stable weapon ID.
 for(const auto& E:BuildPersistenceSnapshot()) if(E.Owned) if(const auto* P=NeonCityWeapons::Profile(E.WeaponID); P && P->AmmoType==Type) return E.ReserveAmmo;
 return 0;
}
bool UWeaponInventoryComponent::GrantWeapon(FName ID)
{
 if(!GetOwner()->HasAuthority() || Owns(ID)) return false;
 auto Type=NeonCityWeapons::Resolve(ID); if(!Type) return false;
 CaptureCurrent(); OwnedWeapons.RemoveAll([&](const auto& E){return E.WeaponID==ID;});
 OwnedWeapons.Add(GetDefault<AWeaponBase>(Type)->BuildDefaultSaveData()); return true;
}
bool UWeaponInventoryComponent::EquipSlot(int32 Slot,USceneComponent* Mount,FName Socket)
{
 const FName ID=NeonCityWeapons::Slot(Slot); if(!Owns(ID)) return false;
 auto* C=GetOwner()->FindComponentByClass<UCombatComponent>(); return C && C->EquipWeapon(NeonCityWeapons::Resolve(ID),Mount,Socket);
}
bool UWeaponInventoryComponent::PurchaseWeapon(FName ID)
{
 if(!GetWorld() || !GetWorld()->GetGameInstance()) return false;
 const auto* P=NeonCityWeapons::Profile(ID);
 auto* E=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 if(bTransaction || !GetOwner()->HasAuthority() || !P || !P->IsValidProfile() || Owns(ID) || !E || E->GetBalance()<P->Price) return false;
 TGuardValue<bool> Lock(bTransaction,true);
 if(!E->RemoveMoney(P->Price,FName(*FString::Printf(TEXT("WeaponPurchase.%s"),*ID.ToString())))) return false;
 if(GrantWeapon(ID)) return true;
 E->AddMoney(P->Price,TEXT("WeaponPurchase.Refund")); return false;
}
int32 UWeaponInventoryComponent::AmmoPackSize(ENeonCityAmmoType Type)
{ const int32 N[]={24,60,48,12}; return uint8(Type)<4?N[uint8(Type)]:0; }
int64 UWeaponInventoryComponent::AmmoPackPrice(ENeonCityAmmoType Type)
{ const int64 N[]={60,100,140,90}; return uint8(Type)<4?N[uint8(Type)]:0; }
bool UWeaponInventoryComponent::PurchaseAmmo(ENeonCityAmmoType Type)
{
 if(!GetWorld() || !GetWorld()->GetGameInstance() || bTransaction || !GetOwner()->HasAuthority() || uint8(Type)>3) return false;
 CaptureCurrent(); auto* Entry=OwnedWeapons.FindByPredicate([&](const auto& V){const auto* P=NeonCityWeapons::Profile(V.WeaponID); return V.Owned && P && P->AmmoType==Type;});
 auto* E=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 if(!Entry || !E || Entry->ReserveAmmo>1000-AmmoPackSize(Type) || E->GetBalance()<AmmoPackPrice(Type)) return false;
 const FName ID=Entry->WeaponID; TGuardValue<bool> Lock(bTransaction,true);
 if(!E->RemoveMoney(AmmoPackPrice(Type),FName(*FString::Printf(TEXT("AmmoPurchase.%s"),*ID.ToString())))) return false;
 // Re-find after delegates: never retain an array element reference across the economy signal.
 Entry=OwnedWeapons.FindByPredicate([&](const auto& V){return V.WeaponID==ID && V.Owned;});
 if(!Entry || Entry->ReserveAmmo>1000-AmmoPackSize(Type)) { E->AddMoney(AmmoPackPrice(Type),TEXT("AmmoPurchase.Refund")); return false; }
 Entry->ReserveAmmo+=AmmoPackSize(Type);
 if(auto* C=GetOwner()->FindComponentByClass<UCombatComponent>()) if(auto* W=C->GetWeapon(); IsValid(W) && W->GetData()->WeaponId==ID) W->SetReserveAmmo(Entry->ReserveAmmo);
 return true;
}
