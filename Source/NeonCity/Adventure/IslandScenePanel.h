#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "IslandScenePanel.generated.h"
class UBoxComponent; class UStaticMeshComponent; class UTextRenderComponent;
class AIslandLandLight;
/** Three bounded physical lighting presets, confined to explicit room lamps. */
UCLASS()
class NEONCITY_API AIslandScenePanel:public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandScenePanel();
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) FName ZoneID;
 UPROPERTY(EditAnywhere) FText ZoneName=FText::FromString(TEXT("Leisure room"));
 UPROPERTY(EditInstanceOnly) TArray<TObjectPtr<AIslandLandLight>> Lights;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Face;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Display;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 bool SetScene(int32 Index);
 int32 GetScene()const{return Scene;}
protected:
 virtual void BeginPlay()override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 int32 Scene=0;
 void Apply();
};
