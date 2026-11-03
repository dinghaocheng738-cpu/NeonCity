#pragma once
#include "CoreMinimal.h"
class AActor;
class USceneComponent;
class UStaticMeshComponent;

/** Presentation-only asset adapter. Never changes mass, collision, ownership or save IDs. */
namespace NeonCityVisuals
{
 NEONCITY_API bool Apply(UStaticMeshComponent* Component, const TCHAR* Category, const TCHAR* Name);
 /** Constructor-only. Old parts are retained as an explicit missing-asset fallback. */
 NEONCITY_API UStaticMeshComponent* Create(AActor& Owner, USceneComponent* Parent, const TCHAR* Category,
  const TCHAR* Name, const TArray<FName>& ReplacedParts, FName ComponentName=TEXT("ArtVisual"));
}
