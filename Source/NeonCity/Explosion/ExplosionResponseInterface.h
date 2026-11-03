#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Explosion/ExplosionEvent.h"
#include "ExplosionResponseInterface.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API UExplosionResponseInterface : public UInterface { GENERATED_BODY() };
/** Opt-in reaction for vehicles, structures and special reactors. Damage still flows through DamageSystem. */
class NEONCITY_API IExplosionResponseInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Explosion")
 bool ReceiveExplosion(const FNeonCityExplosionEvent& Event);
};