#include "Combat/DamageableCharacterComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
UDamageableCharacterComponent::UDamageableCharacterComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UDamageableCharacterComponent::BeginPlay()
{
	Super::BeginPlay(); Health=GetOwner()->FindComponentByClass<UHealthComponent>();
	if (Health)
	{
		Health->OnDamageTaken.AddDynamic(this,&UDamageableCharacterComponent::Damaged);
		Health->OnHealthChanged.AddDynamic(this,&UDamageableCharacterComponent::HealthChanged);
		Resolve();
	}
}
void UDamageableCharacterComponent::SetState(ENeonCityBodyState Value)
{
	if (Value==State) { return; }
	const auto Previous=State; State=Value;
 UpdateMovementLock();
	OnBodyStateChanged.Broadcast(Previous,State);
}
void UDamageableCharacterComponent::SetPhysicalLock(bool Locked) { bPhysicalLock=Locked; UpdateMovementLock(); }
void UDamageableCharacterComponent::UpdateMovementLock()
{
	if (auto* Character=Cast<ACharacter>(GetOwner()))
	{
		auto* Move=Character->GetCharacterMovement();
		if (bPhysicalLock || State==ENeonCityBodyState::Downed || State==ENeonCityBodyState::Dead)
		{
			if (!bStopped) { SavedMovementMode=Move->MovementMode; SavedCustomMode=Move->CustomMovementMode; bStopped=true; }
			Move->StopMovementImmediately(); Move->DisableMovement();
		}
		else if (bStopped)
		{
			Move->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode),SavedCustomMode); bStopped=false;
		}
		else if (State==ENeonCityBodyState::HitReaction) { Move->StopMovementImmediately(); }
	}
}
void UDamageableCharacterComponent::RestoreLocomotionMode(uint8 Mode,uint8 CustomMode)
{
	if (auto* Character=Cast<ACharacter>(GetOwner()))
	{
		const bool bLocked=bPhysicalLock || State==ENeonCityBodyState::Downed || State==ENeonCityBodyState::Dead;
		if (bLocked)
		{
			SavedMovementMode=Mode; SavedCustomMode=CustomMode; bStopped=true;
			Character->GetCharacterMovement()->DisableMovement();
		}
		else { Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(Mode),CustomMode); }
	}
}
void UDamageableCharacterComponent::Resolve()
{
	if (!Health) { return; }
	const float Fraction=Health->GetMaxHealth()>0 ? Health->GetCurrentHealth()/Health->GetMaxHealth() : 0;
	SetState(Health->IsDead() ? ENeonCityBodyState::Dead : Fraction<=.2f ? ENeonCityBodyState::Downed
		: Fraction<=.5f ? ENeonCityBodyState::Injured : ENeonCityBodyState::Normal);
}
void UDamageableCharacterComponent::HealthChanged(float Current,float Maximum)
{
	GetWorld()->GetTimerManager().ClearTimer(ReactionTimer); Resolve();
}
void UDamageableCharacterComponent::Damaged(const FNeonCityDamageEvent& Event)
{
	if (!Health || Health->IsDead() || State==ENeonCityBodyState::Downed) { Resolve(); return; }
	SetState(ENeonCityBodyState::HitReaction);
	GetWorld()->GetTimerManager().SetTimer(ReactionTimer,this,&UDamageableCharacterComponent::Resolve,.35f,false);
}
void UDamageableCharacterComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (Health) { Health->OnDamageTaken.RemoveAll(this); Health->OnHealthChanged.RemoveAll(this); }
	GetWorld()->GetTimerManager().ClearTimer(ReactionTimer); Super::EndPlay(Reason);
}
