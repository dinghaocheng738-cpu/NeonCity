#pragma once
#include "CoreMinimal.h"
#include "DestructionTypes.generated.h"
/** Unified environmental state for every breakable surface, prop and structure part. */
UENUM(BlueprintType)
enum class EPhysicsPropState : uint8 { Normal, Damaged, Broken };
/** Glass keeps a visible intermediate stage instead of jumping from intact to gone. */
UENUM(BlueprintType)
enum class ENeonCityGlassState : uint8 { Normal, Cracked, Broken };