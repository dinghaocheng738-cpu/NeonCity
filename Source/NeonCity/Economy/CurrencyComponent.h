#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CurrencyComponent.generated.h"
class UNeonCityEconomySubsystem;

/** Pawn-facing adapter; never owns a second balance. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCurrencyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCurrencyComponent();
	UFUNCTION(BlueprintPure, Category="NeonCity|Economy") int64 GetBalance() const;
	UFUNCTION(BlueprintCallable, Category="NeonCity|Economy") bool AddMoney(int64 Amount);
	UFUNCTION(BlueprintCallable, Category="NeonCity|Economy") bool RemoveMoney(int64 Amount);
private:
	UNeonCityEconomySubsystem* GetEconomy() const;
};
