#include "Wanted/SearchSystem.h"
#include "Wanted/WantedSubsystem.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "Police/PoliceObservation.h"
#include "Police/PoliceResponseData.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCThreatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
void USearchSystem::Initialize(FSubsystemCollectionBase& C) { Super::Initialize(C); Response=NewObject<UPoliceResponseData>(this); }
void USearchSystem::RegisterSensor(UPoliceSensorComponent* S) { if(S && S->GetWorld()==GetWorld()) Sensors.AddUnique(S); }
void USearchSystem::UnregisterSensor(UPoliceSensorComponent* S) { Sensors.Remove(S); Recognitions.RemoveAll([&](const auto& R){return R.Sensor.Get()==S;}); }
bool USearchSystem::CanSee(UPoliceSensorComponent* S,AActor* Target,FVector P) const
{
 if(!IsValid(S) || !S->CanObserve() || P.ContainsNaN()) return false;
 const FVector Eye=S->GetEyeLocation(),D=P-Eye;
 if(!FMath::IsFinite(S->SightRange) || !FMath::IsFinite(S->HalfAngleDegrees) || D.SizeSquared()>FMath::Square(FMath::Clamp(S->SightRange,100.f,5000.f))) return false;
 if(!D.IsNearlyZero() && FVector::DotProduct(D.GetSafeNormal(),S->GetViewDirection())<FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(S->HalfAngleDegrees,10.f,120.f)))) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceIncident),false,S->GetOwner()); Q.AddIgnoredActor(Target);
 return !GetWorld()->LineTraceTestByChannel(Eye,P,ECC_Visibility,Q);
}
bool USearchSystem::Observe(UPoliceSensorComponent* S,AActor* Candidate,FSuspectDescriptor& Evidence) const
{
 // The only candidate transform access is inside the visibility boundary; decisions only receive snapshots.
 if(!IsValid(S) || !S->CanObserve() || !NeonCityObservation::Visible(GetWorld(),S->GetOwner(),S->GetEyeLocation(),S->GetViewDirection(),S->SightRange,S->HalfAngleDegrees,Candidate)) return false;
 Evidence=NeonCityObservation::Capture(Candidate,S->bSurveillance?EPoliceEvidenceSource::Camera:EPoliceEvidenceSource::Police); return Evidence.IsValid();
}
void USearchSystem::Remember(FVector P,float T)
{ if(P.ContainsNaN() || !FMath::IsFinite(T) || T<LastEvidenceTime) return; LastKnownLocation=P; LastEvidenceTime=T; bHasLocation=true; }
void USearchSystem::RecordObservation(const FSuspectDescriptor& D,bool Visual)
{
 if(!D.IsValid() || !FMath::IsFinite(D.LastSeenTime) || !FMath::IsFinite(D.Confidence) || D.LastSeenTime>GetWorld()->GetTimeSeconds()+.1f || D.LastSeenTime<LastEvidenceTime) return;
 const auto Previous=Description; if(D.InVehicle && (!Previous.InVehicle || D.Vehicle.VehicleID!=Previous.Vehicle.VehicleID)) LastVehicleMotionTime=GetWorld()->GetTimeSeconds(); Description=D; Description.Confidence=FMath::Clamp(D.Confidence,0.f,1.f);
 if(Description.CharacterAppearance.IsNone()) { Description.CharacterAppearance=Previous.CharacterAppearance; Description.ClothingDescription=Previous.ClothingDescription; }
 if(!D.InVehicle && !D.Vehicle.IsKnown()) Description.Vehicle=Previous.Vehicle;
 if(D.InVehicle) VehicleLocation=D.LastKnownLocation;
 Remember(D.LastKnownLocation,D.LastSeenTime); UnidentifiedUntil=-1; if(Visual) bVisualContact=true;
}
bool USearchSystem::WitnessCrime(const FNeonCityCrimeEvent& Event)
{
 // The authoritative event supplies a candidate, never an already trusted location or identity.
 AActor* Candidate=Event.Instigator.IsValid()?Event.Instigator->GetPawn():nullptr;
 for(auto Sensor:Sensors)
 {
  FSuspectDescriptor D;
  if(Sensor.IsValid() && (Event.Victim.Get()==Sensor->GetOwner() || CanSee(Sensor.Get(),Event.Victim.Get(),Event.Location)) && Observe(Sensor.Get(),Candidate,D))
  { RecordObservation(D,true); return true; }
 }
 return false;
}
void USearchSystem::RecordReport(AController* Suspect,FVector Location,float Time)
{ if(IsValid(Suspect) && (!bHasLocation || Time>LastEvidenceTime)) Remember(Location,Time); UnidentifiedUntil=-1; }
bool USearchSystem::ObserveVehicleTransition(AActor* Person,AActor* Vehicle)
{
 bool Observed=false;
 if(Description.IsValid() && NeonCityObservation::Matches(Person,Description)) for(auto Sensor:Sensors)
 {
  FSuspectDescriptor P,V;
  if(Observe(Sensor.Get(),Person,P) && Observe(Sensor.Get(),Vehicle,V) && FVector::DistSquared(P.LastKnownLocation,V.LastKnownLocation)<FMath::Square(750.f))
  { V.CharacterAppearance=P.CharacterAppearance; V.ClothingDescription=P.ClothingDescription; RecordObservation(V,true); Observed=true; }
 }
 for(TActorIterator<ANeonCityNPCCharacter> It(GetWorld());It;++It) Observed|=It->GetThreatComponent()->ObserveVehicleTransition(Person,Vehicle);
 return Observed;
}
bool USearchSystem::ObserveVehicleExit(AActor* Person,AActor* Vehicle)
{
 bool Observed=false;
 if(Description.IsValid() && NeonCityObservation::Matches(Vehicle,Description)) for(auto Sensor:Sensors)
 {
  FSuspectDescriptor P,V;
  if(Observe(Sensor.Get(),Vehicle,V) && Observe(Sensor.Get(),Person,P) && FVector::DistSquared(P.LastKnownLocation,V.LastKnownLocation)<FMath::Square(750.f))
  { P.Vehicle=V.Vehicle; RecordObservation(P,true); Observed=true; }
 }
 for(TActorIterator<ANeonCityNPCCharacter> It(GetWorld());It;++It) Observed|=It->GetThreatComponent()->ObserveVehicleTransition(Person,Vehicle,false);
 return Observed;
}
void USearchSystem::UpdateSight(AController* AttributionOnly)
{
 Response=GetWorld()->GetSubsystem<UWantedSubsystem>()->GetResponseProfile();
 bVisualContact=false; const float Now=GetWorld()->GetTimeSeconds();
 Sensors.RemoveAll([](auto S){return !S.IsValid();});
 Recognitions.RemoveAll([&](const auto& R){return !R.Sensor.IsValid() || !R.Candidate.IsValid() || Now-R.Last>2.5f;});
 if(!Description.IsValid() || GetConfidence()<=.05f) return;
 bool SawPerson=false;
 for(auto Sensor:Sensors)
 {
  if(!Sensor.IsValid() || !Sensor->CanObserve()) continue;
  // Throttle expensive perception for far observers using evidence distance, never hidden suspect coordinates.
  const bool Far=FVector::DistSquared(Sensor->GetOwner()->GetActorLocation(),LastKnownLocation)>FMath::Square(3500.f);
  if(Far && Now<Sensor->NextObservationTime) continue;
  Sensor->NextObservationTime=Now+(Far?1.f:.2f);
  int32 Examined=0;
  for(TActorIterator<APawn> It(GetWorld());It && Examined<64;++It,++Examined)
  {
   if(!NeonCityObservation::Matches(*It,Description)) continue;
   FSuspectDescriptor D;
   if(!Observe(Sensor.Get(),*It,D)) continue;
   auto* R=Recognitions.FindByPredicate([&](const auto& V){return V.Sensor==Sensor && V.Candidate.Get()==*It;});
   if(!R) { if(Recognitions.Num()>=64) Recognitions.RemoveAt(0); Recognitions.Add({Sensor,*It,Now,Now}); R=&Recognitions.Last(); }
   if(Now-R->Last>(Far?1.5f:.35f)) R->Since=Now; R->Last=Now;
   const bool Continuous=(Now-Description.LastSeenTime<=1.f) && D.InVehicle==Description.InVehicle;
   if(!Continuous && Now-R->Since<(D.InVehicle?Response->KnownVehicleIdentificationSeconds:Response->PersonIdentificationSeconds)) continue;
   // A found parked/abandoned car updates the vehicle clue, but cannot hold the player's wanted forever.
   if(D.InVehicle)
   {
    VehicleLocation=D.LastKnownLocation;
    if(It->GetVelocity().Size2D()>20) LastVehicleMotionTime=Now;
    if(SawPerson || Now-LastVehicleMotionTime>1.f) continue;
   }
   else SawPerson=true;
   RecordObservation(D,true);
  }
 }
}
float USearchSystem::GetConfidence() const
{ return Description.IsValid()?FMath::Max(0.f,Description.Confidence-(GetWorld()->GetTimeSeconds()-Description.LastSeenTime)/Response->EvidenceMemorySeconds):0; }
float USearchSystem::GetDynamicSearchRadius(float Base) const
{ return FMath::Clamp(Base+(1-GetConfidence())*600+FMath::Max(0.f,GetWorld()->GetTimeSeconds()-LastEvidenceTime)*12,Base,3000.f); }
void USearchSystem::RecordUnidentified(FVector P,float Time)
{ if(!bHasLocation || Time>LastEvidenceTime) Remember(P,Time); UnidentifiedUntil=GetWorld()->GetTimeSeconds()+12; }
bool USearchSystem::HasUnidentifiedIncident() const { return GetWorld()->GetTimeSeconds()<UnidentifiedUntil; }
FVector USearchSystem::GetSearchPoint(int32 Index,float Radius) const
{ const float Angle=FMath::DegreesToRadians(FMath::Abs(Index)%16*137.5f); return LastKnownLocation+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*GetDynamicSearchRadius(Radius)*(.35f+.1f*(FMath::Abs(Index)%3)); }
void USearchSystem::Clear()
{ Description=FSuspectDescriptor(); Recognitions.Reset(); VehicleLocation=FVector::ZeroVector; bHasLocation=bVisualContact=false; LastEvidenceTime=UnidentifiedUntil=LastVehicleMotionTime=-1; LastKnownLocation=FVector::ZeroVector; for(auto S:Sensors) if(S.IsValid()) S->NextObservationTime=0; }

void USearchSystem::NotifyVehiclePossessed(AActor* Vehicle)
{
 // Possession is not evidence by itself. Only a still-visible matching person beside the car can establish an association.
 if(Description.InVehicle) return;
 for(TActorIterator<ANeonCityPlayerCharacter> It(GetWorld());It;++It)
  if(!It->IsHidden() && ObserveVehicleTransition(*It,Vehicle)) return;
}
