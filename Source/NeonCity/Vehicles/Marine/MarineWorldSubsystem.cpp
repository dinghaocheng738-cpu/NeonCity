#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Water/CoastalOceanSurface.h"
#include "Vehicles/Marine/ReferenceBoatGeometry.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "World/NeonCityMainlandSite.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
bool QueryRegisteredWater(AActor* Water,const FVector& Point,FWaterSurfaceSample& Sample)
{
 // The exact native class has no Blueprint override to dispatch. Preserve the
 // interface for every adapter/subclass, including Blueprint-authored water.
 if(Water && Water->GetClass()==ANeonCityWaterVolume::StaticClass())
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(NeonCityNativeWaterSurface356);
  return static_cast<const ANeonCityWaterVolume*>(Water)->QueryWaterSurface_Implementation(Point,Sample);
 }
 return IsValid(Water) && IWaterStateInterface::Execute_QueryWaterSurface(Water,Point,Sample);
}
}
void UMarineWorldSubsystem::RegisterWater(AActor* Water)
{
 if(IsValid(Water) && Water->GetWorld()==GetWorld() && Water->Implements<UWaterStateInterface>()) Waters.AddUnique(Water);
 if(auto* Volume=Cast<ANeonCityWaterVolume>(Water);Volume && Volume->OceanSurface && Ocean.Get()!=Volume->OceanSurface)
 {
  // the sea comes up at runtime (MarineTestArea): hulls that began play before it still need their swell samples
  Ocean=Volume->OceanSurface;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It)if(It->HasActorBegunPlay())RegisterOceanSamples(*It);
 }
}
void UMarineWorldSubsystem::UnregisterWater(AActor* Water) { Waters.Remove(Water); }
bool UMarineWorldSubsystem::SampleWater(FVector Point,float& Height,FVector& Current) const
{
 FWaterSurfaceSample Sample; if(!QueryWater(Point,Sample)) return false;
 Height=Sample.SurfacePoint.Z; Current=Sample.Flow; return true;
}
bool UMarineWorldSubsystem::QueryWater(FVector Point,FWaterSurfaceSample& Sample) const
{
 Sample=FWaterSurfaceSample();
 if(Point.ContainsNaN()) return false;
 // Architectural pools can overlap the ocean's XY footprint high above sea
 // level. Prefer their finite basin, rather than the first registered ocean
 // column; below the pool bottom they deliberately supply no water sample.
 const ANeonCityWaterVolume* Best=nullptr;
 for(const auto& Water:Waters)
 {
  const auto* Pool=Cast<ANeonCityWaterVolume>(Water.Get());
  FWaterSurfaceSample Candidate;
  if(Pool && Pool->bContainedPoolWater && (!Best || Pool->BasinPriority>Best->BasinPriority) &&
     QueryRegisteredWater(Water.Get(),Point,Candidate) &&
     !Candidate.SurfacePoint.ContainsNaN() && !Candidate.Normal.ContainsNaN() &&
     !Candidate.Flow.ContainsNaN() && Candidate.Normal.Normalize() && Candidate.Normal.Z>0)
  {Sample=Candidate;Best=Pool;}
 }
 if(Best)return true;
 for(const auto& Water:Waters)
 {
  // An exact native finite basin was already evaluated above. Its pure
  // geometric miss/invalid sample cannot change within this synchronous
  // query. Unknown adapters keep the original second-pass semantics.
  const auto* NativePool=Cast<ANeonCityWaterVolume>(Water.Get());
  if(NativePool && NativePool->GetClass()==ANeonCityWaterVolume::StaticClass() && NativePool->bContainedPoolWater)continue;
  FWaterSurfaceSample Candidate;
  if(Water.IsValid() && QueryRegisteredWater(Water.Get(),Point,Candidate)
   && !Candidate.SurfacePoint.ContainsNaN() && !Candidate.Normal.ContainsNaN() && !Candidate.Flow.ContainsNaN()
   && Candidate.Normal.Normalize() && Candidate.Normal.Z>0)
  { Sample=Candidate; return true; }
 }
 return false;
}
bool UMarineWorldSubsystem::IsDeepWater(FVector Point,float Submersion) const
{
 float Height; FVector Current; return SampleWater(Point,Height,Current) && Point.Z<Height-Submersion;
}
#if WITH_DEV_AUTOMATION_TESTS
FNeonCityOceanLeaseDiagnostic UMarineWorldSubsystem::QueryHullLeaseDiagnostic(UPrimitiveComponent* Body,int32 Index,FVector Point)const
{return Ocean.IsValid()?Ocean->GetBodyLeaseDiagnostic(Body,Index,Point):FNeonCityOceanLeaseDiagnostic();}
#endif
bool UMarineWorldSubsystem::QueryHullWater(UPrimitiveComponent* Body,int32 Index,FVector Point,FWaterSurfaceSample& Sample,FTidewaterWaterSample* Snapshot)const
{
 if(!QueryWater(Point,Sample))return false;
 // A contained basin (lake, pool) is never the ocean: its sample is final.
 if(Ocean.IsValid() && !Sample.ContainedWater)
 {
  float Height;FVector Normal,Velocity;
  if(Ocean->SampleBody(Body,Index,Point,Height,Normal,Velocity,Snapshot))
  {Sample.SurfacePoint.Z=Height;Sample.Normal=Normal;Sample.Flow=Velocity;}
 }
 return true;
}
void UMarineWorldSubsystem::RegisterOceanSamples(ANeonCityMarineVehicleBase* Boat)
{
 // the sea's swell samples apply to every seagoing hull, persistent or not (a launched tender)
 if(!Ocean.IsValid() || !IsValid(Boat) || Boat->GetProfile()->bInlandWater)return;
 TArray<FVector> Points;const auto* Profile=Boat->GetProfile();
 if(Profile->bReferenceDisplacementHull)
  for(const auto& H:NeonCityReferenceBoat::HullSamples)Points.Add(NeonCityReferenceBoat::ToUEPolar(H.Position)*100);
 else for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})Points.Emplace(X*Profile->HullExtent.X*.8f,Y*Profile->HullExtent.Y*.85f,-Profile->HullExtent.Z);
 if(Ocean->RegisterSamples(Boat->GetHull(),Points))Boat->GetMarineMovement()->AddTickPrerequisiteActor(Ocean.Get());
}
void UMarineWorldSubsystem::RegisterBoat(ANeonCityMarineVehicleBase* Boat)
{
 // Authored/test spawners supply the identity before FinishSpawning; never infer it from an Actor name.
 if(!Boat) return;
 if(Boat->bPersistent)
 {
  if(!Boat->VehicleID.IsValid() || Boats.Contains(Boat->VehicleID))
  { UE_LOG(LogTemp,Warning,TEXT("Marine registration requires a unique authored VehicleID; duplicate/empty identity was not registered.")); return; }
  Boats.Add(Boat->VehicleID,Boat);
 }
 RegisterOceanSamples(Boat);
 if(!Boat->bPersistent) return;
 FNeonCityMarineSaveData Default; Default.VehicleID=Boat->VehicleID; Default.DataAssetID=Boat->GetProfile()->VehicleType;
 Default.Owned=Boat->Owned; Default.Transform=Boat->GetActorTransform(); Default.AuthoredBerthVersion=1; DesignDefaults.Add(Default.VehicleID,Default);
}
void UMarineWorldSubsystem::UnregisterBoat(ANeonCityMarineVehicleBase* Boat)
{
 if(Boat && Ocean.IsValid())Ocean->UnregisterSamples(Boat->GetHull());
 if(Boat && Boats.FindRef(Boat->VehicleID).Get()==Boat) Boats.Remove(Boat->VehicleID);
}
TArray<FNeonCityMarineSaveData> UMarineWorldSubsystem::BuildPersistenceSnapshot(bool Defaults) const
{
 TArray<FNeonCityMarineSaveData> Result;
 for(const auto& Pair:DesignDefaults)
 {
  auto Record=Pair.Value;
  if(!Defaults) if(auto* Boat=Boats.FindRef(Pair.Key).Get())
  { Record.Transform=Boat->GetActorTransform(); Record.Owned=Boat->Owned; Record.Condition=Boat->GetHealthComponent()->GetCurrentHealth()/Boat->GetHealthComponent()->GetMaxHealth(); }
  Result.Add(Record);
 }
 Result.Sort([](const auto& A,const auto& B){return A.VehicleID.ToString()<B.VehicleID.ToString();}); return Result;
}
bool UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(const ANeonCityMainlandSite* Site,FNeonCityMarineSaveData& Record)
{
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles || Record.VehicleID!=SpeedboatID()
  || Record.DataAssetID!=GetDefault<UMarineVehicleData>()->VehicleType
  || (Record.AuthoredBerthVersion!=0 && Record.AuthoredBerthVersion!=1)
  || !NeonCityPersistence::ValidTransform(Record.Transform)
  || !FMath::IsFinite(Record.Condition) || Record.Condition<0 || Record.Condition>1)return false;
 // Physics bob and fender contact change even an untouched boat's exact pose.
 // Only the retired berth neighborhood is eligible, and actual obstruction is
 // required. Clear player-chosen positions, including nearby water, stay exact.
 const FBox RetiredBerth(FVector(4560,-4000,-330),FVector(5160,-2850,60));
 UWorld* World=Site->GetWorld();
 if(!World || !RetiredBerth.IsInsideOrOn(Record.Transform.GetLocation()))return false;
 const auto* Profile=GetDefault<UMarineVehicleData>();
 // Include boarding headroom above the deck. The replacement ramp can trap a
 // boat below it while never touching the submerged buoyancy collision box.
 const FVector ClearanceExtent(Profile->HullExtent.X,Profile->HullExtent.Y,Profile->HullExtent.Z+100);
 if(!NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,Record.Transform,ClearanceExtent,FVector(0,0,100)))return false;
 const FTransform AuthoredBerth=Site->GetSpeedboatBerth();
 if(!NeonCityPersistence::ValidTransform(AuthoredBerth) || AuthoredBerth.Equals(Record.Transform,.01f)
  || NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,AuthoredBerth,ClearanceExtent,FVector(0,0,100)))return false;
 Record.Transform=AuthoredBerth;
 return true;
}

