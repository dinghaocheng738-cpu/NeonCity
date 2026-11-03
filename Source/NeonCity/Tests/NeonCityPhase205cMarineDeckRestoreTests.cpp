#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Core/Persistence/NeonCityMarineDeckRestore.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeSavedMarineDeck
{
class FFloorCases : public IAutomationLatentCommand
{
public:
 explicit FFloorCases(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>50){Test->AddError(TEXT("Read-only actual saved deck floor cases timed out"));return true;}if(!P || W->GetTimeSeconds()<6)return false;
  auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(56000,-174000,0))))return false;ANeonCityFishingBoat* Boat=nullptr;for(TActorIterator<ANeonCityFishingBoat> It(W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boat=*It;
  if(!Boat || !Boat->GetMarineMovement()->IsMoored()){Test->AddError(TEXT("Actual unchanged moored Harborline needed for deck collision tests"));return true;}
  FNeonCityMarineSaveData Record;Record.VehicleID=Boat->VehicleID;Record.DataAssetID=Boat->GetProfile()->VehicleType;Record.Owned=Boat->Owned;Record.Condition=Boat->GetHealthComponent()->GetCurrentHealth()/Boat->GetProfile()->Durability;Record.Transform=Boat->GetActorTransform();
  const float Half=P->GetSimpleCollisionHalfHeight();const FVector DeckFeet=Boat->GetActorTransform().TransformPosition(Boat->GetProfile()->DeckExit);const FTransform Standing(FRotator(0,Boat->GetActorRotation().Yaw,0),DeckFeet+FVector(0,0,Half+3));
  const bool Positive=NeonCityValidateSavedMarineDeck(W,P,Record,Standing);Test->TestTrue(TEXT("Actual native deck surface and full standing capsule qualify"),Positive);
  auto Reject=[&](const TCHAR* Name,const FNeonCityMarineSaveData& R,const FTransform& S){const bool Refused=!NeonCityValidateSavedMarineDeck(W,P,R,S);Test->TestTrue(Name,Refused);return Refused;};bool Negatives=true;
  auto WrongID=Record;WrongID.VehicleID=FGuid::NewGuid();Negatives &= Reject(TEXT("Absent saved vessel identity rejected"),WrongID,Standing);
  auto WrongProfile=Record;WrongProfile.DataAssetID=TEXT("Marine.InvalidProfile");Negatives &= Reject(TEXT("Wrong saved vessel profile rejected"),WrongProfile,Standing);
  auto Unowned=Record;Unowned.Owned=false;Negatives &= Reject(TEXT("Unowned saved vessel rejected"),Unowned,Standing);
  auto Wreck=Record;Wreck.Condition=0;Negatives &= Reject(TEXT("Saved wrecked vessel rejected"),Wreck,Standing);
  auto Rolled=Record;Rolled.Transform.SetRotation(FRotator(0,Boat->GetActorRotation().Yaw,35).Quaternion());Negatives &= Reject(TEXT("Unsafe saved hull attitude rejected"),Rolled,Standing);
  auto CabinRoof=Standing;CabinRoof.SetLocation(Boat->GetActorTransform().TransformPosition(FVector(110,0,360)));Negatives &= Reject(TEXT("Cabin roof is not a supported native deck floor"),Record,CabinRoof);
  auto Hull=Standing;Hull.SetLocation(Boat->GetActorTransform().TransformPosition(FVector(0,0,-30)));Negatives &= Reject(TEXT("Hull interior/submerged capsule rejected"),Record,Hull);
  auto Water=Standing;Water.SetLocation(Boat->GetActorTransform().TransformPosition(FVector(-80,400,30)));Negatives &= Reject(TEXT("Water beside the deck with no ground rejected"),Record,Water);
  auto Air=Standing;Air.SetLocation(Standing.GetLocation()+FVector(0,0,80));Negatives &= Reject(TEXT("Airborne capsule above deck rejected"),Record,Air);
  auto Side=Standing;Side.SetLocation(Boat->GetActorTransform().TransformPosition(FVector(0,180,Half+35)));Negatives &= Reject(TEXT("Gunwale/hull-side contact cannot qualify as deck floor"),Record,Side);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Positive && Negatives && !Test->HasAnyErrors());J->SetBoolField(TEXT("actual_deck_positive"),Positive);J->SetBoolField(TEXT("all_negative_cases_rejected"),Negatives);J->SetStringField(TEXT("scope"),TEXT("Read-only current original Harborline native collision. Diagnostic saved-value records only; no disk save, player/boat/NPC pose, condition, velocity or ownership changes. Positive requires actual deck contract/full capsule support; wrong GUID/profile/unowned/wreck/unsafe roll/cabin roof/hull/water/air/gunwale must fail. Not genuine gameplay or cold-process acceptance."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("MarineDeckRestoreFloors");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineDeckRestoreFloors,"NeonCity.Phase205c.Activity.SeaRescue.DeckFloorPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineDeckRestoreFloors::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeSavedMarineDeck::FFloorCases(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
