#pragma once
#include "CoreMinimal.h"
#include "BankingData.generated.h"

UENUM(BlueprintType)
enum class ENeonCityLoanPurpose : uint8 { Vehicle, Property, Business };

/** Values only. A paid loan clears this active record; the bounded ledger retains its ID. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityLoanData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid LoanID;
 UPROPERTY(SaveGame,BlueprintReadOnly) ENeonCityLoanPurpose Purpose=ENeonCityLoanPurpose::Vehicle;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 Principal=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 Fee=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 Remaining=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) FDateTime IssuedAt;
};

USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityLoanQuote
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Allowed=false;
 UPROPERTY(BlueprintReadOnly) int64 Limit=0;
 UPROPERTY(BlueprintReadOnly) int64 Principal=0;
 UPROPERTY(BlueprintReadOnly) int64 Fee=0;
 UPROPERTY(BlueprintReadOnly) int64 TotalRepayment=0;
 UPROPERTY(BlueprintReadOnly) FText Reason;
};
