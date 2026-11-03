#include "Interaction/InteractableInterface.h"

bool IInteractableInterface::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

FText IInteractableInterface::GetInteractionName_Implementation() const
{
	return FText::GetEmpty();
}

void IInteractableInterface::Interact_Implementation(AActor* Interactor)
{
}
