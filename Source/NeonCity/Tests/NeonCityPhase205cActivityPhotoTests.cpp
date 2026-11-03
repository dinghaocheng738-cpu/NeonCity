#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandActivityJob.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace ActivityPhotoTest
{
class FPhoto:public IAutomationLatentCommand
{
public:
 explicit FPhoto(FAutomationTestBase* T,bool Cold=false,bool Quality=false):Test(T),bCold(Cold),bQuality(Quality){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<4)return false;if(FPlatformTime::Seconds()-Started>130)return End(TEXT("Activity photo timed out"));const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57200,-164000,300))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Photo.Village"))Job=*It;
   if(!Job)return End(TEXT("Authored village photo provider missing"));
   for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job && It->Index==1)Clerk=*It;
   Phone=PC->FindComponentByClass<UPhoneComponent>();Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Phone || !Clerk || !Save->ConfigureSlot(TEXT("NeonCity_Test_IslandPhoto02"),false))return End(TEXT("Phone, clerk and the isolated photo slot required"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Prior genuine submitted-photo checkpoint required"));Key(EKeys::F7,IE_Pressed);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Retain existing actual photo checkpoint; run only its cold replay test"));
   const FVector From=Job->GetActorTransform().TransformPosition(Job->PickupLocal)+FVector(300,100,0);FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(PhotoFixtureFloor),false,Player);
   if(!W->LineTraceSingleByChannel(Ground,From+FVector(0,0,700),From-FVector(0,0,1000),ECC_Pawn,Q) || Ground.ImpactNormal.Z<.7)return End(TEXT("Actual photography start has no clear ground support"));
   Player->SetActorLocation(Ground.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();Key(EKeys::P,IE_Pressed);return Next(1);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,IE_Released);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));Key(EKeys::O,IE_Pressed);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,IE_Released);return Next(3);}
  if(Stage==3 && Age>.2)
  {
   if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Activity-Photo-Offer.png"));Key(EKeys::Enter,IE_Pressed);return Next(5);}
   if(++Browsed>30)return End(TEXT("Real opportunities UI cannot discover the photo activity"));Key(EKeys::Right,IE_Pressed);return Next(4);
  }
  if(Stage==4 && Age>.15){Key(EKeys::Right,IE_Released);return Next(3);}
  if(Stage==5 && Age>.2)
  {Key(EKeys::Enter,IE_Released);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || (bCold && (D.ActivityRunID==RunID || D.ActivityCompletions!=1)))return End(TEXT("Actual ENTER did not accept a genuine independently receipted activity run"));if(bCold && !bQuality){Key(EKeys::Delete,IE_Pressed);return Next(17);}RunID=D.ActivityRunID;return Next(6);}
  if(Stage==6)
  {
   const FVector Target=Job->GetActorTransform().TransformPosition(Job->PhotoTargetLocal);
   PC->SetControlRotation((Target-PC->PlayerCameraManager->GetCameraLocation()).Rotation());
   if(Age<.8)return false;Key(EKeys::Enter,IE_Pressed);return Next(7);
  }
  if(Stage==7 && Age>.9)
  {
   Key(EKeys::Enter,IE_Released);const auto D=Job->BuildOpportunitySnapshot(false);PhotoFile=D.ActivityPhotoFile;
   if(D.ActivityStage==0 && Age<3)return false;
   TArray<uint8> Bytes;if(D.ActivityStage!=1 || !FFileHelper::LoadFileToArray(Bytes,*PhotoFile) || Bytes.Num()<20000 || Bytes.Num()<24 || Bytes[0]!=137 || Bytes[1]!=80 || Bytes[2]!=78 || Bytes[3]!=71)return End(TEXT("Composed native photo action did not generate a genuine reviewable PNG"));
   auto U32=[&](int I){return uint32(Bytes[I])<<24|uint32(Bytes[I+1])<<16|uint32(Bytes[I+2])<<8|uint32(Bytes[I+3]);};
   if(U32(16)!=1600 || U32(20)!=900 || Wallet->GetBalance()!=Before || PC->HiddenActors.Contains(Player))return End(TEXT("Photo dimensions, restored player view or capture-only wallet state incorrect"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Activity-Photo-ActualPreview.png"));Key(EKeys::P,IE_Pressed);return Next(bQuality?25:8);
  }
  if(Stage==8 && Age>.2)
  {
   Key(EKeys::P,IE_Released);const FVector Pickup=Job->GetActorTransform().TransformPosition(Job->PickupLocal),Return=Clerk->GetActorLocation();
   // The east shortcut crosses the neighbouring cottage and its porch. Use
   // the measured public strip between the fishing house and that cottage.
   Path={NeonCityCoastalLayout::FromLegacyIsland(FVector(57300,-163500,Pickup.Z)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57150,-163400,Pickup.Z)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57150,-162500,Pickup.Z)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57500,-162000,Return.Z)),FVector(57500,Return.Y-200,Return.Z),Return-FVector(0,130,0)};return Next(9);
  }
  if(Stage==9)
  {
   const FVector D=Path[Waypoint]-Player->GetActorLocation();
   if(D.Size2D()<40){Key(EKeys::W,IE_Released);Held=false;if(++Waypoint==Path.Num())return Next(10);At=W->GetTimeSeconds();}
   else{PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));if(!Held){Key(EKeys::W,IE_Pressed);Held=true;}if(Age>18)return End(FString::Printf(TEXT("Actual village-to-clerk walk blocked waypoint%d at%s"),Waypoint,*Player->GetActorLocation().ToString()));}
  }
  if(Stage==10 && Age>.6)
  {
   PC->SetControlRotation((Clerk->Interaction->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Clerk)return End(TEXT("Actual submit-clerk F prompt inaccessible"));Key(EKeys::F,IE_Pressed);return Next(11);
  }
  if(Stage==11 && Age>.2)
  {
   Key(EKeys::F,IE_Released);const auto D=Job->BuildOpportunitySnapshot(false);
   if(D.State!=2 || D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Actual photo submission failed shared once-only settlement"));Key(EKeys::F,IE_Pressed);return Next(12);
  }
  if(Stage==12 && Age>.2)
  {Key(EKeys::F,IE_Released);if(Wallet->GetBalance()!=Before+Job->Reward)return End(TEXT("Repeat F duplicated payment"));Key(EKeys::F6,IE_Pressed);return Next(13);}
  if(Stage==13 && Age>.6)
  {
   Key(EKeys::F6,IE_Released);auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.ActivityRunID==RunID && D.State==2 && D.ActivityCompletions==1 && D.ActivityPhotoFile==PhotoFile;}))return End(TEXT("Shared normal save lost genuine photograph, run or receipt"));Key(EKeys::P,IE_Pressed);return Next(14);
  }
  if(Stage==14 && Age>.2){Key(EKeys::P,IE_Released);Key(EKeys::O,IE_Pressed);return Next(15);}
  if(Stage==15 && Age>.2){Key(EKeys::O,IE_Released);Browsed=0;return Next(23);}
  if(Stage==23 && Age>.2){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,IE_Pressed);return Next(16);}if(++Browsed>30)return End(TEXT("Actual replay list cannot discover the completed photo activity"));Key(EKeys::Right,IE_Pressed);return Next(24);}
  if(Stage==24 && Age>.15){Key(EKeys::Right,IE_Released);return Next(23);}
  if(Stage==16 && Age>.2)
  {Key(EKeys::Enter,IE_Released);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || D.ActivityRunID==RunID || D.ActivityCompletions!=1)return End(TEXT("Replay did not start a new independently receipted run"));Key(EKeys::Delete,IE_Pressed);return Next(17);}
  if(Stage==17 && Age>.2){Key(EKeys::Delete,IE_Released);if(Job->BuildOpportunitySnapshot(false).State==1 || Wallet->GetBalance()!=(bCold?Before:Before+Job->Reward))return End(TEXT("Normal activity cancel did not preserve settlement"));return End(TEXT(""));}
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,IE_Released);const auto D=Job->BuildOpportunitySnapshot(false);PhotoFile=D.ActivityPhotoFile;RunID=D.ActivityRunID;TArray<uint8> Bytes;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || D.State!=2 || D.ActivityCompletions!=1 || !RunID.IsValid() || !FFileHelper::LoadFileToArray(Bytes,*PhotoFile) || Bytes.Num()<20000 || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent shared load lost the actual image, submitted state or once-only receipt"));
   Before=Wallet->GetBalance();if(Before!=1980)return End(TEXT("Actual $130 shared photo income did not survive cold load"));
   if(bQuality)
   {
    // Only the failed image export is retested. One explicit photography
    // position; actual prior delivery/reward/save results remain cached.
    const FVector From=Job->GetActorTransform().TransformPosition(Job->PickupLocal)+FVector(300,100,0);FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(PhotoCaptureQualityFloor),false,Player);
    if(!W->LineTraceSingleByChannel(H,From+FVector(0,0,700),From-FVector(0,0,1000),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Actual image-quality fixture has no real floor"));
    Player->SetActorLocation(H.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   }
   Key(EKeys::P,IE_Pressed);return Next(1);
  }
  if(Stage==25 && Age>.3){Key(EKeys::P,IE_Released);if(!Save->ConfigureSlot(TEXT("NeonCity_Test_PhotoRender06"),false) || Save->DoesSaveExist())return End(TEXT("Retain old submitted photo; fresh rendered-image snapshot required"));Key(EKeys::F6,IE_Pressed);return Next(26);}
  if(Stage==26 && Age>.6)
  {
   Key(EKeys::F6,IE_Released);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.ActivityRunID==RunID && D.ActivityStage==1 && D.ActivityPhotoFile==PhotoFile;}) || Wallet->GetBalance()!=Before)return End(TEXT("Real native camera photo did not survive normal shared snapshot"));return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);for(auto K:{EKeys::P,EKeys::O,EKeys::Enter,EKeys::Right,EKeys::W,EKeys::F,EKeys::F6,EKeys::Delete})Key(K,IE_Released);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("photo_file"),PhotoFile);J->SetStringField(TEXT("run_id"),RunID.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetStringField(TEXT("scope"),bCold?TEXT("Independent process genuine F7 shared save restores submitted actual photo, run ID, receipt and $130 income. Native P/O/RIGHT/ENTER finds replay despite normal list reordering; new run ID and DELETE cancel preserve prior income. No pose, job-state or reward injection; original image/save retained."):TEXT("One clear photography start fixture; genuine P/O/RIGHT/ENTER capture and rendered phone preview; normal W walk to physical clerk, F submit, exactly one shared payment, F6 shared save, replay and DELETE cancel. No grant, completion or reward injection. Cross-process photo restore separate."));
  J->SetBoolField(TEXT("capture_quality_only"),bQuality);if(bQuality)J->SetStringField(TEXT("scope"),TEXT("Fix-specific actual camera export and phone preview: genuine prior submitted checkpoint loads first, one declared photography fixture, native P/O/ENTER replay and composed shot, no repeated route or reward. F6 stores the new PNG with its actual active run in a separate fresh slot; original receipt and image retained. Native photo contrast/scene match require visual evidence inspection."));FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("ActivityPhoto.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Clerk=nullptr;UPhoneComponent* Phone=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;UNeonCitySaveSubsystem* Save=nullptr;double Started=0;float At=0;int32 Stage=0,Browsed=0,Waypoint=0;int64 Before=0;bool Held=false,bCold=false,bQuality=false;FGuid RunID;FString PhotoFile;TArray<FVector> Path;FNeonCityVisualEvidence Evidence{TEXT("ActivityPhoto")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandActivityPhotoTest,"NeonCity.Phase205c.Activity.PhotoNativePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandActivityPhotoTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(ActivityPhotoTest::FPhoto(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandActivityPhotoColdTest,"NeonCity.Phase205c.Activity.PhotoColdReplayPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandActivityPhotoColdTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(ActivityPhotoTest::FPhoto(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandActivityPhotoQualityTest,"NeonCity.Phase205c.Activity.PhotoCaptureQualityPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandActivityPhotoQualityTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(ActivityPhotoTest::FPhoto(this,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
