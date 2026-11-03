#include "AI/NPCThreatComponent.h"
#include "Police/PoliceObservation.h"
#include "Wanted/SearchSystem.h"
#include "Wanted/WantedSubsystem.h"
#include "TimerManager.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCData.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Crime/CrimeSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
UNPCThreatComponent::UNPCThreatComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UNPCThreatComponent::BeginPlay()
{
 Super::BeginPlay(); NPC=Cast<ANeonCityNPCCharacter>(GetOwner());
 if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->OnHealthChanged.AddDynamic(this,&UNPCThreatComponent::WitnessHealthChanged);
 if(auto* P=GetOwner()->FindComponentByClass<UCharacterImpactComponent>()) P->OnStateChanged.AddDynamic(this,&UNPCThreatComponent::PhysicalStateChanged);
 if (NPC.IsValid() && NPC->HasAuthority()) { GetWorld()->GetSubsystem<UThreatSubsystem>()->OnThreat.AddDynamic(this,&UNPCThreatComponent::Perceive); }
}
void UNPCThreatComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if (auto* Threat=GetWorld()->GetSubsystem<UThreatSubsystem>()) { Threat->OnThreat.RemoveAll(this); }
 if(auto* H=GetOwner()->FindComponentByClass<UHealthComponent>()) H->OnHealthChanged.RemoveAll(this);
 if(auto* P=GetOwner()->FindComponentByClass<UCharacterImpactComponent>()) P->OnStateChanged.RemoveAll(this);
 CancelPendingReports(); Super::EndPlay(Reason);
}
bool UNPCThreatComponent::HasPerceived(FGuid Id) const { return Memory.ContainsByPredicate([&](const FMemory& M) { return M.Id==Id; }); }
bool UNPCThreatComponent::IdentifiedInstigator(FGuid Id) const
{
 const auto* M=Memory.FindByPredicate([&](const FMemory& Item) { return Item.Id==Id; }); return M && M->bIdentified;
}
bool UNPCThreatComponent::CanSee(FVector Point,AActor* Target) const
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCThreatSight),false,GetOwner()); Query.AddIgnoredActor(Target);
 const FVector Eyes=NPC->GetPawnViewLocation();
 return !GetWorld()->LineTraceTestByChannel(Eyes,Point,ECC_Visibility,Query);
}
void UNPCThreatComponent::Perceive(const FNeonCityThreatEvent& Event)
{
 if (!NPC.IsValid() || NPC->IsPopulationDormant() || (Event.Instigator.IsValid() && Event.Instigator.Get()==NPC->GetController())) { return; }
 auto* Body=NPC->FindComponentByClass<UDamageableCharacterComponent>();
 if (!Body || Body->GetBodyState()==ENeonCityBodyState::Downed || Body->GetBodyState()==ENeonCityBodyState::Dead) { return; }
 const bool bDirect=Event.Victim.Get()==NPC.Get();
 const float Distance=FVector::Dist(NPC->GetActorLocation(),Event.Location);
 // A blast is heard through cover; gunshots and blasts are the two distance-only stimuli.
 if (!bDirect && (Distance>Event.Radius || ((Event.Type!=ENeonCityThreatType::Gunshot && Event.Type!=ENeonCityThreatType::Explosion)
  && !CanSee(Event.Location,Event.Victim.IsValid() ? Event.Victim.Get() : Event.SourceActor.Get())))) { return; }
 AActor* Culprit=Event.Instigator.IsValid() ? Event.Instigator->GetPawn() : nullptr;
 const bool bIdentified=Culprit && FVector::DistSquared(Culprit->GetActorLocation(),NPC->GetActorLocation())<FMath::Square(1200.f)
  && CanSee(Culprit->GetActorLocation(),Culprit);
 if (Memory.Num()>=32)
 {
  const int32 Oldest=Memory.IndexOfByPredicate([&](const FMemory& M) { return !bActive || M.Id!=Incident.Id; });
  Memory.RemoveAt(Oldest==INDEX_NONE ? 0 : Oldest);
 }
 FMemory Observed; Observed.Id=Event.Id; Observed.bIdentified=bIdentified;
 if(bIdentified) { Observed.Observation=NeonCityObservation::Capture(Culprit,EPoliceEvidenceSource::Witness); Observed.Observation.Confidence=.8f; KnownTransitionDescription=Observed.Observation; }
 Memory.Add(Observed);
 const float Now=GetWorld()->GetTimeSeconds();
 if(Event.Type==ENeonCityThreatType::Gunshot)
 {
  if(Now-LastThreatTime>2.f || RecentGunshots==0) { RecentGunshots=0; FirstGunshotTime=Now; }
  ++RecentGunshots;
 }
 LastThreatTime=Now;
 const bool Sustained=RecentGunshots>=3 && Now-FirstGunshotTime>1.f;
 const float Cooldown=Sustained?NPC->GetNPCData()->SustainedDangerCooldown:NPC->GetNPCData()->DangerCooldown;
 DangerUntil=FMath::Max(DangerUntil,Now+(FMath::IsFinite(Cooldown)?FMath::Clamp(Cooldown,6.f,30.f):6.f));
 // Armed actors retain witness memory, but the shared combat adapter owns their response.
 if(NPC->GetCombatAI()->CanFight()) return;
 // Preserve the first incident during an episode; direct injury takes precedence over distant noise.
 if (!bActive || bDirect) { Incident=Event; }
 DangerLocation=bDirect && Event.SourceActor.IsValid() ? Event.SourceActor->GetActorLocation() : Event.Location;
 bReplan=true;
 if (!bActive || NPC->GetNPCState()->GetState()==ENeonCityNPCState::Recover)
 {
  Incident=Event; NPC->GetNPCState()->SetState(ENeonCityNPCState::Alert);
  NPC->GetCharacterMovement()->StopMovementImmediately();
 }
 bActive=true;
}
bool UNPCThreatComponent::SafeSegment(FVector Start,FVector End) const
{
 const auto* Capsule=NPC->GetCapsuleComponent();
 const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-3.f);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCFlee),false,NPC.Get());
 FHitResult Hit;
 if (GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,Shape,Query)) { return false; }
 // Sample the entire segment, not just its end, so a clear trace cannot send a civilian over a gap.
 const int32 Steps=FMath::CeilToInt(FVector::Dist2D(Start,End)/75.f);
 for (int32 I=1; I<=Steps; ++I)
 {
  const FVector Sample=FMath::Lerp(Start,End,float(I)/Steps);
  if (!GetWorld()->LineTraceSingleByChannel(Hit,Sample+FVector(0,0,30),Sample-FVector(0,0,140),ECC_Visibility,Query)
   || Hit.ImpactNormal.Z<NPC->GetCharacterMovement()->GetWalkableFloorZ()
   || FMath::Abs(Hit.ImpactPoint.Z+Capsule->GetScaledCapsuleHalfHeight()-Sample.Z)>18.f) { return false; }
 }
 return true;
}
bool UNPCThreatComponent::FindSafeFleePoint(FVector& Point) const
{
 const FVector Start=NPC->GetActorLocation();
 const float Initial=FVector::Dist2D(Start,DangerLocation);
 float Best=Initial+50.f; bool bFound=false;
 for (FVector RoutePoint:NPC->GetRoute())
 {
  RoutePoint.Z=Start.Z;
  const FVector Delta=RoutePoint-Start;
  for (float Limit:{650.f,350.f,150.f})
  {
   const FVector Candidate=Start+Delta.GetClampedToMaxSize(Limit);
   const float Distance=FVector::Dist2D(Candidate,DangerLocation);
   if (Distance>Best && SafeSegment(Start,Candidate)) { Point=Candidate; Best=Distance; bFound=true; }
  }
 }
 return bFound;
}
void UNPCThreatComponent::LookAtDanger(float DeltaSeconds)
{
 const FRotator Desired(0,(DangerLocation-NPC->GetActorLocation()).Rotation().Yaw,0);
 NPC->SetActorRotation(FMath::RInterpConstantTo(NPC->GetActorRotation(),Desired,DeltaSeconds,180));
}
bool UNPCThreatComponent::UpdateReaction(float DeltaSeconds)
{
 if (!bActive || !NPC.IsValid()) { return false; }
 auto* State=NPC->GetNPCState(); auto* Move=NPC->GetCharacterMovement();
 switch (State->GetState())
 {
 case ENeonCityNPCState::Alert:
  LookAtDanger(DeltaSeconds);
  if (State->GetStateSeconds()>.4f)
  {
   bReplan=false;
   State->SetState(FindSafeFleePoint(FleePoint) ? ENeonCityNPCState::Flee : ENeonCityNPCState::CallHelp);
  }
  break;
 case ENeonCityNPCState::Flee:
 {
  if (bReplan && GetWorld()->GetTimeSeconds()>=NextFleePlan) { NextFleePlan=GetWorld()->GetTimeSeconds()+.5f; bReplan=false; if (!FindSafeFleePoint(FleePoint)) { State->SetState(ENeonCityNPCState::CallHelp); break; } }
  Move->MaxWalkSpeed=320.f;
  const FVector Delta=(FleePoint-NPC->GetActorLocation())*FVector(1,1,0);
  if (Delta.Size()<40 || State->GetStateSeconds()>3.f)
  { Move->StopMovementImmediately(); State->SetState(ENeonCityNPCState::CallHelp); }
  else { NPC->AddMovementInput(Delta.GetSafeNormal()); }
  break;
 }
 case ENeonCityNPCState::Fight:
  // Reserved extension state: civilian profiles never attack or equip weapons.
  State->SetState(ENeonCityNPCState::CallHelp); break;
 case ENeonCityNPCState::CallHelp:
  Move->StopMovementImmediately(); LookAtDanger(DeltaSeconds);
  if (State->GetStateSeconds()>.8f)
  { if(GetWorld()->GetSubsystem<UCrimeSubsystem>()->ReportEvent(Incident.Id,NPC.Get())) SubmitTransition(); State->SetState(ENeonCityNPCState::Recover); }
  break;
 case ENeonCityNPCState::Recover:
  Move->StopMovementImmediately(); LookAtDanger(DeltaSeconds);
  if(bReplan && GetWorld()->GetTimeSeconds()<DangerUntil && GetWorld()->GetTimeSeconds()>=NextFleePlan)
  { NextFleePlan=GetWorld()->GetTimeSeconds()+2.f; bReplan=false; if(FindSafeFleePoint(FleePoint)) { State->SetState(ENeonCityNPCState::Flee); break; } }
  if (GetWorld()->GetTimeSeconds()>DangerUntil)
  {
   bActive=false; Move->MaxWalkSpeed=FMath::Clamp(NPC->GetNPCData()->WalkSpeed,60.f,220.f);
   State->SetState(ENeonCityNPCState::Return);
  }
  break;
 default: State->SetState(ENeonCityNPCState::Alert); break;
 }
 return true;
}

