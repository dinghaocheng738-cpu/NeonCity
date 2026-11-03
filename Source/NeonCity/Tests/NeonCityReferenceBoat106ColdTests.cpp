#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace ReferenceBoat106
{
class FColdRestore : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),LoadedAt=0;FString Label,Slot,Producer;
 UNeonCitySaveSubsystem* Save=nullptr;TStrongObjectPtr<UNeonCitySaveGame> Model;bool Began=false;
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
  J->SetNumberField(TEXT("consumer_process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("slot"),Slot);J->SetStringField(TEXT("actual_producer_receipt"),Producer);
  J->SetStringField(TEXT("scope"),TEXT("Fresh OS process loads the genuine normal-return quay SaveGame. Player ground support, both working yacht transforms, identity, ownership and damage checked; no test pose writes, save fixture injection, drive rerun or human slot writes."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/Label;IFileManager::Get().MakeDirectory(*Dir,true);if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("Reference106BoatColdRestore.json"))))Test->AddError(TEXT("Cold restore receipt could not be saved"));return true;
 }
public:
 explicit FColdRestore(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Started>70)return End(TEXT("Cold shore restore exceeded70s"));
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<8)return false;
  if(!Began)
  {
   FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);Label=FPaths::MakeValidFileName(Label);FString Prior=Label;
   if(!Prior.RemoveFromEnd(TEXT("Consumer")) || !Prior.EndsWith(TEXT("RefBoat106")))return End(TEXT("Unique RefBoat106Consumer run label required"));
   Prior+=TEXT("Producer");Producer=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/Prior/TEXT("Yacht0-SternBerth13.json");FString Text;TSharedPtr<FJsonObject> Receipt;
   if(!FFileHelper::LoadFileToString(Text,*Producer) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Receipt)
    || !Receipt->GetBoolField(TEXT("passed")) || !Receipt->GetBoolField(TEXT("actual_shore_save_passed"))
    || uint32(Receipt->GetNumberField(TEXT("process_id")))==FPlatformProcess::GetCurrentProcessId())return End(TEXT("Successful genuine producer from a different OS process required"));
   Slot=Receipt->GetStringField(TEXT("shore_save_slot"));if(!Slot.StartsWith(TEXT("NeonCity_Test_C923_Ref106_")))return End(TEXT("Owned dedicated shore slot required"));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Save || !Save->ConfigureSlot(Slot,false) || !Save->DoesSaveExist())return End(TEXT("Genuine producer save absent"));
   Model.Reset(Save->ReadSaveForInspection());if(!Model.IsValid() || !Model->HasMarine || !Model->HasPlayer)return End(TEXT("Detached producer model missing actual player/marine data"));
   if(!Save->LoadGame())return End(TEXT("Production cold LoadGame rejected: ")+Save->GetLastDetail());Began=true;return false;
  }
  if(Save->IsOperationInProgress())return false;if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Production cold restore failed: ")+Save->GetLastDetail());
  if(!LoadedAt){LoadedAt=W->GetTimeSeconds();return false;}if(W->GetTimeSeconds()-LoadedAt<1)return false;
  if(!P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist(P->GetActorLocation(),Model->Player.Transform.GetLocation())>120)return End(TEXT("Cold restored player failed actual saved quay support/position"));
  const auto Actual=W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot(false);int32 Checked=0;
  for(const auto& Saved:Model->Marine)if(Saved.VehicleID==FGuid(0xA5620B90,0x39085C58,0xA507A26E,0x1EFD1981) || Saved.VehicleID==FGuid(0x3A8B376B,0x91695E5C,0xB164FD5E,0xEA9E8457))
  {
   const auto* Now=Actual.FindByPredicate([&](const auto& V){return V.VehicleID==Saved.VehicleID;});
   if(!Now || Now->DataAssetID!=Saved.DataAssetID || Now->Owned!=Saved.Owned || !FMath::IsNearlyEqual(Now->Condition,Saved.Condition,.001f)
    || FVector::Dist(Now->Transform.GetLocation(),Saved.Transform.GetLocation())>100
    || FMath::Abs(FMath::FindDeltaAngleDegrees(Now->Transform.Rotator().Yaw,Saved.Transform.Rotator().Yaw))>3)return End(TEXT("Cold restored working yacht pose/identity/ownership/condition differs from genuine save"));++Checked;
  }
  return End(Checked==2?TEXT(""):TEXT("Both working yacht records required"));
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceBoat106Cold,"NeonCity.Phase205c.MainlandC923.Standalone.Reference106BoatColdRestore",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FReferenceBoat106Cold::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Isolated automatic persistence opt-out required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(ReferenceBoat106::FColdRestore(this));return true;}
#endif
