#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Destruction/DestructionTypes.h"
#include "Destruction/BreakableSurfaceData.h"
#include "Combat/DamageEvent.h"
#include "Explosion/ExplosionEvent.h"
#include "DestructibleStateComponent.generated.h"
class UHealthComponent;
class UPrimitiveComponent;
class UMaterialInterface;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDestructibleStateSignal,EPhysicsPropState,Previous,EPhysicsPropState,Current);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDestructibleBreakSignal,FName,SurfaceID,FVector,Origin,int32,BreakCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDestructibleFragmentSignal,FVector,Origin,ENeonCitySurfaceKind,Kind,float,BudgetCost);
/** Unified Normal/Damaged/Broken state for props, glass, doors, signs, street lights and structure parts. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UDestructibleStateComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDestructibleStateComponent();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Destruction") TObjectPtr<UBreakableSurfaceData> SurfaceProfile;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Destruction") ENeonCitySurfaceKind SurfaceKind=ENeonCitySurfaceKind::Concrete;
 /** Prototype material feedback for the cracked stage; a Niagara/material system may replace it. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Destruction") TObjectPtr<UMaterialInterface> CrackedMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Destruction") TObjectPtr<UMaterialInterface> BrokenMaterial;
 /** Reference-returning accessors stay C++ only; UnrealHeaderTool cannot reflect them. */
 const UBreakableSurfaceData& GetSurface() const;
 UFUNCTION(BlueprintPure,Category="Destruction") EPhysicsPropState GetState() const { return State; }
 UFUNCTION(BlueprintPure,Category="Destruction") ENeonCityGlassState GetGlassState() const;
 UFUNCTION(BlueprintPure,Category="Destruction") bool IsBroken() const { return State==EPhysicsPropState::Broken; }
 UFUNCTION(BlueprintPure,Category="Destruction") bool IsPersistent() const { return GetSurface().bPersistent; }
 UFUNCTION(BlueprintPure,Category="Destruction") int32 GetBreakCount() const { return BreakCount; }
 UFUNCTION(BlueprintPure,Category="Destruction") int32 GetFragmentEffectCount() const { return FragmentEffects; }
 UFUNCTION(BlueprintPure,Category="Destruction") int32 GetRefusedFragmentCount() const { return RefusedFragments; }
 UFUNCTION(BlueprintPure,Category="Destruction") float GetLastDamageTime() const { return LastDamageTime; }
 /** Health remains the truth source; this only reads the outcome and applies surface policy. */
 bool ApplyDamageEvent(const FNeonCityDamageEvent& Event);
 /** Pushed by the physics adapter, which owns the released prop policy. Never re-evaluated from health. */
 bool ApplyState(EPhysicsPropState Value,FVector Origin);
 bool ApplyImpact(float ContactSpeed,FVector Location);
 bool ApplyExplosion(const FNeonCityExplosionEvent& Event);
 UFUNCTION(BlueprintCallable,Category="Destruction") bool Break(FVector Origin,float ImpulseScale=1);
 /** True when a destroyed surface was actually restored to Normal. */
 UFUNCTION(BlueprintCallable,Category="Destruction") bool ResetDestruction();
 bool RequestFragmentEffect(FVector Origin,float BudgetCost);
 UPROPERTY(BlueprintAssignable) FDestructibleStateSignal OnStateChanged;
 UPROPERTY(BlueprintAssignable) FDestructibleBreakSignal OnSurfaceBroken;
 UPROPERTY(BlueprintAssignable) FDestructibleFragmentSignal OnFragmentEffectRequested;
 UFUNCTION(BlueprintImplementableEvent,Category="Destruction") void OnDestructionFeedback(EPhysicsPropState NewState,FVector Origin);
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void HealthChanged(float Current,float Maximum);
 UFUNCTION() void Damaged(const FNeonCityDamageEvent& Event);
 void Evaluate(FVector Origin);
 bool SetState(EPhysicsPropState Value,FVector Origin);
 /** Props keep their released Phase 15 policy; this register then only stores and publishes it. */
 bool bAdapterDriven=false;
 void ApplyStateVisuals();
 UPROPERTY() TObjectPtr<UHealthComponent> Health;
 UPROPERTY() TObjectPtr<UPrimitiveComponent> Surface;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> IntactMaterials;
 EPhysicsPropState State=EPhysicsPropState::Normal;
 int32 BreakCount=0,FragmentEffects=0,RefusedFragments=0;
 float LastDamageTime=-1000;
};