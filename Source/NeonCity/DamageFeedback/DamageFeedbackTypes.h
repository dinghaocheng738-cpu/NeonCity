#pragma once
#include "CoreMinimal.h"
#include "Combat/DamageEvent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "DamageFeedbackTypes.generated.h"
/** Presentation-only severity tier. Health remains the single authoritative state source. */
UENUM(BlueprintType)
enum class ENeonCityFeedbackTier : uint8 { Healthy, Injured, Critical, Downed, Dead };
/** One resolved hit, as the feedback layer saw it. Bounded copy: no pointers to the damage pipeline. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityDamageFeedbackEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float Amount=0;
 UPROPERTY(BlueprintReadOnly) ENeonCityDamageType DamageType=ENeonCityDamageType::Ballistic;
 UPROPERTY(BlueprintReadOnly) ENeonCityBodyState BodyState=ENeonCityBodyState::Normal;
 UPROPERTY(BlueprintReadOnly) ENeonCityFeedbackTier Tier=ENeonCityFeedbackTier::Healthy;
 UPROPERTY(BlueprintReadOnly) FVector HitLocation=FVector::ZeroVector;
 /** Unit vector from the victim toward the damage source. */
 UPROPERTY(BlueprintReadOnly) FVector DamageDirection=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float HealthFraction=1;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 bool IsValidFeedback() const { return FMath::IsFinite(Amount) && Amount>0 && !HitLocation.ContainsNaN() && !DamageDirection.ContainsNaN() && !FMath::IsNaN(HealthFraction); }
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityDamageFeedbackSignal,const FNeonCityDamageFeedbackEvent&,Event);
