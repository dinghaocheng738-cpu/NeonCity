#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/IslandParkingMigration.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Vehicles/VehicleInteraction.h"
#include "Combat/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityMainlandSite.h"

namespace NeonCityVehiclePersistence
{
static const TCHAR* AssetNames[]={TEXT("DA_StarterSedan"),TEXT("DA_SportsCar"),TEXT("DA_SUV"),TEXT("DA_Pickup")};
static bool IsExpedition(uint8 Type,FName Name)
{
 return (Type==uint8(ENeonCityVehicleType::SUV) && Name==TEXT("DA_ExpeditionSUV"))
  || (Type==uint8(ENeonCityVehicleType::Pickup) && Name==TEXT("DA_ExpeditionPickup"));
}
static bool KnownProfile(uint8 Type,FName Name)
{
 return Type<UE_ARRAY_COUNT(AssetNames) && (Name==FName(AssetNames[Type]) || IsExpedition(Type,Name)
  || (Type==uint8(ENeonCityVehicleType::SportsCar) && (Name==TEXT("DA_HeroAster") || Name==TEXT("DA_HeroKestrel") || Name==TEXT("DA_HeroNacre") || Name==TEXT("DA_HeroCarrera914"))));
}
static bool IsHero(FName Name){return Name==TEXT("DA_HeroAster") || Name==TEXT("DA_HeroKestrel") || Name==TEXT("DA_HeroNacre") || Name==TEXT("DA_HeroCarrera914");}
bool MigrateDefaultIslandParking(FNeonCityVehicleSaveData& Data)
{
 if(Data.IslandFleetMigrationVersion>=2)return false;
 struct FDefault{const TCHAR* ID;const TCHAR* Profile;FVector Location;float PreviousYaw,NewYaw;};
 // Source identities: Supercars/placement.json and Nacre/generated.json.
 // Destinations: ParkingFacing807 native authoring receipt, island world offset.
 const FDefault Defaults[]={
  {TEXT("5FF7803645833C8D593E77A50B006245"),TEXT("DA_HeroKestrel"),NeonCityCoastalLayout::FromLegacyIsland(FVector(59249.2771,-153126.5078,747.5470)),14.04705f,105.07603f},
  {TEXT("7F242340469FEA4D1A7D21A5D5DED892"),TEXT("DA_HeroAster"),NeonCityCoastalLayout::FromLegacyIsland(FVector(57072.7507,-153875.1075,735.2460)),23.11380f,180.f},
  {TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8"),TEXT("DA_HeroNacre"),NeonCityCoastalLayout::FromLegacyIsland(FVector(60384.6401,-152870.3611,825.f)),10.65951f,101.29916f}};
 const FString ID=Data.VehicleID.ToString(EGuidFormats::Digits);
 for(const auto& D:Defaults)if(ID==D.ID && Data.DataAssetID==D.Profile)
 {
  const FVector P=Data.Transform.GetLocation();const FRotator R=Data.Transform.Rotator();
  const bool AtDefault=Data.StoredPropertyID.IsNone() && Data.GarageSlot==INDEX_NONE && !Data.LegacyFleetStored
   && FVector::Dist2D(P,D.Location)<25 && FMath::Abs(P.Z-D.Location.Z)<125
   && FMath::Abs(FMath::FindDeltaAngleDegrees(R.Yaw,D.PreviousYaw))<2 && FMath::Abs(R.Pitch)<5 && FMath::Abs(R.Roll)<5;
  if(AtDefault){Data.Transform.SetLocation(D.Location);Data.Transform.SetRotation(FRotator(0,D.NewYaw,0).Quaternion());}
  Data.IslandFleetMigrationVersion=2;return AtDefault;
 }
 return false;
}
static void MigrateIslandFleet(FNeonCityVehicleSaveData& Data)
{
 const FString ID=Data.VehicleID.ToString(EGuidFormats::Digits);
 const bool Aster=ID==TEXT("7F242340469FEA4D1A7D21A5D5DED892") || ID==TEXT("8BD22EB34C404BB67EF8ECAC243B9DB9");
 const bool Kestrel=ID==TEXT("5FF7803645833C8D593E77A50B006245") || ID==TEXT("3071B4354B42495A92AE5FA3467F35A0");
 if(IsExpedition(Data.VehicleType,Data.DataAssetID) && (Aster || Kestrel))
 {Data.VehicleType=uint8(ENeonCityVehicleType::SportsCar);Data.DataAssetID=Aster?TEXT("DA_HeroAster"):TEXT("DA_HeroKestrel");}
 // This is a default-placement migration, not an owned-collection limit.
 // Moved vehicles survive at their saved positions; untouched defaults are
 // retained in owned inventory and can be retrieved from a clear garage bay.
 FVector OldDefault;bool Retired=false;
 if(ID==TEXT("8BD22EB34C404BB67EF8ECAC243B9DB9") && Data.DataAssetID==TEXT("DA_HeroAster"))
 {OldDefault=NeonCityCoastalLayout::FromLegacyIsland(FVector(58143.1746,-153457.7973,737.8793));Retired=true;}
 if(ID==TEXT("3071B4354B42495A92AE5FA3467F35A0") && Data.DataAssetID==TEXT("DA_HeroKestrel"))
 {OldDefault=NeonCityCoastalLayout::FromLegacyIsland(FVector(60384.6401,-152870.3611,747.2213));Retired=true;}
 if(Retired && Data.IslandFleetMigrationVersion==0)
 {
  if(Data.StoredPropertyID.IsNone() && FVector::Dist(Data.Transform.GetLocation(),OldDefault)<250)Data.LegacyFleetStored=true;
  Data.IslandFleetMigrationVersion=1;
 }
}
// Retired fleet saves can retain the former 305m mansion court after the
// residence was rebuilt below it. Keep the original value snapshot in owned
// storage, available through the existing garage retrieval control. Never
// move a supported player choice, a current fleet car or a live vehicle.
static bool RecoverUnsupportedRetiredEstateCar(UWorld* World,FNeonCityVehicleSaveData& Data)
{
 const FString ID=Data.VehicleID.ToString(EGuidFormats::Digits);
 const bool Retired=(ID==TEXT("8BD22EB34C404BB67EF8ECAC243B9DB9") && Data.DataAssetID==TEXT("DA_HeroAster"))
  || (ID==TEXT("3071B4354B42495A92AE5FA3467F35A0") && Data.DataAssetID==TEXT("DA_HeroKestrel"));
 // Bounds of the obsolete court, retained in the original land-final route.
 const FBox OldCourt(NeonCityCoastalLayout::FromLegacyIsland(FVector(45000,-122500,30000)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48500,-117500,31000)));
 if(!World || !Retired || !Data.Owned || Data.LegacyFleetStored || Data.IslandFleetMigrationVersion>=3
  || !Data.StoredPropertyID.IsNone() || Data.GarageSlot!=INDEX_NONE || !OldCourt.IsInsideOrOn(Data.Transform.GetLocation()))return false;
 // Vehicle normalization precedes player-destination streaming on disk load.
 // A no-hit in an unloaded level is not evidence that parking lost support.
 auto* Region=ANeonCityCoastalRegion::Find(World);
 if(!Region || !Region->EnsureDestinationReady(Data.Transform.GetLocation()))return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(RetiredEstateRestoreSupport),false);
 for(TActorIterator<APawn> I(World);I;++I)Query.AddIgnoredActor(*I);
 // Conservative footprint survey: any real support within5m prevents this
 // migration. It therefore cannot reinterpret an ordinary elevated parking
 // choice as obsolete just because the chassis origin is above the floor.
 for(const FVector Offset:{FVector::ZeroVector,FVector(180,80,0),FVector(180,-80,0),FVector(-180,80,0),FVector(-180,-80,0)})
 {
  const FVector P=Data.Transform.TransformPosition(Offset);FHitResult Hit;
  if(World->LineTraceSingleByChannel(Hit,P+FVector(0,0,50),P-FVector(0,0,500),ECC_Pawn,Query) && Hit.ImpactNormal.Z>.5)return false;
 }
 Data.LegacyFleetStored=true;Data.IslandFleetMigrationVersion=3;return true;
}
static UVehicleData* Profile(uint8 Type,FName Name)
{
 if(Type>=UE_ARRAY_COUNT(AssetNames)) return nullptr;
 if(Name==TEXT("DA_HeroNacre"))return LoadObject<UVehicleData>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Nacre/DA_HeroNacre.DA_HeroNacre"));
 if(Name==TEXT("DA_HeroCarrera914"))return LoadObject<UVehicleData>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland914/Cars/DA_HeroCarrera914.DA_HeroCarrera914"));
 if(IsHero(Name))return LoadObject<UVehicleData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/Polish/Adventure/Supercars/%s.%s"),*Name.ToString(),*Name.ToString()));
 if(IsExpedition(Type,Name))return LoadObject<UVehicleData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/Polish/Adventure/Offroad/%s.%s"),*Name.ToString(),*Name.ToString()));
 return LoadObject<UVehicleData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Vehicles/%s.%s"),AssetNames[Type],AssetNames[Type]));
}
TSubclassOf<ANeonCityBaseVehicle> RestoreVehicleClass(uint8 Type,FName Name)
{
 if(!KnownProfile(Type,Name))return nullptr;
 if(IsHero(Name))return AIslandSupercar::StaticClass();
 if(IsExpedition(Type,Name))return AIslandOffroadVehicle::StaticClass();
 return Type==uint8(ENeonCityVehicleType::Sedan)?ANeonCityStarterSedan::StaticClass():ANeonCityPrototypeVehicle::StaticClass();
}
static void ConfigureRestoredVehicle(ANeonCityBaseVehicle& Vehicle,const FNeonCityVehicleSaveData& Data,UVehicleData* VehicleProfile)
{
 Vehicle.VehicleID=Data.VehicleID;Vehicle.bStartsOwned=false;Vehicle.VehicleData=VehicleProfile;
 if(auto* Expedition=Cast<AIslandOffroadVehicle>(&Vehicle))Expedition->bPickup=Data.VehicleType==uint8(ENeonCityVehicleType::Pickup);
 if(auto* Hero=Cast<AIslandSupercar>(&Vehicle))
 {Hero->bRoadster=Data.DataAssetID==TEXT("DA_HeroKestrel");Hero->bRetractable=Data.DataAssetID==TEXT("DA_HeroNacre");}
}
}
FNeonCityVehicleSaveData UVehicleOwnershipSubsystem::CapturePersistence(ANeonCityBaseVehicle* Vehicle) const
{
 FNeonCityVehicleSaveData Data; Data.VehicleID=Vehicle->VehicleID; Data.VehicleType=uint8(Vehicle->GetVehicleProfile()->VehicleType);
 Data.DataAssetID=Vehicle->GetVehicleProfile()->GetFName(); Data.Transform=Vehicle->GetActorTransform();
 Data.Owned=IsOwned(Data.VehicleID); Data.Condition=Vehicle->GetDamageState()->GetConditionFraction();
 if(const auto* Existing=PersistentVehicles.Find(Data.VehicleID))Data.IslandFleetMigrationVersion=Existing->IslandFleetMigrationVersion;
 if(const auto* Hero=Cast<AIslandSupercar>(Vehicle);Hero && Hero->bRetractable)
 {Data.RoofProgress=Hero->GetRoofMechanism()->GetProgress();Data.RoofTargetOpen=Hero->GetRoofMechanism()->IsTargetOpen();}
 return Data;
}
TArray<FNeonCityVehicleSaveData> UVehicleOwnershipSubsystem::BuildPersistenceSnapshot(bool Defaults)
{
 TArray<FNeonCityVehicleSaveData> Result;
 if(Defaults) { bInitialSnapshotSealed=true; InitialVehicles.GenerateValueArray(Result); }
 else
 {
  for(const auto& Pair:LiveVehicles) if(Pair.Value.IsValid()) PersistentVehicles.Add(Pair.Key,CapturePersistence(Pair.Value.Get()));
  PersistentVehicles.GenerateValueArray(Result);
 }
 Result.Sort([](const auto& A,const auto& B){ return A.VehicleID.ToString()<B.VehicleID.ToString(); }); return Result;
}
void UVehicleOwnershipSubsystem::NormalizePersistenceSnapshot(TArray<FNeonCityVehicleSaveData>& Snapshot,TArray<FString>& Warnings) const
{
 TSet<FGuid> Seen;
 int32 MainlandParkingRecoveries=0,MainlandStoredRecoveries=0,MainlandDefaultParkingUpdates=0;
 const auto* MainlandSite=ANeonCityMainlandSite::Find(GetWorld());
 const auto SavedParkingChoices=Snapshot;
 Snapshot.RemoveAll([&](auto& Data)
 {
  NeonCityVehiclePersistence::MigrateIslandFleet(Data);
  NeonCityVehiclePersistence::MigrateDefaultIslandParking(Data);
  const bool KnownType=Data.VehicleType<UE_ARRAY_COUNT(NeonCityVehiclePersistence::AssetNames);
  const bool Invalid=!Data.VehicleID.IsValid() || Seen.Contains(Data.VehicleID) || !KnownType
   || !NeonCityVehiclePersistence::KnownProfile(Data.VehicleType,Data.DataAssetID) || !NeonCityPersistence::ValidTransform(Data.Transform)
   || !FMath::IsFinite(Data.Condition) || Data.Condition<0 || Data.Condition>1
   || !FMath::IsFinite(Data.RoofProgress) || Data.RoofProgress<0 || Data.RoofProgress>1;
  if(Invalid) { Warnings.Add(TEXT("Invalid VehicleID, profile or vehicle values skipped.")); return true; }
  NeonCityVehiclePersistence::MigrateAuthoredMainlandVehicleProfile(MainlandSite,Data);
  auto* Live=FindLiveVehicle(Data.VehicleID);
  if((Live && (uint8(Live->GetVehicleProfile()->VehicleType)!=Data.VehicleType || Live->GetVehicleProfile()->GetFName()!=Data.DataAssetID)) || (!Live && !Data.Owned))
  { Warnings.Add(TEXT("Unmatched vehicle identity/type skipped.")); return true; }
  if(!NeonCityVehiclePersistence::Profile(Data.VehicleType,Data.DataAssetID)) { Warnings.Add(TEXT("Unavailable vehicle asset skipped.")); return true; }
  FString DefaultParkingDiagnostic;
  if(NeonCityVehiclePersistence::MigrateDefaultMainlandParking(GetWorld(),Data,Live?InitialVehicles.Find(Data.VehicleID):nullptr,&DefaultParkingDiagnostic,SavedParkingChoices))++MainlandDefaultParkingUpdates;
  else if(!DefaultParkingDiagnostic.IsEmpty())Warnings.Add(DefaultParkingDiagnostic);
  const auto MainlandRecovery=NeonCityVehiclePersistence::RecoverUnsupportedMainlandVehicle(GetWorld(),Data,Live?InitialVehicles.Find(Data.VehicleID):nullptr);
  if(MainlandRecovery==NeonCityVehiclePersistence::EMainlandVehicleRecovery::AuthoredParking)++MainlandParkingRecoveries;
  if(MainlandRecovery==NeonCityVehiclePersistence::EMainlandVehicleRecovery::OwnedStorage)++MainlandStoredRecoveries;
  if(!Live && NeonCityVehiclePersistence::RecoverUnsupportedRetiredEstateCar(GetWorld(),Data))
   Warnings.Add(TEXT("A retired island car lost support at the obsolete mansion court. Its original identity, pose and condition are preserved in owned storage; retrieve it through the clear garage bay."));
  Seen.Add(Data.VehicleID); return false;
 });
 if(MainlandDefaultParkingUpdates)Warnings.Add(FString::Printf(TEXT("Aligned %d untouched released mainland default vehicle(s) to their current authored parking bays; customized parking and all non-pose saved values retained."),MainlandDefaultParkingUpdates));
 if(MainlandParkingRecoveries || MainlandStoredRecoveries)
  Warnings.Add(FString::Printf(TEXT("Replaced mainland parking lost static support: %d owned vehicles recovered to matching authored bays, %d retained in owned storage for garage retrieval. Vehicle identities, profiles, condition and roof state are preserved; supported saved choices were unchanged."),MainlandParkingRecoveries,MainlandStoredRecoveries));
 // New authored vehicles introduced by later content retain their original defaults.
 for(const auto& Pair:InitialVehicles) if(!Seen.Contains(Pair.Key)) Snapshot.Add(Pair.Value);
}
void UVehicleOwnershipSubsystem::ApplyPersistenceSnapshot(const TArray<FNeonCityVehicleSaveData>& Snapshot)
{
 Records.Reset(); PersistentVehicles.Reset();
 for(const auto& Data:Snapshot)
 {
  auto* Vehicle=FindLiveVehicle(Data.VehicleID);
  // Stored means absent from the playable world. A live authored default may
  // exist on a fresh session; do not apply the retired unsupported pose to it or
  // let the next capture silently overwrite the preserved owned-storage record.
  if(Vehicle && Data.LegacyFleetStored){Vehicle->Destroy();Vehicle=nullptr;}
  if(!Vehicle && Data.Owned && !Data.LegacyFleetStored)
  {
   const TSubclassOf<ANeonCityBaseVehicle> Type=NeonCityVehiclePersistence::RestoreVehicleClass(Data.VehicleType,Data.DataAssetID);
   auto* Profile=NeonCityVehiclePersistence::Profile(Data.VehicleType,Data.DataAssetID);
   if(Type && Profile)Vehicle=GetWorld()->SpawnActorDeferred<ANeonCityBaseVehicle>(Type,Data.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(Vehicle)
   {
    NeonCityVehiclePersistence::ConfigureRestoredVehicle(*Vehicle,Data,Profile);
    Vehicle->FinishSpawning(Data.Transform);
   }
  }
  PersistentVehicles.Add(Data.VehicleID,Data);
  if(Data.Owned)
  {
   FNeonCityOwnedVehicle Record; Record.VehicleID=Data.VehicleID; Record.VehicleType=static_cast<ENeonCityVehicleType>(Data.VehicleType);
   Record.Location=Data.Transform.GetLocation(); Record.Condition=Data.Condition; Records.Add(Data.VehicleID,Record);
  }
  if(!Vehicle) continue; // A missing optional asset/actor never invalidates the wallet or other domains.
  Vehicle->GetDrivingComponent()->SetDrivingEnabled(false); Vehicle->GetChaosMovement()->ResetVehicleState();
  Vehicle->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector); Vehicle->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
  Vehicle->SetActorTransform(Data.Transform,false,nullptr,ETeleportType::TeleportPhysics);
  Vehicle->GetHealthComponent()->ApplyPersistenceHealth(Data.Condition*Vehicle->GetVehicleProfile()->Durability,Vehicle->GetVehicleProfile()->Durability);
  Vehicle->GetDamageState()->SuppressRestoreCollision();
  if(auto* Hero=Cast<AIslandSupercar>(Vehicle);Hero && Hero->bRetractable)
   Hero->GetRoofMechanism()->RestoreState(Data.RoofProgress,Data.RoofTargetOpen);
 }
}
bool UVehicleOwnershipSubsystem::IsLegacyFleetStored(FGuid ID) const
{const auto* Data=PersistentVehicles.Find(ID);return Data && Data->Owned && Data->LegacyFleetStored;}
ANeonCityBaseVehicle* UVehicleOwnershipSubsystem::DeployStoredVehicle(FGuid ID,const FTransform& BayTransform)
{
 auto* Data=PersistentVehicles.Find(ID);if(!Data || !IsLegacyFleetStored(ID) || FindLiveVehicle(ID) || !NeonCityPersistence::ValidTransform(BayTransform))return nullptr;
 const FCollisionQueryParams Query(SCENE_QUERY_STAT(RetiredFleetBay),false);
 if(GetWorld()->OverlapBlockingTestByChannel(BayTransform.GetLocation()+FVector(0,0,60),BayTransform.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(FVector(255,115,58)),Query))return nullptr;
 const FNeonCityVehicleSaveData Saved=*Data;
 const auto Type=NeonCityVehiclePersistence::RestoreVehicleClass(Saved.VehicleType,Saved.DataAssetID);
 auto* Profile=NeonCityVehiclePersistence::Profile(Saved.VehicleType,Saved.DataAssetID);
 if(!Type || !Profile)return nullptr;
 auto* Vehicle=GetWorld()->SpawnActorDeferred<ANeonCityBaseVehicle>(Type,BayTransform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
 if(!Vehicle)return nullptr;
 NeonCityVehiclePersistence::ConfigureRestoredVehicle(*Vehicle,Saved,Profile);Vehicle->FinishSpawning(BayTransform);
 if(!IsValid(Vehicle))return nullptr;
 Vehicle->GetHealthComponent()->ApplyPersistenceHealth(Saved.Condition*Vehicle->GetVehicleProfile()->Durability,Vehicle->GetVehicleProfile()->Durability);
 Vehicle->GetDamageState()->SuppressRestoreCollision();
 if(auto* Hero=Cast<AIslandSupercar>(Vehicle);Hero && Hero->bRetractable)Hero->GetRoofMechanism()->RestoreState(Saved.RoofProgress,Saved.RoofTargetOpen);
 auto Updated=Saved;Updated.Transform=BayTransform;Updated.LegacyFleetStored=false;PersistentVehicles.Add(ID,Updated);Capture(Vehicle);return Vehicle;
}
void UVehicleDamageComponent::SuppressRestoreCollision()
{ if(auto* Impact=GetOwner()->FindComponentByClass<UPhysicalImpactComponent>()) Impact->Suppress(); FeedbackUntil=0; LastDamage=0; }
