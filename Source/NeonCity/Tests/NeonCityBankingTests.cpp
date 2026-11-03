#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Tests/NeonCityBankingObserver.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityBankingLedger,"NeonCity.Economy.BankingLedger",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityBankingLedger::RunTest(const FString&)
{
 TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
 TStrongObjectPtr<UNeonCityEconomySubsystem> E(NewObject<UNeonCityEconomySubsystem>(GI.Get()));
 TestEqual(TEXT("Starting cash retained"),E->GetBalance(),int64(1850));
 TestEqual(TEXT("Bank starts empty"),E->GetBankBalance(),int64(0));
 TestFalse(TEXT("Cannot withdraw absent funds"),E->Withdraw(1));
 TestFalse(TEXT("Cannot overdraw cash"),E->Deposit(1851));
 TestFalse(TEXT("Zero transfer rejected"),E->Deposit(0));
 TestFalse(TEXT("Negative transfer rejected"),E->Withdraw(-100));
 TestEqual(TEXT("Rejected transfers leave no receipts"),E->GetTransactions().Num(),0);
 TStrongObjectPtr<UNeonCityBankingObserver> Observer(NewObject<UNeonCityBankingObserver>());Observer->Accounts=E.Get();
 E->OnAccountsChanged.AddDynamic(Observer.Get(),&UNeonCityBankingObserver::AccountsChanged);
 TestTrue(TEXT("Deposit cash"),E->Deposit(1000));
 TestEqual(TEXT("Transfer broadcasts once"),Observer->Calls,1);
 TestEqual(TEXT("Listener sees final cash"),Observer->SeenCash,int64(850));
 TestEqual(TEXT("Listener sees final bank"),Observer->SeenBank,int64(1000));
 TestFalse(TEXT("Listener cannot mutate a transfer reentrantly"),Observer->NestedMutationAccepted);
 TestFalse(TEXT("Rejected nested reward cannot claim a receipt"),E->HasCredit(TEXT("ReentrantReward")));
 E->OnAccountsChanged.RemoveAll(Observer.Get());
 TestEqual(TEXT("Deposit cash debit"),E->GetBalance(),int64(850));
 TestEqual(TEXT("Deposit bank credit"),E->GetBankBalance(),int64(1000));
 TestTrue(TEXT("Withdraw bank funds"),E->Withdraw(150));
 TestEqual(TEXT("Transfers conserve cash plus bank"),E->GetBalance()+E->GetBankBalance(),int64(1850));
 const auto Quote=E->GetLoanQuote(500,ENeonCityLoanPurpose::Vehicle);
 TestTrue(TEXT("Eligible capped quote"),Quote.Allowed);
 TestEqual(TEXT("Liquidity limit independent of deposit placement"),Quote.Limit,int64(925));
 TestEqual(TEXT("Disclosed one-time fee"),Quote.Fee,int64(50));
 TestFalse(TEXT("Limit cannot be bypassed"),E->RequestLoan(926,ENeonCityLoanPurpose::Vehicle));
 TestFalse(TEXT("Unknown purpose rejected"),E->RequestLoan(500,ENeonCityLoanPurpose(255)));
 TestTrue(TEXT("Vehicle loan deposited into bank"),E->RequestLoan(500,ENeonCityLoanPurpose::Vehicle));
 const FGuid LoanID=E->GetActiveLoan().LoanID;
 TestTrue(TEXT("Loan has stable value ID"),LoanID.IsValid());
 TestEqual(TEXT("Loan does not masquerade as cash reward"),E->GetBalance(),int64(1000));
 TestEqual(TEXT("Loan raises bank"),E->GetBankBalance(),int64(1350));
 TestEqual(TEXT("Debt includes quoted fee"),E->GetDebt(),int64(550));
 TestFalse(TEXT("Only one outstanding loan"),E->RequestLoan(100,ENeonCityLoanPurpose::Property));
 TestFalse(TEXT("No negative repayment"),E->RepayLoan(-1));
 TestFalse(TEXT("No overpayment"),E->RepayLoan(551));
 TestTrue(TEXT("Partial repayment"),E->RepayLoan(200));
 TestEqual(TEXT("Partial repayment retains loan identity"),E->GetActiveLoan().LoanID,LoanID);
 const auto Saved=E->BuildPersistenceSnapshot();
 TStrongObjectPtr<UNeonCityEconomySubsystem> Restored(NewObject<UNeonCityEconomySubsystem>(GI.Get()));
 Restored->ApplyPersistenceSnapshot(Saved);
 TestEqual(TEXT("Cash snapshot restored"),Restored->GetBalance(),int64(1000));
 TestEqual(TEXT("Bank snapshot restored"),Restored->GetBankBalance(),int64(1150));
 TestEqual(TEXT("Debt snapshot restored"),Restored->GetDebt(),int64(350));
 TestEqual(TEXT("Loan ID survives snapshot"),Restored->GetActiveLoan().LoanID,LoanID);
 TestEqual(TEXT("History restored without synthetic income"),Restored->GetTransactions().Num(),Saved.Transactions.Num());
 TestEqual(TEXT("History ID preserved"),Restored->GetTransactions().Last().Id,Saved.Transactions.Last().Id);
 TestTrue(TEXT("Loan can be fully repaid"),Restored->RepayLoan(350));
 TestFalse(TEXT("Paid active record clears"),Restored->GetActiveLoan().LoanID.IsValid());
 TestEqual(TEXT("Full repayment costs only disclosed fee"),Restored->GetBalance()+Restored->GetBankBalance(),int64(1800));
 TestEqual(TEXT("Paid receipt retains loan identity"),Restored->GetTransactions().Last().LoanID,LoanID);
 TestFalse(TEXT("No repayment after settlement"),Restored->RepayLoan(1));
 TestTrue(TEXT("Business purpose supported after settlement"),Restored->RequestLoan(100,ENeonCityLoanPurpose::Business));
 auto Overflow=Saved;Overflow.Balance=MAX_int64;Overflow.BankBalance=1;Overflow.ActiveLoan=FNeonCityLoanData();Overflow.Transactions.Reset();
 Restored->ApplyPersistenceSnapshot(Overflow);
 TestFalse(TEXT("Withdrawal cannot overflow cash"),Restored->Withdraw(1));
 Overflow.Balance=1;Overflow.BankBalance=MAX_int64;Restored->ApplyPersistenceSnapshot(Overflow);
 TestFalse(TEXT("Deposit cannot overflow bank"),Restored->Deposit(1));
 TestFalse(TEXT("Loan cannot overflow bank"),Restored->RequestLoan(100,ENeonCityLoanPurpose::Vehicle));
 TestEqual(TEXT("Overflow failure keeps cash"),Restored->GetBalance(),int64(1));
 TestEqual(TEXT("Overflow failure keeps bank"),Restored->GetBankBalance(),int64(MAX_int64));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityBankingCompatibility,"NeonCity.Economy.BankingCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityBankingCompatibility::RunTest(const FString&)
{
 TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
 TStrongObjectPtr<UNeonCityEconomySubsystem> E(NewObject<UNeonCityEconomySubsystem>(GI.Get()));
 E->Deposit(1000);E->RequestLoan(500,ENeonCityLoanPurpose::Property);
 FNeonCityEconomySaveData Legacy;Legacy.Balance=2200;Legacy.CreditReceipts.Add(TEXT("ExistingReward"));
 E->ApplyPersistenceSnapshot(Legacy);
 TestEqual(TEXT("Pre-banking save retains its cash"),E->GetBalance(),int64(2200));
 TestEqual(TEXT("Legacy bank defaults to zero"),E->GetBankBalance(),int64(0));
 TestEqual(TEXT("Legacy snapshot clears newer active debt"),E->GetDebt(),int64(0));
 TestTrue(TEXT("Legacy receipt remains authoritative"),E->HasCredit(TEXT("ExistingReward")));
 TestFalse(TEXT("Legacy reward cannot be replayed"),E->CreditOnce(350,TEXT("ExistingReward")));
 TestTrue(TEXT("Legacy does not invent transaction history"),E->GetTransactions().IsEmpty());
 E->Deposit(1000);E->RequestLoan(500,ENeonCityLoanPurpose::Property);
 const auto Valid=E->BuildPersistenceSnapshot();FString Error;
 auto Bad=Valid;Bad.BankBalance=-1;
 TestFalse(TEXT("Negative bank rejected"),E->ValidatePersistenceSnapshot(Bad,Error));
 E->ApplyPersistenceSnapshot(Bad);TestEqual(TEXT("Bad snapshot causes no partial restore"),E->GetBankBalance(),Valid.BankBalance);
 Bad=Valid;Bad.ActiveLoan.Remaining=MAX_int64;
 TestFalse(TEXT("Corrupt debt rejected without overflow"),E->ValidatePersistenceSnapshot(Bad,Error));
 Bad=Valid;Bad.ActiveLoan.LoanID.Invalidate();
 TestFalse(TEXT("Missing persistent loan ID rejected"),E->ValidatePersistenceSnapshot(Bad,Error));
 Bad=Valid;Bad.ActiveLoan.Purpose=ENeonCityLoanPurpose(255);
 TestFalse(TEXT("Unknown purpose rejected during restore"),E->ValidatePersistenceSnapshot(Bad,Error));
 Bad=Valid;Bad.Transactions.Add(Valid.Transactions[0]);auto Entry=Valid.Transactions[0];Entry.Id=FGuid::NewGuid();Entry.Amount=MIN_int64;Bad.Transactions.Add(Entry);
 TArray<FString> Warnings;E->NormalizePersistenceSnapshot(Bad,Warnings);
 TestEqual(TEXT("Bad or duplicate informational receipts skipped"),Bad.Transactions.Num(),Valid.Transactions.Num());
 TestEqual(TEXT("Each rejected history record is reported"),Warnings.Num(),2);
 TestEqual(TEXT("Normalization never derives money from history"),Bad.BankBalance,Valid.BankBalance);
 TStrongObjectPtr<UNeonCitySaveGame> Model(NewObject<UNeonCitySaveGame>());
 Model->FormatID=TEXT("NeonCity.SaveGame");Model->SaveID=FGuid::NewGuid();Model->HasEconomy=true;Model->Economy=Valid;
 TestTrue(TEXT("Version 1 envelope accepts new banking values"),Model->ValidateEnvelope(Error));
 Model->Economy.BankBalance=-1;TestFalse(TEXT("Coordinator envelope blocks bad banking before any system restore"),Model->ValidateEnvelope(Error));
 for(int32 I=0;I<260;++I)E->CreditOnce(1,*FString::Printf(TEXT("Activity.%d"),I));
 TestEqual(TEXT("Persisted ledger stays bounded"),E->BuildPersistenceSnapshot().Transactions.Num(),256);
 TestFalse(TEXT("Trimming history never unlocks an old reward"),E->CreditOnce(350,TEXT("ExistingReward")));
 auto Empty=FNeonCityEconomySaveData();E->ApplyPersistenceSnapshot(Empty);
 const auto OldQuote=E->GetLoanQuote(900,ENeonCityLoanPurpose::Vehicle);
 TestTrue(TEXT("Initial quote eligible"),OldQuote.Allowed);E->RemoveMoney(1800);
 TestFalse(TEXT("Execution re-evaluates stale quotes"),E->RequestLoan(900,ENeonCityLoanPurpose::Vehicle));
 TestEqual(TEXT("Stale quote cannot create money"),E->GetBankBalance(),int64(0));
 return true;
}
#endif
