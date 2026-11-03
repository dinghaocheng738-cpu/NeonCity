#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OceanHabitat.generated.h"
class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;
USTRUCT(BlueprintType)
struct FOceanHabitatLayer
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere) TArray<FTransform> Instances;
 UPROPERTY(EditAnywhere) bool bSolid=false;
};
/** Cooked, terrain-conforming habitat patches; one hierarchical draw group per asset. */
UCLASS()
class NEONCITY_API AOceanHabitat:public AActor
{
 GENERATED_BODY()
public:
 AOceanHabitat();
 UPROPERTY(EditAnywhere,Category="Habitat") TArray<FOceanHabitatLayer> HabitatLayers;
 UPROPERTY(EditAnywhere,Category="Habitat") bool bStreamed=false;
 virtual void OnConstruction(const FTransform& Transform)override;
 UFUNCTION(BlueprintCallable) void RebuildHabitat();
protected:
 virtual void BeginPlay()override;
private:
 UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Groups;
};
