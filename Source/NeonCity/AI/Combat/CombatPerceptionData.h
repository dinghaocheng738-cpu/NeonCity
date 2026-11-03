#pragma once
#include "CoreMinimal.h"
#include "CombatPerceptionData.generated.h"
/** A sensor snapshot, never a live target-position accessor. Transient, excluded from SaveGame. */
USTRUCT(BlueprintType)
struct FCombatPerceptionData
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FVector LastKnownTargetLocation=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector LastKnownAimPoint=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float LastEvidenceTime=-1000;
 UPROPERTY(BlueprintReadOnly) float LastSeenTime=-1000;
 UPROPERTY(BlueprintReadOnly) bool HasLocation=false;
 UPROPERTY(BlueprintReadOnly) bool HasVisualContact=false;
};
