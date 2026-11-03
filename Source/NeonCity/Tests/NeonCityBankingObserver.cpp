#include "Tests/NeonCityBankingObserver.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
void UNeonCityBankingObserver::AccountsChanged()
{
#if WITH_DEV_AUTOMATION_TESTS
 if(!Accounts)return;
 ++Calls;SeenCash=Accounts->GetBalance();SeenBank=Accounts->GetBankBalance();
 // With a broken guard the second callback returns, so this test cannot recurse indefinitely.
 if(Calls!=1)return;
 NestedMutationAccepted=Accounts->Withdraw(100);
 NestedMutationAccepted|=Accounts->CreditOnce(1,TEXT("ReentrantReward"));
 auto Snapshot=Accounts->BuildPersistenceSnapshot();Snapshot.Balance=0;
 Accounts->ApplyPersistenceSnapshot(Snapshot);
 if(Save)NestedLoadAccepted=Save->LoadGame();
#endif
}
