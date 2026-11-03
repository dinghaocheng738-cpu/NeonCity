#pragma once
#include "CoreMinimal.h"
#include "TransactionData.generated.h"

UENUM(BlueprintType)
enum class ENeonCityTransactionType : uint8 { Income, Spending, Deposit, Withdrawal, Loan, Repayment };

/** One atomic operation, in signed whole currency units. Balances are AFTER the operation. */
USTRUCT(BlueprintType)
struct NEONCITY_API FTransactionData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid Id;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 Amount=0; // Cash delta; preserved for existing callers.
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 BalanceAfter=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) FName Reason;
 UPROPERTY(SaveGame,BlueprintReadOnly) FDateTime Timestamp;
 UPROPERTY(SaveGame,BlueprintReadOnly) ENeonCityTransactionType Type=ENeonCityTransactionType::Income;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 BankDelta=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 BankBalanceAfter=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 DebtDelta=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) int64 DebtAfter=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid LoanID;
};
