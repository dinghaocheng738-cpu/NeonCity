#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "WeaponStoreActor.generated.h"
class UStaticMesh;
/** Small interaction/debug shop. No new map, shop interior or art dependency. */
UCLASS()
class NEONCITY_API AWeaponStoreActor : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 AWeaponStoreActor();
 bool CanInteract_Implementation(AActor* Interactor) const override;
 FText GetInteractionName_Implementation() const override;
 void Interact_Implementation(AActor* Interactor) override;
 bool Buy(AActor* Buyer,int32 Slot,bool Ammo);
 FText Describe(AActor* Buyer) const;
 /** Existing shop service fitted onto an authored physical terminal. Placement is its support plane. */
 void ApplyAuthoredPresentation(UStaticMesh* Mesh,const FText& Title);
private:
 FText StoreTitle=FText::FromString(TEXT("WEAPON TEST STORE"));
};
