#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "FishingStation.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ENeonCityFishingStation : uint8 { CastingPoint, FishMarket, Grill, Aquarium };

/** Authored point, optionally attached to a slow moving deck. It never moves the player. */
UCLASS()
class NEONCITY_API AFishingStation : public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AFishingStation();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName StationID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityFishingStation Kind=ENeonCityFishingStation::CastingPoint;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName Habitat=TEXT("Coastal");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector CastTargetLocal=FVector(500,0,0);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> InteractionBounds;
 bool IsInReach(const AActor* Visitor) const;
 bool ResolveWaterTarget(FVector& Target) const;
 bool IsFacingWater(const AActor* Visitor) const;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
};
