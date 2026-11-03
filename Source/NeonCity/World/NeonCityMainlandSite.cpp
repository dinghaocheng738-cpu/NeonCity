#include "World/NeonCityMainlandSite.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "AI/PedestrianSpawnPoint.h"
#include "AssetCompilingManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/App.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderingThread.h"
#endif

ANeonCityMainlandSite::ANeonCityMainlandSite()
{
 PrimaryActorTick.bCanEverTick = false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MainlandSiteOrigin")));
}

#if WITH_EDITOR
void ANeonCityMainlandSite::PostInitializeComponents()
{
 Super::PostInitializeComponents();
 UWorld* World=GetWorld();
 if(!bPrepareEditorPresentationBeforePlay || !World || !World->IsGameWorld()
  || World->HasBegunPlay() || GetLevel()!=World->PersistentLevel || !FApp::CanEverRender())return;
 TRACE_CPUPROFILER_EVENT_SCOPE(NeonCity_MainlandPresentationLoading);
 const double Started=FPlatformTime::Seconds();
 TArray<UObject*> Assets;
 TSet<USkeletalMesh*> AmbientSkins;
 TSet<UStaticMesh*> AuthoredProps;
 if(WeaponStoreMesh)AuthoredProps.Add(WeaponStoreMesh);
 // Runtime bootstrap visuals have no persistent mesh component to visit.
 // Include the hard reference in the same existing pre-play preparation.
 if(SpeedboatVisualMesh)AuthoredProps.Add(SpeedboatVisualMesh);
 // These hard references already belong to the map. Preparing both skin
 // variants also covers pedestrians activated later by the world scheduler.
 for(TActorIterator<AActor> It(World);It;++It)
 {
  if(It->GetLevel()!=World->PersistentLevel)continue;
  TInlineComponentArray<UPrimitiveComponent*> Components(*It);
  for(auto* Component:Components)
  {
   if(auto* Static=Cast<UStaticMeshComponent>(Component))
    if(Static->GetStaticMesh())Assets.AddUnique(Static->GetStaticMesh());
   if(auto* Skinned=Cast<USkinnedMeshComponent>(Component))
    if(Skinned->GetSkinnedAsset())Assets.AddUnique(Skinned->GetSkinnedAsset());
  }
  if(auto* Marker=Cast<APedestrianSpawnPoint>(*It);Marker && Marker->bAmbientEnabled)
  {
   if(Marker->AmbientMaleSkin)AmbientSkins.Add(Marker->AmbientMaleSkin);
   if(Marker->AmbientFemaleSkin)AmbientSkins.Add(Marker->AmbientFemaleSkin);
   if(Marker->AmbientCupMesh)AuthoredProps.Add(Marker->AmbientCupMesh);
  }
 }
 for(auto* Skin:AmbientSkins)Assets.AddUnique(Skin);
 for(auto* Prop:AuthoredProps)Assets.AddUnique(Prop);
 FAssetCompilingManager::Get().FinishCompilationForObjects(Assets);

 TSet<UMaterialInterface*> Materials;
 if(OceanRenderingMaterial)Materials.Add(OceanRenderingMaterial);
 for(TActorIterator<AActor> It(World);It;++It)
 {
  if(It->GetLevel()!=World->PersistentLevel)continue;
  TInlineComponentArray<UPrimitiveComponent*> Components(*It);
  for(auto* Component:Components)
  {
   TArray<UMaterialInterface*> Used;
   Component->GetUsedMaterials(Used);
   for(auto* Material:Used)if(Material)Materials.Add(Material);
  }
 }
 for(auto* Skin:AmbientSkins)
  for(const auto& Slot:Skin->GetMaterials())if(Slot.MaterialInterface)Materials.Add(Slot.MaterialInterface);
 for(auto* Prop:AuthoredProps)
  for(const auto& Slot:Prop->GetStaticMaterials())if(Slot.MaterialInterface)Materials.Add(Slot.MaterialInterface);
 // An empty compile queue is insufficient: uncooked shader maps may still
 // request missing permutations from a draw task when first made visible.
 // EnsureIsComplete resolves the actual resource, including MI parents, and
 // submits/finishes those permutations before the world becomes interactive.
 for(auto* Material:Materials)Material->EnsureIsComplete();
 FlushRenderingCommands();
 UE_LOG(LogTemp,Display,TEXT("Mainland presentation loading prepared %d referenced assets and %d materials in %.3fs before play."),
  Assets.Num(),Materials.Num(),FPlatformTime::Seconds()-Started);
 if(SpeedboatVisualMesh || OceanRenderingMaterial)
  UE_LOG(LogTemp,Display,TEXT("Mainland pre-play runtime visuals prepared: boat=%s ocean=%s before_world_begin=%d."),
   *GetPathNameSafe(SpeedboatVisualMesh.Get()),*GetPathNameSafe(OceanRenderingMaterial.Get()),!World->HasBegunPlay());
}
#endif

ANeonCityMainlandSite* ANeonCityMainlandSite::Find(const UWorld* World)
{
 ANeonCityMainlandSite* Found = nullptr;
 if (World) for (TActorIterator<ANeonCityMainlandSite> It(World); It; ++It)
 {
  if (It->GetLevel() != World->PersistentLevel) continue;
  if (Found)
  {
   UE_LOG(LogTemp, Error, TEXT("Mainland transport placement requires one persistent-level site, found %s and %s."),
    *Found->GetPathName(), *It->GetPathName());
   break;
  }
  Found = *It;
 }
 return Found;
}

