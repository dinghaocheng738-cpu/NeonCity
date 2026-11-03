#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReplicaAssetLibrary.generated.h"
class UStaticMesh;
class UMaterialInterface;
class UIslandSupercarPresentationData;

/** Scoped authoring utility. Never changes gameplay, levels or player saves. */
UCLASS()
class NEONCITYARTEDITOR_API UReplicaAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Batch presentation policy before rebuilding; caller owns package saving. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Replica")
    static FString ConfigureRoofPresentationMesh(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials);
    /** Contact uses a separate owned copy; the reviewed original stays immutable. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Editor|Replica")
    static FString ConfigureReplicaRoofContact(UStaticMesh* Mesh, UIslandSupercarPresentationData* Profile);
};
