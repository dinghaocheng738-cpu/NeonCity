#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehicleCombatTestArea.generated.h"
UCLASS()
class NEONCITY_API AVehicleCombatTestArea : public AActor
{
 GENERATED_BODY()
public:
 AVehicleCombatTestArea();
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
};
