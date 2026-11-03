#include "Combat/CombatContextSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
UCombatContextSubsystem::FEpisode* UCombatContextSubsystem::Find(AActor* A,AActor* B)
{
 const float Now=GetWorld()->GetTimeSeconds();
 Episodes.RemoveAll([&](const auto& E){ return !E.Aggressor.IsValid() || !E.Defender.IsValid() || E.Expires<Now; });
 return Episodes.FindByPredicate([&](const auto& E){ return (E.Aggressor.Get()==A && E.Defender.Get()==B) || (E.Aggressor.Get()==B && E.Defender.Get()==A); });
}
void UCombatContextSubsystem::RecordAggression(AActor* Attacker,AActor* Victim)
{
 if(!IsValid(Attacker) || !IsValid(Victim) || Attacker==Victim || Attacker->GetWorld()!=GetWorld() || Victim->GetWorld()!=GetWorld()) return;
 if(auto* E=Find(Attacker,Victim)) { E->Expires=GetWorld()->GetTimeSeconds()+20; return; }
 if(Episodes.Num()>=128) Episodes.RemoveAt(0);
 Episodes.Add({FGuid::NewGuid(),Attacker,Victim,float(GetWorld()->GetTimeSeconds()+20)});
}
void UCombatContextSubsystem::AnnotateDamage(AActor* Victim,FNeonCityDamageEvent& Event)
{
 // Authoritative context replaces caller-provided annotations; admin/environment changes are not aggression.
 Event.CombatContextID.Invalidate(); Event.OriginalAggressor.Reset(); Event.Disposition=ENeonCityAttackDisposition::Unspecified;
 if(Event.DamageType!=ENeonCityDamageType::Ballistic && Event.DamageType!=ENeonCityDamageType::Impact && Event.DamageType!=ENeonCityDamageType::Explosion) return;
 AActor* Attacker=IsValid(Event.Instigator)?Event.Instigator->GetPawn():nullptr;
 RecordAggression(Attacker,Victim);
 if(auto* E=Find(Attacker,Victim))
 {
  Event.CombatContextID=E->Id; Event.OriginalAggressor=E->Aggressor;
  Event.Disposition=E->Aggressor.Get()==Attacker?ENeonCityAttackDisposition::InitiatingAttack:ENeonCityAttackDisposition::ResponseToAggression;
 }
}
void UCombatContextSubsystem::ResetForLoad() { Episodes.Reset(); OnReset.Broadcast(); }
