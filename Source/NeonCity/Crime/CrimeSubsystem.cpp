#include "Crime/CrimeSubsystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/HealthComponent.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Physics/DestructiblePropComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "AI/NPCInterface.h"
#include "AI/NPCThreatComponent.h"
#include "Vehicles/VehicleInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
void UCrimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UThreatSubsystem>();
 GetWorld()->GetSubsystem<UThreatSubsystem>()->OnThreat.AddDynamic(this,&UCrimeSubsystem::RecordThreat);
}
void UCrimeSubsystem::Deinitialize()
{
 if (auto* Threat=GetWorld()->GetSubsystem<UThreatSubsystem>()) { Threat->OnThreat.RemoveAll(this); }
 Events.Reset(); Reports.Reset(); ReportedWitnesses.Reset(); Super::Deinitialize();
}
void UCrimeSubsystem::RecordThreat(const FNeonCityThreatEvent& Threat)
{
 FNeonCityCrimeEvent Event; Event.Id=Threat.Id; Event.Location=Threat.Location; Event.Timestamp=Threat.Time;
 Event.DamageAmount=Threat.DamageAmount;
 Event.Offense=Threat.Type==ENeonCityThreatType::Gunshot?EPoliceOffense::WeaponAttack:EPoliceOffense::Attention;
 Event.Instigator=Threat.Instigator; Event.Victim=Threat.Victim;
 Event.CombatContextID=Threat.CombatContextID; Event.Disposition=Threat.Disposition; Event.OriginalAggressor=Threat.OriginalAggressor;
 AActor* Victim=Threat.Victim.Get();
 const bool bAttack=Threat.Type==ENeonCityThreatType::Attack
  && (Threat.DamageType==ENeonCityDamageType::Ballistic || Threat.DamageType==ENeonCityDamageType::Impact || Threat.DamageType==ENeonCityDamageType::Explosion);
 // An unattributed blast is public danger even before the per-victim attack records arrive.
 if (Threat.Type==ENeonCityThreatType::Explosion && !Victim) { Event.CrimeType=ENeonCityCrimeType::ThreatEvent; Event.Offense=EPoliceOffense::PublicDanger; }
 if (bAttack && Victim)
 {
  if(Victim->FindComponentByClass<UDestructiblePropComponent>()) { Event.CrimeType=ENeonCityCrimeType::PropertyDamage; }
  else if (Victim->Implements<UNPCInterface>()) { Event.CrimeType=ENeonCityCrimeType::DamageNPC; }
  else if (auto* Health=Victim->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead() && Victim->Implements<UVehicleInterface>())
  { Event.CrimeType=ENeonCityCrimeType::DestroyVehicle; }
 }
 if(bAttack)
 {
  Event.Offense=Event.CrimeType==ENeonCityCrimeType::PropertyDamage?EPoliceOffense::PropertyDamage:Event.CrimeType==ENeonCityCrimeType::DestroyVehicle?EPoliceOffense::VehicleDestruction:Threat.DamageType==ENeonCityDamageType::Impact?EPoliceOffense::VehicleHit:EPoliceOffense::Assault;
  if(Victim && UCombatTargetComponent::GetFaction(Victim)==ENeonCityFaction::Police) Event.Offense=EPoliceOffense::PoliceAttack;
 }
 if (Events.Num()>=Capacity) { ReportedWitnesses.Remove(Events[0].Id); Events.RemoveAt(0); }
 Events.Add(Event); OnCrimeRecorded.Broadcast(Event);
}
bool UCrimeSubsystem::ReportEvent(FGuid CrimeId,AActor* Witness)
{
 if (GetWorld()->GetNetMode()==NM_Client || !IsValid(Witness) || Witness->GetWorld()!=GetWorld() || !Witness->Implements<UNPCInterface>()) { return false; }
 auto* Body=Witness->FindComponentByClass<UDamageableCharacterComponent>();
 auto* Perception=Witness->FindComponentByClass<UNPCThreatComponent>();
 auto* Event=Events.FindByPredicate([&](const FNeonCityCrimeEvent& E) { return E.Id==CrimeId; });
 if (!Event || !Body || !Body->CanAct() || !Perception || !Perception->HasPerceived(CrimeId)) { return false; }
 auto& Witnesses=ReportedWitnesses.FindOrAdd(CrimeId);
 if (Witnesses.Num()>=64 || Witnesses.Contains(Witness)) { return false; }
 Witnesses.Add(Witness);
 if (!Event->Witness.IsValid()) { Event->Witness=Witness; }
 FNeonCityReportEvent Report; Report.CombatContextID=Event->CombatContextID; Report.Disposition=Event->Disposition; Report.OriginalAggressor=Event->OriginalAggressor; Report.Offense=Event->Offense; Report.DamageAmount=Event->DamageAmount; Report.Observation=Perception->GetObservation(CrimeId); Report.CrimeId=Event->Id; Report.CrimeType=Event->CrimeType;
 Report.Location=Report.Observation.IsValid()?Report.Observation.LastKnownLocation:Event->Location; Report.Timestamp=GetWorld()->GetTimeSeconds(); Report.Witness=Witness;
 if (Perception->IdentifiedInstigator(CrimeId)) { Report.Instigator=Event->Instigator; }
 if (Reports.Num()>=Capacity) { Reports.RemoveAt(0); }
 Reports.Add(Report); OnReportEvent.Broadcast(Report); return true;
}

void UCrimeSubsystem::ResetForLoad() { Events.Reset(); Reports.Reset(); ReportedWitnesses.Reset(); }
