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
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeIslandLandmark
{
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("HiddenCold"):TEXT("HiddenSource")){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>80)return End(TEXT("Native landmark discovery timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(46000,-120000,22000))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Hidden.SeaStrata"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job && It->Index==0)Point=*It;
   auto* GI=W->GetGameInstance();Phone=PC->FindComponentByClass<UPhoneComponent>();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityHiddenRun="),Suffix);Suffix=FPaths::MakeValidFileName(Suffix.IsEmpty()?TEXT("01"):Suffix);Slot=TEXT("NeonCity_Test_HiddenDone")+Suffix;Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("Hidden-")+Suffix)/TEXT("Source.json");
   if(!Job || !Point || !Phone || !Save || !Wallet || !Save->ConfigureSlot(Slot,false))return End(TEXT("Actual authored landmark/phone/save domains missing"));
   if(bCold)
   {
    FString Text;TSharedPtr<FJsonObject> J;double PID=0;FString ID;
    if(!Save->DoesSaveExist() || !FFileHelper::LoadFileToString(Text,*Bridge) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid() || !J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetNumberField(TEXT("balance"),Expected) || !J->TryGetStringField(TEXT("run_id"),ID) || !FGuid::Parse(ID,Run))return End(TEXT("Independent genuine discovered-landmark source required"));Key(EKeys::F7,true);return Next(20);
   }
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*Bridge))return End(TEXT("Preserve genuine landmark evidence; fresh explicit run suffix required after changes"));
   FString Text;TSharedPtr<FJsonObject> Survey;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Survey))return End(TEXT("Actual current road survey required"));
   const auto& Points=Survey->GetArrayField(TEXT("points"));if(Points.Num()<667)return End(TEXT("Actual landmark road station missing"));auto V=[&](int32 I){const auto A=Points[I]->AsArray();return FVector(NeonCityCoastalLayout::IslandOrigin.X+100*A[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*A[1]->AsNumber(),100*A[2]->AsNumber()-124);};
   const FVector Direction=(V(663)-V(661)).GetSafeNormal2D();const FVector Nearby=V(662)+FVector(-Direction.Y,Direction.X,0)*300;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(HiddenSupportedRoadStart),false,P);
   if(!W->LineTraceSingleByChannel(H,Nearby+FVector(0,0,500),Nearby-FVector(0,0,700),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Actual station665 roadside lacks supported discovery approach"));const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual station665 roadside standing clearance blocked"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();StartPosition=Stand;Before=Wallet->GetBalance();Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);const auto D=Job->BuildOpportunitySnapshot(false);const auto* M=Save->ReadSaveForInspection();Before=Wallet->GetBalance();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.State!=2 || D.ActivityCheckpoint!=1 || D.ActivityCompletions!=1 || D.ActivityRunID!=Run || Before!=Expected || !Wallet->HasCredit(Job->GetReceiptID()) || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>20)return End(TEXT("Independent F7 lost actual discovery/run/single90credit/safe saved player"));Key(EKeys::P,true);return Next(1);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual hidden-landmark phone offer undiscoverable"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run)return End(TEXT("Native landmark acceptance/replay failed"));if(bCold){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Landmark replay duplicated prior income"));Key(EKeys::Delete,true);return Next(21);}Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==6)
  {
   if(Age>.2)Key(EKeys::P,false);const FVector Approach=Point->GetActorLocation()+(StartPosition-Point->GetActorLocation()).GetSafeNormal2D()*150;
   if(FVector::Dist2D(P->GetActorLocation(),Approach)>40){PC->SetControlRotation(FRotator(0,(Approach-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);if(Age>18)return End(FString::Printf(TEXT("Actual held-W landmark approach blocked at%s; no cliff teleport applied"),*P->GetActorLocation().ToString()));return false;}
   Key(EKeys::W,false);if(!P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),StartPosition)<150)return End(TEXT("Discovery did not physically walk from supported roadside onto the landmark approach"));PC->SetControlRotation((Point->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Point)return End(TEXT("Actual landmark normal F eye trace inaccessible"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hidden-Native-Discovery-Prompt.png"));Key(EKeys::F,true);return Next(7);
  }
  if(Stage==7 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCheckpoint!=1 || D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Actual landmark F discovery did not record one shared90credit/receipt"));return Next(8);
  }
  if(Stage==8 && Age>.2){Key(EKeys::F,true);return Next(9);}
  if(Stage==9 && Age>.2){Key(EKeys::F,false);if(Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1 || Wallet->GetBalance()!=Before+Job->Reward)return End(TEXT("Repeated separate native F duplicated discovery payment"));Key(EKeys::F6,true);return Next(10);}
  if(Stage==10 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityCheckpoint==1 && D.ActivityCompletions==1 && D.ActivityRunID==Run;}))return End(TEXT("Actual discovery F6 failed to persist its result"));
   auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Bridge),true);if(!FFileHelper::SaveStringToFile(Text,*Bridge))return End(TEXT("Genuine landmark bridge could not be preserved"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hidden-Native-Completed-Saved.png"));return End(TEXT(""));
  }
  if(Stage==21 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Wallet->GetBalance()!=Before)return End(TEXT("Native landmark replay cancellation paid an extra reward"));return End(TEXT(""));}return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetBoolField(TEXT("independent_cold_process"),bCold);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetStringField(TEXT("scope"),bCold?TEXT("Independent genuine F7 discovery result preserves original run/checkpoint/one90credit/receipt/safe saved player; normal phone replay and DELETE cancellation without another reward. Zero pose/state/reward fixtures."):TEXT("One current survey-derived supported player start on retained station665 roadside. Native phone acceptance, actual heldW walk to the existing sea-strata landmark, real normal F prompt/discovery/one shared90credit, separate repeatedF guard and genuine F6. No teleport to the cliff/landmark, no post-setup pose/state/reward fixtures. Landmark art, whole-cliff quality and subjective exploration remain separate."));FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(bCold?TEXT("HiddenCold"):TEXT("HiddenSource"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;bool bCold;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Point=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;TMap<FKey,bool> Keys;FNeonCityVisualEvidence Evidence;int32 Stage=0,Browsed=0;float At=0;double Started=0,Expected=0;int64 Before=0;FGuid Run;FVector StartPosition=FVector::ZeroVector;FString Slot,Bridge;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandHiddenSource,"NeonCity.Phase205c.Activity.Hidden.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandHiddenSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandLandmark::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandHiddenCold,"NeonCity.Phase205c.Activity.Hidden.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandHiddenCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandLandmark::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
