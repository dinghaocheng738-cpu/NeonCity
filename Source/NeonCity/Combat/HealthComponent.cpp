#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
UHealthComponent::UHealthComponent() { PrimaryComponentTick.bCanEverTick=false; }
float UHealthComponent::TakeDamage(const FNeonCityDamageEvent& Event)
{
	auto* System=GetWorld() ? GetWorld()->GetSubsystem<UDamageSystem>() : nullptr;
	return System ? System->ApplyToHealth(this,Event) : 0;
}
float UHealthComponent::CommitDamage(const FNeonCityDamageEvent& Event)
{
	CurrentHealth=FMath::Max(0.f,CurrentHealth-Event.DamageAmount);
	const bool bDied=CurrentHealth<=0 && !bDead;
	bDead|=bDied;
	OnHealthChanged.Broadcast(CurrentHealth,MaxHealth);
	OnDamageTaken.Broadcast(Event);
	if (bDied) { OnDeath.Broadcast(); }
	return Event.DamageAmount;
}
void UHealthComponent::Die()
{
	FNeonCityDamageEvent Event; Event.DamageAmount=CurrentHealth;
	Event.DamageType=ENeonCityDamageType::Administrative; Event.SourceActor=GetOwner();
	TakeDamage(Event);
}
void UHealthComponent::SetCurrentHealth(float Value)
{
	if (!FMath::IsFinite(Value) || bDead || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	Value=FMath::Clamp(Value,0.f,MaxHealth);
	if (Value<CurrentHealth)
	{
		FNeonCityDamageEvent Event; Event.DamageAmount=CurrentHealth-Value;
		Event.DamageType=ENeonCityDamageType::Administrative; Event.SourceActor=GetOwner(); TakeDamage(Event);
	}
	else { Heal(Value-CurrentHealth); }
}
void UHealthComponent::SetMaxHealth(float Value)
{
	if (!FMath::IsFinite(Value) || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	MaxHealth=FMath::Max(0.f,Value);
	if (CurrentHealth>MaxHealth) { SetCurrentHealth(MaxHealth); }
	else { OnHealthChanged.Broadcast(CurrentHealth,MaxHealth); }
}

float UHealthComponent::Heal(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || !FMath::IsFinite(Amount) || Amount<=0) { return 0; }
	const float Applied=FMath::Min(Amount,MaxHealth-CurrentHealth);
	CurrentHealth+=Applied;
	if (Applied>0) { OnHealthChanged.Broadcast(CurrentHealth,MaxHealth); }
	return Applied;
}
