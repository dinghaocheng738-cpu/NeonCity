#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "World/Marine/NeonCityReferenceGrassInstances.h"
#include "NeonCityReferenceGrass.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class APlayerController;

USTRUCT(BlueprintType)
struct FNeonCityReferenceGrassCell
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) int32 Index=0;
 UPROPERTY(EditAnywhere) float MinimumHeightM=0;
 UPROPERTY(EditAnywhere) float MaximumHeightM=0;
};

/** Exact source mask occupancy/terrain bounds; no per-blade actors or traces. */
UCLASS()
class NEONCITY_API UNeonCityReferenceGrassData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) FString ReferenceCommit;
 UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UStaticMesh>> PatchMeshes;
 UPROPERTY(EditAnywhere) TArray<FNeonCityReferenceGrassCell> Cells;
 UPROPERTY(EditAnywhere) float MaximumPlantRadiusM=4;
 /** Opt-in runtime instance deltas. Existing island assets keep their retained
  * adapter unless explicitly authored and validated with this policy. */
 UPROPERTY(EditAnywhere,Category="Grass") bool bIncrementalInstances=false;
};

/** Source GrassField adapter. Updated by the existing vegetation observer/wind. */
UCLASS()
class NEONCITY_API UNeonCityReferenceGrassComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityReferenceGrassComponent();
 bool InitializeGrass(UNeonCityReferenceGrassData* InData);
 void SetWind(FVector2D Direction,float Speed,FVector2D GustOffset);
 void UpdateObserver(const FVector& Eye,APlayerController* Controller);
 int32 GetActiveCellCount(int32 Level) const;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UNeonCityReferenceGrassData> Data;
 UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Patches;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
 TArray<int32> CellLookup;
 TArray<int32> ActiveCells[3];
 FNeonCityReferenceGrassInstances CellInstances[3];
 FMatrix LastProjection=FMatrix::Identity;
 FVector LastEye=FVector::ZeroVector;
 bool bHasView=false;
 bool bUseIncrementalInstances=false;
};
