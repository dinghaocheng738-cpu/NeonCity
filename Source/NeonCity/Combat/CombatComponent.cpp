#include "Combat/CombatComponent.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
UCombatComponent::UCombatComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) { H->OnDeath.AddUniqueDynamic(this,&UCombatComponent::UnequipWeapon); }
}
ENeonCityCombatState UCombatComponent::GetCombatState() const
{
	if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>(); H && H->IsDead()) { return ENeonCityCombatState::Dead; }
	if (const auto* Character=Cast<ACharacter>(GetOwner()); Character && Character->GetCharacterMovement()->IsSwimming()) return ENeonCityCombatState::Disabled;
	if (auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { return ENeonCityCombatState::Disabled; }
	if (!IsValid(Weapon) || !Weapon->IsEquipped()) { return ENeonCityCombatState::Unarmed; }
	return Weapon->IsReloading() ? ENeonCityCombatState::Reloading : bAiming ? ENeonCityCombatState::Aiming : ENeonCityCombatState::Ready;
}
bool UCombatComponent::EquipWeapon(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,FName Socket)
{ const bool bEquipped=EquipWeaponInternal(Type,Mount,Socket,false); if(bEquipped) bInjuryStowed=false; return bEquipped; }
bool UCombatComponent::RestoreMountedWeapon(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,const FNeonCityWeaponSaveData& Snapshot)
{
 if(!EquipWeaponInternal(Type,Mount,NAME_None,true)) return false;
 Weapon->ApplyPersistenceAmmo(Snapshot);
 const auto State=GetCombatState(); if(State==ENeonCityCombatState::Dead || State==ENeonCityCombatState::Disabled) Weapon->Unequip();
 return true;
}
bool UCombatComponent::EquipWeaponInternal(TSubclassOf<AWeaponBase> Type,USceneComponent* Mount,FName Socket,bool bRestoring)
{
	if (!Type || !IsValid(Mount) || Mount->GetOwner()!=GetOwner() || !GetOwner()->HasAuthority()
		|| (!bRestoring && (GetCombatState()==ENeonCityCombatState::Dead || GetCombatState()==ENeonCityCombatState::Disabled))) { return false; }
 auto* Inventory=GetOwner()->FindComponentByClass<UWeaponInventoryComponent>();
 const FName ID=GetDefault<AWeaponBase>(Type)->BuildDefaultSaveData().WeaponID;
 FNeonCityWeaponSaveData Saved;
 if(Inventory && !Inventory->GetWeaponRecord(ID,Saved)) return false;
 if(!Inventory && bHasRestoredInventory && !RestoredInventory.ContainsByPredicate([&](const auto& V){return V.WeaponID==ID && V.Owned;})) return false;
 if(IsValid(Weapon) && Weapon->GetClass()!=Type)
 {
  UnequipWeapon(); if(Inventory) Inventory->CaptureCurrent(); Weapon->Destroy(); Weapon=nullptr;
 }

	if (!IsValid(Weapon))
	{
		FActorSpawnParameters Params; Params.Owner=GetOwner(); Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Weapon=GetWorld()->SpawnActor<AWeaponBase>(Type,FTransform::Identity,Params);
  if(Weapon && Inventory) Weapon->ApplyPersistenceAmmo(Saved);
	}
	return Weapon && Weapon->IsA(Type) && Weapon->Equip(Mount,Socket);
}
void UCombatComponent::UnequipWeapon() { bAiming=false; bInjuryStowed=false; InjuryMount.Reset(); InjurySocket=NAME_None; if (IsValid(Weapon)) { Weapon->Unequip(); } }
void UCombatComponent::StowForIncapacitation()
{
	bAiming=false;
	// A weapon the player stowed themselves stays stowed; only an equipped weapon is remembered for recovery.
	if(bInjuryStowed || !IsValid(Weapon) || !Weapon->IsEquipped()) return;
	const USceneComponent* Root=Weapon->GetRootComponent();
	if(!Root) return;
	InjuryMount=Root->GetAttachParent(); InjurySocket=Root->GetAttachSocketName();
	bInjuryStowed=true; ++InjuryStowCount; Weapon->Unequip();
}
bool UCombatComponent::RestoreAfterIncapacitation()
{
	if(!bInjuryStowed) return false;
	if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>(); H && H->IsDead()) return false;
	if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body
		&& (Body->GetBodyState()==ENeonCityBodyState::Downed || Body->GetBodyState()==ENeonCityBodyState::Dead)) return false;
	// The actor survives the stow, so magazine, reserve and WeaponID survive it too.
	if(!IsValid(Weapon) || !Weapon->Equip(InjuryMount.Get(),InjurySocket)) return false;
	bInjuryStowed=false; ++InjuryRestoreCount; return true;
}
bool UCombatComponent::Attack(const FCombatAttackRequest& Request)
{
	const auto State=GetCombatState();
	return (State==ENeonCityCombatState::Ready || State==ENeonCityCombatState::Aiming) && Weapon->Fire(Request);
}
bool UCombatComponent::ReloadWeapon()
{
	const auto State=GetCombatState();
	return (State==ENeonCityCombatState::Ready || State==ENeonCityCombatState::Aiming) && Weapon->Reload();
}
void UCombatComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) { H->OnDeath.RemoveDynamic(this,&UCombatComponent::UnequipWeapon); }
	if (IsValid(Weapon)) { Weapon->Destroy(); }
	Super::EndPlay(Reason);
}
