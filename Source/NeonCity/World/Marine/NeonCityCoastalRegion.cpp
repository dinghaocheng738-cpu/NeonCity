#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/SceneComponent.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

ANeonCityCoastalRegion::ANeonCityCoastalRegion()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CoastalRegionOrigin")));
 IslandLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island.NeonCity_Tidewater_Island")));
}

ANeonCityCoastalRegion* ANeonCityCoastalRegion::Find(const UWorld* World)
{
 if (World) for (TActorIterator<ANeonCityCoastalRegion> It(World); It; ++It) return *It;
 return nullptr;
}

void ANeonCityCoastalRegion::BeginPlay()
{
 Super::BeginPlay();
 const FString IslandName = FPackageName::GetShortName(IslandLevel.ToSoftObjectPath().GetLongPackageName());
 for (auto* Stream : GetWorld()->GetStreamingLevels())
  if (Stream && !IslandName.IsEmpty() && Stream->GetWorldAssetPackageName().EndsWith(IslandName))
  {
   IslandStream = Stream;
   break;
  }
 if (!IslandStream)
 {
  UE_LOG(LogTemp, Error, TEXT("Coastal region has no authored island stream; mainland retained, region not ready."));
  return;
 }
 IslandStream->OnLevelShown.AddDynamic(this, &ANeonCityCoastalRegion::ApplyIslandMaterialOrigin);
 IslandStream->SetShouldBeLoaded(true);
 IslandStream->SetShouldBeVisible(true);
 if (IslandStream->IsLevelVisible()) ApplyIslandMaterialOrigin();
 GetWorldTimerManager().SetTimer(StreamingTimer, this, &ANeonCityCoastalRegion::RefreshStreaming, .5f, true);
}

void ANeonCityCoastalRegion::ApplyIslandMaterialOrigin()
{
 if (!IslandStream || !IslandStream->GetLoadedLevel()) return;
 const FVector Origin = GetActorLocation();
 bool bTerrainReady = false;
 for (auto Actor : IslandStream->GetLoadedLevel()->Actors)
  if (auto* Terrain = Cast<ALandscapeProxy>(Actor))
  {
   // Only the world-coordinate material adapter changes; source heightfield,
   // vegetation, physics and origin-local assets remain the same.
   if (!Terrain->bUseDynamicMaterialInstance)
   {
    UE_LOG(LogTemp, Error, TEXT("Streamed island requires authored Landscape dynamic materials."));
    continue;
   }
   Terrain->SetLandscapeMaterialVectorParameterValue(TEXT("IslandOriginCm"),
    FLinearColor(float(Origin.X), float(Origin.Y), float(Origin.Z), 0));
   bTerrainReady = true;
  }
 bInitialRegionReady |= bTerrainReady;
}

void ANeonCityCoastalRegion::RefreshStreaming()
{
 if (!IslandStream || !bInitialRegionReady || GetWorld()->GetTimeSeconds() < PersistenceHoldUntil) return;
 const auto* PC = GetWorld()->GetFirstPlayerController();
 if (!PC || !PC->GetPawn()) return;
 // Look-ahead avoids a late request during fast flight. Separate unload radius
 // prevents boundary thrashing. Only this offshore region is controlled here.
 const FVector Observer = PC->GetPawn()->GetActorLocation() + PC->GetPawn()->GetVelocity() * 5;
 const double DistanceSquared = FVector::DistSquared(Observer, GetActorLocation());
 const float Load = FMath::Max(300000.f, LoadDistanceCm);
 const float Unload = FMath::Max(Load + 50000.f, UnloadDistanceCm);
 if (DistanceSquared < FMath::Square(Load))
 {
  IslandStream->SetShouldBeLoaded(true);
  IslandStream->SetShouldBeVisible(true);
 }
 else if (DistanceSquared > FMath::Square(Unload))
 {
  IslandStream->SetShouldBeVisible(false);
  IslandStream->SetShouldBeLoaded(false);
 }
}

bool ANeonCityCoastalRegion::IsInitialRegionReady() const
{
 return bInitialRegionReady;
}

bool ANeonCityCoastalRegion::EnsureDestinationReady(const FVector& Destination)
{
 if (Destination.ContainsNaN()) return false;
 // Canonical terrain spans 2 km. This bound includes its shore/pier without
 // forcing the offshore level to load for an unrelated mainland destination.
 const FVector Local = Destination - GetActorLocation();
 if (FMath::Abs(Local.X) > 130000 || FMath::Abs(Local.Y) > 130000) return true;
 if (!IslandStream) return false;
 PersistenceHoldUntil = GetWorld()->GetTimeSeconds() + 10;
 IslandStream->SetShouldBeLoaded(true);
 IslandStream->SetShouldBeVisible(true);
 // An explicit disk load may wait for this one region. Never done in Tick,
 // autosave or the ordinary async distance-based streaming update.
 if (!IslandStream->IsLevelVisible()) GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
 if (!IslandStream->IsLevelVisible()) return false;
 ApplyIslandMaterialOrigin();
 return bInitialRegionReady;
}

FTransform ANeonCityCoastalRegion::GetFishingBerth() const
{
 // Canonical source berth (64.5, 0, 36.5) m, source Y-up -> UE Z-up.
 // Source forward +Z is native forward -Y; keep scale/rotation unchanged.
 return FTransform(FRotator(0, -90, 0), GetActorLocation() + FVector(6450, -3650, 0));
}

void ANeonCityCoastalRegion::EndPlay(EEndPlayReason::Type Reason)
{
 GetWorldTimerManager().ClearTimer(StreamingTimer);
 if (IslandStream) IslandStream->OnLevelShown.RemoveDynamic(this, &ANeonCityCoastalRegion::ApplyIslandMaterialOrigin);
 Super::EndPlay(Reason);
}
