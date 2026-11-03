#pragma once
#include "CoreMinimal.h"
#include "PoliceState.generated.h"
UENUM(BlueprintType)
enum class ENeonCityPoliceState : uint8 { Patrol, Investigate, Search, Chase };
