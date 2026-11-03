#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarineTestArea.generated.h"
/** Legacy dock fixtures and canonical marine bootstrap; authored sites replace its presentation. */
UCLASS()
class NEONCITY_API AMarineTestArea : public AActor
{
 GENERATED_BODY()
public:
 AMarineTestArea();
protected:
 virtual void PostInitializeComponents() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
};
