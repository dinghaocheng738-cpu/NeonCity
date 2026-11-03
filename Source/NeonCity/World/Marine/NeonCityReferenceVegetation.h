#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "NeonCityReferenceVegetation.generated.h"

class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UNeonCityFoliageInteractionComponent;
class UNeonCityReferenceGrassComponent;
class UNeonCityReferenceGrassData;
class UNeonCityReferenceSoundscapeComponent;
class UNeonCityFoliageContactAudioComponent;

/** Stable, island-local source records. No generated actor names or runtime pointers. */
USTRUCT(BlueprintType)
struct FNeonCityReferencePlant
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform=FTransform::Identity;
 /** seed, height, scale, lean azimuth (canopy: source yaw). */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ShaderData0=FLinearColor(0,1,1,0);
 /** lean (canopy: signed vertical scale; shrubs negative), base X/Y/Z metres. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ShaderData1=FLinearColor(0,0,0,0);
};

USTRUCT(BlueprintType)
struct FNeonCityReferencePlantGroup
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceFamily;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FNeonCityReferencePlant> Instances;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FadeStartCm=12000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CullEndCm=14000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCastShadows=true;
};

/** Version-pinned plants: original scatter or a separately identified authored layer. */
UCLASS(BlueprintType)
class NEONCITY_API UNeonCityReferenceVegetationData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ReferenceCommit;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FNeonCityReferencePlantGroup> Groups;
 /** Optional for older full-geometry data; exact source view atlas/quad. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> CanopyImpostor;
 /** Original reduced palm, complementary to its 120 m detailed representation. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> PalmFar;
};

/** Native HISM ownership. One observer and shared materials, never per-plant Tick. */
UCLASS()
class NEONCITY_API ANeonCityReferenceVegetation : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityReferenceVegetation();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UFUNCTION(CallInEditor, BlueprintCallable, Category="NeonCity|Vegetation") bool RebuildVegetation();
 UFUNCTION(BlueprintPure, Category="NeonCity|Vegetation") int32 GetPlantCount() const;
 UFUNCTION(BlueprintPure, Category="NeonCity|Vegetation") int32 GetImpostorCount() const;
 UFUNCTION(BlueprintPure, Category="NeonCity|Vegetation") int32 GetFarPalmCount() const;
 UHierarchicalInstancedStaticMeshComponent* GetPlantGroup(int32 Index) const { return PlantGroups.IsValidIndex(Index)?PlantGroups[Index].Get():nullptr; }
 UNeonCityFoliageInteractionComponent* GetFoliageInteraction() const { return FoliageInteraction; }
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NeonCity|Vegetation") TObjectPtr<UNeonCityReferenceVegetationData> Data;
 /** Only the canonical island owner supplies a grass library; density layers do not duplicate it. */
 UPROPERTY(EditAnywhere, Category="NeonCity|Vegetation") TObjectPtr<UNeonCityReferenceGrassData> GrassData;
 /** The canonical island supplies its sound library; authored urban lawns only
  * need the existing grass observer and must not duplicate island ambience. */
 UPROPERTY(EditAnywhere, Category="NeonCity|Audio") bool bUseCanonicalGrassSoundscape=true;
 UPROPERTY(EditAnywhere, Category="NeonCity|Vegetation",meta=(ClampMin="0",ClampMax="25")) float WindSpeed=7;
 UPROPERTY(EditAnywhere, Category="NeonCity|Vegetation") FVector2D WindDirection=FVector2D(.6,.8);
 /** Native quality default retains all source crowns. Original browser uses .5. */
 UPROPERTY(EditAnywhere, Category="NeonCity|Vegetation",meta=(ClampMin="0",ClampMax=".5")) float DistantCanopyThinning=0;
private:
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Vegetation") TObjectPtr<UNeonCityFoliageInteractionComponent> FoliageInteraction;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Vegetation") TObjectPtr<UNeonCityReferenceGrassComponent> Grass;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Audio") TObjectPtr<UNeonCityReferenceSoundscapeComponent> Soundscape;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Audio") TObjectPtr<UNeonCityFoliageContactAudioComponent> ContactAudio;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Vegetation") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PlantGroups;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Vegetation") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ImpostorGroups;
 UPROPERTY(VisibleAnywhere, Category="NeonCity|Vegetation") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FarPalmGroups;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
 FVector2D GustOffset=FVector2D::ZeroVector;
 float WindRefreshElapsed=0;
 void InitializeMaterials();
 void UpdateMaterials();
 void UpdateObserver();
};
