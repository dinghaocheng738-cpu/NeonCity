#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandHeroDisplayLight.generated.h"
class UStaticMeshComponent;
class USpotLightComponent;
class UBoxComponent;
class UPostProcessComponent;
UCLASS()
class NEONCITY_API AIslandHeroDisplayLight : public AActor
{
 GENERATED_BODY()
public:
 AIslandHeroDisplayLight();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Fixture;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GroundStand;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Light;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> MeteringBounds;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Metering;
protected:
 virtual void BeginPlay()override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
};
