#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandResidenceLift.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace LiftColdTest
{
class FRestore:public IAutomationLatentCommand
{
public:
 explicit FRestore(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;if(FPlatformTime::Seconds()-Started>60)return End(TEXT("Cold lift restore timed out"));const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30500))))return false;
   for(TActorIterator<AIslandResidenceLift> It(W);It;++It)Lift=*It;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Lift || Lift->GetCurrentFloor()!=0 || !Save->ConfigureSlot(TEXT("NeonCity_Test_LiftUpgrade01"),false) || !Save->DoesSaveExist())return End(TEXT("Fresh native lift world and completed source slot required"));
   auto* M=Save->ReadSaveForInspection();if(!M)return End(TEXT("Saved envelope invalid"));Expected=M->Player.Transform.GetLocation();Key(EKeys::F7,IE_Pressed);return Next(1);
  }
  if(Stage==1 && Age>.5)
  {
   Key(EKeys::F7,IE_Released);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || FVector::Dist(P->GetActorLocation(),Expected)>20 || !P->GetCharacterMovement()->IsMovingOnGround() || P->GetMovementBaseObject()==Lift->Floor)return End(TEXT("Normal F7 cold restore did not stand safely on the saved static roof bridge"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Lift-Cold-SafeRoofLanding.png"));PC->SetControlRotation((Lift->CallPanels[4]->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return End(TEXT("Saved roof landing cannot reach normal lift call panel"));Key(EKeys::F,IE_Pressed);return Next(2);
  }
  if(Stage==2 && Age>.2){Key(EKeys::F,IE_Released);return Next(3);}
  if(Stage==3)
  {
   if(Age>22)return End(TEXT("Cold reset ground cabin cannot be called back to saved roof landing"));
   if(Lift->GetCurrentFloor()==4 && Lift->AreDoorsOpen()){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Lift-Cold-CalledCabin.png"));return End(TEXT(""));}
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);for(auto K:{EKeys::F,EKeys::F7})Key(K,IE_Released);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("scope"),TEXT("Independent process; actual F7 shared restore from prior native F6 lift save; static roof landing supports player while cabin resets below; actual F call returns the physical cabin. No player fixture or test pose write, no save overwrite."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString D=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*D,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(D/TEXT("LiftColdRestore.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandResidenceLift* Lift=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FVector Expected;double Started=0;float At=0;int32 Stage=0;FNeonCityVisualEvidence Evidence{TEXT("LiftColdRestore")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResidenceLiftCold,"NeonCity.Phase205c.ResidenceLiftColdPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FResidenceLiftCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(LiftColdTest::FRestore(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
