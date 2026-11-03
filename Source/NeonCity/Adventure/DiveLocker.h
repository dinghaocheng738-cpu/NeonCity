#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "Blueprint/UserWidget.h"
#include "DiveLocker.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UTextBlock;
UCLASS()
class NEONCITY_API UDivingStationPrompt : public UUserWidget
{
 GENERATED_BODY()
};
UCLASS()
class NEONCITY_API ADiveLocker : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 ADiveLocker();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionTarget;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Prompt;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 virtual void Tick(float DeltaSeconds) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UDivingStationPrompt> ScreenPrompt;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptLabel;
};
