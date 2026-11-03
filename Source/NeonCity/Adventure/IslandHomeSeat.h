#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "IslandHomeSeat.generated.h"
class UBoxComponent;class UTransportOccupantComponent;class ANeonCityPlayerCharacter;class UAnimSequence;
/** A real seated rest on the existing character, licensed sitting animation
 * and shared player/save domains. The visible furniture retains its collision. */
UCLASS()
class NEONCITY_API AIslandHomeSeat:public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandHomeSeat();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTransportOccupantComponent> Occupant;
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) FText SeatName=FText::FromString(TEXT("Lounge chair"));
 UPROPERTY(EditAnywhere) FVector SeatHipLocal=FVector(0,0,62);
 UPROPERTY(EditAnywhere) float SeatBodyYaw=0;
 /** Optional visual fit to a particular mattress. Standing entry/exit and the
  * player capsule/save anchor remain unchanged; existing beds default to zero. */
 UPROPERTY(EditAnywhere) FVector BedRestOffset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,meta=(ClampMin="-12",ClampMax="12")) float BedRestPitch=0;
 /** Opt-in framing through the player's existing collision-aware orbit boom. */
 UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="100")) float SeatedViewLift=0;
 UPROPERTY(EditAnywhere) float SeatedViewYaw=0;
 UPROPERTY(EditAnywhere) float SeatedViewPitch=0;
 /** Bed uses the same ownership, health, player and safe standing save domains. */
 UPROPERTY(EditAnywhere) bool bSleepBed=false;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> BedEnter;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> BedSleep;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> BedExit;
 bool IsSleeping()const{return IsOccupied() && bSleepBed && BedStage==2;}
 float GetSleptHours()const{return SleptHours;}
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 bool IsOccupied()const{return User.IsValid();}
 bool FindPersistenceExit(FTransform& Transform)const;
 bool Leave();
protected:
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 void Restore(const FTransform& Transform);
 void RequestStand();
 bool PlayBedMotion(UAnimSequence* Motion,bool Loop=false);
 void FitRestPose(float Blend);
 FVector BedBodyBaseLocation=FVector::ZeroVector;
 FQuat BedBodyBaseRotation=FQuat::Identity;
 FVector SavedViewOffset=FVector::ZeroVector;
 FRotator SavedViewRotation=FRotator::ZeroRotator;
 bool bAdjustedSeatView=false;
 TWeakObjectPtr<ANeonCityPlayerCharacter> User;
 FTransform Entry;
 bool bHiddenBefore=false,bCollisionBefore=true;
 float CapsuleRadiusBefore=0,CapsuleHalfHeightBefore=0;
 TEnumAsByte<ECollisionEnabled::Type> CapsuleCollisionBefore=ECollisionEnabled::QueryAndPhysics;
 int32 BedStage=0;
 float BedStageSeconds=0,SleptHours=0;
 bool bReverseEntry=false;
 FTransform PendingExit;
};
