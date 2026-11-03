#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityPhysicsTestArea.generated.h"
UCLASS()
class NEONCITY_API ANeonCityPhysicsTestArea : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityPhysicsTestArea() { PrimaryActorTick.bCanEverTick=false; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
};
