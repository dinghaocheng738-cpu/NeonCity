#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AircraftTestArea.generated.h"
UCLASS()
class NEONCITY_API AAircraftTestArea : public AActor
{
 GENERATED_BODY()
public:
 AAircraftTestArea();
protected:
 virtual void PostInitializeComponents() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<AActor> Aircraft;
};
