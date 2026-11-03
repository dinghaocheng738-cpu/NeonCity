#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/InteractableInterface.h"
#include "Adventure/IslandResidenceLifeSave.h"
#include "IslandHomePet.generated.h"
class UAnimSequence;class UTextRenderComponent;
/** One stable household companion using ordinary CMC and real floor/body
 * probes. Asset quality is independent of this bounded follow/stay mechanism. */
UCLASS()
class NEONCITY_API AIslandHomePet : public ACharacter,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandHomePet();
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) FName PetID=TEXT("Pet.Aurelia.Benny");
 UPROPERTY(EditAnywhere) bool bStartsOwned=true;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> IdleAnimation;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TrotAnimation;
 /** Mansion-relative cm; ordinary activity stays inside the public living floor. */
 UPROPERTY(EditAnywhere) TArray<FVector> HomePointsLocal;
 UPROPERTY(EditAnywhere) FVector SafeLocalMin=FVector(-70,-1600,390);
 UPROPERTY(EditAnywhere) FVector SafeLocalMax=FVector(900,850,470);
 UFUNCTION(BlueprintPure) uint8 GetActivityMode()const{return Mode;}
 UFUNCTION(BlueprintPure) bool IsOwnedPet()const{return bOwned;}
 FNeonCityResidencePetSaveData BuildPersistenceSnapshot(bool Defaults=false)const;
 void ApplyPersistenceSnapshot(const FNeonCityResidencePetSaveData& Data);
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 virtual void Tick(float Delta)override;
protected:
 virtual void BeginPlay()override;
private:
 bool InSafeRoom(const FVector& Position)const;
 bool HasSafeSupport(const FVector& Position,FVector& Supported)const;
 bool ClearBodyStep(const FVector& Direction,float Distance)const;
 void PresentAnimation();
 FTransform ResidenceOrigin;
 FTransform HomeTransform;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Prompt;
 uint8 Mode=0;
 bool bOwned=true,bPlayingTrot=false;
 int32 HomePoint=0;
 float HomeWait=3,StandNotice=0;
};
