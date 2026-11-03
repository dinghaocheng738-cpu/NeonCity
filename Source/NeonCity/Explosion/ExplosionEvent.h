#pragma once
#include "CoreMinimal.h"
#include "Combat/DamageEvent.h"
#include "Explosion/ExplosionData.h"
#include "ExplosionEvent.generated.h"
/** Bounded, copyable blast record. Broadcast to future world-event, weather and random-event subscribers. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityExplosionEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FGuid ExplosionId;
 UPROPERTY(BlueprintReadOnly) FName DataID=TEXT("NeonCity.SmallExplosion");
 UPROPERTY(BlueprintReadOnly) FVector Center=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Damage=0;
 UPROPERTY(BlueprintReadOnly) float InnerRadius=0;
 UPROPERTY(BlueprintReadOnly) float OuterRadius=0;
 UPROPERTY(BlueprintReadOnly) float ImpulseStrength=0;
 UPROPERTY(BlueprintReadOnly) float FireDuration=0;
 UPROPERTY(BlueprintReadOnly) float SmokeDuration=0;
 UPROPERTY(BlueprintReadOnly) ENeonCityDamageType DamageType=ENeonCityDamageType::Explosion;
 UPROPERTY(BlueprintReadOnly) ENeonCityExplosionSourceType SourceType=ENeonCityExplosionSourceType::Generic;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> SourceActor;
 /** A vehicle destroyed by another blast keeps a bounded chain depth; deeper blasts are refused a fuse. */
 UPROPERTY(BlueprintReadOnly) int32 ChainDepth=0;
 UPROPERTY(BlueprintReadOnly) FGuid ParentExplosionId;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 bool IsValidEvent() const { return ExplosionId.IsValid() && !Center.ContainsNaN() && FMath::IsFinite(Damage) && Damage>0 && OuterRadius>0; }
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityExplosionSignal,const FNeonCityExplosionEvent&,Event);
/** Outcome of one detonation request. Rejections are explicit so tests never infer silent failure. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityExplosionResult
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bApplied=false;
 UPROPERTY(BlueprintReadOnly) int32 DamageTargets=0;
 UPROPERTY(BlueprintReadOnly) int32 ImpulseTargets=0;
 /** Simulating bodies outside the combat target list (debris, loose props) that the blast pushed. */
 UPROPERTY(BlueprintReadOnly) int32 LooseBodyTargets=0;
 /** Characters thrown into a stagger / knockdown reaction. */
 UPROPERTY(BlueprintReadOnly) int32 CharacterReactions=0;
 UPROPERTY(BlueprintReadOnly) int32 ResponseTargets=0;
 UPROPERTY(BlueprintReadOnly) int32 RefusedTargets=0;
 UPROPERTY(BlueprintReadOnly) int32 ChainDepth=0;
 UPROPERTY(BlueprintReadOnly) float TotalDamage=0;
 UPROPERTY(BlueprintReadOnly) FString RefusedReason;
 UPROPERTY(BlueprintReadOnly) FGuid ExplosionId;
};