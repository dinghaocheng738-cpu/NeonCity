#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityCoastalRegion.generated.h"

class ULevelStreaming;

/** Authored connection between the persistent coast and one bounded offshore region.
 * Uses UE level streaming and the existing island's Landscape/HISM LODs. */
UCLASS()
class NEONCITY_API ANeonCityCoastalRegion : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityCoastalRegion();
 UPROPERTY(EditAnywhere, Category="Coastal Region") TSoftObjectPtr<UWorld> IslandLevel;
 UPROPERTY(EditAnywhere, Category="Coastal Region", meta=(ClampMin="300000")) float LoadDistanceCm = 600000;
 UPROPERTY(EditAnywhere, Category="Coastal Region", meta=(ClampMin="350000")) float UnloadDistanceCm = 750000;
 bool IsInitialRegionReady() const;
 /** Explicit load boundary only: make destination collision available before save validation. */
 bool EnsureDestinationReady(const FVector& Destination);
 FTransform GetFishingBerth() const;
 static ANeonCityCoastalRegion* Find(const UWorld* World);
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void ApplyIslandMaterialOrigin();
 void RefreshStreaming();
 UPROPERTY(Transient) TObjectPtr<ULevelStreaming> IslandStream;
 FTimerHandle StreamingTimer;
 bool bInitialRegionReady = false;
 double PersistenceHoldUntil = 0;
};
