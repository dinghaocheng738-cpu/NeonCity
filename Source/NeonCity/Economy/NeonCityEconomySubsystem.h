#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/TransactionData.h"
#include "Economy/BankingData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "NeonCityEconomySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityBalanceChanged,int64,Balance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityAccountsChanged);

/** Single local player's atomic cash/bank/loan ledger. Existing GetBalance remains CASH. */
UCLASS(Config=Game,DefaultConfig)
class NEONCITY_API UNeonCityEconomySubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 bool IsAccountOperationInProgress() const { return bChangingAccounts; }
 FNeonCityEconomySaveData BuildPersistenceSnapshot() const;
 void ApplyPersistenceSnapshot(const FNeonCityEconomySaveData& Snapshot);
 static bool ValidatePersistenceSnapshot(const FNeonCityEconomySaveData& Snapshot,FString& Error);
 static void NormalizePersistenceSnapshot(FNeonCityEconomySaveData& Snapshot,TArray<FString>& Warnings);
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 UFUNCTION(BlueprintPure,Category="NeonCity|Economy") int64 GetBalance() const { return Balance; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Economy") int64 GetBankBalance() const { return BankBalance; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Economy") int64 GetDebt() const { return ActiveLoan.Remaining; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Economy") FNeonCityLoanData GetActiveLoan() const { return ActiveLoan; }
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool AddMoney(int64 Amount,FName Reason=NAME_None);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool RemoveMoney(int64 Amount,FName Reason=NAME_None);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool Deposit(int64 Amount);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool Withdraw(int64 Amount);
 UFUNCTION(BlueprintPure,Category="NeonCity|Economy") FNeonCityLoanQuote GetLoanQuote(int64 Amount,ENeonCityLoanPurpose Purpose) const;
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool RequestLoan(int64 Amount,ENeonCityLoanPurpose Purpose);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Economy") bool RepayLoan(int64 Amount);
 bool CreditOnce(int64 Amount,FName ReceiptKey);
 bool HasCredit(FName ReceiptKey) const { return CreditReceipts.Contains(ReceiptKey); }
 const TArray<FTransactionData>& GetTransactions() const { return Transactions; }
 UPROPERTY(BlueprintAssignable,Category="NeonCity|Economy") FNeonCityBalanceChanged OnBalanceChanged;
 UPROPERTY(BlueprintAssignable,Category="NeonCity|Economy") FNeonCityAccountsChanged OnAccountsChanged;
private:
 void Record(int64 CashDelta,FName Reason,ENeonCityTransactionType Type,int64 BankDelta=0,int64 DebtDelta=0,FGuid LoanID=FGuid());
 // Prototype policy, in whole game currency units. A quote is always checked again at execution.
 UPROPERTY(Config) int64 MinimumLoan=100;
 UPROPERTY(Config) int64 MaximumLoan=2000;
 UPROPERTY(Config) int32 LoanFeePercent=10;
 UPROPERTY(Config) int32 LoanLiquidityPercent=50;
 bool bChangingAccounts=false;
 int64 Balance=1850;
 int64 BankBalance=0;
 UPROPERTY() FNeonCityLoanData ActiveLoan;
 UPROPERTY() TArray<FTransactionData> Transactions;
 // Independent from visible history: trimming the ledger never permits a duplicate activity reward.
 TSet<FName> CreditReceipts;
};
