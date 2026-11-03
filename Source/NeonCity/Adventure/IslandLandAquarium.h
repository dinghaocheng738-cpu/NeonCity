#pragma once
#include "CoreMinimal.h"
#include "Fishing/FishingStation.h"
#include "IslandLandAquarium.generated.h"

/** Presentation of the fish already owned in the shared fishing inventory. */
UCLASS()
class NEONCITY_API AIslandLandAquarium : public AFishingStation
{
 GENERATED_BODY()
public:
 AIslandLandAquarium();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName ContainerID=TEXT("Island.Mansion.Aquarium.Main");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bSaltwater=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector TankExtent=FVector(140,32,58);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector WaterCentre=FVector(0,0,138);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DisplayLumens=500;
 virtual FText GetInteractionName_Implementation() const override;
 bool TransferFish(FGuid ID);
 bool GetDisplayedFishTransform(FGuid ID,FTransform& Result) const;
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
private:
 UPROPERTY(Transient) TMap<FGuid,TObjectPtr<UStaticMeshComponent>> FishVisuals;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WaterSurface;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WaterBody;
 UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> TankLight;
 float RefreshClock=1;
};
