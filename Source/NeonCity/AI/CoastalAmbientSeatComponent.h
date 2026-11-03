#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CoastalAmbientSeatComponent.generated.h"
class ANeonCityNPCCharacter;
class APedestrianSpawnPoint;
class UTransportOccupantComponent;
class UStaticMeshComponent;

/** Ambient café seating reuses the production seated person renderer. The
 * real civilian retains identity, damage, sensing and a checked standing exit. */
UCLASS()
class NEONCITY_API UCoastalAmbientSeatComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCoastalAmbientSeatComponent();
 /** True while entry, sitting or standing owns ordinary locomotion. */
 bool UpdateSeating(float DeltaSeconds,bool Interrupt);
 void CancelForIncapacity();
 float GetSeatAlpha()const{return Blend;}
 bool IsSeatedPresentationActive()const{return bActive;}
 float GetSeatedSeconds()const{return SeatedSeconds;}
 bool HasCup()const{return Cup!=nullptr;}
 /** Checked destination used by this seat's standing transition; observation only. */
 bool GetPlannedStandingExit(FTransform& Exit)const;
 /** Read-only admission/departure evidence; never drives the civilian. */
 FString GetSeatingDiagnostic()const;
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 bool StartSeating(ANeonCityNPCCharacter& NPC,APedestrianSpawnPoint& Marker);
 bool FindStandingExit(FTransform& Exit)const;
 bool IsStandingPathClear(FVector Start,FVector End)const;
 void FitBody();
 void StowCup();
 void Restore(const FTransform& Exit,bool RestoreMovement);
 UPROPERTY() TObjectPtr<UTransportOccupantComponent> Occupant;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Cup;
 FTransform Entry;
 FTransform SeatRoot,LockedExit;
 FVector StandingHipWorld=FVector::ZeroVector,SeatHipWorld=FVector::ZeroVector;
 FVector ExitHipWorld=FVector::ZeroVector;
 TEnumAsByte<ECollisionEnabled::Type> CollisionBefore=ECollisionEnabled::QueryAndPhysics;
 TEnumAsByte<EMovementMode> MovementBefore=MOVE_Walking;
 uint8 CustomMovementBefore=0;
 float Blend=0,SeatedSeconds=0,Cooldown=2.f;
 bool bActive=false,bStanding=false,bExitLocked=false,bMeshVisible=true,bMeshTick=true;
 FName AdmissionGate=TEXT("NotUpdated");
 FString LastAdmissionFailure=TEXT("NotAttempted");
 mutable FString LastExitFailure,LastPathFailure;
 uint32 StartAttempts=0,StartFailures=0,CompletedEntries=0;
};
