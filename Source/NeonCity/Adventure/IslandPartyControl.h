#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "Adventure/IslandResidenceLifeSave.h"
#include "IslandPartyControl.generated.h"
class UBoxComponent;class UAudioComponent;class USoundBase;class ANeonCityNPCCharacter;class UStaticMeshComponent;

/** Four adult visitors use physical character movement along a surveyed deck
 * loop. Start/stop belong to the existing owned property and shared save. */
UCLASS()
class NEONCITY_API AIslandPartyControl:public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandPartyControl();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Panel;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Music;
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 /** Ground-level deck points, with clear floor and capsule space surveyed by
  * native authoring. A common loop keeps guests clear of the pool and BBQ. */
 UPROPERTY(EditAnywhere) TArray<FVector> DeckRouteLocal;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<USoundBase> PartyMusic;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 bool IsActive()const{return bActive;}
 bool IsEnding()const{return bEnding;}
 int32 GetGuestCount()const;
 /** Evidence only: no actor/input/activity changes. Writes the actual guest
  * blockers and densely sampled authored floor/capsule route. */
 bool WriteReadOnlyDiagnostics(const FString& Directory,bool IncludeRoute=true)const;
 FNeonCityResidencePartySaveData BuildPersistenceSnapshot(bool Defaults=false)const;
 void ApplyPersistenceSnapshot(const FNeonCityResidencePartySaveData& State);
protected:
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 struct FVisitor
 {
  TWeakObjectPtr<ANeonCityNPCCharacter> Pawn;
  FName ID;
  int32 Station=0;
  float Wait=0,ProgressSeconds=0,LastDistance=0;
  float AvoidSide=0,AvoidSeconds=0;
 };
 bool SupportedPoint(FVector Local,FVector& Location,const AActor* Ignore=nullptr)const;
 bool SpawnVisitor(int32 Index,const FNeonCityPartyGuestSaveData* Saved=nullptr);
 void ClearGuests();
 void BeginEnding();
 void Notice(const FString& Text)const;
 TArray<FVisitor> Guests;
 bool bActive=false,bEnding=false;
 float Arrivals=0;
 int32 NextArrival=0;
};
