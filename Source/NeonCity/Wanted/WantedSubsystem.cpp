#include "Wanted/WantedSubsystem.h"
#include "Wanted/WantedLevelData.h"
#include "Police/PoliceResponseData.h"
#include "Police/PoliceDispatchSubsystem.h"
#include "Wanted/SearchSystem.h"
#include "Crime/CrimeSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
void UWantedSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UCrimeSubsystem>(); Collection.InitializeDependency<USearchSystem>();
 Response=NewObject<UPoliceResponseData>(this);
 Profile=LoadObject<UWantedLevelData>(nullptr,TEXT("/Game/NeonCity/Wanted/DA_WantedLevels.DA_WantedLevels"));
 if (!Profile || !Profile->IsValidProfile()) { Profile=NewObject<UWantedLevelData>(this); }
 auto* Crime=GetWorld()->GetSubsystem<UCrimeSubsystem>();
 Crime->OnCrimeRecorded.AddDynamic(this,&UWantedSubsystem::CrimeRecorded);
 Crime->OnReportEvent.AddDynamic(this,&UWantedSubsystem::ReportReceived);
}
void UWantedSubsystem::Deinitialize()
{
 if (auto* Crime=GetWorld()->GetSubsystem<UCrimeSubsystem>()) { Crime->OnCrimeRecorded.RemoveAll(this); Crime->OnReportEvent.RemoveAll(this); }
 Evaluated.Reset(); Suspect.Reset(); Super::Deinitialize();
}
bool UWantedSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game || Type==EWorldType::PIE; }
bool UWantedSubsystem::IsTickable() const { return !IsTemplate() && GetWorld() && GetWorld()->HasBegunPlay() && GetWorld()->GetNetMode()!=NM_Client; }
void UWantedSubsystem::CrimeRecorded(const FNeonCityCrimeEvent& Event)
{
 if (GetWorld()->GetNetMode()==NM_Client || !Event.Instigator.IsValid() || !Event.Instigator->IsPlayerController()) { return; }
 if (GetWorld()->GetSubsystem<USearchSystem>()->WitnessCrime(Event)) { Confirm(Event); }
}
void UWantedSubsystem::ReportReceived(const FNeonCityReportEvent& Report)
{
 if (GetWorld()->GetNetMode()==NM_Client) { return; }
 auto* Crime=GetWorld()->GetSubsystem<UCrimeSubsystem>();
 const auto* Event=Crime->GetEvents().FindByPredicate([&](const FNeonCityCrimeEvent& E) { return E.Id==Report.CrimeId; });
 if (!Event) { return; }
 if (!Report.Instigator.IsValid())
 {
  if (Level==0) { GetWorld()->GetSubsystem<USearchSystem>()->RecordUnidentified(Report.Location,Event->Timestamp); }
  return;
 }
 if (Report.Instigator->IsPlayerController() && Report.Instigator==Event->Instigator) { GetWorld()->GetSubsystem<USearchSystem>()->RecordObservation(Report.Observation); Confirm(*Event); }
}
void UWantedSubsystem::Confirm(const FNeonCityCrimeEvent& Event)
{
 if (Evaluated.Contains(Event.Id) || !Event.Instigator.IsValid()) { return; }
 // Limited context rule: retaliation against the actual aggressor is not a new initiating assault.
 // Public gunfire and harm to unrelated civilians still follow the existing evaluation.
 if(Event.Disposition==ENeonCityAttackDisposition::ResponseToAggression && Event.Victim.IsValid() && Event.Victim==Event.OriginalAggressor) return;
 if (Evaluated.Num()>=256) { Evaluated.RemoveAt(0); } Evaluated.Add(Event.Id);
 const float Now=GetWorld()->GetTimeSeconds();
 const bool bThreat=Event.Offense==EPoliceOffense::Attention || Event.Offense==EPoliceOffense::WeaponAttack;
 // A hit and its gunshot do not double-charge one burst; additional serious attacks remain distinct.
 if (bThreat && (Now-LastThreatScoreTime<2.f || Now-LastSevereTime<2.f)) { return; }
 if (bThreat) { LastThreatScoreTime=Now; } else { LastSevereTime=Now; }
 Suspect=Event.Instigator;
 GetWorld()->GetSubsystem<USearchSystem>()->RecordReport(Suspect.Get(),Event.Location,Event.Timestamp);
 const int32 Weight=Response->GetSeverity(Event.Offense,Event.DamageAmount);
 Score=FMath::Min(Score+Weight,Profile->Levels.Last().EvidenceThreshold);
 int32 NewLevel=0; for (int32 I=1; I<6; ++I) { if (Score>=Profile->Levels[I].EvidenceThreshold) { NewLevel=I; } }
 NextDecay=Now+Profile->LostSightGrace+Profile->DecayInterval; SetLevel(NewLevel);
}
void UWantedSubsystem::SetLevel(int32 NewLevel)
{
 NewLevel=FMath::Clamp(NewLevel,0,5); if (Level==NewLevel) { return; }
 const int32 Old=Level; Level=NewLevel;
 const FVector Known=GetWorld()->GetSubsystem<USearchSystem>()->GetLastKnownLocation();
 OnWantedLevelChanged.Broadcast(Old,Level);
 if (Old<3 && Level>=3) { OnRoadblockRequested.Broadcast(Level,Known); }
 if (Old<4 && Level>=4) { OnSpecialUnitRequested.Broadcast(Level,Known); }
 if (Old<5 && Level>=5) { OnHighIntensityRequested.Broadcast(Level,Known); }
}
void UWantedSubsystem::Tick(float DeltaSeconds)
{
 TickSeconds+=DeltaSeconds; if (TickSeconds<.2f) { return; } TickSeconds=0;
 auto* Search=GetWorld()->GetSubsystem<USearchSystem>(); const float Now=GetWorld()->GetTimeSeconds();
 if (Level==0) { if (Search->HasLocation() && !Search->HasUnidentifiedIncident()) { Search->Clear(); } return; }
 Search->UpdateSight(Suspect.Get());
 if (Search->HasVisualContact()) { NextDecay=Now+Profile->LostSightGrace+Profile->DecayInterval; }
 else if (Now>=NextDecay)
 {
  Score=Profile->Levels[Level-1].EvidenceThreshold; SetLevel(Level-1); NextDecay=Now+Profile->DecayInterval;
  if (Level==0) { Suspect.Reset(); Search->Clear(); }
 }
}
int32 UWantedSubsystem::GetVehicleBudget() const { return Profile ? Profile->Levels[Level].VehicleCount : 0; }
float UWantedSubsystem::GetSearchRadius() const { return Response ? Response->Levels[Level].SearchRadius : 600; }

void UWantedSubsystem::ResetForLoad()
{
 GetWorld()->GetSubsystem<UPoliceDispatchSubsystem>()->ResetForLoad();
 Suspect.Reset(); Evaluated.Reset(); Score=0; NextDecay=0; LastThreatScoreTime=LastSevereTime=-100; TickSeconds=0;
 GetWorld()->GetSubsystem<USearchSystem>()->Clear(); SetLevel(0);
}

bool UWantedSubsystem::ConfigureResponse(UPoliceResponseData* Data) { if(!IsValid(Data) || !Data->IsValidProfile()) return false; Response=Data; return true; }
