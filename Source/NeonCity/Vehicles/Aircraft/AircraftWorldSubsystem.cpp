#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "World/NeonCityMainlandSite.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
void UAircraftWorldSubsystem::RegisterAircraft(ANeonCityAircraftVehicleBase* Aircraft)
{
 // Authored/test spawners supply the identity before FinishSpawning; never infer it from an Actor name.
 if(!Aircraft) return;
 if(!Aircraft->AircraftID.IsValid() || Aircrafts.Contains(Aircraft->AircraftID))
 { UE_LOG(LogTemp,Warning,TEXT("Aircraft registration requires a unique authored AircraftID; duplicate/empty identity was not registered.")); return; }
 Aircrafts.Add(Aircraft->AircraftID,Aircraft);
 FNeonCityAircraftSaveData Default; Default.AircraftID=Aircraft->AircraftID; Default.DataAssetID=Aircraft->GetProfile()->AircraftType;
 Default.Owned=Aircraft->Owned; Default.Transform=Aircraft->GetActorTransform(); DesignDefaults.Add(Default.AircraftID,Default);
}
void UAircraftWorldSubsystem::UnregisterAircraft(ANeonCityAircraftVehicleBase* Aircraft)
{
 if(Aircraft && Aircrafts.FindRef(Aircraft->AircraftID).Get()==Aircraft) Aircrafts.Remove(Aircraft->AircraftID);
}
TArray<FNeonCityAircraftSaveData> UAircraftWorldSubsystem::BuildPersistenceSnapshot(bool Defaults) const
{
 TArray<FNeonCityAircraftSaveData> Result;
 for(const auto& Pair:DesignDefaults)
 {
  auto Record=Pair.Value;
  if(!Defaults) if(auto* Aircraft=Aircrafts.FindRef(Pair.Key).Get())
  { Record.Transform=Aircraft->GetActorTransform(); Record.Owned=Aircraft->Owned; Record.Condition=Aircraft->GetHealthComponent()->GetCurrentHealth()/Aircraft->GetHealthComponent()->GetMaxHealth(); }
  Result.Add(Record);
 }
 Result.Sort([](const auto& A,const auto& B){return A.AircraftID.ToString()<B.AircraftID.ToString();}); return Result;
}
bool UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(const ANeonCityMainlandSite* Site,FNeonCityAircraftSaveData& Record)
{
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles || Record.AircraftID!=HelicopterID()
  || Record.DataAssetID!=GetDefault<UAircraftVehicleData>()->AircraftType
  || !NeonCityPersistence::ValidTransform(Record.Transform)
  || !FMath::IsFinite(Record.Condition) || Record.Condition<0 || Record.Condition>1)return false;
 // The retired physical pad was 8.8m by 11m. Settling and small drift are
 // expected, but airborne/offsite/island choices never enter this policy.
 const FBox RetiredPad(FVector(-3440,1900,30),FVector(-2560,3000,260));
 UWorld* World=Site->GetWorld();
 if(!World || !RetiredPad.IsInsideOrOn(Record.Transform.GetLocation()))return false;
 const FVector HullExtent=GetDefault<UAircraftVehicleData>()->BodyExtent-FVector(3);
 bool ReplacedByRoad=false;
 // A clear road is valid collision support but is no longer a landing stand.
 // Require the actual authored QuayRear guide AND a real static road surface;
 // an empty/clear custom location elsewhere on the retired pad is preserved.
 for(TActorIterator<ANeonCityTrafficRoute> It(World);It && !ReplacedByRoad;++It)
 {
  if(It->GetLevel()!=World->PersistentLevel || It->RoadID!=TEXT("C920.QuayRear")
   || !It->ActorHasTag(TEXT("NeonCity.MainlandC920")))continue;
  const FVector RoadPoint=It->GetRoadSpline()->FindLocationClosestToWorldLocation(Record.Transform.GetLocation(),ESplineCoordinateSpace::World);
  if(FVector::Dist2D(RoadPoint,Record.Transform.GetLocation())>350
   || FMath::Abs(RoadPoint.Z-Record.Transform.GetLocation().Z)>160)continue;
  FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandRetiredHelipadRoad),false);
  for(TActorIterator<APawn> Pawn(World);Pawn;++Pawn)Query.AddIgnoredActor(*Pawn);
  FHitResult Hit;
  ReplacedByRoad=World->LineTraceSingleByObjectType(Hit,Record.Transform.GetLocation()+FVector(0,0,40),
   Record.Transform.GetLocation()-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Query)
   && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics() && Hit.ImpactNormal.Z>.8
   && FMath::Abs(Hit.ImpactPoint.Z-RoadPoint.Z)<25;
 }
 if(!ReplacedByRoad && !NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,Record.Transform,HullExtent))return false;
 const FTransform AuthoredLanding=Site->GetHelicopterLanding();
 if(!NeonCityPersistence::ValidTransform(AuthoredLanding) || AuthoredLanding.Equals(Record.Transform,.01f)
  || NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,AuthoredLanding,HullExtent))return false;
 Record.Transform=AuthoredLanding;
 return true;
}

