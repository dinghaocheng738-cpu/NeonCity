#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "Property/InteriorInterface.h"
#include "NeonCityStarterApartment.generated.h"
class UPropertyComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class ACharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityInteriorChanged,AActor*,Visitor,bool,bInside);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityHomeServiceRequested,AActor*,Visitor,ENeonCityHomeService,Service);

/** A short doorway transition into the existing building, with collision-safe destinations. */
UCLASS()
class NEONCITY_API ANeonCityStarterApartment : public AActor, public IInteractableInterface, public IInteriorInterface
{
 GENERATED_BODY()
public:
 ANeonCityStarterApartment();
 UFUNCTION(BlueprintPure,Category="Property") UPropertyComponent* GetPropertyComponent() const { return Property; }
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 virtual bool CanEnterInterior_Implementation(AActor* Visitor) const override;
 virtual bool EnterInterior_Implementation(AActor* Visitor) override;
 virtual bool ExitInterior_Implementation(AActor* Visitor) override;
 virtual bool IsInsideInterior_Implementation(AActor* Visitor) const override;
 virtual bool RequestHomeService_Implementation(AActor* Visitor,ENeonCityHomeService Service) override;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property|Interior") FTransform EntryLocal=FTransform(FRotator(0,90,0),FVector(0,240,99));
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property|Interior") FTransform ExitLocal=FTransform(FRotator(0,-90,0),FVector(0,-250,99));
 UPROPERTY(BlueprintAssignable) FNeonCityInteriorChanged OnInteriorChanged;
 UPROPERTY(BlueprintAssignable) FNeonCityHomeServiceRequested OnHomeServiceRequested;
protected:
 virtual void Tick(float DeltaTime) override;
private:
 bool IsResident(AActor* Visitor) const;
 bool IsAtDoor(AActor* Visitor) const;
 bool MoveThroughDoor(ACharacter* Visitor,const FTransform& LocalDestination);
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPropertyComponent> Property;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Door;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteriorBounds;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Prompt;
};
