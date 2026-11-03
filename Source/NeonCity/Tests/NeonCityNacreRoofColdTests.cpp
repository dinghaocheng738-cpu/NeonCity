#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NacreGenuineCold
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("NacreRoofCold"):TEXT("NacreRoofSave")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>100)return End(TEXT("Genuine roof save/cold sequence timed out"));if(!PC || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(60380,-152870,750))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Ownership=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->VehicleID==FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8))Car=*It;
   const bool Replica=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRequire720S"));
   if(!P || !Car || !Save->ConfigureSlot(Replica?TEXT("NeonCity_Test_720SReplicaRoofSave01"):TEXT("NeonCity_Test_NacreRoofSave01"),false) || !Fleet())return End(TEXT("Current genuine3car fleet/roof/save domain missing or duplicated"));Roof=Car->GetRoofMechanism();
   if(Replica && !Car->GetPresentationProfile())return End(TEXT("Required replacement 720S presentation is absent"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine prior F6 roof checkpoint required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Preserve prior genuine roof source evidence"));
   const FVector Nearby=Car->GetActorLocation()-Car->GetActorRightVector()*220;FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(NacreSupportedSaveStart),false,P);Q.AddIgnoredActor(Car);
   if(!W->LineTraceSingleByChannel(Floor,Nearby+FVector(0,0,500),Nearby-FVector(0,0,550),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Parked Nacre lacks actual supported player approach"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Parked Nacre approach has blocked standing clearance"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Vehicles.FindByPredicate([&](const auto& V){return V.VehicleID==Car->VehicleID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || !Fleet() || !Roof->IsTargetOpen() || D->RoofProgress<.38 || D->RoofProgress>.65 || Roof->GetProgress()<D->RoofProgress || !P || P->IsHidden() || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>10)return End(TEXT("Genuine F7 lost original3car IDs, in-progress roof target/progress or safe saved player"));
   SavedProgress=D->RoofProgress;return Next(21);
  }
  if(Stage==21 && !Roof->IsCycling())
  {if(Roof->GetProgress()!=1 || FVector::Dist2D(P->GetActorLocation(),Car->GetActorLocation())>330)return End(TEXT("Cold actual roof continuation or saved normal re-entry approach failed"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Nacre-Cold-Roof-Finished-Open.png"));return Next(1);}
  if(Stage==1 && Age>.3)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Actual saved-player F eye trace cannot focus Nacre"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.2){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Actual F cannot enter restored Nacre"));return Next(3);}
  if(Stage==3 && Age>.3){Key(EKeys::R,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::R,false);return Next(5);}
  if(Stage==5)
  {
   if(bCold)
   {
    if(Roof->IsCycling())return false;if(Roof->GetProgress()!=0 || Roof->IsTargetOpen() || !Fleet())return End(TEXT("Restored Nacre native R closing or stable3car fleet failed"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Nacre-Cold-Roof-Closed.png"));Key(EKeys::F,true);return Next(8);
   }
   if(!Roof->IsTargetOpen())return End(TEXT("Native R did not open actual roof"));if(Roof->GetProgress()<.4)return false;Key(EKeys::F6,true);return Next(6);
  }
  if(Stage==6 && Age>.6)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Vehicles.FindByPredicate([&](const auto& V){return V.VehicleID==Car->VehicleID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || !D->RoofTargetOpen || D->RoofProgress<.38 || D->RoofProgress>.65 || !Fleet())return End(TEXT("Actual F6 did not retain in-progress roof under original identity"));
   for(const FGuid ID:Ids())if(!M->Vehicles.ContainsByPredicate([&](const auto& V){return V.VehicleID==ID && V.Owned;}))return End(TEXT("Actual F6 lost an owned default supercar"));SavedProgress=D->RoofProgress;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Nacre-Native-MidRoof-Saved.png"));Key(EKeys::F,true);return Next(8);
  }
  if(Stage==8 && Age>.2){Key(EKeys::F,false);if(PC->GetPawn()!=P || P->IsHidden() || !P->GetActorEnableCollision())return End(TEXT("Actual F exit lost normal visible player"));return End(TEXT(""));}
  return false;
 }
 static TArray<FGuid> Ids(){return {FGuid(0x5FF78036,0x45833C8D,0x593E77A5,0x0B006245),FGuid(0x7F242340,0x469FEA4D,0x1A7D21A5,0xD5DED892),FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8)};}
 bool Fleet()const
 {
  int Count=0;TSet<FGuid> Seen;for(TActorIterator<AIslandSupercar> It(W);It;++It){if(Seen.Contains(It->VehicleID))return false;Seen.Add(It->VehicleID);if(It->ActorHasTag(TEXT("NeonCity.IslandDefaultFleet")))++Count;}
  if(Count!=3)return false;for(const auto ID:Ids())if(!Seen.Contains(ID) || !Ownership->IsOwned(ID))return false;return true;
 }
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& V=Keys.FindOrAdd(K);if(V!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));V=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetBoolField(TEXT("independent_cold_process"),bCold);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("actual_saved_roof_progress"),SavedProgress);J->SetStringField(TEXT("scope"),bCold?TEXT("Independent genuine F7 of native F6 mid-opening checkpoint. Actual3default GUIDs/ownership/uniqueness; saved visible on-foot position; physical roof continues open, real F reboard/R fully close/F exit. Zero pose/state/reward/ownership fixtures. Legacy production fleet and subjective coachwork remain separate."):TEXT("One supported on-foot player start beside existing default Nacre only. Real F board/R open; actual F6 saves continuous mid-opening roof and all3owned default GUIDs; actual F exit. No vehicle position/velocity, roof state, reward or ownership writes. Prior full roof/reversal test is cached."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(bCold?TEXT("NacreRoofCold"):TEXT("NacreRoofSave"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;bool bCold;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;UNacreRoofComponent* Roof=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UVehicleOwnershipSubsystem* Ownership=nullptr;TMap<FKey,bool> Keys;int Stage=0;float At=0,SavedProgress=0;double Start=0;FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNacreRoofGenuineSave,"NeonCity.Phase205c.NacreRoofSave.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNacreRoofGenuineSave::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NacreGenuineCold::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNacreRoofGenuineCold,"NeonCity.Phase205c.NacreRoofSave.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNacreRoofGenuineCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NacreGenuineCold::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
