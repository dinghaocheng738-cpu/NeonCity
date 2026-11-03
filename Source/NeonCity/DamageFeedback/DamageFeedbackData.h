#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DamageFeedback/DamageFeedbackTypes.h"
#include "DamageFeedbackData.generated.h"
class UParticleSystem;
class UMaterialInterface;
/** Authored hit-feedback policy. This asset never carries health, damage or crime data. */
UCLASS(BlueprintType)
class NEONCITY_API UCharacterDamageFeedbackData : public UDataAsset
{
 GENERATED_BODY()
public:
 UCharacterDamageFeedbackData();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback") FName FeedbackID=TEXT("NeonCity.Feedback.Character");
 /** Hits below this damage are grazes and produce no reaction at all. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0",ClampMax="100")) float MinimumDamage=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="2")) float HitReactionSeconds=.35f;
 /** Injuries and the downed state hold the reaction longer so the hit reads clearly. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="3")) float InjuredReactionScale=1.3f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="3")) float DownedReactionScale=1.6f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.1",ClampMax="15")) float DamageDirectionSeconds=2.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="1")) float InjuredFraction=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="1")) float CriticalFraction=.35f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.05",ClampMax="1")) float DownedFraction=.2f;
 /** Optional authored effect. Without it the component uses the bounded prototype marker. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects") TSoftObjectPtr<UParticleSystem> BloodHitEffect;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects") TSoftObjectPtr<UMaterialInterface> BloodDecalMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects",meta=(ClampMin="0",ClampMax="30")) float DecalLifeSpan=6;
 /** Hard cap on simultaneous blood decals; nothing here is permanent. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects",meta=(ClampMin="0",ClampMax="16")) int32 MaximumConcurrentDecals=3;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects") bool bPrototypeImpactEffect=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects",meta=(ClampMin="1",ClampMax="16")) int32 MaximumConcurrentEffects=6;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects",meta=(ClampMin="0.05",ClampMax="5")) float EffectLifetime=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects",meta=(ClampMin="1",ClampMax="60")) float EffectRadius=7;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effects") FLinearColor BloodColor=FLinearColor(.55f,.03f,.05f);
 bool IsValidProfile() const;
 float ReactionSecondsFor(ENeonCityBodyState State) const;
 ENeonCityFeedbackTier TierFor(float HealthFraction,ENeonCityBodyState State) const;
};
namespace NeonCityFeedback
{
 /** Native fallback so an actor without an authored asset still produces readable feedback. */
 NEONCITY_API const UCharacterDamageFeedbackData& DefaultProfile();
}
