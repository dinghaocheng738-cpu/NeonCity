#pragma once
#include "CoreMinimal.h"
#include "Combat/DamageEvent.h"
#include "Police/SuspectDescriptor.h"
#include "Police/PoliceResponseData.h"
#include "CrimeEvent.generated.h"
class AController;
UENUM(BlueprintType)
enum class ENeonCityCrimeType : uint8 { DamageNPC, DestroyVehicle, ThreatEvent, PropertyDamage };
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityCrimeEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) EPoliceOffense Offense=EPoliceOffense::Attention;
 UPROPERTY(BlueprintReadOnly) float DamageAmount=0;
 UPROPERTY(BlueprintReadOnly) FSuspectDescriptor Observation;
 UPROPERTY(BlueprintReadOnly) FGuid CombatContextID;
 UPROPERTY(BlueprintReadOnly) ENeonCityAttackDisposition Disposition=ENeonCityAttackDisposition::Unspecified;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> OriginalAggressor;
 UPROPERTY(BlueprintReadOnly) FGuid Id;
 UPROPERTY(BlueprintReadOnly) ENeonCityCrimeType CrimeType=ENeonCityCrimeType::ThreatEvent;
 UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Victim;
 // First actual reporting witness, not every NPC inside the event radius.
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Witness;
};
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityReportEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) EPoliceOffense Offense=EPoliceOffense::Attention;
 UPROPERTY(BlueprintReadOnly) float DamageAmount=0;
 UPROPERTY(BlueprintReadOnly) FSuspectDescriptor Observation;
 UPROPERTY(BlueprintReadOnly) FGuid CombatContextID;
 UPROPERTY(BlueprintReadOnly) ENeonCityAttackDisposition Disposition=ENeonCityAttackDisposition::Unspecified;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> OriginalAggressor;
 UPROPERTY(BlueprintReadOnly) FGuid CrimeId;
 UPROPERTY(BlueprintReadOnly) ENeonCityCrimeType CrimeType=ENeonCityCrimeType::ThreatEvent;
 UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Witness;
 // Hearing alone does not identify the shooter, although the authoritative crime log knows the source.
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
};
