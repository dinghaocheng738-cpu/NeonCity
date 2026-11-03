#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "IslandLandDoor.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;

/** A physical hinged door in the same world as its interior. */
UCLASS()
class NEONCITY_API AIslandLandDoor : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandLandDoor();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Leaf;
 UPROPERTY(EditAnywhere) FText RoomName;
 UPROPERTY(EditAnywhere) float OpenAngle=100;
 UPROPERTY(EditAnywhere) float Width=120;
 UPROPERTY(EditAnywhere) float Height=230;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 UFUNCTION(BlueprintPure) bool IsOpen() const { return FMath::Abs(Angle)>80; }
protected:
 virtual void Tick(float Delta) override;
private:
 void RefreshPrompt();
 virtual void BeginPlay() override;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
 FTimerHandle PromptTimer;
 float Angle=0;
 bool bOpening=false;
};