void UAircraftWorldSubsystem::NormalizePersistenceSnapshot(TArray<FNeonCityAircraftSaveData>& Records,TArray<FString>& Warnings) const
{
 const auto* Site=ANeonCityMainlandSite::Find(GetWorld());
 TSet<FGuid> Seen;
 Records.RemoveAll([&](auto& Record)
 {
  const auto* Default=DesignDefaults.Find(Record.AircraftID);
  const bool Valid=Default && Record.AircraftID.IsValid() && !Seen.Contains(Record.AircraftID) && Record.DataAssetID==Default->DataAssetID
   && NeonCityPersistence::ValidTransform(Record.Transform) && FMath::IsFinite(Record.Condition) && Record.Condition>=0 && Record.Condition<=1;
  bool Blocked=false;
  if(Valid)
  {
   // The retired pad can now be supported by a road. Relocate eligible poses
   // before generic restore support checks accept it as a road obstruction.
   if(MigrateUntouchedMainlandHelicopterLanding(Site,Record))
    Warnings.Add(TEXT("Retired mainland helipad pose moved to the authored landing; identity, ownership and condition preserved."));
   const auto* Actor=Aircrafts.FindRef(Record.AircraftID).Get();
   if(Actor)
   {
    const FTransform Requested=Record.Transform;
    if(!Actor->ResolveRestoreTransform(Record.Transform,Default->Transform))
    { Warnings.Add(TEXT("No clear aircraft restore stand; live aircraft will be retained if still obstructed during apply.")); return true; }
    if(!Record.Transform.Equals(Requested,.01f)) Warnings.Add(TEXT("Unsafe aircraft position restored to clear ground stand; ownership and condition preserved."));
   }
   const FVector Extent=Actor?Actor->GetProfile()->BodyExtent:GetDefault<UAircraftVehicleData>()->BodyExtent;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(AircraftRestore),false,Actor);
   Blocked=GetWorld()->OverlapAnyTestByObjectType(Record.Transform.GetLocation(),Record.Transform.GetRotation(),
    FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(Extent-FVector(3)),Query);
  }
  if(!Valid || Blocked)
  { Warnings.Add(TEXT("Invalid/unknown aircraft record ignored; authored aircraft defaults retained.")); return true; }
  Seen.Add(Record.AircraftID); return false;
 });
 for(const auto& Pair:DesignDefaults) if(!Seen.Contains(Pair.Key)) Records.Add(Pair.Value);
}
void UAircraftWorldSubsystem::ApplyPersistenceSnapshot(const TArray<FNeonCityAircraftSaveData>& Records)
{
 for(const auto& Record:Records) if(auto* Aircraft=Aircrafts.FindRef(Record.AircraftID).Get())
 {
  FTransform Resolved=Record.Transform;
  const auto* Default=DesignDefaults.Find(Record.AircraftID);
  if(!Default || !Aircraft->ResolveRestoreTransform(Resolved,Default->Transform))
  { UE_LOG(LogTemp,Warning,TEXT("Aircraft restore skipped: no clear supported location; live actor retained.")); continue; }
  Aircraft->GetAircraftMovement()->StopEngine(); Aircraft->FindComponentByClass<UPhysicalImpactComponent>()->Suppress();
  Aircraft->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector); Aircraft->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Aircraft->SetActorTransform(Resolved,false,nullptr,ETeleportType::TeleportPhysics);
  Aircraft->Owned=Record.Owned; Aircraft->GetHealthComponent()->ApplyPersistenceHealth(Record.Condition*Aircraft->GetProfile()->Durability,Aircraft->GetProfile()->Durability);
  Aircraft->GetAircraftMovement()->ResetAfterLoad();
 }
}
