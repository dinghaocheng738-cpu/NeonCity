#include "Economy/NeonCityEconomySubsystem.h"
void UNeonCityEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 TGuardValue<bool> Guard(bChangingAccounts,true);
 Record(1850,TEXT("StartingFunds"),ENeonCityTransactionType::Income);
}
void UNeonCityEconomySubsystem::Record(int64 CashDelta,FName Reason,ENeonCityTransactionType Type,int64 BankDelta,int64 DebtDelta,FGuid LoanID)
{
 FTransactionData Entry;
 Entry.Id=FGuid::NewGuid();Entry.Amount=CashDelta;Entry.BalanceAfter=Balance;
 Entry.Reason=Reason;Entry.Timestamp=FDateTime::UtcNow();Entry.Type=Type;
 Entry.BankDelta=BankDelta;Entry.BankBalanceAfter=BankBalance;Entry.DebtDelta=DebtDelta;Entry.DebtAfter=GetDebt();Entry.LoanID=LoanID;
 if(Transactions.Num()>=256)Transactions.RemoveAt(0);
 Transactions.Add(Entry);
 // Every listener sees the fully updated record and both account balances. Nested writes are rejected.
 if(CashDelta!=0)OnBalanceChanged.Broadcast(Balance);
 OnAccountsChanged.Broadcast();
}
bool UNeonCityEconomySubsystem::AddMoney(int64 Amount,FName Reason)
{
 if(bChangingAccounts || Amount<=0 || Amount>MAX_int64-Balance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 Balance+=Amount;Record(Amount,Reason,ENeonCityTransactionType::Income);return true;
}
bool UNeonCityEconomySubsystem::RemoveMoney(int64 Amount,FName Reason)
{
 if(bChangingAccounts || Amount<=0 || Amount>Balance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 Balance-=Amount;Record(-Amount,Reason,ENeonCityTransactionType::Spending);return true;
}
bool UNeonCityEconomySubsystem::CreditOnce(int64 Amount,FName ReceiptKey)
{
 if(bChangingAccounts || CreditReceipts.Num()>=1024 || ReceiptKey.IsNone() || HasCredit(ReceiptKey) || Amount<=0 || Amount>MAX_int64-Balance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 CreditReceipts.Add(ReceiptKey);Balance+=Amount;Record(Amount,ReceiptKey,ENeonCityTransactionType::Income);return true;
}
bool UNeonCityEconomySubsystem::Deposit(int64 Amount)
{
 if(bChangingAccounts || Amount<=0 || Amount>Balance || Amount>MAX_int64-BankBalance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 Balance-=Amount;BankBalance+=Amount;Record(-Amount,TEXT("BankDeposit"),ENeonCityTransactionType::Deposit,Amount);return true;
}
bool UNeonCityEconomySubsystem::Withdraw(int64 Amount)
{
 if(bChangingAccounts || Amount<=0 || Amount>BankBalance || Amount>MAX_int64-Balance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 BankBalance-=Amount;Balance+=Amount;Record(Amount,TEXT("BankWithdrawal"),ENeonCityTransactionType::Withdrawal,-Amount);return true;
}
FNeonCityLoanQuote UNeonCityEconomySubsystem::GetLoanQuote(int64 Amount,ENeonCityLoanPurpose Purpose) const
{
 FNeonCityLoanQuote Quote;Quote.Principal=Amount;
 // Clamp before multiplication: even two independently full int64 accounts cannot overflow this quote.
 const int64 Cap=FMath::Clamp<int64>(MaximumLoan,100,1000000);
 const int64 Liquid=FMath::Min(Balance,Cap*100)+FMath::Min(BankBalance,Cap*100);
 Quote.Limit=FMath::Min(Cap,Liquid*FMath::Clamp(LoanLiquidityPercent,1,100)/100);
 if(uint8(Purpose)>uint8(ENeonCityLoanPurpose::Business))Quote.Reason=NSLOCTEXT("NeonCity","LoanPurpose","Choose a supported loan purpose.");
 else if(GetDebt()>0)Quote.Reason=NSLOCTEXT("NeonCity","LoanActive","Repay your current loan before borrowing again.");
 else if(Amount<FMath::Clamp<int64>(MinimumLoan,100,Cap) || Amount>Quote.Limit)Quote.Reason=NSLOCTEXT("NeonCity","LoanLimit","Choose an amount within your available limit.");
 else if(Amount>MAX_int64-BankBalance)Quote.Reason=NSLOCTEXT("NeonCity","LoanAccountFull","Your bank account cannot receive this amount.");
 else
 {
  Quote.Fee=(Amount*FMath::Clamp(LoanFeePercent,0,100)+99)/100;
  Quote.TotalRepayment=Amount+Quote.Fee;Quote.Allowed=true;
 }
 return Quote;
}
bool UNeonCityEconomySubsystem::RequestLoan(int64 Amount,ENeonCityLoanPurpose Purpose)
{
 if(bChangingAccounts)return false;
 const auto Quote=GetLoanQuote(Amount,Purpose);if(!Quote.Allowed)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 ActiveLoan.LoanID=FGuid::NewGuid();ActiveLoan.Purpose=Purpose;ActiveLoan.Principal=Amount;
 ActiveLoan.Fee=Quote.Fee;ActiveLoan.Remaining=Quote.TotalRepayment;ActiveLoan.IssuedAt=FDateTime::UtcNow();
 BankBalance+=Amount;Record(0,TEXT("LoanDisbursal"),ENeonCityTransactionType::Loan,Amount,Quote.TotalRepayment,ActiveLoan.LoanID);return true;
}
bool UNeonCityEconomySubsystem::RepayLoan(int64 Amount)
{
 if(bChangingAccounts || Amount<=0 || Amount>GetDebt() || Amount>BankBalance)return false;
 TGuardValue<bool> Guard(bChangingAccounts,true);
 const FGuid ID=ActiveLoan.LoanID;BankBalance-=Amount;ActiveLoan.Remaining-=Amount;
 if(ActiveLoan.Remaining==0)ActiveLoan=FNeonCityLoanData();
 Record(0,TEXT("LoanRepayment"),ENeonCityTransactionType::Repayment,-Amount,-Amount,ID);return true;
}
