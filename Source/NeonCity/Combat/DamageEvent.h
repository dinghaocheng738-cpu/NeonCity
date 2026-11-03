#pragma once
#include "CoreMinimal.h"
#include "DamageEvent.generated.h"
class AController;
UENUM(BlueprintType)
enum class ENeonCityDamageType : uint8 { Ballistic, Impact, Environmental, Administrative, Explosion, Drowning };
UENUM(BlueprintType)
enum class ENeonCityAttackDisposition : uint8 { Unspecified, InitiatingAttack, ResponseToAggression };
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityDamageEvent
{
	GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FGuid CombatContextID;
 UPROPERTY(BlueprintReadOnly) ENeonCityAttackDisposition Disposition=ENeonCityAttackDisposition::Unspecified;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> OriginalAggressor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageAmount=0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ENeonCityDamageType DamageType=ENeonCityDamageType::Ballistic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AController> Instigator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> SourceActor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HitLocation=FVector::ZeroVector;
};
