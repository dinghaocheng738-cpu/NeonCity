#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "NeonCityPlayerDataComponent.generated.h"

/**
 * Compatibility player attributes; health delegates to the unified HealthComponent.
 * Runtime Money access delegates to the session economy. Detached instances retain
 * their legacy local value for data-only use; it is never the active player's wallet.
 * All values remain nonnegative, Health never exceeds MaxHealth, and nonfinite
 * float setter inputs are ignored. Stamina has no implicit maximum in this phase.
 */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityPlayerDataComponent : public UActorComponent
{
	GENERATED_BODY()
public:
 FNeonCityPlayerSaveData BuildPersistenceSnapshot() const;
 void ApplyPersistenceSnapshot(const FNeonCityPlayerSaveData& Snapshot);

	UNeonCityPlayerDataComponent();

	UFUNCTION(BlueprintPure, Category="NeonCity|Player Data")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category="NeonCity|Player Data")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintPure, Category="NeonCity|Player Data")
	float GetMaxHealth() const;

	/** Reducing MaxHealth also clamps current Health; increasing it does not heal. */
	UFUNCTION(BlueprintCallable, Category="NeonCity|Player Data")
	void SetMaxHealth(float NewMaxHealth);

	UFUNCTION(BlueprintPure, Category="NeonCity|Player Data")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintCallable, Category="NeonCity|Player Data")
	void SetStamina(float NewStamina);

	/** Compatibility access to the economy's whole-unit cash balance. Prefer CurrencyComponent. */
	UFUNCTION(BlueprintPure, Category="NeonCity|Player Data")
	int64 GetMoney() const;

	UFUNCTION(BlueprintCallable, Category="NeonCity|Player Data")
	void SetMoney(int64 NewMoney);

private:
	// Detached data fallbacks only; runtime getters expose the shared HealthComponent.
	float Health = 100.0f;

	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NeonCity|Player Data", meta=(AllowPrivateAccess="true"))
	float Stamina = 100.0f;

	// Detached data fallback only. Do not expose a stale second balance in the editor.
	int64 Money = 0;
};
