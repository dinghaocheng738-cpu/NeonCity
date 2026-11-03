#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "NeonCityFishingBoat.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class AFishingStation;

/** Working boat built from the licensed canonical source; shares marine ownership, input and damage. */
UCLASS()
class NEONCITY_API ANeonCityFishingBoat : public ANeonCityMarineVehicleBase
{
 GENERATED_BODY()
public:
 ANeonCityFishingBoat();
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Interactor) override;
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 AFishingStation* GetDeckFishingStation() const { return DeckFishing; }
 bool CanBoard(const ACharacter* Character) const;
 bool Board(ACharacter* Character);
protected:
 virtual void PostInitializeComponents() override;
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 bool IsOnDeck(const ACharacter* Character) const;
 bool FindBoardingTransform(const ACharacter* Character,FTransform& Out) const;
 UPROPERTY() TArray<TObjectPtr<UBoxComponent>> DeckCollision;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BoatParts;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SurfaceMaterials;
 UPROPERTY() TObjectPtr<USpotLightComponent> DeckLight;
 UPROPERTY() TObjectPtr<AFishingStation> DeckFishing;
 float PresentationTime=0,PropAngle=0;
};
