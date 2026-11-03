#include "Combat/DamageSystem.h"
#include "Combat/CombatContextSubsystem.h"
#include "Engine/World.h"
#include "Combat/DamageInterface.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
float UDamageSystem::ApplyDamage(AActor* Victim,const FNeonCityDamageEvent& Event)
{
	if (!IsValid(Victim) || !Victim->Implements<UDamageInterface>()) { return 0; }
	UHealthComponent* Health=IDamageInterface::Execute_GetDamageHealth(Victim);
	return Health && Health->GetOwner()==Victim ? ApplyToHealth(Health,Event) : 0;
}
float UDamageSystem::ApplyToHealth(UHealthComponent* Health,const FNeonCityDamageEvent& Event)
{
	if (!IsValid(Health) || !IsValid(Health->GetOwner()) || !Health->GetOwner()->HasAuthority()
		|| Health->GetWorld()!=GetWorld() || Health->IsDead()
		|| !FMath::IsFinite(Event.DamageAmount) || Event.DamageAmount<=0
		|| Event.HitLocation.ContainsNaN()
		|| !StaticEnum<ENeonCityDamageType>()->IsValidEnumValue(static_cast<int64>(Event.DamageType))) { return 0; }
	FNeonCityDamageEvent Applied=Event;
 GetWorld()->GetSubsystem<UCombatContextSubsystem>()->AnnotateDamage(Health->GetOwner(),Applied);
	Applied.DamageAmount=FMath::Min(Event.DamageAmount,Health->GetCurrentHealth());
	// State is committed before any observer is notified.
	const float Amount=Health->CommitDamage(Applied);
	if (Amount>0)
	{
		LastDamage=Applied; ++DamageCount;
		OnDamage.Broadcast(Health->GetOwner(),Applied);
	}
	return Amount;
}
