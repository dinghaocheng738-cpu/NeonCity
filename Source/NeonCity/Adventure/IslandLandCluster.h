#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandLandCluster.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
UCLASS()
class NEONCITY_API AIslandLandCluster : public AActor
{
 GENERATED_BODY()
public:
 AIslandLandCluster();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
