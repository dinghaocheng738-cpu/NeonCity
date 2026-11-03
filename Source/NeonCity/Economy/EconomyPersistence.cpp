#include "Economy/NeonCityEconomySubsystem.h"
namespace
{
// Reconstructing a previous nonnegative balance must be representable without signed overflow.
bool ValidDelta(int64 After,int64 Delta)
{ return After>=0 && (Delta>=0?Delta<=After:(Delta!=MIN_int64 && -Delta<=MAX_int64-After)); }
bool ValidEntry(const FTransactionData& E)
{
 if(!E.Id.IsValid() || E.Timestamp.GetTicks()<=0 || E.Timestamp>FDateTime::MaxValue() || !ValidDelta(E.BalanceAfter,E.Amount)
  || !ValidDelta(E.BankBalanceAfter,E.BankDelta) || !ValidDelta(E.DebtAfter,E.DebtDelta))return false;
 switch(E.Type)
 {
 case ENeonCityTransactionType::Income:return E.Amount>0 && E.BankDelta==0 && E.DebtDelta==0;
 case ENeonCityTransactionType::Spending:return E.Amount<0 && E.BankDelta==0 && E.DebtDelta==0;
 case ENeonCityTransactionType::Deposit:return E.Amount<0 && E.BankDelta>0 && E.Amount==-E.BankDelta && E.DebtDelta==0;
 case ENeonCityTransactionType::Withdrawal:return E.Amount>0 && E.BankDelta<0 && E.BankDelta==-E.Amount && E.DebtDelta==0;
 case ENeonCityTransactionType::Loan:return E.Amount==0 && E.BankDelta>0 && E.DebtDelta>=E.BankDelta && E.LoanID.IsValid();
 case ENeonCityTransactionType::Repayment:return E.Amount==0 && E.BankDelta<0 && E.BankDelta==E.DebtDelta && E.LoanID.IsValid();
 default:return false;
 }
}
}
FNeonCityEconomySaveData UNeonCityEconomySubsystem::BuildPersistenceSnapshot() const
{
 FNeonCityEconomySaveData Result;Result.Balance=Balance;Result.CreditReceipts=CreditReceipts.Array();
 Result.CreditReceipts.Sort(FNameLexicalLess());Result.HasBanking=true;Result.BankBalance=BankBalance;
 Result.ActiveLoan=ActiveLoan;Result.Transactions=Transactions;return Result;
}
bool UNeonCityEconomySubsystem::ValidatePersistenceSnapshot(const FNeonCityEconomySaveData& S,FString& Error)
{
 if(S.Balance<0 || S.CreditReceipts.Num()>1024 || S.Transactions.Num()>256)
 { Error=TEXT("Invalid wallet or oversized economy snapshot.");return false; }
 if(!S.HasBanking)return true;
 const auto& L=S.ActiveLoan;
 if(S.BankBalance<0 || uint8(L.Purpose)>uint8(ENeonCityLoanPurpose::Business) || L.Remaining<0)
 { Error=TEXT("Invalid bank balance or loan state.");return false; }
 if(L.Remaining==0)
 {
  if(L.LoanID.IsValid() || L.Principal!=0 || L.Fee!=0 || L.IssuedAt.GetTicks()!=0)
  { Error=TEXT("Inconsistent empty loan.");return false; }
 }
 else if(!L.LoanID.IsValid() || L.Principal<100 || L.Principal>1000000 || L.Fee<0 || L.Fee>L.Principal
  || L.Remaining>L.Principal+L.Fee || L.IssuedAt.GetTicks()<=0 || L.IssuedAt>FDateTime::MaxValue())
 { Error=TEXT("Invalid active loan identity or amounts.");return false; }
 return true;
}
void UNeonCityEconomySubsystem::NormalizePersistenceSnapshot(FNeonCityEconomySaveData& S,TArray<FString>& Warnings)
{
 if(!S.HasBanking)
 { S.HasBanking=true;S.BankBalance=0;S.ActiveLoan=FNeonCityLoanData();S.Transactions.Reset();return; }
 TSet<FGuid> Seen;
 S.Transactions.RemoveAll([&](const FTransactionData& E)
 {
  if(!ValidEntry(E) || Seen.Contains(E.Id)) { Warnings.Add(TEXT("Invalid or duplicate transaction history record skipped."));return true; }
  Seen.Add(E.Id);return false;
 });
 // History is informational; it never becomes authority for balances, loans or reward receipts.
 if(!S.Transactions.IsEmpty())
 {
  const auto& Last=S.Transactions.Last();
  if(Last.BalanceAfter!=S.Balance || Last.BankBalanceAfter!=S.BankBalance || Last.DebtAfter!=S.ActiveLoan.Remaining)
  { S.Transactions.Reset();Warnings.Add(TEXT("Stale transaction history cleared; account balances retained.")); }
 }
}
void UNeonCityEconomySubsystem::ApplyPersistenceSnapshot(const FNeonCityEconomySaveData& Snapshot)
{
 FString Error;if(bChangingAccounts || !ValidatePersistenceSnapshot(Snapshot,Error))return;
 auto Safe=Snapshot;TArray<FString> Warnings;NormalizePersistenceSnapshot(Safe,Warnings);
 TGuardValue<bool> Guard(bChangingAccounts,true);
 Balance=Safe.Balance;BankBalance=Safe.BankBalance;ActiveLoan=Safe.ActiveLoan;Transactions=MoveTemp(Safe.Transactions);
 CreditReceipts.Reset();for(FName ID:Safe.CreditReceipts)if(!ID.IsNone())CreditReceipts.Add(ID);
 // Restore emits state notifications, never an income/deposit receipt or a second loan disbursal.
 OnBalanceChanged.Broadcast(Balance);OnAccountsChanged.Broadcast();
}
