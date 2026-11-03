#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandAdventureLighting.generated.h"
class UBoxComponent;
class UPostProcessComponent;
/** Bounded mountain exposure with a daylight floor; actual night remains dark. */
UCLASS()
class NEONCITY_API AIslandAdventureLighting:public AActor
{
 GENERATED_BODY()
public:
 AIslandAdventureLighting();
 virtual void Tick(float Delta)override;
protected:
 virtual void BeginPlay()override;
private:
 void UpdateExposure();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Bounds;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Exposure;
};
