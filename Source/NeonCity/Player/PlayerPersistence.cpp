#include "Player/NeonCityPlayerDataComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Adventure/IslandOwnedAppearanceState.h"
FNeonCityPlayerSaveData UNeonCityPlayerDataComponent::BuildPersistenceSnapshot() const
{
 FNeonCityPlayerSaveData Result; Result.Transform=GetOwner()->GetActorTransform();
 if(const auto* Kit=GetOwner()->FindComponentByClass<UDivingComponent>())
 {Result.OwnsR05=Kit->OwnsSuit();Result.OwnsDPV=Kit->OwnsDPV();Result.R05Equipped=Kit->IsEquipped();}
 if(const auto* Wingsuit=GetOwner()->FindComponentByClass<UWingsuitComponent>())Result.WingsuitEquipped=Wingsuit->IsEquipped();
 if(const auto* Appearance=GetOwner()->FindComponentByClass<UIslandOwnedAppearanceState>())
 {Result.OwnedAppearancePalette=Appearance->GetSelectedPalette();Result.OwnedAppearanceMask=Appearance->GetOwnedPaletteMask();}
 Result.Health=GetHealth(); Result.MaxHealth=GetMaxHealth(); Result.Stamina=Stamina; Result.Money=GetMoney(); return Result;
}
void UNeonCityPlayerDataComponent::ApplyPersistenceSnapshot(const FNeonCityPlayerSaveData& Snapshot)
{
 if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->ApplyPersistenceHealth(Snapshot.Health,Snapshot.MaxHealth);
 Stamina=Snapshot.Stamina;
 // Transform is applied by the save coordinator after its capsule preflight; wallet owns Money.
}
