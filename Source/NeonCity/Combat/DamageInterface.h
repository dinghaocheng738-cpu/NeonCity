#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageInterface.generated.h"
class UHealthComponent;
UINTERFACE(BlueprintType)
class NEONCITY_API UDamageInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IDamageInterface
{
	GENERATED_BODY()
public:
	/** Expose the receiver; damage is applied only by the world damage subsystem. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Damage")
	UHealthComponent* GetDamageHealth() const;
};
