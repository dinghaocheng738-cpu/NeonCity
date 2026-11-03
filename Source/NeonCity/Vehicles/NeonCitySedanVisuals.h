#pragma once
#include "CoreMinimal.h"
class AActor;
class USceneComponent;
class UStaticMeshComponent;
class UIslandSupercarPresentationData;
/** Shared fleet appearance; called only from actor constructors. No movement or physics policy. */
struct FNeonCitySedanVisuals
{
 static TArray<TObjectPtr<UStaticMeshComponent>> Create(AActor& Owner, USceneComponent* Parent, const TCHAR* Style=TEXT("Sedan"));
 static UIslandSupercarPresentationData* GetFleetPresentation();
};
