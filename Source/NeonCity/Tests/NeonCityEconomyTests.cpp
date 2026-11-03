#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Engine/GameInstance.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityMoneyInvariants,"NeonCity.Economy.Invariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityMoneyInvariants::RunTest(const FString& Parameters)
{
	auto* GI=NewObject<UGameInstance>();
	auto* Wallet=NewObject<UNeonCityEconomySubsystem>(GI);
	TestEqual(TEXT("Default cash"),Wallet->GetBalance(),int64(1850));
	TestFalse(TEXT("Negative credit rejected"),Wallet->AddMoney(-1));
	TestFalse(TEXT("Zero credit rejected"),Wallet->AddMoney(0));
	TestFalse(TEXT("Negative debit rejected"),Wallet->RemoveMoney(-10));
	TestFalse(TEXT("Overdraft rejected"),Wallet->RemoveMoney(1851));
	TestEqual(TEXT("Rejected operations create no receipts"),Wallet->GetTransactions().Num(),0);
	TestTrue(TEXT("Debit succeeds"),Wallet->RemoveMoney(50,TEXT("TestDebit")));
	TestTrue(TEXT("Credit succeeds"),Wallet->AddMoney(50,TEXT("TestCredit")));
	TestTrue(TEXT("Reward succeeds once"),Wallet->CreditOnce(350,TEXT("TestReward")));
	TestFalse(TEXT("Duplicate reward rejected"),Wallet->CreditOnce(350,TEXT("TestReward")));
	TestEqual(TEXT("Resulting balance"),Wallet->GetBalance(),int64(2200));
	TestEqual(TEXT("Signed debit receipt"),Wallet->GetTransactions()[0].Amount,int64(-50));
	TestTrue(TEXT("Receipt uses UTC timestamp"),Wallet->GetTransactions()[0].Timestamp.GetTicks()>0);
	TestTrue(TEXT("Boundary credit allowed"),Wallet->AddMoney(MAX_int64-2200));
	TestFalse(TEXT("Overflow rejected"),Wallet->AddMoney(1));
	TestFalse(TEXT("Reward overflow rejected"),Wallet->CreditOnce(1,TEXT("OverflowReward")));
	TestFalse(TEXT("Failed reward leaves no claim"),Wallet->HasCredit(TEXT("OverflowReward")));
	TestTrue(TEXT("Whole balance may be removed"),Wallet->RemoveMoney(MAX_int64));
	TestEqual(TEXT("No negative balance"),Wallet->GetBalance(),int64(0));
	for (int32 I=0; I<260; ++I) { Wallet->AddMoney(1); }
	TestEqual(TEXT("Bounded transaction history"),Wallet->GetTransactions().Num(),256);
	TestFalse(TEXT("Trimmed history cannot permit repeat reward"),Wallet->CreditOnce(350,TEXT("TestReward")));
	return true;
}
#endif