bool UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(const ANeonCityMarineVehicleBase* Boat,const FTransform& AuthoredBerth,FNeonCityMarineSaveData& Record,const TArray<FNeonCityMarineSaveData>* OtherRecords)
{
 const FGuid RetiredID(0x3A8B376B,0x91695E5C,0xB164FD5E,0xEA9E8457);
 const FVector RetiredLocation(-4000,-3420,-130);
 const FVector NewLocation(-9500,-3796.9651413,-130);
 if(!IsValid(Boat) || !Boat->ActorHasTag(TEXT("NeonCity.MainlandC923.Reference86.RelocatedYacht"))
  || Boat->VehicleID!=RetiredID || Record.VehicleID!=RetiredID || Record.AuthoredBerthVersion!=1
  || Record.DataAssetID!=Boat->GetProfile()->VehicleType || !NeonCityPersistence::ValidTransform(Record.Transform)
  || !NeonCityPersistence::ValidTransform(AuthoredBerth) || !FMath::IsFinite(Record.Condition) || Record.Condition<0 || Record.Condition>1
  || !AuthoredBerth.GetLocation().Equals(NewLocation,1)
  || FMath::Abs(FMath::FindDeltaAngleDegrees(AuthoredBerth.Rotator().Yaw,-90))>.1
  || FVector::Dist2D(Record.Transform.GetLocation(),RetiredLocation)>100
  || FMath::Abs(Record.Transform.GetLocation().Z-RetiredLocation.Z)>200
  || FMath::Abs(FMath::FindDeltaAngleDegrees(Record.Transform.Rotator().Yaw,-90))>8
  || FMath::Abs(Record.Transform.Rotator().Pitch)>12 || FMath::Abs(Record.Transform.Rotator().Roll)>12)return false;
 UWorld* World=Boat->GetWorld();if(!World)return false;
 const auto* Site=ANeonCityMainlandSite::Find(World);
 if(!Site || !Site->bRecoverUnsupportedMainlandVehicles)return false;
 const FVector Extent=Boat->GetProfile()->HullExtent+FVector(0,0,100);
 if(!NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,Record.Transform,Extent,FVector(0,0,100))
  || NeonCityVehiclePersistence::HasStaticVehicleObstruction(*World,AuthoredBerth,Extent,FVector(0,0,100)))return false;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);Objects.AddObjectTypesToQuery(ECC_Vehicle);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(ReferenceYachtBerthOccupants),false,Boat);
 const FVector Center=AuthoredBerth.TransformPosition(FVector(0,0,100));
 TArray<FOverlapResult> Occupants;
 World->OverlapMultiByObjectType(Occupants,Center,AuthoredBerth.GetRotation(),Objects,FCollisionShape::MakeBox(Extent),Query);
 // Object queries include overlap-only water/trigger envelopes. Occupancy
 // means a component that actually blocks this boat's collision channel.
 for(const auto& Occupant:Occupants)
  if(const auto* Component=Occupant.GetComponent();Component
   && Component->GetCollisionResponseToChannel(Boat->GetHull()->GetCollisionObjectType())==ECR_Block)return false;
 // Saved boat positions have not been applied yet. Compare against a stable
 // copy of the incoming records as well as the current live occupants.
 const FBox Destination=FBox(-Extent,Extent).ShiftBy(FVector(0,0,100)).TransformBy(AuthoredBerth.ToMatrixWithScale());
 if(OtherRecords)for(const auto& Other:*OtherRecords)
 {
  if(Other.VehicleID==Record.VehicleID || !Other.VehicleID.IsValid() || Other.AuthoredBerthVersion!=1
   || !NeonCityPersistence::ValidTransform(Other.Transform) || !FMath::IsFinite(Other.Condition) || Other.Condition<0 || Other.Condition>1)continue;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(World);It;++It)
   if(It->VehicleID==Other.VehicleID && It->GetProfile()->VehicleType==Other.DataAssetID)
   {
    const FVector OtherExtent=It->GetProfile()->HullExtent;
    if(Destination.Intersect(FBox(-OtherExtent,OtherExtent).TransformBy(Other.Transform.ToMatrixWithScale())))return false;
    break;
   }
 }
 Record.Transform=AuthoredBerth;
 return true;
}