void UNPCThreatComponent::ClearResponse() { bActive=false; bReplan=false; Incident=FNeonCityThreatEvent(); CancelPendingReports(); }

FSuspectDescriptor UNPCThreatComponent::GetObservation(FGuid Id) const
{ const auto* M=Memory.FindByPredicate([&](const FMemory& V){return V.Id==Id;}); return M?M->Observation:FSuspectDescriptor(); }

void UNPCThreatComponent::WitnessHealthChanged(float Current,float Maximum)
{ if(Current<=Maximum*.2f) { CancelPendingReports(); } }
void UNPCThreatComponent::PhysicalStateChanged(ECharacterImpactState S)
{ if(S==ECharacterImpactState::Knockdown || S==ECharacterImpactState::Downed || S==ECharacterImpactState::Dead) { CancelPendingReports(); } }

void UNPCThreatComponent::CancelPendingReports()
{ Memory.Reset(); Incident.Id.Invalidate(); PendingTransition=KnownTransitionDescription=FSuspectDescriptor(); GetWorld()->GetTimerManager().ClearTimer(TransitionTimer); }
bool UNPCThreatComponent::ObserveVehicleTransition(AActor* Person,AActor* Vehicle,bool Entering)
{
 if(!NPC.IsValid() || NPC->IsPopulationDormant() || !IsValid(Person) || !IsValid(Vehicle) || !GetOwner()->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()) return false;
 if(Person->IsHidden() || FVector::DistSquared(Person->GetActorLocation(),Vehicle->GetActorLocation())>FMath::Square(750.f)) return false;
 const auto Car=NeonCityObservation::Capture(Vehicle,EPoliceEvidenceSource::Witness);
 if(!Car.Vehicle.IsKnown() || !(KnownTransitionDescription.IsValid() && (Entering?!KnownTransitionDescription.CharacterAppearance.IsNone():KnownTransitionDescription.Vehicle.VehicleID==Car.Vehicle.VehicleID))) return false;
 if(FVector::DistSquared(NPC->GetActorLocation(),Person->GetActorLocation())>FMath::Square(1200.f) || !CanSee(Person->GetActorLocation(),Person) || !CanSee(Vehicle->GetActorLocation()+FVector(0,0,45),Vehicle)) return false;
 PendingTransition=Entering?Car:NeonCityObservation::Capture(Person,EPoliceEvidenceSource::Witness); PendingTransition.Vehicle=Car.Vehicle;
 PendingTransition.CharacterAppearance=NeonCityObservation::Capture(Person,EPoliceEvidenceSource::Witness).CharacterAppearance; PendingTransition.Confidence=.8f; KnownTransitionDescription=PendingTransition;
 GetWorld()->GetTimerManager().SetTimer(TransitionTimer,this,&UNPCThreatComponent::SubmitTransition,1.f,false); return true;
}
bool UNPCThreatComponent::CanSuspendPopulation() const
{
 const bool PendingFresh=PendingTransition.IsValid() && GetWorld()->GetTimeSeconds()-PendingTransition.LastSeenTime<=10.f;
 return !bActive && !PendingFresh && !GetWorld()->GetTimerManager().IsTimerActive(TransitionTimer);
}
void UNPCThreatComponent::SubmitTransition()
{
 if(!PendingTransition.IsValid()) return;
 if(GetWorld()->GetTimeSeconds()-PendingTransition.LastSeenTime>10 || !GetOwner()->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()) { PendingTransition=FSuspectDescriptor(); return; }
 // A transition seen before the initial phone call waits for that call to establish the crime.
 if(GetWorld()->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()==0) return;
 GetWorld()->GetSubsystem<USearchSystem>()->RecordObservation(PendingTransition); PendingTransition=FSuspectDescriptor();
}
