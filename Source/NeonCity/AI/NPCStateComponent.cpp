#include "AI/NPCStateComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "GameFramework/Actor.h"
UNPCStateComponent::UNPCStateComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UNPCStateComponent::SetState(ENeonCityNPCState NewState)
{
	if (NewState == State || !StaticEnum<ENeonCityNPCState>()->IsValidEnumValue(static_cast<int64>(NewState))) { return; }
	if(NewState==ENeonCityNPCState::Fight)
 { const auto* Capability=GetOwner()?GetOwner()->FindComponentByClass<UNeonCityCombatAIComponent>():nullptr; if(!Capability || !Capability->CanFight()) return; }
 const ENeonCityNPCState Previous = State;
	State = NewState;
	StateSeconds = 0.f;
	VisitedStateMask |= 1u << static_cast<uint8>(State);
	OnStateChanged.Broadcast(Previous, State);
}
void UNPCStateComponent::AdvanceTime(float DeltaSeconds)
{
	if (FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f) { StateSeconds += DeltaSeconds; }
}
