#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceTestArea.generated.h"
UCLASS()
class NEONCITY_API APoliceTestArea : public AActor
{
 GENERATED_BODY()
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
};
