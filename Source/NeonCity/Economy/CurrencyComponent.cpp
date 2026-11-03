#include "Economy/CurrencyComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
UCurrencyComponent::UCurrencyComponent() { PrimaryComponentTick.bCanEverTick = false; }
UNeonCityEconomySubsystem* UCurrencyComponent::GetEconomy() const
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNeonCityEconomySubsystem>() : nullptr;
}
int64 UCurrencyComponent::GetBalance() const { auto* E=GetEconomy(); return E ? E->GetBalance() : 0; }
bool UCurrencyComponent::AddMoney(int64 Amount) { auto* E=GetEconomy(); return E && E->AddMoney(Amount,TEXT("CurrencyComponent")); }
bool UCurrencyComponent::RemoveMoney(int64 Amount) { auto* E=GetEconomy(); return E && E->RemoveMoney(Amount,TEXT("CurrencyComponent")); }