void UMarineWorldSubsystem::NormalizePersistenceSnapshot(TArray<FNeonCityMarineSaveData>& Records,TArray<FString>& Warnings) const
{
 const auto* Site=ANeonCityMainlandSite::Find(GetWorld());
 for(auto& Record:Records)
 {
  const auto* Default=DesignDefaults.Find(Record.VehicleID);
  if(!Default || Record.AuthoredBerthVersion!=0) continue;
  if(Record.VehicleID==HarborlineID() && Record.DataAssetID==Default->DataAssetID
   && NeonCityPersistence::ValidTransform(Record.Transform)
   && Record.Transform.GetLocation().Equals(FVector(6000,-4700,-130),10)
   && FVector::DistSquared(Default->Transform.GetLocation(),Record.Transform.GetLocation())>FMath::Square(5000.))
  {
   Record.Transform=Default->Transform;
   Warnings.Add(TEXT("Legacy untouched prototype fishing berth moved to the island dock; ownership and condition retained."));
  }
  Record.AuthoredBerthVersion=1;
 }
 const TArray<FNeonCityMarineSaveData> DestinationRecords=Records;
 TSet<FGuid> Seen;
 Records.RemoveAll([&](auto& Record)
 {
  const auto* Default=DesignDefaults.Find(Record.VehicleID);
  const bool Valid=Default && Record.VehicleID.IsValid() && !Seen.Contains(Record.VehicleID) && Record.DataAssetID==Default->DataAssetID
   && Record.AuthoredBerthVersion==1
   && NeonCityPersistence::ValidTransform(Record.Transform) && FMath::IsFinite(Record.Condition) && Record.Condition>=0 && Record.Condition<=1;
  if(Valid && MigrateUntouchedMainlandSpeedboatBerth(Site,Record))
   Warnings.Add(TEXT("Obstructed legacy mainland speedboat berth moved to the authored berth; identity, ownership and condition preserved."));
  if(Valid && Default && MigrateRetiredReferenceYachtBerth(Boats.FindRef(Record.VehicleID).Get(),Default->Transform,Record,&DestinationRecords))
   Warnings.Add(TEXT("Obstructed retired marina yacht berth moved to its authored berth; identity, ownership and condition preserved."));
  float Height; FVector Current;
  if(!Valid || !SampleWater(Record.Transform.GetLocation(),Height,Current)
   || FMath::Abs(Record.Transform.GetLocation().Z-Height)>450)
  { Warnings.Add(TEXT("Invalid/unknown marine record ignored; authored boat defaults retained.")); return true; }
  Seen.Add(Record.VehicleID); return false;
 });
 for(const auto& Pair:DesignDefaults) if(!Seen.Contains(Pair.Key)) Records.Add(Pair.Value);
}
void UMarineWorldSubsystem::ApplyPersistenceSnapshot(const TArray<FNeonCityMarineSaveData>& Records)
{
 for(const auto& Record:Records) if(auto* Boat=Boats.FindRef(Record.VehicleID).Get())
 {
  Boat->FindComponentByClass<UPhysicalImpactComponent>()->Suppress();
  Boat->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector); Boat->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Boat->SetActorTransform(Record.Transform,false,nullptr,ETeleportType::TeleportPhysics);
  // Reclassify after the saved pose/zero velocity has been restored. Keeping
  // the previous authored-berth flag would attract a sea save back to shore.
  Boat->GetMarineMovement()->StopEngine();
  Boat->Owned=Record.Owned; Boat->GetHealthComponent()->ApplyPersistenceHealth(Record.Condition*Boat->GetProfile()->Durability,Boat->GetProfile()->Durability);
 }
}
