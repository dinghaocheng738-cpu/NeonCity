#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageFeedback/DamageFeedbackData.h"
#include "DamageFeedback/DamageFeedbackTypes.h"
#include "CharacterDamageFeedbackComponent.generated.h"
class UHealthComponent;
class UDamageableCharacterComponent;
class UParticleSystem;
class UMaterialInterface;
class UDecalComponent;
/** Observes the released health pipeline and produces readable hit feedback. It never owns health. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCharacterDamageFeedbackComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCharacterDamageFeedbackComponent();
 /** Optional authored policy; the native fallback is used when it is unset or invalid. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="NeonCity|Feedback") TObjectPtr<UCharacterDamageFeedbackData> Profile;
 /** Reference-returning accessors stay C++ only; UnrealHeaderTool cannot reflect them. */
 const UCharacterDamageFeedbackData& GetProfile() const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") ENeonCityBodyState GetFeedbackState() const { return FeedbackState; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") ENeonCityFeedbackTier GetFeedbackTier() const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") float GetHealthFraction() const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") bool IsHitReactionActive() const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") bool IsDamageDirectionActive() const;
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") FVector GetLastDamageDirection() const { return LastSample.DamageDirection; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") FVector GetLastHitLocation() const { return LastSample.HitLocation; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Feedback") float GetLastDamageAmount() const { return LastSample.Amount; }
 const FNeonCityDamageFeedbackEvent& GetLastFeedback() const { return LastSample; }
 int32 GetHitReactionCount() const { return HitReactionCount; }
 int32 GetImpactEffectCount() const { return ImpactEffectCount; }
 int32 GetInjuredFeedbackCount() const { return InjuredCount; }
 int32 GetDownedFeedbackCount() const { return DownedCount; }
 int32 GetDeathFeedbackCount() const { return DeathCount; }
 int32 GetRefusedEffectCount() const { return RefusedEffectCount; }
 int32 GetActiveEffectCount() const;
 /** Pooled actors and save/load restore call this so no stale feedback survives a reset. */
 void ResetFeedback();
 UPROPERTY(BlueprintAssignable) FNeonCityDamageFeedbackSignal OnDamageFeedback;
 UFUNCTION(BlueprintImplementableEvent,Category="NeonCity|Feedback") void OnHitReactionFeedback(FVector DamageDirection,float Amount);
 UFUNCTION(BlueprintImplementableEvent,Category="NeonCity|Feedback") void OnInjuredFeedback(float HealthFraction);
 UFUNCTION(BlueprintImplementableEvent,Category="NeonCity|Feedback") void OnCriticalFeedback(float HealthFraction);
 UFUNCTION(BlueprintImplementableEvent,Category="NeonCity|Feedback") void OnDownedFeedback();
 UFUNCTION(BlueprintImplementableEvent,Category="NeonCity|Feedback") void OnDeathFeedback();
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void Damaged(const FNeonCityDamageEvent& Event);
 UFUNCTION() void HealthChanged(float Current,float Maximum);
 UFUNCTION() void BodyStateChanged(ENeonCityBodyState Previous,ENeonCityBodyState Current);
 void ApplyTier();
 void EmitImpactEffect(FVector Location);
 void EmitBloodDecal(FVector Location);
 void PruneEffects();
 UPROPERTY() TObjectPtr<UHealthComponent> Health;
 UPROPERTY() TObjectPtr<UDamageableCharacterComponent> Body;
 UPROPERTY() TObjectPtr<UParticleSystem> BloodEffect;
 UPROPERTY() TObjectPtr<UMaterialInterface> DecalMaterial;
 UPROPERTY() TArray<TWeakObjectPtr<AActor>> ActiveEffects;
 UPROPERTY() TArray<TWeakObjectPtr<UDecalComponent>> ActiveDecals;
 FNeonCityDamageFeedbackEvent LastSample;
 ENeonCityBodyState FeedbackState=ENeonCityBodyState::Normal;
 ENeonCityFeedbackTier Tier=ENeonCityFeedbackTier::Healthy;
 float ReactionUntil=-1,DirectionUntil=-1;
 int32 HitReactionCount=0,InjuredCount=0,DownedCount=0,DeathCount=0,ImpactEffectCount=0,RefusedEffectCount=0,DecalCount=0;
};
