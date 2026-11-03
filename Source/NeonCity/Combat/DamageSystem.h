#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DamageEvent.h"
#include "DamageSystem.generated.h"
class UHealthComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityWorldDamage,AActor*,Victim,const FNeonCityDamageEvent&,Event);
/** Common authoritative entry for all weapon/platform/environmental damage. */
UCLASS()
class NEONCITY_API UDamageSystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="NeonCity|Damage")
	float ApplyDamage(AActor* Victim,const FNeonCityDamageEvent& Event);
	float ApplyToHealth(UHealthComponent* Health,const FNeonCityDamageEvent& Event);
	UPROPERTY(BlueprintAssignable) FNeonCityWorldDamage OnDamage;
	const FNeonCityDamageEvent& GetLastDamage() const { return LastDamage; }
	int32 GetDamageCount() const { return DamageCount; }
private:
	UPROPERTY() FNeonCityDamageEvent LastDamage;
	int32 DamageCount=0;
};
