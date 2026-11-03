#include "AI/Combat/CombatTargetComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "GameFramework/Pawn.h"
ENeonCityFaction UCombatTargetComponent::GetFaction(const AActor* Actor)
{
 if(!IsValid(Actor)) return ENeonCityFaction::Neutral;
 if(const auto* Identity=Actor->FindComponentByClass<UCombatTargetComponent>()) return Identity->Faction;
 if(const auto* Pawn=Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled()) return ENeonCityFaction::Player;
 return ENeonCityFaction::Neutral;
}
bool UCombatTargetComponent::AreFriendly(const AActor* A,const AActor* B)
{ const auto Team=GetFaction(A); return A==B || (Team!=ENeonCityFaction::Neutral && Team==GetFaction(B)); }
bool UCombatTargetComponent::IsAliveTarget(const AActor* Actor)
{
 if(!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return false;
 const auto* Health=Actor->FindComponentByClass<UHealthComponent>();
 const auto* Body=Actor->FindComponentByClass<UDamageableCharacterComponent>();
 return Health && !Health->IsDead() && (!Body || (Body->GetBodyState()!=ENeonCityBodyState::Downed && Body->GetBodyState()!=ENeonCityBodyState::Dead));
}
