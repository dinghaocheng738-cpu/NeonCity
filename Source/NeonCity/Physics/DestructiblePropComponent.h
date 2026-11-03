#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Physics/PhysicalImpactEvent.h"
#include "Combat/DamageEvent.h"
#include "Destruction/DestructionTypes.h"
#include "DestructiblePropComponent.generated.h"
class UStaticMeshComponent;
class UPrimitiveComponent;
class ULightComponent;
class UDestructibleStateComponent;
UENUM(BlueprintType)
enum class EPhysicsPropMode : uint8 { Static, Movable, Breakable };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhysicsPropStateSignal,EPhysicsPropState,State);
/** Physics/mesh adapter for one breakable prop. The unified state register lives in DestructibleStateComponent. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UDestructiblePropComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDestructiblePropComponent();
 void ReceiveImpact(const FPhysicalImpactEvent& Event);
 UPROPERTY(EditAnywhere) EPhysicsPropMode Mode=EPhysicsPropMode::Breakable;
 UPROPERTY(EditAnywhere) float Mass=35;
 UPROPERTY(EditAnywhere) bool bGlass=false;
 UPROPERTY(BlueprintAssignable) FPhysicsPropStateSignal OnStateChanged;
 EPhysicsPropState GetState() const;
 /** Pushes a state decided elsewhere (explosion shatter) into the shared register. */
 bool ApplyExternalState(EPhysicsPropState Value);
 float GetLastDamageTime() const { return LastDamageTime; }
 bool IsPhysicallyReleased() const { return bReleased; }
 void SetPhysicsLOD(bool Active);
 void ResetForPool();
 FVector GetImpactVelocity() const { return PrePhysicsVelocity; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 UFUNCTION() void Woke(UPrimitiveComponent* Component,FName Bone);
 UFUNCTION() void Slept(UPrimitiveComponent* Component,FName Bone);
 UFUNCTION() void Damaged(const FNeonCityDamageEvent& Damage);
 bool SetState(EPhysicsPropState Value);
 void Monitor();
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY() TObjectPtr<UDestructibleStateComponent> StateRegister;
 EPhysicsPropState State=EPhysicsPropState::Normal;
 FTimerHandle Timer;
 float ActiveUntil=0,LastDamageTime=-1000;
 bool bPhysicsLOD=true,bReleased=false;
 FVector PrePhysicsVelocity=FVector::ZeroVector;
 TMap<TWeakObjectPtr<ULightComponent>,bool> InitialLights;
 float InitialHealth=100,InitialMaxHealth=100;
};