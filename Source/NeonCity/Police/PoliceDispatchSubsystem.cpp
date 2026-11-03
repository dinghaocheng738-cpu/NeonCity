#include "Police/PoliceDispatchSubsystem.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Police/PoliceUnitManager.h"
#include "Police/PoliceResponseData.h"
#include "Police/PoliceObservation.h"
#include "Police/RoadblockManager.h"
#include "AI/Police/PoliceResponseDirector.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceVehicle.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Wanted/WantedSubsystem.h"
#include "Wanted/SearchSystem.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
void UPoliceDispatchSubsystem::Initialize(FSubsystemCollectionBase& C)
{ Super::Initialize(C); C.InitializeDependency<UWantedSubsystem>(); C.InitializeDependency<URoadblockManager>(); Profile=NewObject<UPoliceResponseData>(this); Units=NewObject<UPoliceUnitManager>(this); }
bool UPoliceDispatchSubsystem::DoesSupportWorldType(EWorldType::Type T) const { return T==EWorldType::Game || T==EWorldType::PIE; }
bool UPoliceDispatchSubsystem::IsTickable() const { return !IsTemplate() && GetWorld() && GetWorld()->HasBegunPlay() && GetWorld()->GetNetMode()!=NM_Client; }
void UPoliceDispatchSubsystem::RegisterStation(APoliceResponseDirector* S)
{
 if(!IsValid(S) || Station.IsValid()) return; Station=S;
 FActorSpawnParameters P; P.Owner=S; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
 if(auto* Officer=GetWorld()->SpawnActor<APoliceCharacter>(S->FootPatrolOrigin,FRotator::ZeroRotator,P)) { Officer->SetDispatchSlot(0); Units->Officers.Add(Officer); }
}
void UPoliceDispatchSubsystem::UnregisterStation(APoliceResponseDirector* S) { if(Station.Get()==S) { ResetForLoad(); for(auto O:Units->Officers) if(IsValid(O)) O->Destroy(); Units->Officers.Reset(); Station.Reset(); } }
int32 UPoliceDispatchSubsystem::GetActiveOfficers() const { int32 N=0; if(Units) for(auto O:Units->Officers) if(IsValid(O) && O->CanRespond() && O->IsResponseEnabled()) ++N; return N; }
int32 UPoliceDispatchSubsystem::GetActiveVehicles() const { int32 N=0; if(Units) for(auto V:Units->Vehicles) if(IsValid(V) && V->IsResponding() && V->FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()>0) ++N; return N; }
void UPoliceDispatchSubsystem::Tick(float Delta)
{
 Accumulator+=Delta; if(Accumulator<.25f) return; Accumulator=0;
 Profile=GetWorld()->GetSubsystem<UWantedSubsystem>()->GetResponseProfile();
 if(!Station.IsValid() || !IsValid(Station->Road) || !Profile->IsValidProfile()) return;
 Units->Officers.RemoveAll([](auto O){return !IsValid(O);}); Units->Vehicles.RemoveAll([](auto V){return !IsValid(V);});
 auto* Wanted=GetWorld()->GetSubsystem<UWantedSubsystem>(); auto* Search=GetWorld()->GetSubsystem<USearchSystem>(); auto* Blocks=GetWorld()->GetSubsystem<URoadblockManager>();
 const int32 Level=Wanted->GetWantedLevel(); const auto Budget=Profile->Levels[Level]; const float Now=GetWorld()->GetTimeSeconds();
 if(Level!=LastLevel) { if(Level>LastLevel) NextSpawn=FMath::Min(NextSpawn,Now); LastLevel=Level; OnPoliceAlert.Broadcast(Search->GetDescriptor()); }
 Blocks->TrimTo(Budget.Roadblocks);
 int32 AssignedOfficers=0;
 for(auto O:Units->Officers)
 {
  O->SetResponseEnabled(O->CanRespond() && AssignedOfficers<Budget.Officers-Blocks->GetCount()); if(O->IsResponseEnabled()) ++AssignedOfficers;
  const bool Far=Search->HasLocation() && FVector::DistSquared(O->GetActorLocation(),Search->GetLastKnownLocation())>FMath::Square(4500.f);
  O->GetCombatAI()->SetDistantLOD(Far); if(O->GetController()) O->GetController()->SetActorTickInterval(Far?.5f:0.f);
 }
 int32 AssignedCars=0;
 for(auto V:Units->Vehicles) { V->SetResponding(Level>=2 && V->FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()>0 && AssignedCars<Budget.Vehicles-Budget.Roadblocks); if(V->IsResponding()) ++AssignedCars; V->SetResponseSpeed(Budget.ChaseSpeed); V->SetActorTickInterval(Search->HasLocation() && FVector::Dist2D(V->GetActorLocation(),Search->GetLastKnownLocation())>5000?.3f:0.f); }
 if(Level==0) return;
 if(Now>=NextRoadblock && Blocks->GetCount()<Budget.Roadblocks && GetActiveVehicles()+Blocks->GetCount()<Budget.Vehicles)
 { Blocks->TryDeploy(Station->Road,Budget.Roadblocks); NextRoadblock=Now+3; }
 if(Now<NextSpawn) return; NextSpawn=Now+Profile->ReplacementInterval;
 // Finite slots include disabled wrecks. Replacement is bounded; no infinite spawning under repeated attacks.
 if(GetActiveOfficers()+Blocks->GetCount()<Budget.Officers && Units->Officers.Num()<8)
 {
  for(int32 I=0;I<4;++I)
  {
   const FVector P=Station->FootPatrolOrigin+FVector((NextUnitSlot%4)*150,I*120,0);
   if(!NeonCityObservation::SafeDeployment(GetWorld(),P,1200)) continue;
   FActorSpawnParameters Params; Params.Owner=Station.Get(); Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
   if(auto* O=GetWorld()->SpawnActor<APoliceCharacter>(P,FRotator::ZeroRotator,Params))
   {
    O->SetDispatchSlot(NextUnitSlot++);
    if(Profile->EnableRifleResponse && Level>=Profile->RifleMinimumLevel && (Units->Officers.Num()+1)%Profile->RifleEveryNthOfficer==0)
     O->GetCombatAI()->ConfigureWeapon(ANeonCityRifle::StaticClass());
    Units->Officers.Add(O); break;
   }
  }
 }
 const int32 Needed=FMath::Min(2,Budget.Vehicles-GetActiveVehicles()-Budget.Roadblocks);
 for(int32 I=0;I<Needed && Units->Vehicles.Num()<6;++I)
 {
  auto* S=Station->Road->GetRoadSpline(); const float Length=S->GetSplineLength();
  for(int32 Attempt=0;Attempt<6 && Length>100;++Attempt)
  {
   const float Entry=(Units->Vehicles.Num()%2?Station->SecondRoadEntry:Station->FirstRoadEntry)+Attempt*900;
   const float D=FMath::Fmod(Entry,Length); const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
   if(!NeonCityObservation::SafeDeployment(GetWorld(),P,1200)) continue;
   FActorSpawnParameters Params; Params.Owner=Station.Get(); Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
   auto* V=GetWorld()->SpawnActor<APoliceVehicle>(P,S->GetRotationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World),Params);
   if(V) { V->InitializeRoute(Station->Road,D); V->SetDispatchSlot(NextUnitSlot++); V->SetResponding(true); Units->Vehicles.Add(V); break; }
  }
 }
}
void UPoliceDispatchSubsystem::ResetForLoad()
{
 if(auto* B=GetWorld()->GetSubsystem<URoadblockManager>()) B->Reset();
 if(Units) { for(auto V:Units->Vehicles) if(IsValid(V)) V->Destroy(); Units->Vehicles.Reset(); for(int32 I=Units->Officers.Num()-1;I>=1;--I) if(IsValid(Units->Officers[I])) Units->Officers[I]->Destroy(); if(Units->Officers.Num()>1) Units->Officers.SetNum(1); for(auto O:Units->Officers) if(IsValid(O)) O->GetCombatAI()->Disengage(); }
 NextSpawn=NextRoadblock=0; LastLevel=0; NextUnitSlot=1;
}
void UPoliceDispatchSubsystem::Deinitialize() { Station.Reset(); Units=nullptr; Super::Deinitialize(); }
