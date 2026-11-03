#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneWidget.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"

namespace NeonCityOpportunityTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
FString Slot(bool CrossSession,bool Ignored)
{ return FString(TEXT("NeonCity_Test_Opportunity_"))+(CrossSession?TEXT("Cross_"):TEXT("Local_"))+(Ignored?TEXT("Ignored"):TEXT("Declined")); }
bool Manifest(FAutomationTestBase* Test,const FString& Mode,const FGuid& IgnoredID,const FGuid& DeclinedID,int64 Balance)
{
 const auto Json=MakeShared<FJsonObject>();Json->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());
 Json->SetStringField(TEXT("ignoredSaveId"),IgnoredID.ToString());Json->SetStringField(TEXT("declinedSaveId"),DeclinedID.ToString());
 Json->SetStringField(TEXT("opportunityId"),ANeonCityTransportTest::ReceiptKey().ToString());Json->SetNumberField(TEXT("balance"),Balance);
 FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("Opportunities");IFileManager::Get().MakeDirectory(*Directory,true);
 return Test->TestTrue(TEXT("Choice restart evidence saved"),FFileHelper::SaveStringToFile(Text,*(Directory/(TEXT("Session")+Mode+TEXT(".json")))));
}
class FChoices : public IAutomationLatentCommand
{
public:
 explicit FChoices(FAutomationTestBase* T,bool SessionA=false):Test(T),CrossSession(SessionA) {}
 bool Next(int32 Value) { Stage=Value;At=W->GetTimeSeconds();return false; }
 void Shot(const TCHAR* Name) { Test->TestTrue(TEXT("Actual opportunity screen captured"),FNeonCityVisualEvidence::CaptureHUD(W,Name)); }
 bool SaveChoice(bool Ignored)
 {
  Save->ConfigureSlot(Slot(CrossSession,Ignored));Save->DeleteSave();
  if(!Test->TestTrue(TEXT("Choice writes through SaveSubsystem"),Save->SaveGame()))return false;
  TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("Choice has readable disk data"),Model.IsValid()))return false;
  (Ignored?IgnoredID:DeclinedID)=Model->SaveID;SavedBalance=Model->Economy.Balance;return true;
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(FString::Printf(TEXT("Opportunity choices timed out at %d"),Stage));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* GI=W->GetGameInstance();auto* Router=GI->GetSubsystem<UNeonCityOpportunitySubsystem>();
  auto* Phone=PC->GetPhoneComponent();auto* Nav=PC->GetNavigationComponent();
  auto* Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();
  if(Stage==0)
  {
   auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityTransportTest> It(W);It;++It)Contract=*It;
   if(!Test->TestNotNull(TEXT("Authored Marcus provider exists"),Contract))return true;
   if(!Test->TestEqual(TEXT("Exactly one authored opportunity"),Router->GetOpportunities().Num(),1))return true;
   Test->TestTrue(TEXT("Registration is idempotent"),Router->RegisterProvider(Contract));
   Test->TestFalse(TEXT("Objects without provider interface rejected"),Router->RegisterProvider(Player));
   auto* Duplicate=W->SpawnActorDeferred<ANeonCityTransportTest>(ANeonCityTransportTest::StaticClass(),FTransform::Identity);
   if(!Test->TestNotNull(TEXT("Duplicate-provider state fixture spawned"),Duplicate))return true;
   Duplicate->DeliveryVehicle=Contract->DeliveryVehicle;Duplicate->FinishSpawning(FTransform::Identity);
   Test->TestFalse(TEXT("Another owner cannot hijack the stable ID"),Router->RegisterProvider(Duplicate));
   Duplicate->Destroy();Test->TestEqual(TEXT("Duplicate teardown preserves original provider"),Router->GetOpportunities().Num(),1);
   auto Records=Router->BuildSnapshot(true);if(!Test->TestEqual(TEXT("Default opportunity snapshot available"),Records.Num(),1))return true;
   auto Bad=Records[0];Bad.OpportunityID=TEXT("Unknown.Job");Records.Add(Bad);
   Bad=Records[0];Bad.VehicleID=FGuid::NewGuid();Records.Add(Bad);Bad=Records[0];Records.Add(Bad);
   TArray<FString> Warnings;Router->NormalizeSnapshot(Records,Warnings);
   if(!Test->TestEqual(TEXT("Unknown IDs, mismatched assigned cars and duplicates skipped"),Records.Num(),1))return true;
   Test->TestEqual(TEXT("Each bad record produces a warning"),Warnings.Num(),3);
   Records[0].State=250;Warnings.Reset();Router->NormalizeSnapshot(Records,Warnings);
   Test->TestTrue(TEXT("Unknown future states fail safely"),Records.IsEmpty());
   const auto AssignedCar=Contract->DeliveryVehicle;Contract->DeliveryVehicle=nullptr;
   Test->TestFalse(TEXT("Unavailable assigned car cannot be accepted"),Router->CanChoose(Contract->ReceiptKey(),ENeonCityOpportunityChoice::Accept));
   Test->TestTrue(TEXT("Unavailable offer explains its reason"),!Router->GetOpportunities()[0].UnavailableReason.IsEmpty());
   Test->TestTrue(TEXT("Unavailable offer can still be declined"),Router->CanChoose(Contract->ReceiptKey(),ENeonCityOpportunityChoice::Decline));
   Contract->DeliveryVehicle=AssignedCar;
   Key(PC,EKeys::P,IE_Pressed);return Next(1);
  }
  if(W->GetTimeSeconds()-At<.4f)return false;
  if(Stage==1) { Key(PC,EKeys::P,IE_Released);Key(PC,EKeys::O,IE_Pressed);return Next(2); }
  if(Stage==2)
  {
   Key(PC,EKeys::O,IE_Released);Test->TestEqual(TEXT("Actual O opens opportunities"),Phone->GetActiveApp(),ENeonCityPhoneApp::Opportunities);
   Shot(TEXT("Opportunity-Available.png"));Key(PC,EKeys::BackSpace,IE_Pressed);return Next(3);
  }
  if(Stage==3)
  {
   Key(PC,EKeys::BackSpace,IE_Released);Test->TestEqual(TEXT("Later records ignored choice"),Contract->GetState(),ENeonCityTransportState::Ignored);
   Test->TestFalse(TEXT("Ignoring never forces a route"),Nav->GetDisplay().Active);
   Test->TestEqual(TEXT("Ignoring does not charge or reward"),Economy->GetBalance(),int64(1850));
   Test->TestTrue(TEXT("Ignored opportunity can be revisited"),Router->CanChoose(Contract->ReceiptKey(),ENeonCityOpportunityChoice::Accept));
   Shot(TEXT("Opportunity-Later.png"));if(!SaveChoice(true))return true;
   Key(PC,EKeys::Delete,IE_Pressed);return Next(4);
  }
  if(Stage==4)
  {
   Key(PC,EKeys::Delete,IE_Released);Test->TestEqual(TEXT("Actual Delete declines"),Contract->GetState(),ENeonCityTransportState::Declined);
   Test->TestFalse(TEXT("Declined offer cannot silently accept"),Router->Choose(Contract->ReceiptKey(),ENeonCityOpportunityChoice::Accept,PC));
   Test->TestEqual(TEXT("Declining does not charge or reward"),Economy->GetBalance(),int64(1850));
   Shot(TEXT("Opportunity-Declined.png"));if(!SaveChoice(false))return true;
   if(CrossSession) { Manifest(Test,TEXT("A"),IgnoredID,DeclinedID,SavedBalance);return true; }
   Save->ConfigureSlot(Slot(false,true));Test->TestTrue(TEXT("Restore ignored decision"),Save->LoadGame());return Next(5);
  }
  if(Stage==5)
  {
   Test->TestEqual(TEXT("Ignored save preserves choice"),Contract->GetState(),ENeonCityTransportState::Ignored);
   Test->TestTrue(TEXT("Screen reflects saved-for-later status"),Phone->GetDisplayedText().ToString().Contains(TEXT("Saved for later")));
   Key(PC,EKeys::Enter,IE_Pressed);return Next(6);
  }
  if(Stage==6)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Ignored opportunity accepts via actual Enter"),Contract->GetState(),ENeonCityTransportState::Accepted);
   Test->TestTrue(TEXT("Accept starts real existing delivery navigation"),Nav->GetDisplay().Active);
   Save->ConfigureSlot(Slot(false,false));Test->TestTrue(TEXT("Restore declined decision"),Save->LoadGame());return Next(7);
  }
  if(Stage==7)
  {
   Test->TestEqual(TEXT("Declined save stays declined"),Contract->GetState(),ENeonCityTransportState::Declined);
   Test->TestFalse(TEXT("Declined restore removes prior accepted route"),Nav->GetDisplay().Active);
   GI->GetSubsystem<UMessageSystem>()->SendMessage(TEXT("Harbor bulletin"),FText::FromString(TEXT("The boardwalk is open.")));
   Key(PC,EKeys::O,IE_Pressed);return Next(8);
  }
  if(Stage==8)
  {
   Key(PC,EKeys::O,IE_Released);Test->TestTrue(TEXT("Latest message visible"),Phone->GetDisplayedText().ToString().Contains(TEXT("Harbor bulletin")));
   Key(PC,EKeys::Left,IE_Pressed);return Next(9);
  }
  if(Stage==9)
  {
   Key(PC,EKeys::Left,IE_Released);Test->TestTrue(TEXT("Previous message is Marcus reply"),Phone->GetDisplayedText().ToString().Contains(TEXT("another driver")));
   return Next(10);
  }
  if(Stage==10) { Key(PC,EKeys::Left,IE_Pressed);return Next(11); }
  if(Stage==11)
  {
   Key(PC,EKeys::Left,IE_Released);Test->TestTrue(TEXT("Original invitation remains browsable"),Phone->GetDisplayedText().ToString().Contains(TEXT("Can you deliver")));
   Shot(TEXT("Opportunity-MessageHistory.png"));Key(PC,EKeys::Enter,IE_Pressed);return Next(12);
  }
  if(Stage==12)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Stale message cannot reaccept declined task"),Contract->GetState(),ENeonCityTransportState::Declined);
   Test->TestEqual(TEXT("Browsing old messages cannot mint money"),Economy->GetBalance(),int64(1850));
   Save->ConfigureSlot(Slot(false,true));Save->DeleteSave();Save->ConfigureSlot(Slot(false,false));Save->DeleteSave();
   // Explicit long-message fixture verifies accessibility, not a new city job.
   FString LongMessage;for(int32 I=1;I<=40;++I)LongMessage+=FString::Printf(TEXT("Boardwalk notice %02d: keep the public path clear and enjoy the coast.\n"),I);
   GI->GetSubsystem<UMessageSystem>()->SendMessage(TEXT("Harbor noticeboard"),FText::FromString(LongMessage));return Next(13);
  }
  if(Stage==13)
  {
   for(TObjectIterator<UPhoneWidget> It;It;++It)if(It->GetWorld()==W && It->IsInViewport())Screen=*It;
   if(!Test->TestNotNull(TEXT("Actual open phone widget available"),Screen))return true;
   Test->TestTrue(TEXT("New message begins at its top"),FMath::IsNearlyZero(Screen->GetScrollOffset(),1.f));
   Shot(TEXT("Opportunity-LongMessageTop.png"));Key(PC,EKeys::PageDown,IE_Pressed);return Next(14);
  }
  if(Stage==14)
  {
   Key(PC,EKeys::PageDown,IE_Released);Test->TestTrue(TEXT("Actual Page Down scrolls long message"),Screen->GetScrollOffset()>100);
   Shot(TEXT("Opportunity-LongMessageScrolled.png"));Key(PC,EKeys::P,IE_Pressed);return Next(15);
  }
  if(Stage==15)
  {
   Key(PC,EKeys::P,IE_Released);Test->TestFalse(TEXT("Phone remains closable while scrolled"),Phone->IsPhoneOpen());return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerController* PC=nullptr;ANeonCityTransportTest* Contract=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;int32 Stage=0;float At=0;double Started=0;bool CrossSession=false;FGuid IgnoredID,DeclinedID;int64 SavedBalance=0;
 UPhoneWidget* Screen=nullptr;
};
class FRestartChoices : public IAutomationLatentCommand
{
public:
 explicit FRestartChoices(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>45) { Test->AddError(TEXT("Choice restart timed out"));return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
  auto* GI=W->GetGameInstance();auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();auto* Router=GI->GetSubsystem<UNeonCityOpportunitySubsystem>();
  FString Previous;FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Opportunities/SessionA.json")));
  TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Previous),Json);double PID=0;
  if(!Test->TestTrue(TEXT("Choice restart runs in a new OS process"),Json.IsValid() && Json->TryGetNumberField(TEXT("processId"),PID) && PID>0 && PID!=FPlatformProcess::GetCurrentProcessId()))return true;
  FGuid IgnoredID,DeclinedID;
  for(const bool Ignored:{true,false})
  {
   Save->ConfigureSlot(Slot(true,Ignored));if(!Test->TestTrue(TEXT("Restart finds actual choice save"),Save->DoesSaveExist()))return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
   FString ExpectedID;Json->TryGetStringField(Ignored?TEXT("ignoredSaveId"):TEXT("declinedSaveId"),ExpectedID);
   if(!Test->TestTrue(TEXT("Choice belongs to same disk generation"),Model.IsValid() && Model->SaveID.ToString()==ExpectedID))return true;
   (Ignored?IgnoredID:DeclinedID)=Model->SaveID;
   if(!Test->TestTrue(TEXT("Choice loads through SaveSubsystem"),Save->LoadGame()))return true;
   const auto Offers=Router->GetOpportunities();if(!Test->TestEqual(TEXT("One restored opportunity"),Offers.Num(),1))return true;
   Test->TestEqual(TEXT("Saved choice retained after process restart"),Offers[0].State,Ignored?ENeonCityOpportunityState::Ignored:ENeonCityOpportunityState::Declined);
   Test->TestEqual(TEXT("Restart does not charge/reward choice"),GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance(),int64(1850));
   Test->TestFalse(TEXT("Unaccepted opportunity does not create route"),PC->GetNavigationComponent()->GetDisplay().Active);
   Test->TestEqual(TEXT("Only ignored invitation can be revisited"),Router->CanChoose(Offers[0].OpportunityID,ENeonCityOpportunityChoice::Accept),Ignored);
   Save->DeleteSave();
  }
  Manifest(Test,TEXT("B"),IgnoredID,DeclinedID,GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance());return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOpportunityChoices,"NeonCity.Opportunities.ChoicesPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityOpportunityChoices::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityOpportunityTests::FChoices>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FNeonCityOpportunityRestart,"NeonCity.Opportunities.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FNeonCityOpportunityRestart::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityOpportunitySession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode);Commands.Add(Mode); }
}
bool FNeonCityOpportunityRestart::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 if(Parameters==TEXT("A"))FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityOpportunityTests::FChoices>(this,true));
 else FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityOpportunityTests::FRestartChoices>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
