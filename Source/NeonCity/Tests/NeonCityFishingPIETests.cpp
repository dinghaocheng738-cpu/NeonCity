#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/FishingStation.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Property/NeonCityStarterApartment.h"
#include "Interaction/InteractionComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonSerializer.h"
namespace NeonCityFishingTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
FString Signature(const FNeonCityFishingSaveData& Snapshot)
{
 FString Text;
 for(const auto& Fish:Snapshot.Fish)Text+=FString::Printf(TEXT("%s|%s|%.8f|%.8f|%lld|%s|%d|%s;"),*Fish.FishID.ToString(),*Fish.SpeciesID.ToString(),Fish.WeightKg,Fish.SizeCm,Fish.CaughtAt.GetTicks(),*Fish.FishingPointID.ToString(),int32(Fish.Storage),*Fish.ContainerID.ToString());
 for(const auto& Meal:Snapshot.Meals)Text+=FString::Printf(TEXT("%s|%s|%s;"),*Meal.MealID.ToString(),*Meal.RecipeID.ToString(),*Meal.SpeciesID.ToString());return Text;
}
bool Manifest(FAutomationTestBase* Test,const TCHAR* Mode,const UNeonCitySaveGame* Model)
{
 auto Json=MakeShared<FJsonObject>();Json->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());Json->SetStringField(TEXT("saveId"),Model->SaveID.ToString());
 Json->SetStringField(TEXT("fishSignature"),Signature(Model->Fishing));Json->SetNumberField(TEXT("cash"),Model->Economy.Balance);
 FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));const FString Directory=FPaths::ProjectSavedDir()/TEXT("Fishing");IFileManager::Get().MakeDirectory(*Directory,true);
 return Test->TestTrue(TEXT("Fish persistence evidence written"),FFileHelper::SaveStringToFile(Text,*(Directory/(FString(TEXT("Session"))+Mode+TEXT(".json")))));
}
class FCoastalLoop : public IAutomationLatentCommand
{
public:
 explicit FCoastalLoop(FAutomationTestBase* T,bool Cross=false):Test(T),CrossSession(Cross) {}
 bool Next(int32 Value) { Stage=Value;At=W->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("Coastal route stage %d at %s"),Stage,Player?*Player->GetActorLocation().ToString():TEXT("none"));return false; }
 void Shot(const TCHAR* Name) { Test->TestTrue(TEXT("Actual coastal activity screen captured"),FNeonCityVisualEvidence::CaptureHUD(W,Name)); }
 bool Walk(FVector Goal)
 {
  const float Distance=FVector::Dist2D(Goal,Player->GetActorLocation());if(Distance<25)return true;
  const FVector Direction=(Goal-Player->GetActorLocation()).GetSafeNormal2D();PC->SetControlRotation(FRotator(-8,Direction.Rotation().Yaw,0));
  Player->AddMovementInput(Direction,FMath::Clamp(Distance/180,.15f,1.f));return false;
 }
 bool Focus(AActor* Target,FVector Aim)
 { PC->SetControlRotation((Aim-Player->GetPawnViewLocation()).Rotation());return Player->GetInteractionComponent()->RefreshFocusedInteractable()==Target; }
 void Reel(bool Pressed)
 { if(Held!=Pressed) { Held=Pressed;Key(PC,EKeys::LeftMouseButton,Held?IE_Pressed:IE_Released); } }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>300) { if(W)Shot(TEXT("Fishing-Timeout.png"));Test->AddError(FString::Printf(TEXT("Coastal loop timed out stage %d at %s"),Stage,Player?*Player->GetActorLocation().ToString():TEXT("no player")));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;if(!PC || W->GetTimeSeconds()<3)return false;
  Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
  auto* GI=W->GetGameInstance();auto* Inventory=GI->GetSubsystem<UNeonCityFishingSubsystem>();auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();
  auto* Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();auto* Fishing=PC->GetFishingComponent();auto* Phone=PC->GetPhoneComponent();const float Elapsed=W->GetTimeSeconds()-At;
  if(Elapsed>65 && Stage!=0)
  { Shot(TEXT("Fishing-Stalled.png"));Test->AddError(FString::Printf(TEXT("Coastal stage stalled %d waypoint %d location %s grounded %d health %.1f"),Stage,Index,*Player->GetActorLocation().ToString(),Player->GetCharacterMovement()->IsMovingOnGround(),Player->GetHealthComponent()->GetCurrentHealth()));return true; }
  if(Stage==0)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityStarterApartment> It(W);It;++It)Home=*It;
   Point=Inventory->FindNearestStation(Player->GetActorLocation(),ENeonCityFishingStation::CastingPoint);
   Market=Inventory->FindNearestStation(Player->GetActorLocation(),ENeonCityFishingStation::FishMarket);Grill=Inventory->FindNearestStation(Player->GetActorLocation(),ENeonCityFishingStation::Grill);
   if(!Test->TestTrue(TEXT("Normal map contains home and authored coastal activities"),Home && Point && Market && Grill && Inventory->GetSpecies().Num()==3))return true;
   FVector Surface;Test->TestTrue(TEXT("Authored cast destination is open water"),Point->ResolveWaterTarget(Surface));
   const FVector OriginalTarget=Point->CastTargetLocal;
   Point->CastTargetLocal=Point->GetActorTransform().InverseTransformPosition(FVector(5960,-2800,-130));
   Test->TestFalse(TEXT("Water volume below solid beach is not a fishing surface"),Point->ResolveWaterTarget(Surface));Point->CastTargetLocal=OriginalTarget;
   Save->ConfigureSlot(CrossSession?TEXT("NeonCity_Test_Fishing_Cross"):TEXT("NeonCity_Test_Fishing_Local"));Save->DeleteSave();
   Test->TestTrue(TEXT("Ordinary game begins in home"),Home->IsInsideInterior_Implementation(Player));Test->TestEqual(TEXT("No debug fish granted"),Inventory->PouchCount(),0);
   Key(PC,EKeys::P,IE_Pressed);return Next(1);
  }
  if(Stage==1 && Elapsed>.3) { Key(PC,EKeys::P,IE_Released);Key(PC,EKeys::N,IE_Pressed);return Next(2); }
  if(Stage==2 && Elapsed>.3)
  { Key(PC,EKeys::N,IE_Released);Test->TestEqual(TEXT("Coastal app opens by actual N"),Phone->GetActiveApp(),ENeonCityPhoneApp::Fishing);Key(PC,EKeys::BackSpace,IE_Pressed);return Next(3); }
  if(Stage==3 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Released);return Next(4); }
  if(Stage==4 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Pressed);return Next(5); }
  if(Stage==5 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Released);Key(PC,EKeys::Enter,IE_Pressed);return Next(6); }
  if(Stage==6 && Elapsed>.3)
  { Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Phone guides to stable physical fishing point"),PC->GetNavigationComponent()->GetDestination().DestinationID,Point->StationID);Shot(TEXT("Fishing-Places.png"));Key(PC,EKeys::P,IE_Pressed);return Next(7); }
  if(Stage==7 && Elapsed>.3) { Key(PC,EKeys::P,IE_Released);return Next(8); }
  if(Stage==8)
  {
   if(!Walk(Home->GetActorTransform().TransformPosition(FVector(0,230,99))) || !Focus(Home,Home->GetActorLocation()+FVector(0,0,130)))return false;
   Key(PC,EKeys::F,IE_Pressed);return Next(9);
  }
  if(Stage==9 && Elapsed>.4)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestFalse(TEXT("Normal door exits apartment"),Home->IsInsideInterior_Implementation(Player)))return true;
   if(const auto* Weapon=Player->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())Key(PC,EKeys::One,IE_Pressed);
   Route={FVector(6300,1450,139),FVector(6300,-700,139),FVector(5200,-950,139),FVector(5200,-2100,59),FVector(5100,-3650,59),FVector(5220,-3650,59)};Index=0;return Next(10);
  }
  if(Stage==10)
  {
   Key(PC,EKeys::One,IE_Released);
   if(Index<Route.Num())
   {
    if(Index==5 && !FacingChecked)
    {
     FacingChecked=true;Test->TestFalse(TEXT("Sideways approach is not a forward cast"),Point->IsFacingWater(Player));
     Test->TestFalse(TEXT("Fishing cannot start facing away from water"),Fishing->Start(Point));Test->TestFalse(TEXT("Rejected start does not lock movement"),PC->IsMoveInputIgnored());
    }
    if(Walk(Route[Index]))++Index;return false;
   }
   if(Player->GetVelocity().Size2D()>30)return false;
   if(!Focus(Point,Point->GetActorLocation()+FVector(0,0,95)))return false;
   DefaultArmLength=Player->GetCameraBoom()->TargetArmLength;DefaultFOV=Player->GetFollowCamera()->FieldOfView;DefaultOffset=Player->GetCameraBoom()->SocketOffset;
   Shot(TEXT("Fishing-Approach.png"));Key(PC,EKeys::F,IE_Pressed);return Next(11);
  }
  if(Stage==11 && Elapsed>.4)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F starts actual coastal session"),Fishing->IsActive()))return true;
   Test->TestEqual(TEXT("Entering activity never awards a fish"),Inventory->PouchCount(),0);
   auto* Anim=Cast<UCharacterPresentationAnimInstance>(Player->GetMesh()->GetAnimInstance());
   Test->TestTrue(TEXT("Fishing hand contact opts into existing template wrapper"),Anim && Anim->FishingAlpha>.8f);
   Test->TestNotNull(TEXT("Reviewed rod is visible"),Fishing->GetRodVisual());Reel(true);return Next(12);
  }
  if(Stage==12)
  {
   const auto& Session=Fishing->GetSession();
   if(!Fishing->IsActive()) { Test->AddError(TEXT("Activity interrupted on the normal pier"));return true; }
   if(Session.State==ENeonCityFishingState::Windup && Session.CastPower>.88f)Reel(false);
   if(Session.State==ENeonCityFishingState::Waiting && !WaitShot) { WaitShot=true;Shot(TEXT("Fishing-Waiting.png")); }
   if(Session.State==ENeonCityFishingState::Bite)Reel(true);
   if(Session.State==ENeonCityFishingState::Fighting)
   {
    Reel(!(Session.Tension>.68f || (Session.Struggling && Session.Tension>.35f)));
    if(Session.Progress>.4f && !FightShot) { FightShot=true;Shot(TEXT("Fishing-Reeling.png")); }
    if(Session.Reeling && Fishing->IsReelAudioPlaying())HeardReel=true;
   }
   if(Session.State==ENeonCityFishingState::Escaped) { Test->AddError(TEXT("Player-readable tension strategy failed the live catch"));return true; }
   if(Session.State==ENeonCityFishingState::Caught)
   {
    Reel(false);Test->TestEqual(TEXT("A landed catch requires explicit Keep"),Inventory->PouchCount(),Kept);return Next(31);
   }
   return false;
  }
  if(Stage==31 && Elapsed>.4f)
  { Shot(*FString::Printf(TEXT("Fishing-Caught-%d.png"),Kept+1));Key(PC,Kept<2?EKeys::One:EKeys::Two,IE_Pressed);return Next(13); }
  if(Stage==13 && Elapsed>.3)
  {
   Key(PC,Kept<2?EKeys::One:EKeys::Two,IE_Released);
   if(Kept<2) { ++Kept;Test->TestEqual(TEXT("Keep adds exactly one caught item"),Inventory->PouchCount(),Kept);Reel(true);return Next(12); }
   Test->TestEqual(TEXT("Release does not add fish or money"),Inventory->PouchCount(),2);Test->TestEqual(TEXT("Catching alone grants no cash"),Economy->GetBalance(),int64(1850));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))Test->TestTrue(TEXT("Reel sound played during actual winding"),HeardReel);
   if(!Test->TestTrue(TEXT("Kept catches save through the unified subsystem"),Save->SaveGame()))return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());if(!Test->TestTrue(TEXT("Disk contains fish values"),Model.IsValid() && Model->HasFishing && Model->Fishing.Fish.Num()==2))return true;
   SavedSignature=Signature(Model->Fishing);Test->TestTrue(TEXT("Load stops an active session safely"),Save->LoadGame());
   Test->TestFalse(TEXT("Transient reel state is not restored"),Fishing->IsActive());Test->TestFalse(TEXT("Load clears only the activity input lock"),PC->IsMoveInputIgnored());
   Test->TestTrue(TEXT("Fishing camera restores template context"),Player->GetCameraExtensionComponent()->GetCameraContext()==ENeonCityCameraContext::Default);
   Test->TestEqual(TEXT("Fishing camera restores arm length"),Player->GetCameraBoom()->TargetArmLength,DefaultArmLength);
   Test->TestEqual(TEXT("Fishing camera restores FOV"),Player->GetFollowCamera()->FieldOfView,DefaultFOV);
   Test->TestEqual(TEXT("Fishing camera restores socket offset"),Player->GetCameraBoom()->SocketOffset,DefaultOffset);
   Test->TestEqual(TEXT("Load preserves exact catch IDs and attributes"),Signature(Inventory->BuildPersistenceSnapshot()),SavedSignature);
   if(CrossSession) { Manifest(Test,TEXT("A"),Model.Get());return true; }
   Test->TestFalse(TEXT("Load stops reel audio"),Fishing->IsReelAudioPlaying());
   auto Corrupt=Model->Fishing;const auto Duplicate=Corrupt.Fish[0];Corrupt.Fish.Add(Duplicate);auto Unknown=Corrupt.Fish[0];Unknown.FishID=FGuid::NewGuid();Unknown.SpeciesID=TEXT("Missing.Species");Corrupt.Fish.Add(Unknown);
   auto Unsupported=Duplicate;Unsupported.FishID=FGuid::NewGuid();Unsupported.Storage=ENeonCityFishStorage::Pen;Unsupported.ContainerID=TEXT("Unknown.Pen");Corrupt.Fish.Add(Unsupported);
   TArray<FString> Warnings;Inventory->NormalizePersistenceSnapshot(Corrupt,Warnings);Test->TestEqual(TEXT("Duplicate, missing species and unsupported container removed safely"),Corrupt.Fish.Num(),2);Test->TestEqual(TEXT("Bad fish records report warnings"),Warnings.Num(),3);
   Route={FVector(5200,-2100,59),FVector(5200,-1150,139),FVector(5330,-1080,139),FVector(5330,-950,139),FVector(5500,-850,139)};Index=0;return Next(14);
  }
  if(Stage==14)
  {
   if(Index<Route.Num()) { if(Walk(Route[Index]))++Index;return false; }
   if(Player->GetVelocity().Size2D()>30 || !Focus(Market,Market->GetActorLocation()+FVector(0,0,95)))return false;
   SaleValue=Inventory->GetSaleValue(Inventory->GetFish()[0]);Key(PC,EKeys::F,IE_Pressed);return Next(15);
  }
  if(Stage==15 && Elapsed>.3) { Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("Enter authored fish market"),Fishing->GetActiveStation()==Market))return true;Key(PC,EKeys::Enter,IE_Pressed);return Next(16); }
  if(Stage==16 && Elapsed>.3)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Market removes selected fish"),Inventory->PouchCount(),1);Test->TestEqual(TEXT("Market credits data-driven cash once"),Economy->GetBalance(),int64(1850)+SaleValue);
   Shot(TEXT("Fishing-Market.png"));Key(PC,EKeys::F,IE_Pressed);return Next(17);
  }
  if(Stage==17 && Elapsed>.3) { Key(PC,EKeys::F,IE_Released);Route={FVector(5500,-650,139),FVector(4700,-650,139),FVector(4700,-900,139)};Index=0;return Next(18); }
  if(Stage==18)
  {
   if(Index<Route.Num()) { if(Walk(Route[Index]))++Index;return false; }
   if(Player->GetVelocity().Size2D()>30 || !Focus(Grill,Grill->GetActorLocation()+FVector(0,0,95)))return false;
   Key(PC,EKeys::F,IE_Pressed);return Next(19);
  }
  if(Stage==19 && Elapsed>.3) { Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("Enter public grill"),Fishing->GetActiveStation()==Grill))return true;Key(PC,EKeys::Enter,IE_Pressed);return Next(20); }
  if(Stage==20 && Elapsed>.3) { Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Cooking requires elapsed time"),Inventory->GetMeals().Num(),0);Shot(TEXT("Fishing-Grilling.png"));return Next(21); }
  if(Stage==21 && Elapsed>3.2f)
  {
   Test->TestEqual(TEXT("Grill consumes one catch"),Inventory->PouchCount(),0);Test->TestEqual(TEXT("Grill creates one stable meal"),Inventory->GetMeals().Num(),1);
   Key(PC,EKeys::P,IE_Pressed);return Next(22);
  }
  if(Stage==22 && Elapsed>.3)
  {
   Key(PC,EKeys::P,IE_Released);Test->TestFalse(TEXT("Phone leaves activity and restores walking"),Fishing->IsActive() || PC->IsMoveInputIgnored());
   GI->GetSubsystem<UPhoneSubsystem>()->OpenApp(ENeonCityPhoneApp::Messages,PC);Key(PC,EKeys::N,IE_Pressed);return Next(23);
  }
  if(Stage==23 && Elapsed>.3) { Key(PC,EKeys::N,IE_Released);Key(PC,EKeys::BackSpace,IE_Pressed);return Next(24); }
  // App was left on Places; one alternate returns Pouch, another opens Food.
  if(Stage==24 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Released);return Next(25); }
  if(Stage==25 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Pressed);return Next(26); }
  if(Stage==26 && Elapsed>.2) { Key(PC,EKeys::BackSpace,IE_Released);Key(PC,EKeys::Enter,IE_Pressed);return Next(27); }
  if(Stage==27 && Elapsed>.3)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Full player does not waste food"),Inventory->GetMeals().Num(),1);
   // Explicit condition fixture for consumption; travel/catching/selling are ordinary inputs.
   Player->GetPlayerDataComponent()->SetStamina(40);return Next(30);
  }
  if(Stage==30 && Elapsed>.2) { Key(PC,EKeys::Enter,IE_Pressed);return Next(28); }
  if(Stage==28 && Elapsed>.4)
  {
   Key(PC,EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Food restores bounded stamina"),Player->GetPlayerDataComponent()->GetStamina(),60.f);Test->TestEqual(TEXT("Food consumed once"),Inventory->GetMeals().Num(),0);
   Shot(TEXT("Fishing-Food.png"));Test->TestTrue(TEXT("Sale and consumption save"),Save->SaveGame());Test->TestTrue(TEXT("Final state reloads"),Save->LoadGame());Test->TestEqual(TEXT("Sold/eaten fish do not return"),Inventory->PouchCount()+Inventory->GetMeals().Num(),0);
   Test->TestEqual(TEXT("Sale cash persists without repayment"),Economy->GetBalance(),int64(1850)+SaleValue);Save->DeleteSave();Key(PC,EKeys::P,IE_Pressed);return Next(29);
  }
  if(Stage==29 && Elapsed>.3) { Key(PC,EKeys::P,IE_Released);Test->TestFalse(TEXT("Coastal phone closes normally"),Phone->IsPhoneOpen());return true; }
  return false;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityStarterApartment* Home=nullptr;
 AFishingStation* Point=nullptr;AFishingStation* Market=nullptr;AFishingStation* Grill=nullptr;
 int32 Stage=0,Index=0,Kept=0;float At=0;double Started=0;bool Held=false,WaitShot=false,FightShot=false,CrossSession=false,HeardReel=false,FacingChecked=false;
 TArray<FVector> Route;FString SavedSignature;int64 SaleValue=0;
 float DefaultArmLength=0,DefaultFOV=0;FVector DefaultOffset=FVector::ZeroVector;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCoastalFishing,"NeonCity.Fishing.CoastalLoopPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCoastalFishing::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFishingTests::FCoastalLoop>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
namespace NeonCityFishingTests
{
class FRestart : public IAutomationLatentCommand
{
public:
 explicit FRestart(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>45) { Test->AddError(TEXT("Fishing restart timed out"));return true; }
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<3 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
  FString Text;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("Fishing/SessionA.json")));
  TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json);double PID=0;
  if(!Test->TestTrue(TEXT("Fishing restart uses another OS process"),Json.IsValid() && Json->TryGetNumberField(TEXT("processId"),PID) && PID>0 && PID!=FPlatformProcess::GetCurrentProcessId()))return true;
  auto* GI=W->GetGameInstance();auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();auto* Inventory=GI->GetSubsystem<UNeonCityFishingSubsystem>();
  Save->ConfigureSlot(TEXT("NeonCity_Test_Fishing_Cross"));TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());FString Expected;Json->TryGetStringField(TEXT("saveId"),Expected);
  if(!Test->TestTrue(TEXT("Saved generation matches fishing session A"),Model.IsValid() && Model->SaveID.ToString()==Expected))return true;
  if(!Test->TestTrue(TEXT("Unified disk load restores caught fish after restart"),Save->LoadGame()))return true;
  Json->TryGetStringField(TEXT("fishSignature"),Expected);Test->TestEqual(TEXT("All stable caught fish values restored"),Signature(Inventory->BuildPersistenceSnapshot()),Expected);
  Test->TestEqual(TEXT("Two explicit kept catches survive"),Inventory->PouchCount(),2);
  Test->TestEqual(TEXT("Restart does not sell or reward a catch"),GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance(),int64(1850));
  Test->TestFalse(TEXT("Restart never restores an active line/input lock"),PC->GetFishingComponent()->IsActive() || PC->IsMoveInputIgnored());
  Test->TestTrue(TEXT("Repeated load cannot duplicate fish"),Save->LoadGame());Test->TestEqual(TEXT("Repeated load keeps exact inventory"),Signature(Inventory->BuildPersistenceSnapshot()),Expected);
  Manifest(Test,TEXT("B"),Model.Get());Test->TestTrue(TEXT("Delete isolated fish slot"),Save->DeleteSave());return true;
 }
private:FAutomationTestBase* Test;double Started=0;
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FNeonCityFishingRestart,"NeonCity.Fishing.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FNeonCityFishingRestart::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityFishingSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode);Commands.Add(Mode); }
}
bool FNeonCityFishingRestart::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 if(Parameters==TEXT("A"))FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFishingTests::FCoastalLoop>(this,true));
 else FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFishingTests::FRestart>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
