#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IslandLandAuthoringLibrary.generated.h"
class ALandscapeProxy;
class ALandscape;
class UMaterial;
class UTexture2D;
/** Read the native heightfield itself, unaffected by newly placed props. */
UCLASS()
class NEONCITYARTEDITOR_API UIslandLandAuthoringLibrary:public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static double TerrainHeightCm(ALandscapeProxy* Terrain,FVector2D PositionCm);
 /** Read-only immutable export of the current island base edit layer and
  * composed heights. No sculpt, refresh, collision rebuild or level save. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool ExportTheOneLandscapeBaseline(ALandscape* Terrain);
 /** Exact estate/arrival candidates: old revisions require QA; the bounded
  * SummitShoulders788 development revision requires its explicit launch opt-in.
  * Whole-grid baseline/delta/remainder checks precede every native mutation. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool ApplyTheOneLandscapeCandidate(ALandscape* Terrain,FName Revision=NAME_None);
 /** Explicit bounded reviewer input; no navigation, velocity or actor changes. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool HoldReviewKey(FName Key,float Seconds);
 /** One simultaneous normal key chord, with the same finite auto release. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool HoldReviewKeys(const TArray<FName>& Keys,float Seconds);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool StartReviewMetrics(const FString& Label);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static void StopReviewMetrics();
 /** Passive QA telemetry; never changes the mature fishing session. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static FString InspectReviewFishing();
 /** Read-only saved-position collision diagnosis in the current normal PIE. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static FString InspectReviewSave(const FString& Slot);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static FString InspectTerrainMaterials(ALandscapeProxy* Terrain);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool RefreshTerrainDynamicMaterials(ALandscapeProxy* Terrain);
 /** Owned upland sculpt only; immutable protected height samples are checked. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool ApplyMountainRelief(ALandscape* Terrain);
 /** Bounded road earthwork: validates unchanged island samples and current base. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool ApplySummitRoadRelief(ALandscape* Terrain);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool ApplyHikeRoadBatter(ALandscape* Terrain);
 /** Exact 36 diagnosed heightfield vertices; road bed and outside remain intact. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool RemoveHikeDownhillNeedles(ALandscape* Terrain);
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandAuthoring")
 static bool BindGrassHeightDerivative(UMaterial* Material,UTexture2D* Height);
};
