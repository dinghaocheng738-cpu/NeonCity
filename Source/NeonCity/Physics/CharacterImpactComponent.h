#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Physics/PhysicalImpactEvent.h"
#include "CharacterImpactComponent.generated.h"
class ACharacter;
UENUM(BlueprintType)
enum class ECharacterImpactState : uint8 { Normal, Stagger, Knockdown, Recover, Downed, Dead };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterImpactStateChanged,ECharacterImpactState,State);
/** Capsule-authoritative, bounded upper-body physics. No full uncontrolled pelvis ragdoll. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCharacterImpactComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCharacterImpactComponent();
 void ReceiveImpact(const FPhysicalImpactEvent& Event);
 UFUNCTION(BlueprintPure) ECharacterImpactState GetState() const { return State; }
 bool IsPhysicsFeedbackActive() const { return bPhysics; }
 int32 GetPhysicsActivationCount() const { return PhysicsActivations; }
 uint32 GetVisitedStates() const { return Visited; }
 void SetPhysicsLOD(bool Enabled) { bAllowPhysics=Enabled; }
 UPROPERTY(BlueprintAssignable) FCharacterImpactStateChanged OnStateChanged;
 UFUNCTION() void ResetForLoad();
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void HealthChanged(float Current,float Maximum);
 void SetState(ECharacterImpactState Value);
 void UpdateReaction();
 void BeginPhysics(FVector Direction,float Speed);
 void EndPhysics();
 void Finish();
 TWeakObjectPtr<ACharacter> Character;
 ECharacterImpactState State=ECharacterImpactState::Normal;
 FTransform MeshTransform;
 FName PhysicsBone=NAME_None;
 FCollisionResponseContainer MeshResponses;
 ECollisionEnabled::Type MeshCollision=ECollisionEnabled::NoCollision;
 ECollisionResponse VehicleResponse=ECR_Block;
 FTimerHandle Timer;
 float StartedAt=0,LockSeconds=0;
 uint32 Visited=1;
 int32 PhysicsActivations=0;
 bool bPhysics=false,bAllowPhysics=true,bActive=false;
};
