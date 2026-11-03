#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "NeonCityInteractionTestDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Minimal opt-in test actor using the engine cube; never auto-spawned in the game. */
UCLASS()
class NEONCITY_API ANeonCityInteractionTestDoor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
public:
	ANeonCityInteractionTestDoor();
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionName_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="NeonCity|Interaction")
	bool IsOpen() const { return bIsOpen; }

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleInstanceOnly, Category="NeonCity|Interaction")
	bool bIsOpen = false;
};
