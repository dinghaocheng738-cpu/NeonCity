#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IslandLandCollisionLibrary.generated.h"

class UStaticMesh;
class UWorld;

/** Editor-module tooling for separately imported island guard collision candidates. */
UCLASS()
class NEONCITYARTEDITOR_API UIslandLandCollisionLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Returns technical JSON. No package is saved and no parent render Build is performed. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandCollision")
 static FString ConfigureGuardCollision(UStaticMesh* Parent,UStaticMesh* Proxy);
 /** Read-only geometry/material/cooked-Chaos evidence, suitable for a fresh process. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandCollision")
 static FString InspectGuardCollision(UStaticMesh* Parent);
 /** Configure only a newly imported independent guard; no road parent is modified. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandCollision")
 static FString ConfigureIndependentGuardCollision(UStaticMesh* Parent,UStaticMesh* Guard);
 /** Read-only native cooked evidence for the separately filtered guard. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandCollision")
 static FString InspectIndependentGuardCollision(UStaticMesh* Guard);
 /** Refuse shared, partitioned or external-actor maps before isolated authoring. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|LandCollision")
 static FString ValidateGuardAuthoringWorld(UWorld* World);
};
