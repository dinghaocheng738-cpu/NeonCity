#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/ThreatEvent.h"
#include "Police/SuspectDescriptor.h"
#include "Physics/CharacterImpactComponent.h"
#include "NPCThreatComponent.generated.h"
class ANeonCityNPCCharacter;
/** Event-driven perception plus a civilian response; ordinary ecology stays in the controller. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNPCThreatComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNPCThreatComponent();
 bool UpdateReaction(float DeltaSeconds);
 void ClearResponse();
 float GetDangerUntil() const { return DangerUntil; }
 bool HasPerceived(FGuid Id) const;
 bool IdentifiedInstigator(FGuid Id) const;
 FSuspectDescriptor GetObservation(FGuid Id) const;
 bool ObserveVehicleTransition(AActor* Person,AActor* Vehicle,bool Entering=true);
 bool HasActiveThreat() const { return bActive; }
 bool CanSuspendPopulation() const;
 FGuid GetIncidentId() const { return Incident.Id; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void PhysicalStateChanged(ECharacterImpactState State);
 UFUNCTION() void WitnessHealthChanged(float Current,float Maximum);
 UFUNCTION() void Perceive(const FNeonCityThreatEvent& Event);
 bool CanSee(FVector Point,AActor* Target) const;
 bool FindSafeFleePoint(FVector& Point) const;
 bool SafeSegment(FVector Start,FVector End) const;
 void LookAtDanger(float DeltaSeconds);
 void SubmitTransition();
 void CancelPendingReports();
 FSuspectDescriptor PendingTransition,KnownTransitionDescription;
 FTimerHandle TransitionTimer;
 struct FMemory { FGuid Id; bool bIdentified=false; FSuspectDescriptor Observation; };
 TArray<FMemory> Memory;
 TWeakObjectPtr<ANeonCityNPCCharacter> NPC;
 FNeonCityThreatEvent Incident;
 FVector DangerLocation=FVector::ZeroVector,FleePoint=FVector::ZeroVector;
 float LastThreatTime=0,DangerUntil=0,NextFleePlan=0,FirstGunshotTime=-1000;
 int32 RecentGunshots=0;
 bool bActive=false,bReplan=false;
};
