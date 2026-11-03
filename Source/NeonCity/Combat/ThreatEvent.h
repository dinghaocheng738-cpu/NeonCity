#pragma once
#include "CoreMinimal.h"
#include "Combat/DamageEvent.h"
#include "ThreatEvent.generated.h"
UENUM(BlueprintType)
enum class ENeonCityThreatType : uint8 { Gunshot, Attack, NearbyDanger, Explosion };
/** Transient world stimulus, not a verdict or a wanted-level change. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityThreatEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float DamageAmount=0;
 UPROPERTY(BlueprintReadOnly) FGuid CombatContextID;
 UPROPERTY(BlueprintReadOnly) ENeonCityAttackDisposition Disposition=ENeonCityAttackDisposition::Unspecified;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> OriginalAggressor;
 UPROPERTY(BlueprintReadOnly) FGuid Id;
 UPROPERTY(BlueprintReadOnly) ENeonCityThreatType Type=ENeonCityThreatType::NearbyDanger;
 UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Radius=1200;
 UPROPERTY(BlueprintReadOnly) float Time=0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> SourceActor;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Victim;
 UPROPERTY(BlueprintReadOnly) ENeonCityDamageType DamageType=ENeonCityDamageType::Environmental;
};
