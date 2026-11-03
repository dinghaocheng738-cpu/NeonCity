#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/DamageEvent.h"
#include "Explosion/ExplosionEvent.h"
#include "VehicleDamageComponent.generated.h"
class UHealthComponent;
class UPrimitiveComponent;
class UVehicleData;
UENUM(BlueprintType)
enum class ENeonCityVehicleCondition : uint8 { Normal, Damaged, Critical, Destroyed, Disabled };
/** Fire and explosion are consequences of a wreck, never of one instantaneous health value. */
UENUM(BlueprintType)
enum class ENeonCityVehicleExplosionState : uint8 { None, Pending, Detonated };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVehicleConditionChanged,ENeonCityVehicleCondition,Previous,ENeonCityVehicleCondition,Current);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVehicleCollisionFeedback,float,DeltaSpeed,float,AppliedDamage,FVector,Location);
/** Shared vehicle condition policy. Health and all damage stay in the universal combat system. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UVehicleDamageComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVehicleDamageComponent();
 UFUNCTION(BlueprintPure,Category="Vehicle") ENeonCityVehicleCondition GetCondition() const { return Condition; }
 UFUNCTION(BlueprintPure,Category="Vehicle") float GetConditionFraction() const;
 UFUNCTION(BlueprintPure,Category="Vehicle") float GetDrivePowerMultiplier() const;
 float GetRecentDamage() const;
 int32 GetCollisionDamageCount() const { return CollisionDamageCount; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") bool CanExplode() const;
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") bool IsExplosionPending() const { return ExplosionState==ENeonCityVehicleExplosionState::Pending; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") ENeonCityVehicleExplosionState GetExplosionState() const { return ExplosionState; }
 /** Seconds left on the escape window; 0 when no fuse is burning. */
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") float GetFuseRemaining() const;
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") int32 GetExplosionCount() const { return ExplosionCount; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") int32 GetChainDepth() const { return ChainDepth; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") int32 GetRefusedExplosionCount() const { return RefusedExplosionCount; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") int32 GetChainOfferCount() const { return ChainOfferCount; }
 UFUNCTION(BlueprintPure,Category="Vehicle|Explosion") FName GetExplosionDataID() const;
 /** Authored vehicle profile when the owner is a city vehicle; null for platform-only actors. */
 const UVehicleData* GetProfile() const;
 const FNeonCityExplosionResult& GetLastExplosionResult() const { return LastResult; }
 TWeakObjectPtr<AController> GetExplosionInstigator() const { return LastInstigator; }
 /** Runtime opt-in for test areas and authored set pieces; the profile remains the source of truth. */
 void EnableExplosion(FName DataID,float MinDelay,float MaxDelay);
 /** A blast that damaged this wreck may light its own fuse. Deeper chains are refused outright. */
 bool OfferChainExplosion(const FNeonCityExplosionEvent& Event);
 /** Fuse expiry and explicit triggers share one path. */
 UFUNCTION(BlueprintCallable,Category="Vehicle|Explosion") bool DetonateNow();
 void CancelExplosionFuse();
 void ResetExplosionState();
 UPROPERTY(BlueprintAssignable) FVehicleConditionChanged OnConditionChanged;
 UPROPERTY(BlueprintAssignable) FVehicleCollisionFeedback OnCollisionFeedback;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
public:
 void SuppressRestoreCollision();
 void NotifyPhysicalImpact(float Speed,float AppliedDamage,FVector Location);
private:
 UFUNCTION() void HealthChanged(float Current,float Maximum);
 UFUNCTION() void Damaged(const FNeonCityDamageEvent& Event);
 UFUNCTION() void FuseElapsed();
 void StartFuse(float Delay,int32 Depth);
 void ResolveExplosionTiming(float& OutMin,float& OutMax) const;
 void ConditionFeedback(ENeonCityVehicleCondition Previous,ENeonCityVehicleCondition Next);
 UPROPERTY() TObjectPtr<UHealthComponent> Health;
 FTimerHandle FuseTimer;
 ENeonCityVehicleExplosionState ExplosionState=ENeonCityVehicleExplosionState::None;
 FNeonCityExplosionResult LastResult;
 TWeakObjectPtr<AController> LastInstigator;
 TWeakObjectPtr<AActor> LastSourceActor;
 FName OverrideExplosionDataID=NAME_None;
 float OverrideDelayMin=0,OverrideDelayMax=0,FuseSeconds=0;
 int32 ChainDepth=0,ExplosionCount=0,RefusedExplosionCount=0,ChainOfferCount=0,FuseCount=0;
 ENeonCityVehicleCondition Condition=ENeonCityVehicleCondition::Normal;
 float FeedbackUntil=0,LastDamage=0;
 int32 CollisionDamageCount=0;
};
