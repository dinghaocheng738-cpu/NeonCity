#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class NEONCITY_API UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API IInteractableInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Interaction")
	FText GetInteractionName() const;

	/** Implementations must revalidate eligibility/authority before changing state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Interaction")
	void Interact(AActor* Interactor);

	virtual bool CanInteract_Implementation(AActor* Interactor) const;
	virtual FText GetInteractionName_Implementation() const;
	virtual void Interact_Implementation(AActor* Interactor);
};