FTransform ANeonCityMainlandSite::ToWorldPose(const FTransform& LocalPose) const
{
 // A placement marker cannot rescale a physical hull or move its berth when an
 // artist adjusts the marker's widget scale. Both poses use rigid transforms.
 return FTransform(LocalPose.GetRotation(), LocalPose.GetLocation())
  * FTransform(GetActorQuat(), GetActorLocation());
}

FTransform ANeonCityMainlandSite::GetSpeedboatBerth() const
{
 return ToWorldPose(SpeedboatBerth);
}

FTransform ANeonCityMainlandSite::GetHelicopterLanding() const
{
 return ToWorldPose(HelicopterLanding);
}

FTransform ANeonCityMainlandSite::GetWeaponStorePlacement() const
{
 return ToWorldPose(WeaponStorePlacement);
}

void ANeonCityMainlandSite::ApplyOceanCoverage(UMaterialInstanceDynamic& OceanMaterial) const
{
 if(!bReplaceLegacyMainlandOceanCoverage)return;
 const FName Parameters[]={TEXT("Riviera915DryLandBounds"),TEXT("Riviera915MarinaNotch"),TEXT("Riviera915RoadConnector")};
 // Validate the complete existing seam before changing anything. An impossible
 // rectangle leaves ExistingCoverage authoritative for every world position.
 for(const FName Parameter:Parameters)
 {
  FLinearColor Previous;
  if(!OceanMaterial.GetVectorParameterValue(FMaterialParameterInfo(Parameter),Previous))
  {
   UE_LOG(LogTemp,Error,TEXT("Authored mainland ocean coverage parameter missing: %s"),*Parameter.ToString());
   return;
  }
 }
 for(const FName Parameter:Parameters)
  OceanMaterial.SetVectorParameterValue(Parameter,FLinearColor(1,1,-1,-1));
}

bool ANeonCityMainlandSite::GetFleetProfileReplacement(FGuid VehicleID,FName SavedProfile,FName& ReplacementProfile) const
{
 return ResolveFleetProfileReplacement(FleetProfileReplacements,VehicleID,SavedProfile,ReplacementProfile);
}

bool ANeonCityMainlandSite::ResolveFleetProfileReplacement(TConstArrayView<FNeonCityMainlandFleetReplacement> Replacements,
 FGuid VehicleID,FName SavedProfile,FName& ReplacementProfile)
{
 ReplacementProfile=NAME_None;
 // These three released island identities are protected even if accidentally
 // copied into a mainland authoring table. There is no hardcoded mainland fleet.
 const bool IslandIdentity=VehicleID==FGuid(0x5FF78036,0x45833C8D,0x593E77A5,0x0B006245)
  || VehicleID==FGuid(0x7F242340,0x469FEA4D,0x1A7D21A5,0xD5DED892)
  || VehicleID==FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8);
 if(!VehicleID.IsValid() || IslandIdentity)return false;
 const FNeonCityMainlandFleetReplacement* Match=nullptr;
 for(const auto& Entry:Replacements) if(Entry.VehicleID==VehicleID && Entry.PreviousProfile==SavedProfile)
 {
  // Old retired-city and C920 saves may name different previous profiles for
  // the same car. Only an exact identity/profile pair selects a rule; duplicate
  // pairs, including identical duplicates, still fail closed.
  if(Match)return false;
  Match=&Entry;
 }
 if(!Match)return false;
 const bool RetiredProfile=SavedProfile==TEXT("DA_StarterSedan") || SavedProfile==TEXT("DA_SportsCar")
  || SavedProfile==TEXT("DA_SUV") || SavedProfile==TEXT("DA_Pickup") || SavedProfile==TEXT("DA_HeroCarrera914");
 const bool SelectedHero=Match->ReplacementProfile==TEXT("DA_HeroAster")
  || Match->ReplacementProfile==TEXT("DA_HeroKestrel") || Match->ReplacementProfile==TEXT("DA_HeroNacre");
 const bool MainlandCoupeUpgrade=SavedProfile==TEXT("DA_HeroAster")
  && (Match->ReplacementProfile==TEXT("DA_HeroKestrel") || Match->ReplacementProfile==TEXT("DA_HeroNacre"));
 if((!RetiredProfile && !MainlandCoupeUpgrade) || !SelectedHero)return false;
 ReplacementProfile=Match->ReplacementProfile;
 return true;
}

void ANeonCityMainlandSite::ApplyLegacyTransportFixturePolicy(AActor& Helper) const
{
 if (!bReplaceLegacyTransportFixtures) return;
 // Remove collision as well as presentation, including the legacy aircraft
 // ImpactBarrier. Hiding meshes alone leaves an invisible wall in the new city.
 Helper.SetActorHiddenInGame(true);
 Helper.SetActorEnableCollision(false);
 TInlineComponentArray<UPrimitiveComponent*> Components(&Helper);
 for (auto* Component : Components)
 {
  Component->SetGenerateOverlapEvents(false);
  Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Component->SetCanEverAffectNavigation(false);
  Component->SetVisibility(false, true);
  Component->SetHiddenInGame(true, true);
 }
}
