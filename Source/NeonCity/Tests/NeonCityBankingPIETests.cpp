#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityBankingObserver.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonSerializer.h"
namespace NeonCityBankingTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
FString Slot(bool Cross) { return Cross?TEXT("NeonCity_Test_Banking_Cross"):TEXT("NeonCity_Test_Banking_Local"); }
FString LedgerSignature(const UNeonCityEconomySubsystem* E)
{
 FString Result;
 for(const auto& T:E->GetTransactions())Result+=FString::Printf(TEXT("%s|%d|%lld|%lld|%lld|%lld|%lld|%lld|%s|%s|%lld;"),
  *T.Id.ToString(),int32(T.Type),T.Amount,T.BalanceAfter,T.BankDelta,T.BankBalanceAfter,T.DebtDelta,T.DebtAfter,*T.LoanID.ToString(),*T.Reason.ToString(),T.Timestamp.GetTicks());
 return Result;
}
bool Manifest(FAutomationTestBase* Test,const TCHAR* Mode,const FGuid& SaveID,UNeonCityEconomySubsystem* E)
{
 const auto Json=MakeShared<FJsonObject>();Json->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());
 Json->SetStringField(TEXT("saveId"),SaveID.ToString());const auto Loan=E->GetActiveLoan();
 Json->SetStringField(TEXT("loanId"),Loan.LoanID.ToString());Json->SetNumberField(TEXT("purpose"),int32(Loan.Purpose));
 Json->SetNumberField(TEXT("principal"),Loan.Principal);Json->SetNumberField(TEXT("fee"),Loan.Fee);Json->SetStringField(TEXT("issuedAt"),Loan.IssuedAt.ToIso8601());
 Json->SetNumberField(TEXT("cash"),E->GetBalance());Json->SetNumberField(TEXT("bank"),E->GetBankBalance());Json->SetNumberField(TEXT("debt"),E->GetDebt());
 Json->SetNumberField(TEXT("historyCount"),E->GetTransactions().Num());Json->SetStringField(TEXT("ledgerSignature"),LedgerSignature(E));
 FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("Banking");IFileManager::Get().MakeDirectory(*Directory,true);
 return Test->TestTrue(TEXT("Bank restart evidence saved"),FFileHelper::SaveStringToFile(Text,*(Directory/(FString(TEXT("Session"))+Mode+TEXT(".json")))));
}
class FPhoneBanking : public IAutomationLatentCommand
{
public:
 explicit FPhoneBanking(FAutomationTestBase* T,bool Cross=false):Test(T),CrossSession(Cross) {}
 bool Next(int32 Value) { Stage=Value;At=W->GetTimeSeconds();return false; }
 void Shot(const TCHAR* Name) { Test->TestTrue(TEXT("Actual banking UI captured"),FNeonCityVisualEvidence::CaptureHUD(W,Name)); }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>65) { Test->AddError(FString::Printf(TEXT("Bank phone timed out at %d"),Stage));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* GI=W->GetGameInstance();auto* E=GI->GetSubsystem<UNeonCityEconomySubsystem>();auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();auto* Phone=PC->GetPhoneComponent();
  if(Stage==0)
  {
   auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   Save->ConfigureSlot(Slot(CrossSession));Save->DeleteSave();
   if(!Test->TestTrue(TEXT("New game includes bank defaults through unified save"),Save->CreateNewGameSave()))return true;
   Test->TestEqual(TEXT("New game cash unchanged"),E->GetBalance(),int64(1850));Test->TestEqual(TEXT("New game bank zero"),E->GetBankBalance(),int64(0));
   Test->TestEqual(TEXT("New game no debt"),E->GetDebt(),int64(0));
   Key(PC,EKeys::P,IE_Pressed);return Next(1);
  }
  if(W->GetTimeSeconds()-At<.45f)return false;
  switch(Stage)
  {
  case 1:Key(PC,EKeys::P,IE_Released);Key(PC,EKeys::B,IE_Pressed);return Next(2);
  case 2:
   Key(PC,EKeys::B,IE_Released);Test->TestEqual(TEXT("B opens registered Bank app"),Phone->GetActiveApp(),ENeonCityPhoneApp::Bank);
   Shot(TEXT("Bank-Accounts.png"));Key(PC,EKeys::End,IE_Pressed);return Next(3);
  case 3:
   Key(PC,EKeys::End,IE_Released);Test->TestTrue(TEXT("Insufficient bank funds give feedback"),Phone->GetDisplayedText().ToString().Contains(TEXT("Withdrawal declined")));
   Test->TestEqual(TEXT("Declined withdrawal does not change cash"),E->GetBalance(),int64(1850));Observer.Reset(NewObject<UNeonCityBankingObserver>());Observer->Accounts=E;Observer->Save=Save;
   E->OnAccountsChanged.AddDynamic(Observer.Get(),&UNeonCityBankingObserver::AccountsChanged);
   Key(PC,EKeys::Home,IE_Pressed);return Next(4);
  case 4:
   Key(PC,EKeys::Home,IE_Released);Test->TestEqual(TEXT("Actual Home deposits cash"),E->GetBalance(),int64(1750));Test->TestEqual(TEXT("Actual deposit credits bank"),E->GetBankBalance(),int64(100));
   Test->TestEqual(TEXT("Real phone transfer notifies once"),Observer->Calls,1);
   Test->TestFalse(TEXT("Nested account writes blocked in PIE"),Observer->NestedMutationAccepted);
   Test->TestFalse(TEXT("Save coordinator rejects reentrant load before changing actors"),Observer->NestedLoadAccepted);
   Test->TestEqual(TEXT("Nested load reports busy"),Save->GetLastResult(),ENeonCitySaveResult::Busy);
   E->OnAccountsChanged.RemoveAll(Observer.Get());Observer.Reset();
   Shot(TEXT("Bank-Deposit.png"));Key(PC,EKeys::End,IE_Pressed);return Next(5);
  case 5:
   Key(PC,EKeys::End,IE_Released);Test->TestEqual(TEXT("Actual End withdraws bank"),E->GetBankBalance(),int64(0));
   Test->TestEqual(TEXT("Withdrawal restores cash"),E->GetBalance(),int64(1850));Key(PC,EKeys::Enter,IE_Pressed);return Next(6);
  case 6:
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Enter without reviewed terms does not borrow"),E->GetDebt(),int64(0));Key(PC,EKeys::L,IE_Pressed);return Next(7);
  case 7:
   Key(PC,EKeys::L,IE_Released);Test->TestTrue(TEXT("Terms disclose full repayment before borrowing"),Phone->GetDisplayedText().ToString().Contains(TEXT("Total to repay $110")));
   Test->TestTrue(TEXT("Terms require explicit confirmation"),Phone->GetDisplayedText().ToString().Contains(TEXT("ENTER  Confirm loan")));
   Test->TestEqual(TEXT("Reviewing quote does not disburse money"),E->GetBankBalance(),int64(0));Shot(TEXT("Bank-LoanTerms.png"));
   Key(PC,EKeys::Enter,IE_Pressed);return Next(8);
  case 8:
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Confirmed loan credited only once to bank"),E->GetBankBalance(),int64(100));
   Test->TestEqual(TEXT("Confirmed loan includes disclosed fee"),E->GetDebt(),int64(110));LoanID=E->GetActiveLoan().LoanID;
   Key(PC,EKeys::Home,IE_Pressed);return Next(9);
  case 9:
   Key(PC,EKeys::Home,IE_Released);Test->TestEqual(TEXT("Second deposit funds repayment"),E->GetBankBalance(),int64(200));Key(PC,EKeys::K,IE_Pressed);return Next(10);
  case 10:
  {
   Key(PC,EKeys::K,IE_Released);Test->TestEqual(TEXT("K repays selected amount"),E->GetDebt(),int64(10));Test->TestEqual(TEXT("Repayment debits bank"),E->GetBankBalance(),int64(100));
   Test->TestEqual(TEXT("Cash unchanged by repayment"),E->GetBalance(),int64(1750));Shot(TEXT("Bank-Repayment.png"));
   if(!Test->TestTrue(TEXT("Bank state writes via SaveSubsystem"),Save->SaveGame()))return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());if(!Test->TestTrue(TEXT("Banking disk model readable"),Model.IsValid()))return true;
   Test->TestTrue(TEXT("New save contains explicit optional banking section"),Model->Economy.HasBanking);
   SaveID=Model->SaveID;History=LedgerSignature(E);
   if(CrossSession) { Manifest(Test,TEXT("A"),SaveID,E);return true; }
   return Next(11);
  }
  case 11:Key(PC,EKeys::K,IE_Pressed);return Next(12);
  case 12:
   Key(PC,EKeys::K,IE_Released);Test->TestEqual(TEXT("Final small remainder can be repaid"),E->GetDebt(),int64(0));
   if(!Test->TestTrue(TEXT("Unified load restores bank state"),Save->LoadGame()))return true;return Next(13);
  case 13:
   Test->TestEqual(TEXT("Load restores cash"),E->GetBalance(),int64(1750));Test->TestEqual(TEXT("Load restores bank"),E->GetBankBalance(),int64(100));
   Test->TestEqual(TEXT("Load restores outstanding debt"),E->GetDebt(),int64(10));Test->TestEqual(TEXT("Load restores stable loan ID"),E->GetActiveLoan().LoanID,LoanID);
   Test->TestEqual(TEXT("Load retains exact transactions without extra income"),LedgerSignature(E),History);
   Test->TestTrue(TEXT("Open phone refreshes from restored accounts"),Phone->GetDisplayedText().ToString().Contains(TEXT("Loan outstanding  $10")));
   Shot(TEXT("Bank-Restored.png"));Key(PC,EKeys::K,IE_Pressed);return Next(14);
  case 14:
   Key(PC,EKeys::K,IE_Released);Test->TestEqual(TEXT("Restored small remainder settles through UI"),E->GetBankBalance(),int64(90));
   Key(PC,EKeys::Insert,IE_Pressed);return Next(15);
  case 15:
   Key(PC,EKeys::Insert,IE_Released);Test->TestTrue(TEXT("All-amount mode is visible"),Phone->GetDisplayedText().ToString().Contains(TEXT("Amount  ALL")));
   Key(PC,EKeys::End,IE_Pressed);return Next(16);
  case 16:
   Key(PC,EKeys::End,IE_Released);Test->TestEqual(TEXT("All withdrawal can recover a balance below the fixed step"),E->GetBankBalance(),int64(0));
   Test->TestEqual(TEXT("All withdrawal conserves remaining money"),E->GetBalance(),int64(1840));Key(PC,EKeys::Home,IE_Pressed);return Next(17);
  case 17:
   Key(PC,EKeys::Home,IE_Released);Test->TestEqual(TEXT("All deposit includes non-round cash"),E->GetBalance(),int64(0));
   Test->TestEqual(TEXT("All cash reaches bank"),E->GetBankBalance(),int64(1840));Shot(TEXT("Bank-AllFunds.png"));
   Test->TestTrue(TEXT("Restore original bank save after all-amount tests"),Save->LoadGame());return Next(18);
  case 18:
   Test->TestEqual(TEXT("All-amount controls do not change saved values"),E->GetBalance(),int64(1750));
   Key(PC,EKeys::P,IE_Pressed);return Next(19);
  case 19:
   Key(PC,EKeys::P,IE_Released);Test->TestFalse(TEXT("Bank phone closes normally"),Phone->IsPhoneOpen());Key(PC,EKeys::Home,IE_Pressed);return Next(20);
  case 20:
   Key(PC,EKeys::Home,IE_Released);Test->TestEqual(TEXT("Closed phone cannot deposit"),E->GetBalance(),int64(1750));
   Test->TestTrue(TEXT("Delete isolated bank save"),Save->DeleteSave());Test->TestFalse(TEXT("Deleted bank slot no longer exists"),Save->DoesSaveExist());return true;
  default:return true;
  }
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerController* PC=nullptr;
 TStrongObjectPtr<UNeonCityBankingObserver> Observer;
 int32 Stage=0;float At=0;double Started=0;bool CrossSession=false;FGuid LoanID,SaveID;FString History;
};
class FRestartBanking : public IAutomationLatentCommand
{
public:
 explicit FRestartBanking(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>45) { Test->AddError(TEXT("Bank restart timed out"));return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
  auto* GI=W->GetGameInstance();auto* E=GI->GetSubsystem<UNeonCityEconomySubsystem>();auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();
  FString Previous;FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Banking/SessionA.json")));
  TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Previous),Json);double PID=0;
  if(!Test->TestTrue(TEXT("Bank restore runs in a new OS process"),Json.IsValid() && Json->TryGetNumberField(TEXT("processId"),PID) && PID>0 && PID!=FPlatformProcess::GetCurrentProcessId()))return true;
  Save->ConfigureSlot(Slot(true));if(!Test->TestTrue(TEXT("Actual saved banking slot exists"),Save->DoesSaveExist()))return true;
  TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());FString Expected;Json->TryGetStringField(TEXT("saveId"),Expected);
  if(!Test->TestTrue(TEXT("Bank save generation matches session A"),Model.IsValid() && Model->SaveID.ToString()==Expected))return true;
  if(!Test->TestTrue(TEXT("Load banking from disk after restart"),Save->LoadGame()))return true;
  Test->TestEqual(TEXT("Restart cash"),E->GetBalance(),int64(1750));Test->TestEqual(TEXT("Restart bank"),E->GetBankBalance(),int64(100));Test->TestEqual(TEXT("Restart debt"),E->GetDebt(),int64(10));
  Json->TryGetStringField(TEXT("loanId"),Expected);Test->TestEqual(TEXT("Loan identity survives restart"),E->GetActiveLoan().LoanID.ToString(),Expected);
  Json->TryGetStringField(TEXT("ledgerSignature"),Expected);Test->TestEqual(TEXT("Every history field survives restart"),LedgerSignature(E),Expected);
  Test->TestTrue(TEXT("Repeated Load does not mint a disbursal"),Save->LoadGame());Test->TestEqual(TEXT("Repeated load has same ledger"),LedgerSignature(E),Expected);
  Manifest(Test,TEXT("B"),Model->SaveID,E);
  Test->TestTrue(TEXT("Restored loan can settle normally"),E->RepayLoan(10));Test->TestEqual(TEXT("Settlement clears debt"),E->GetDebt(),int64(0));
  Test->TestEqual(TEXT("Settlement costs remaining amount only"),E->GetBankBalance(),int64(90));
  Test->TestTrue(TEXT("Paid loan saves"),Save->SaveGame());Test->TestTrue(TEXT("Paid loan restores"),Save->LoadGame());Test->TestEqual(TEXT("Paid debt stays cleared"),E->GetDebt(),int64(0));
  Test->TestTrue(TEXT("Restart cleans only its isolated slot"),Save->DeleteSave());return true;
 }
private:FAutomationTestBase* Test;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityPhoneBank,"NeonCity.Economy.BankingPhonePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityPhoneBank::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityBankingTests::FPhoneBanking>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FNeonCityBankRestart,"NeonCity.Economy.BankingCrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FNeonCityBankRestart::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityBankingSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode);Commands.Add(Mode); }
}
bool FNeonCityBankRestart::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 if(Parameters==TEXT("A"))FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityBankingTests::FPhoneBanking>(this,true));
 else FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityBankingTests::FRestartBanking>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
