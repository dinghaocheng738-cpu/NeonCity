#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WantedLevelData.generated.h"
USTRUCT(BlueprintType)
struct FNeonCityWantedLevel
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 EvidenceThreshold=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 VehicleCount=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float SearchRadius=600;
};
/** Six response tiers. Higher-tier forces are extension signals, not spawned units. */
UCLASS(BlueprintType)
class NEONCITY_API UWantedLevelData : public UDataAsset
{
 GENERATED_BODY()
public:
 UWantedLevelData();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FNeonCityWantedLevel> Levels;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float LostSightGrace=8;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float DecayInterval=6;
 bool IsValidProfile() const;
};
