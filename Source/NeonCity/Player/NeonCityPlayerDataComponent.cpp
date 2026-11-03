#include "Player/NeonCityPlayerDataComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UNeonCityPlayerDataComponent::UNeonCityPlayerDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNeonCityPlayerDataComponent::SetHealth(float NewHealth)
{
	if (auto* H=GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr) { H->SetCurrentHealth(NewHealth); return; }
	if (FMath::IsFinite(NewHealth))
	{
		Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	}
}

void UNeonCityPlayerDataComponent::SetMaxHealth(float NewMaxHealth)
{
	if (auto* H=GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr) { H->SetMaxHealth(NewMaxHealth); return; }
	if (FMath::IsFinite(NewMaxHealth))
	{
		MaxHealth = FMath::Max(0.0f, NewMaxHealth);
		Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	}
}

void UNeonCityPlayerDataComponent::SetStamina(float NewStamina)
{
	if (FMath::IsFinite(NewStamina))
	{
		Stamina = FMath::Max(0.0f, NewStamina);
	}
}

void UNeonCityPlayerDataComponent::SetMoney(int64 NewMoney)
{
	if (UGameInstance* GI=GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		auto* Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();
		NewMoney=FMath::Max(int64(0),NewMoney);
		const int64 Current=Economy->GetBalance();
		if (NewMoney>Current) { Economy->AddMoney(NewMoney-Current,TEXT("LegacyPlayerDataSet")); }
		else if (NewMoney<Current) { Economy->RemoveMoney(Current-NewMoney,TEXT("LegacyPlayerDataSet")); }
		return;
	}
	Money = FMath::Max(int64(0), NewMoney);
}
int64 UNeonCityPlayerDataComponent::GetMoney() const
{
	UGameInstance* GI=GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance() : Money;
}

float UNeonCityPlayerDataComponent::GetHealth() const
{
	auto* H=GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	return H ? H->GetCurrentHealth() : Health;
}
float UNeonCityPlayerDataComponent::GetMaxHealth() const
{
	auto* H=GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	return H ? H->GetMaxHealth() : MaxHealth;
}
