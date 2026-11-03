#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/DamageEvent.h"
#include "HealthComponent.generated.h"
class UDamageSystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityHealthDamage,const FNeonCityDamageEvent&,Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityHealthChanged,float,Current,float,Maximum);
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();
	UFUNCTION(BlueprintPure,Category="NeonCity|Health") float GetCurrentHealth() const { return CurrentHealth; }
	UFUNCTION(BlueprintPure,Category="NeonCity|Health") float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure,Category="NeonCity|Health") bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintCallable,Category="NeonCity|Health") float TakeDamage(const FNeonCityDamageEvent& Event);
	UFUNCTION(BlueprintCallable,Category="NeonCity|Health") void Die();
	UFUNCTION(BlueprintCallable,Category="NeonCity|Health") float Heal(float Amount);
	/** Compatibility/admin setters. Reductions still pass through the unified damage system. */
	void SetCurrentHealth(float Value);
	void SetMaxHealth(float Value);
 /** Load-only value restoration. Emits state changes, never a damage/crime/death event. */
 void ApplyPersistenceHealth(float Current,float Maximum);
	UPROPERTY(BlueprintAssignable) FNeonCityHealthDamage OnDamageTaken;
	UPROPERTY(BlueprintAssignable) FNeonCityDeath OnDeath;
	UPROPERTY(BlueprintAssignable) FNeonCityHealthChanged OnHealthChanged;
private:
	friend class UDamageSystem;
	float CommitDamage(const FNeonCityDamageEvent& Event);
	UPROPERTY(VisibleAnywhere,Category="Health") float CurrentHealth=100;
	UPROPERTY(VisibleAnywhere,Category="Health") float MaxHealth=100;
	bool bDead=false;
};
