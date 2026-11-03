#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "NeonCityGarageMarker.generated.h"
class UTextRenderComponent;
class UStaticMesh;
class UStaticMeshComponent;
/** Small interaction post for a physical parking slot. Never moves or hides vehicles. */
UCLASS()
class NEONCITY_API ANeonCityGarageMarker : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 ANeonCityGarageMarker();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Garage") FName PropertyID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Garage") int32 SlotIndex=0;
 /** Optional authored presentation; unset markers retain their existing default post. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Garage|Presentation") TObjectPtr<UStaticMesh> PostMesh=nullptr;
 /** Absolute local pose of the authored mesh, including its fitted scale. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Garage|Presentation") FTransform PostLocalTransform=FTransform::Identity;
 UFUNCTION(BlueprintCallable,Category="Garage|Presentation") void ApplyAuthoredPresentation();
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaTime) override;
private:
 UPROPERTY(VisibleAnywhere,Category="Garage|Presentation") TObjectPtr<UStaticMeshComponent> Post;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
};
