#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "IslandGarageControl.generated.h"
class UBoxComponent;class UStaticMeshComponent;class UTextRenderComponent;class AIslandGarageDoor;class USpotLightComponent;
UENUM(BlueprintType)
enum class EIslandGarageAction:uint8 { RegisterParked, ReleaseParked, RetrieveStored, DisplayLighting, Door };
UCLASS()
class NEONCITY_API AIslandGarageControl : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandGarageControl();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Face;
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) EIslandGarageAction Action=EIslandGarageAction::RegisterParked;
 UPROPERTY(EditAnywhere) int32 SlotIndex=0;
 UPROPERTY(EditAnywhere) FTransform RetrievalBayLocal;
 UPROPERTY(EditAnywhere) TObjectPtr<AIslandGarageDoor> Door;
 UPROPERTY(EditAnywhere) TArray<TObjectPtr<USpotLightComponent>> DisplayLights;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 virtual void Tick(float Delta)override;
 FGuid FindStoredOwnedVehicle()const;
 bool IsDisplayLit()const{return bDisplayLit;}
private:
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
 bool bDisplayLit=true;
};
