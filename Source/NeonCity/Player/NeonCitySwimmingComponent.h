#pragma once
#include "CoreMinimal.h"
#include "Water/SwimmingComponent.h"
#include "NeonCitySwimmingComponent.generated.h"
/** Minimal input bridge to UE swimming. Walking, falling and jump implementations stay unchanged. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCitySwimmingComponent : public USwimmingComponent
{
 GENERATED_BODY()
public:
 // Retain the Phase 18A serialized class/subobject identity; implementation lives in Water/.
};
