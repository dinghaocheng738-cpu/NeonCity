#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "IslandHomeWardrobe.generated.h"
class UBoxComponent;
class UTextRenderComponent;
UCLASS()
class NEONCITY_API AIslandHomeWardrobe : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandHomeWardrobe();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 virtual void Tick(float Delta)override;
private:
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
};
