#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandBird.generated.h"
class USkeletalMeshComponent;
class UAnimSequence;
UENUM()
enum class EIslandBirdState:uint8 { Perched,Takeoff,Cruise,Landing };
/** Bounded, authored habitat flight. The actor never respawns around the camera. */
UCLASS()
class NEONCITY_API AIslandBird:public AActor
{
 GENERATED_BODY()
public:
 AIslandBird();
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Bird;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Flight;
 UPROPERTY(EditAnywhere) float OrbitRadius=1800;
 UPROPERTY(EditAnywhere) float FlightHeight=900;
 UPROPERTY(EditAnywhere) float FlightSpeed=650;
 UPROPERTY(EditAnywhere) float RestSeconds=12;
 UPROPERTY(EditAnywhere) float Phase=0;
 UPROPERTY(EditAnywhere) float FootOffset=29;
 EIslandBirdState GetBirdState()const{return State;}
 int32 GetCompletedFlights()const{return CompletedFlights;}
 FVector GetPerch()const{return Perch;}
 virtual void Tick(float DeltaSeconds)override;
protected:
 virtual void BeginPlay()override;
private:
 EIslandBirdState State=EIslandBirdState::Perched;
 FVector Perch,Hub,LastPosition,TransitionStart;
 float Clock=0,Angle=0,Fold=1;
 int32 CompletedFlights=0;
 bool bHabitatReady=false;
 void ChangeState(EIslandBirdState Next);
 FVector Orbit(float Radians)const;
};
