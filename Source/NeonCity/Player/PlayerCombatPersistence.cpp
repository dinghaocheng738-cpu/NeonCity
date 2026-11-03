#include "Player/PlayerCombatAdapter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/CombatFeedbackWidget.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Components/SkeletalMeshComponent.h"
TArray<FNeonCityWeaponSaveData> UPlayerCombatAdapter::BuildPersistenceSnapshot(bool Defaults) const
{
 auto* Combat=Player()->GetCombatComponent();
 if(Defaults) return {GetDefault<ANeonCityPistol>()->BuildDefaultSaveData()};
 return Combat->BuildPersistenceSnapshot();
}
void UPlayerCombatAdapter::PrepareForRestore()
{ StopFire(); CloseStore(); StopAim(); RecoilOffset=FRotator::ZeroRotator; HitSeconds=0; Player()->GetCombatComponent()->UnequipWeapon(); if(auto* Light=Player()->FindComponentByClass<UNeonCityFlashlightComponent>())Light->SetEquipped(false); if(Feedback) Feedback->RemoveFromParent(); }
void UPlayerCombatAdapter::ApplyPersistenceSnapshot(const TArray<FNeonCityWeaponSaveData>& Snapshot)
{
 auto* P=Player(); auto* Presentation=P->FindComponentByClass<UCharacterWeaponPresentationComponent>();
 if(!Presentation || !Presentation->GetGrip()) return;
 P->GetCombatComponent()->ApplyPersistenceSnapshot(Snapshot,Presentation->GetGrip(),NAME_None);
 if(P->GetCombatComponent()->GetWeapon() && P->GetCombatComponent()->GetWeapon()->IsEquipped()) AttachWeaponFeedback();
}
