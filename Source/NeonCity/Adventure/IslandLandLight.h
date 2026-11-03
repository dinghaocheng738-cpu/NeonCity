#pragma once
#include "CoreMinimal.h"
#include "Engine/PointLight.h"
#include "IslandLandLight.generated.h"
UCLASS()
class NEONCITY_API AIslandLandLight : public APointLight
{
 GENERATED_BODY()
public:
 AIslandLandLight();
 UPROPERTY(EditAnywhere) float Lumens=850;
 UPROPERTY(EditAnywhere) float DayMultiplier=0;
 UPROPERTY(EditAnywhere) bool bBudgetShadows=false;
 /** Architectural landmark fixtures can remain visible beyond nearby path lamps. */
 UPROPERTY(EditAnywhere,meta=(ClampMin="1",Units="cm")) float CullDistance=6000;
 UPROPERTY(EditAnywhere,meta=(ClampMin="0",Units="cm")) float ShadowDistance=1400;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
};
