#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandActivityJob.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NativeFishOrder
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>95)return End(TEXT("Native fish order timed out"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(58200,-159250,250))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Fish.Provisions"))Job=*It;
   for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job && It->Index==0)Clerk=*It;
   Phone=PC->FindComponentByClass<UPhoneComponent>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Fish=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
   if(!Job || !Clerk || !Phone || !Save->ConfigureSlot(bCold?TEXT("NeonCity_Test_FishOrder01"):TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->DoesSaveExist())return End(TEXT("Original genuine catch or completed order checkpoint required"));Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>1)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());Before=Wallet->GetBalance();Count=Fish->GetFish().Num();
   if(bCold){const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCompletions!=1 || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent load lost completed fish-order run or unique shared reward"));OldRun=D.ActivityRunID;}
   else
   {
    for(const auto& R:Fish->GetFish())if(R.Storage==ENeonCityFishStorage::Pouch && R.SpeciesID==Job->RequiredSpecies && R.WeightKg>=Job->RequiredWeightKg.X && R.WeightKg<=Job->RequiredWeightKg.Y && R.CookFraction==0 && !R.FishingPointID.IsNone() && R.CaughtAt.GetTicks()>0){FishID=R.FishID;Weight=R.WeightKg;Size=R.SizeCm;break;}
    if(!FishID.IsValid())return End(TEXT("No genuine kept catch matches authored sardine order; no invented fish was granted"));
    if(!Save->ConfigureSlot(TEXT("NeonCity_Test_FishOrder01"),false) || Save->DoesSaveExist())return End(TEXT("Preserve prior order evidence; fresh source checkpoint required"));
    const FVector AtClerk=Clerk->GetActorLocation()+FVector(110,100,0);FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(FishOrderFixtureFloor),false,P);
    if(!W->LineTraceSingleByChannel(H,AtClerk+FVector(0,0,500),AtClerk-FVector(0,0,700),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Real receiving clerk has no supported access"));
    P->SetActorLocation(H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   }
   return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stow.IsValid())return End(TEXT("Normal weapon stow failed"));const int I=Weapon->GetData()->SlotType;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(I<1 || I>8)return End(TEXT("Weapon slot unavailable"));Stow=Slots[I-1];Key(Stow,true);return Next(30);}
   Key(EKeys::P,true);return Next(3);
  }
  if(Stage==30 && Age>.2){Key(Stow,false);return Next(2);}
  if(Stage==3 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("P did not open actual phone"));Key(EKeys::O,true);return Next(4);}
  if(Stage==4 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(5);}
  if(Stage==5 && Age>.15)
  {if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(7);}if(++Browsed>30)return End(TEXT("Native phone cannot discover fish order"));Key(EKeys::Right,true);return Next(6);}
  if(Stage==6 && Age>.15){Key(EKeys::Right,false);return Next(5);}
  if(Stage==7 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==OldRun)return End(TEXT("Native ENTER did not accept independently receipted order"));Run=D.ActivityRunID;
   if(bCold){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Replay duplicated the completed order payment"));Key(EKeys::Delete,true);return Next(15);}
   Key(EKeys::P,true);return Next(8);
  }
  if(Stage==8 && Age>.2){Key(EKeys::P,false);PC->SetControlRotation((Clerk->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Clerk)return End(TEXT("Normal receiving-clerk F prompt inaccessible"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("FishOrder-Actual-Catch-Prompt.png"));Key(EKeys::F,true);return Next(9);}
  if(Stage==9 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCompletions!=1 || Fish->FindFish(FishID) || Fish->GetFish().Num()!=Count-1 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Genuine order delivery did not consume one source fish and pay exactly once"));Key(EKeys::F,true);return Next(10);
  }
  if(Stage==10 && Age>.2){Key(EKeys::F,false);if(Fish->GetFish().Num()!=Count-1 || Wallet->GetBalance()!=Before+Job->Reward)return End(TEXT("Repeated F duplicated fish or income"));Key(EKeys::F6,true);return Next(11);}
  if(Stage==11 && Age>.7)
  {
   Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || Model->Fishing.Fish.ContainsByPredicate([&](const auto& R){return R.FishID==FishID;}) || !Model->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.ActivityRunID==Run && D.State==2 && D.ActivityCompletions==1;}))return End(TEXT("F6 lost completed real-fish transaction"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("FishOrder-Native-Settled-Saved.png"));return End(TEXT(""));
  }
  if(Stage==15 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Wallet->GetBalance()!=Before || Fish->GetFish().Num()!=Count)return End(TEXT("Cold replay cancellation changed fish ownership or payment"));return End(TEXT(""));}return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  for(auto K:{EKeys::F7,EKeys::F6,EKeys::P,EKeys::O,EKeys::Right,EKeys::Enter,EKeys::F,EKeys::Delete,Stow})if(K.IsValid())Key(K,false);if(!Failure.IsEmpty())Test->AddError(Failure);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("fish_id"),FishID.ToString());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("weight_kg"),Weight);J->SetNumberField(TEXT("size_cm"),Size);J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent genuine F7 restore of completed real-catch delivery and shared receipt. Native P/O/RIGHT/ENTER replay makes a fresh run, DELETE cancels without payment/inventory changes. Zero pose, catch or job-state fixtures."):TEXT("Retained native-boat catch loaded through F7; one declared receiving-clerk player setup, then native weapon stow, P/O/RIGHT/ENTER acceptance, actual F catch delivery, repeat-F exclusion and F6 save. No fish/reward/state injection. Continuous transport and subjective review are separate."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bCold?TEXT("FishOrderCold.json"):TEXT("FishOrderNative.json"))));return true;
 }
 FAutomationTestBase* Test;bool bCold;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Clerk=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Fish=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;int Stage=0,Browsed=0,Count=0;float At=0,Weight=0,Size=0;double Start=0;int64 Before=0;FGuid FishID,Run,OldRun;FKey Stow;FNeonCityVisualEvidence Evidence{TEXT("FishOrderNative")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishOrderNativeTest,"NeonCity.Phase205c.Activity.FishOrder.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFishOrderNativeTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeFishOrder::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishOrderColdTest,"NeonCity.Phase205c.Activity.FishOrder.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFishOrderColdTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeFishOrder::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
