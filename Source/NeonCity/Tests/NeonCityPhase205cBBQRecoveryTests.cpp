#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandGrill.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
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
namespace BBQRecovery
{
constexpr const TCHAR* Slot=TEXT("NeonCity_Test_BBQReservation02");
class FRecovery:public IAutomationLatentCommand
{
public:
 FRecovery(FAutomationTestBase* T,bool Restore):Test(T),bRestore(Restore){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<4)return false;if(FPlatformTime::Seconds()-Started>85)return End(TEXT("BBQ recovery timed out"));
  const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30500))))return false;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();
   for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(!It->bIndoorCooktop)Grill=*It;if(!Grill || !Fishing)return End(TEXT("Original station required"));
   const FString Source=bRestore?Slot:TEXT("NeonCity_Test_RidgeBBQSequence");
   if(!Save->ConfigureSlot(Source,false) || !Save->DoesSaveExist() || !Save->LoadGame())return End(TEXT("Native source checkpoint missing or invalid"));return Next(1);
  }
  if(Stage==1 && Age>.4)
  {
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());
   if(!bRestore)
   {
    const FGuid Stored(0xAC1C9011,0x49C8F6E2,0x231D21A3,0x7197D642),Eaten(0xB3B277DE,0x40ADDD58,0x3E0BDF9C,0xB673BA01);
    if(Inventory->FindFish(Stored) || Inventory->FindFish(Eaten) || !Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.SourceFishID==Stored;}) || Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.SourceFishID==Eaten;}))return End(TEXT("Cold recovery duplicated or lost the prior Store/Eat result"));
    CompletedMealRecovery=true;
    if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->LoadGame())return End(TEXT("Real caught ingredient checkpoint required"));return Next(2);
   }
   const auto* Model=Save->ReadSaveForInspection();if(!Model)return End(TEXT("Reservation save envelope unavailable"));
   const auto* R=Model->Fishing.Fish.FindByPredicate([](const auto& F){return F.Storage==ENeonCityFishStorage::Grill;});
   if(!R)return End(TEXT("Saved actual reservation missing"));FishID=R->FishID;SavedFraction=R->CookFraction;SavedStage=uint8(R->GrillStage);MealsBefore=Inventory->GetMeals().Num();
   return Next(3);
  }
  if(Stage==2 && Age>.4)
  {
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());
   if(!Save->ConfigureSlot(Slot,false) || Save->DoesSaveExist())return End(TEXT("Fresh reserved cooking checkpoint required"));
   Player->SetActorLocation(Grill->GetActorTransform().TransformPosition(FVector(0,-95,0))+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   MealsBefore=Inventory->GetMeals().Num();PouchBefore=Inventory->PouchCount();return Next(3);
  }
  if(Stage==3 && Age>.5)
  {
   if(auto* Weapon=Player->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stowed)return End(TEXT("Mapped weapon stow failed"));const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};const int32 I=Weapon->GetData()->SlotType-1;if(I<0 || I>7)return End(TEXT("Weapon mapping absent"));Stow=Keys[I];Key(Stow,IE_Pressed);Stowed=true;return Next(30);}
   PC->SetControlRotation((Grill->InteractionBounds->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Grill)return End(TEXT("Actual grill prompt not focusable"));Key(EKeys::F,IE_Pressed);return Next(4);
  }
  if(Stage==30 && Age>.2){Key(Stow,IE_Released);return Next(3);}
  if(Stage==4 && Age>.2)
  {Key(EKeys::F,IE_Released);if(Fishing->GetActiveStation()!=Grill)return End(TEXT("Mapped F did not start station"));if(bRestore)return Next(6);Key(EKeys::Enter,IE_Pressed);return Next(5);}
  if(Stage==5 && Age>.2)
  {Key(EKeys::Enter,IE_Released);FishID=Grill->GetReservedFishID();if(!FishID.IsValid())return End(TEXT("Real ingredient not reserved"));return Next(6);}
  if(Stage==6)
  {
   if(!bRestore && Grill->GetCookingStage()!=ENeonCityGrillStage::CookingFirst)return Age>8?End(TEXT("Cooking feedback never began")):false;
   if(bRestore && Grill->GetCookingStage()==ENeonCityGrillStage::Ready)return Next(10);
   FTransform Right,Left;const auto* F=Inventory->FindFish(FishID);
   if(!F || F->Storage!=ENeonCityFishStorage::Grill || F->ContainerID!=Grill->StationID)return End(TEXT("Reserved source identity changed"));
   if(bRestore && (uint8(F->GrillStage)<SavedStage || F->CookFraction<SavedFraction || !Grill->GetCookingHandTargets(Right,Left)))return End(TEXT("Cold resumed cooking lost its state, provenance or hand presentation"));
   if(Age>1 && !Captured){FNeonCityVisualEvidence::CaptureHUD(W,bRestore?TEXT("BBQ-Cold-ResumedSteam.png"):TEXT("BBQ-Repaired-Steam.png"));Captured=true;}
   if(bRestore)return Age>23?End(TEXT("Cold reservation did not finish naturally")):false;
   if(F->CookFraction<.1f)return false;Key(EKeys::F6,IE_Pressed);return Next(7);
  }
  if(Stage==7 && Age>.35)
  {
   Key(EKeys::F6,IE_Released);auto* M=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Fishing.Fish.ContainsByPredicate([&](const auto& F){return F.FishID==FishID && F.Storage==ENeonCityFishStorage::Grill && F.CookFraction>0;}))return End(TEXT("Mapped F6 failed to persist actual partly cooked reservation: ")+Save->GetLastDetail());
   Key(EKeys::F,IE_Pressed);return Next(8);
  }
  if(Stage==8 && Age>.25)
  {
   Key(EKeys::F,IE_Released);const auto* F=Inventory->FindFish(FishID);
   if(Fishing->IsActive() || PC->IsMoveInputIgnored() || !F || F->Storage!=ENeonCityFishStorage::Pouch || F->bAlive || F->CookFraction<=0 || Inventory->PouchCount()!=PouchBefore || Inventory->GetMeals().Num()!=MealsBefore)return End(TEXT("Leaving partly cooked fish lost, duplicated or revived the catch"));
   if(Inventory->PutInAquarium(FishID,TEXT("Island.Mansion.Aquarium.Main"),true))return End(TEXT("Partly cooked catch incorrectly entered aquarium"));
   return End(TEXT(""));
  }
  if(Stage==10)
  {
   if(Inventory->FindFish(FishID) || Inventory->GetMeals().Num()!=MealsBefore+1 || !Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.SourceFishID==FishID && M.MealID==FishID;}))return End(TEXT("Cold completion lost or duplicated its meal"));
   Key(EKeys::One,IE_Pressed);return Next(11);
  }
  if(Stage==11 && Age>.2){Key(EKeys::One,IE_Released);if(Grill->GetCookingStage()!=ENeonCityGrillStage::Closing)return End(TEXT("Native Store failed after resume"));Key(EKeys::F,IE_Pressed);return Next(12);}
  if(Stage==12 && Age>.2){Key(EKeys::F,IE_Released);if(Fishing->IsActive() || PC->IsMoveInputIgnored())return End(TEXT("Resumed session failed to release normal input"));return End(TEXT(""));}
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC && K.IsValid())PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);for(auto K:{EKeys::F,EKeys::F6,EKeys::Enter,EKeys::One,Stow})Key(K,IE_Released);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetBoolField(TEXT("cold_resume"),bRestore);J->SetBoolField(TEXT("prior_store_eat_cold_recovery"),CompletedMealRecovery);J->SetStringField(TEXT("fish_id"),FishID.ToString());J->SetStringField(TEXT("save_slot"),Slot);J->SetStringField(TEXT("scope"),TEXT("Original genuine boat catch; normal F/ENTER/F6/1; actual partial reservation, leave/cancel without loss or resurrection, and independent-process resume/completion. A declares one grill-front fixture; B restores the actual saved player position. Full pouch is a separate boundary check."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString D=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*D,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(D/(bRestore?TEXT("BBQRecovery-B.json"):TEXT("BBQRecovery-A.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandLandGrill* Grill=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;
 bool bRestore=false,Captured=false,Stowed=false,CompletedMealRecovery=false;FKey Stow;FGuid FishID;double Started=0;float At=0,SavedFraction=0;int32 Stage=0,MealsBefore=0,PouchBefore=0;uint8 SavedStage=0;FNeonCityVisualEvidence Evidence{TEXT("BBQRecovery")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBQRecoveryA,"NeonCity.Phase205c.BBQRecovery.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBQRecoveryA::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(BBQRecovery::FRecovery(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBQRecoveryB,"NeonCity.Phase205c.BBQRecovery.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBQRecoveryB::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(BBQRecovery::FRecovery(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
