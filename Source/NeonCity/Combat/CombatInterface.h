#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/DamageEvent.h"
#include "CombatInterface.generated.h"
UENUM(BlueprintType)
enum class ENeonCityCombatState : uint8 { Unarmed, Ready, Aiming, Reloading, Disabled, Dead };
USTRUCT(BlueprintType)
struct NEONCITY_API FCombatAttackRequest
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector ViewOrigin=FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Direction=FVector::ForwardVector;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AController> Instigator;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool Aiming=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool AvoidFriendlyFire=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool Crouched=false;
	/** Normalized against ordinary walk speed. >1.1 means sprint-like movement. */
	UPROPERTY(EditAnywhere,BlueprintReadWrite) float MovementRatio=0;
};
UINTERFACE(BlueprintType)
class NEONCITY_API UCombatInterface : public UInterface { GENERATED_BODY() };
/** Platform contract: no assumption that the implementing actor is a character or a player. */
class NEONCITY_API ICombatInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Combat")
	bool Attack(const FCombatAttackRequest& Request);
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Combat")
	float TakeCombatDamage(const FNeonCityDamageEvent& Event);
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Combat")
	ENeonCityCombatState GetCombatState() const;
};
