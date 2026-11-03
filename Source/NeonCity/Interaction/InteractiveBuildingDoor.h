#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "InteractiveBuildingDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class NEONCITY_API AInteractiveBuildingDoor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
public:
	AInteractiveBuildingDoor();
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionName_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="NeonCity|Interaction")
	bool IsOpen() const { return bIsOpen; }

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Hinge;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleInstanceOnly, Category="NeonCity|Interaction")
	bool bIsOpen = false;
};
