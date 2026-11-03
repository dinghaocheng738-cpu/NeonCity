#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NeonCityBankingObserver.generated.h"
class UNeonCityEconomySubsystem;
class UNeonCitySaveSubsystem;

/** Uninstantiated outside automation; exercises the actual Blueprint multicast boundary. */
UCLASS(Transient)
class UNeonCityBankingObserver : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UNeonCityEconomySubsystem> Accounts;
 UPROPERTY() TObjectPtr<UNeonCitySaveSubsystem> Save;
 bool NestedLoadAccepted=false;
 int32 Calls=0;
 int64 SeenCash=0,SeenBank=0;
 bool NestedMutationAccepted=false;
 UFUNCTION() void AccountsChanged();
};
