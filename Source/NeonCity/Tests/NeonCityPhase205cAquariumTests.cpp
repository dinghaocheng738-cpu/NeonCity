#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandAquarium.h"
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
namespace NativeAquarium
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>80)return End(TEXT("Native aquarium sequence timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48100,-118400,30900))))return false;
   for(TActorIterator<AIslandLandAquarium> It(W);It;++It)Tank=*It;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();
   if(!Tank || !Fishing || !Save->ConfigureSlot(bCold?TEXT("NeonCity_Test_Aquarium01"):TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->DoesSaveExist())return End(TEXT("Actual authored tank and retained genuine catch/checkpoint required"));
   Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());
   for(const auto& F:Inventory->GetFish())
   {
    const auto* Species=Inventory->FindSpecies(F.SpeciesID);
    if(!F.bAlive || F.CookFraction>0 || !Species || !Species->SuitableForAquarium || !Species->SaltwaterSpecies || F.SizeCm>60 || F.FishingPointID.IsNone() || F.CaughtAt.GetTicks()<=0)continue;
    if(bCold?(F.Storage==ENeonCityFishStorage::Aquarium && F.ContainerID==Tank->ContainerID):F.Storage==ENeonCityFishStorage::Pouch){FishID=F.FishID;Weight=F.WeightKg;Size=F.SizeCm;break;}
   }
   if(!FishID.IsValid())return End(TEXT("No genuine living catch meets this real tank's species and size rules; no inventory fixture was generated"));
   Count=Inventory->GetFish().Num();PouchBefore=Inventory->PouchCount();
   if(!Save->ConfigureSlot(bCold?TEXT("NeonCity_Test_Aquarium02"):TEXT("NeonCity_Test_Aquarium01"),false) || Save->DoesSaveExist())return End(TEXT("Retain earlier source; fresh aquarium checkpoint required"));
   if(!bCold)
   {
    // One declared public-floor setup, solely for this new facility. Transfers,
    // fish animation, save/load and withdrawal use the actual player systems.
    P->SetActorLocation(Tank->GetActorTransform().TransformPosition(FVector(0,-160,P->GetSimpleCollisionHalfHeight()+3)),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   }
   return Next(2);
  }
  if(Stage==2 && Age>.4)
  {
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {
    if(Stowed)return End(TEXT("Native weapon stow failed before tank interaction"));const int32 Slot=Weapon->GetData()->SlotType;
    const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Actual weapon slot mapping required"));
    Stow=Keys[Slot-1];Stowed=true;Key(Stow,true);return Next(30);
   }
   PC->SetControlRotation((Tank->InteractionBounds->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Tank)return End(TEXT("Normal tank F prompt is blocked or unreachable"));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==30 && Age>.2){Key(Stow,false);return Next(2);}
  if(Stage==3 && Age>.2){Key(EKeys::F,false);if(Fishing->GetActiveStation()!=Tank)return End(TEXT("Native F did not open actual tank controls"));Selected=0;return Next(4);}
  if(Stage==4 && Age>.15)
  {
   TArray<FNeonCityCaughtFish> Menu;for(const auto& F:Inventory->GetFish())if(F.Storage==ENeonCityFishStorage::Pouch || (F.Storage==ENeonCityFishStorage::Aquarium && F.ContainerID==Tank->ContainerID))Menu.Add(F);
   const int32 Wanted=Menu.IndexOfByPredicate([&](const auto& F){return F.FishID==FishID;});if(Wanted<0)return End(TEXT("Same catch disappeared from actual menu"));
   if(Selected<Wanted){++Selected;Key(EKeys::Right,true);return Next(31);}Key(EKeys::Enter,true);return Next(5);
  }
  if(Stage==31 && Age>.15){Key(EKeys::Right,false);return Next(4);}
  if(Stage==5 && Age>.8)
  {
   Key(EKeys::Enter,false);const auto* F=Inventory->FindFish(FishID);
   if(!F || !F->bAlive || F->WeightKg!=Weight || F->SizeCm!=Size || Inventory->GetFish().Num()!=Count)return End(TEXT("Actual transfer changed catch identity, anatomy or life state"));
   if(bCold)
   {
    FTransform Pose;if(F->Storage!=ENeonCityFishStorage::Pouch || Inventory->PouchCount()!=PouchBefore+1 || Tank->GetDisplayedFishTransform(FishID,Pose))return End(TEXT("Cold native withdrawal duplicated a fish or retained its tank visual"));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Aquarium-Cold-Withdrawal.png"));Key(EKeys::F,true);return Next(8);
   }
   if(F->Storage!=ENeonCityFishStorage::Aquarium || F->ContainerID!=Tank->ContainerID || Inventory->PouchCount()!=PouchBefore-1 || !Tank->GetDisplayedFishTransform(FishID,FirstPose))return End(TEXT("Native ENTER did not put/display the actual living catch"));
   if(Inventory->ReserveForGrill(FishID,TEXT("Island.Mansion.Grill")))return End(TEXT("A fish in the aquarium was also reserved for BBQ"));return Next(6);
  }
  if(Stage==6 && Age>1)
  {
   FTransform Pose;if(!Tank->GetDisplayedFishTransform(FishID,Pose) || FVector::Dist(Pose.GetLocation(),FirstPose.GetLocation())<1)return End(TEXT("Tank fish visual is stationary or missing"));
   const FVector D=Pose.GetLocation()-Tank->WaterCentre;if(FMath::Abs(D.X)>Tank->TankExtent.X || FMath::Abs(D.Y)>Tank->TankExtent.Y || FMath::Abs(D.Z)>Tank->TankExtent.Z)return End(TEXT("Living fish visual left the real tank envelope"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Aquarium-Real-Living-Catch.png"));Key(EKeys::F,true);return Next(8);
  }
  if(Stage==8 && Age>.3){Key(EKeys::F,false);if(Fishing->IsActive())return End(TEXT("Native F failed to leave aquarium UI"));Key(EKeys::F6,true);return Next(9);}
  if(Stage==9 && Age>.6)
  {
   Key(EKeys::F6,false);const auto* Model=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->Fishing.Fish.ContainsByPredicate([&](const auto& F){return F.FishID==FishID && F.bAlive && F.SizeCm==Size && F.WeightKg==Weight && (bCold?F.Storage==ENeonCityFishStorage::Pouch:F.Storage==ENeonCityFishStorage::Aquarium && F.ContainerID==Tank->ContainerID);}))return End(TEXT("Normal shared save did not retain the exact transfer state"));
   return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  for(auto K:{EKeys::F,EKeys::F6,EKeys::F7,EKeys::Right,EKeys::Enter,Stow})if(K.IsValid())Key(K,false);if(!Failure.IsEmpty())Test->AddError(Failure);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetStringField(TEXT("fish_id"),FishID.ToString());J->SetNumberField(TEXT("stage"),Stage);
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent process: genuine F7 restores actual saved aquarium fish; native F/ENTER withdraws same living GUID, real visual disappears, F6 retains pouch state. No fish/pose fixture."):TEXT("Retained genuine native-boat catch via F7; one declared dry public-floor setup beside existing tank; F/RIGHT/ENTER actual fish transfer, anatomy/life/unique identity, real visual movement and BBQ exclusion, F leave, F6 shared save. No generated catch or reward."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bCold?TEXT("AquariumCold.json"):TEXT("AquariumNative.json"))));return true;
 }
 FAutomationTestBase* Test;bool bCold=false,Stowed=false;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandAquarium* Tank=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;int Stage=0,Selected=0,Count=0,PouchBefore=0;double Started=0;float At=0,Weight=0,Size=0;FGuid FishID;FKey Stow;FTransform FirstPose;FNeonCityVisualEvidence Evidence{TEXT("AquariumNative")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumActual,"NeonCity.Phase205c.Aquarium.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAquariumActual::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeAquarium::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumCold,"NeonCity.Phase205c.Aquarium.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAquariumCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeAquarium::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
