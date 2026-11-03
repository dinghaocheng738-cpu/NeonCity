#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PoliceUnitManager.generated.h"
class APoliceCharacter;
class APoliceVehicle;
/** Registry of dispatch-owned units. Individual actors have no spawning authority. */
UCLASS()
class NEONCITY_API UPoliceUnitManager : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY() TArray<TObjectPtr<APoliceCharacter>> Officers;
 UPROPERTY() TArray<TObjectPtr<APoliceVehicle>> Vehicles;
};
