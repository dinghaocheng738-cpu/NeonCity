#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interaction/InteractableInterface.h"
#include "NeonCityPropertySign.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UNeonCityPropertyData;

/** The "for sale" board in front of a property on the market (a Hefang Street shop, a Longjing tea estate). F buys
 * it through the property registry (cash, then the bank); once bought the board shows it is yours and what it earns
 * a day, and the business is paid every midnight. */
UCLASS()
class NEONCITY_API ANeonCityPropertySign : public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 ANeonCityPropertySign();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") TObjectPtr<UNeonCityPropertyData> Data;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Board;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Caption;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Prompt;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 bool IsOwned() const;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float Delta) override;
private:
 UFUNCTION() void OnPropertyChanged(FName PropertyID);
 void Refresh();
};

/** Pays the owned businesses at midnight (UPropertySubsystem::SettleBusinessIncome) and tells the player. */
UCLASS()
class NEONCITY_API UNeonCityBusinessSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void OnWorldBeginPlay(UWorld& InWorld) override;
 void PayDay(int32 GameDay);
};
