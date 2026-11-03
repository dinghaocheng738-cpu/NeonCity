#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Adventure/MarineSpearDeath.h"
#include "CoastalWildlife.generated.h"
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class USphereComponent;
class UFishData;
class UNeonCityFishingSubsystem;
struct FNeonCityCaughtFish;

/** Authored habitat member. No interaction interface: catches require the diving tool. */
UCLASS()
class NEONCITY_API ACoastalWildlife : public AActor
{
 GENERATED_BODY()
public:
 ACoastalWildlife();
 UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Target;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> AnimatedBody;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> SwimAnimation;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftWing;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightWing;
 UPROPERTY(EditAnywhere) TObjectPtr<UFishData> Species;
 UPROPERTY(EditAnywhere) bool bBird=false;
 UPROPERTY(EditAnywhere,Category="Habitat") bool bCapturable=true;
 UPROPERTY(EditAnywhere) float Radius=350;
 UPROPERTY(EditAnywhere) float Speed=65;
 UPROPERTY(EditAnywhere) float Phase=0;
 UPROPERTY(EditAnywhere,Category="Habitat") FName SchoolID;
 UPROPERTY(EditAnywhere,Category="Habitat",meta=(ClampMin="20")) float SchoolSpacing=85;
 UPROPERTY(EditAnywhere,Category="Habitat",meta=(ClampMin="30")) float MinimumDepth=100;
 UPROPERTY(EditAnywhere,Category="Habitat",meta=(ClampMin="30")) float VerticalRange=160;
 FVector GetSwimHeading()const{return Heading;}
 bool IsFleeing()const{return Alarm>0.1f;}
 bool TryCapture(UNeonCityFishingSubsystem* Inventory);
 bool CanHook() const;
 bool ReserveHook(FGuid Ticket,FNeonCityCaughtFish& Catch);
 bool ReserveSpear(FGuid Ticket,FVector Hit,FVector Direction,FNeonCityCaughtFish& Catch);
 bool GetSpearAttachment(FGuid Ticket,FTransform& Pose,bool& Collectible) const;
 bool PullSpear(FGuid Ticket,FVector Delta);
 EMarineSpearLife GetSpearLife() const { return Death.State; }
 void StartleAt(FVector Point,float Range);
 bool MoveHook(FGuid Ticket,FVector Target,bool Hooked,bool Landed);
 bool FinishHook(FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch,bool Keep);
 virtual void Tick(float Delta) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 FVector Centre=FVector::ZeroVector;
 float Angle=0;
 FVector Heading=FVector::ForwardVector,Steering=FVector::ForwardVector;
 float DecisionClock=0,Alarm=0;
 bool bCaught=false;
 FGuid HookTicket;
 FVector HookTarget=FVector::ZeroVector;
 bool bHooked=false,bLanded=false;
 FMarineSpearDeath Death;
};
