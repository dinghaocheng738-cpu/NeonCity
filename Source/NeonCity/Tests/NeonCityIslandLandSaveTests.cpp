#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Adventure/IslandLandResidence.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Combat/HealthComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
// All disk writes belong to isolated test slots. The runner copies the legacy
// file byte-for-byte; neither this test nor load rewrites the player's source.
const FString LegacySlot=TEXT("NeonCity_Test_LandFinal_Legacy15");
const FString NegativeSlot=TEXT("NeonCity_Test_LandFinal_Negative15");
const FString CrossSlot=TEXT("NeonCity_Test_LandFinal_Cross15");

class FLandSaveCompatibility : public IAutomationLatentCommand
{
public:
 FLandSaveCompatibility(FAutomationTestBase* InTest,FString InMode):Test(InTest),Mode(MoveTemp(InMode)) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(FString::Printf(TEXT("Land save compatibility timeout stage %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || !W->HasBegunPlay() || W->GetTimeSeconds()<2)return false;
  auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  if(Stage==0)
  {
   for(TActorIterator<AIslandLandResidence> It(W);It;++It){Residence=*It;ResidenceTransform=It->GetActorTransform();PropertyID=It->Property->GetPropertyID();}
   if(!Test->TestNotNull(TEXT("Authored residence exists before load"),Residence))return true;
   Test->TestTrue(TEXT("Current official mansion origin retained"),ResidenceTransform.GetLocation().Equals(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480)),1));
   for(TActorIterator<AActor> It(W);It;++It)
   {
    TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
    for(auto* C:Components)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_Land")))StaticTransforms.Add(C,C->GetComponentTransform());
   }
   Test->TestTrue(TEXT("Static mansion/furniture collision and render transforms captured"),StaticTransforms.Num()>10);
   Save->ConfigureSlot(Mode==TEXT("B")?CrossSlot:LegacySlot);
   if(!Test->TestTrue(TEXT("Required isolated save already exists"),Save->DoesSaveExist()))return true;
   Expected.Reset(Save->ReadSaveForInspection());if(!Test->TestNotNull(TEXT("Legacy/current integrity-checked save readable"),Expected.Get()))return true;
   if(Mode==TEXT("B"))
   {
    FString Receipt;FFileHelper::LoadFileToString(Receipt,*(FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/SaveCompatibility15/SessionA.json")));
    uint32 PID=0;FParse::Value(*Receipt,TEXT("\"processId\":"),PID);
    Test->TestTrue(TEXT("New save restored in a different OS process"),PID!=0 && PID!=FPlatformProcess::GetCurrentProcessId());
    Test->TestTrue(TEXT("Exact new save generation reopened"),Receipt.Contains(Expected->SaveID.ToString()));
   }
   if(!Test->TestTrue(TEXT("Load retains current static world authority"),Save->LoadGame())){Test->AddError(Save->GetLastDetail());return true;}
   Verify(W,P,Save,Mode!=TEXT("B"));
   if(Mode==TEXT("B")){WriteReceipt(TEXT("SessionB.json"),P,Save);return true;}
   Stage=1;At=W->GetTimeSeconds();return false;
  }
  if(Stage==1 && W->GetTimeSeconds()-At>.6)
  {
   // A blocked capsule must never be "repaired" through solid geometry.
   Blocker=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Blocker);
   Blocker->SetRootComponent(Box);Box->SetBoxExtent(FVector(90,90,120));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();
   Blocker->SetActorLocation(Expected->Player.Transform.GetLocation());
   Save->ConfigureSlot(NegativeSlot);Test->TestTrue(TEXT("Dedicated blocked legacy fixture written"),Save->WriteTestFixture(Expected.Get()));
   Test->TestTrue(TEXT("Blocked checkpoint falls back safely"),Save->LoadGame());
   Test->TestTrue(TEXT("Blocked position uses explicit safe spawn"),Save->GetLastDetail().Contains(TEXT("authored safe spawn")));
   Test->TestTrue(TEXT("Blocked checkpoint did not move the mansion"),Residence->GetActorTransform().Equals(ResidenceTransform,.01));
   Blocker->Destroy();Blocker=nullptr;
   auto* High=DuplicateObject<UNeonCitySaveGame>(Expected.Get(),GetTransientPackage());
   High->Player.Transform.AddToTranslation(FVector(0,0,200));
   Test->TestTrue(TEXT("Unsupported high checkpoint fixture written"),Save->WriteTestFixture(High));
   Test->TestTrue(TEXT("Large drop fails safely to default spawn"),Save->LoadGame());
   Test->TestTrue(TEXT("Migration cannot rescue arbitrary airborne checkpoints"),Save->GetLastDetail().Contains(TEXT("authored safe spawn")));
   Save->DeleteSave();Save->ConfigureSlot(LegacySlot);
   Test->TestTrue(TEXT("Exact legacy generation can be loaded repeatedly"),Save->LoadGame());Verify(W,P,Save,true);
   Stage=2;At=W->GetTimeSeconds();return false;
  }
  if(Stage==2 && W->GetTimeSeconds()-At>1 && P->GetCharacterMovement()->IsMovingOnGround())
  {
   Save->ConfigureSlot(CrossSlot);
   if(!Test->TestTrue(TEXT("Restored mansion checkpoint saved for cross-process test"),Save->SaveGame())){Test->AddError(Save->GetLastDetail());return true;}
   Expected.Reset(Save->ReadSaveForInspection());WriteReceipt(TEXT("SessionA.json"),P,Save);return true;
  }
  return false;
 }
private:
 void Verify(UWorld* W,ANeonCityPlayerCharacter* P,UNeonCitySaveSubsystem* Save,bool Legacy)
 {
  Test->TestTrue(TEXT("Residence never moved by property/world load"),Residence->GetActorTransform().Equals(ResidenceTransform,.01));
  for(const auto& Entry:StaticTransforms)Test->TestTrue(TEXT("Authored furniture and static world component unchanged"),Entry.Key.IsValid() && Entry.Key->GetComponentTransform().Equals(Entry.Value,.01));
  const FVector Before=Expected->Player.Transform.GetLocation(),After=P->GetActorLocation();
  Test->TestTrue(TEXT("Player restores to same checkpoint XY"),FVector::Dist2D(Before,After)<.1);
  if(Legacy)
  {
   Test->TestTrue(TEXT("Old pavement checkpoint settled by a bounded downward correction"),Before.Z-After.Z>0 && Before.Z-After.Z<=50);
   Test->TestTrue(TEXT("Compatibility correction explicitly reported"),Save->GetLastDetail().Contains(TEXT("floor height adjusted")));
  }
  else Test->TestTrue(TEXT("Current supported checkpoint restores exactly"),P->GetActorTransform().Equals(Expected->Player.Transform,.1));
  Test->TestFalse(TEXT("No silent fallback to distant city spawn"),Save->GetLastDetail().Contains(TEXT("authored safe spawn")));
  Test->TestEqual(TEXT("Saved player health retained"),P->GetHealthComponent()->GetCurrentHealth(),Expected->Player.Health);
  Test->TestEqual(TEXT("Saved player stamina retained"),P->GetPlayerDataComponent()->GetStamina(),Expected->Player.Stamina);
  auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  Test->TestEqual(TEXT("Real ocean catch count retained"),Inventory->GetFish().Num(),Expected->Fishing.Fish.Num());
  for(const auto& Fish:Expected->Fishing.Fish)Test->TestNotNull(TEXT("Original real catch GUID retained"),Inventory->FindFish(Fish.FishID));
  auto* Vehicles=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
  for(const auto& Vehicle:Expected->Vehicles)
  {
   auto* Live=Vehicles->FindLiveVehicle(Vehicle.VehicleID);
   Test->TestNotNull(TEXT("Original authored vehicle GUID still resolves"),Live);
   Test->TestEqual(TEXT("Vehicle ownership restored"),Vehicles->IsOwned(Vehicle.VehicleID),Vehicle.Owned);
  }
  auto* Properties=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
  for(const auto& Property:Expected->Properties)Test->TestEqual(TEXT("Property ownership restored without transform data"),Properties->IsOwned(Property.PropertyID),Property.Owned);
 }
 void WriteReceipt(const TCHAR* Name,ANeonCityPlayerCharacter* P,UNeonCitySaveSubsystem* Save)
 {
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/SaveCompatibility15");IFileManager::Get().MakeDirectory(*Folder,true);
  const FVector M=Residence->GetActorLocation(),L=P->GetActorLocation();
  const FString JSON=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"slot\":\"%s\",\"mansion\":[%.6f,%.6f,%.6f],\"player\":[%.6f,%.6f,%.6f],\"staticComponents\":%d,\"fish\":%d,\"vehicles\":%d,\"detail\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*Expected->SaveID.ToString(),*Save->GetSlotName(),M.X,M.Y,M.Z,L.X,L.Y,L.Z,StaticTransforms.Num(),Expected->Fishing.Fish.Num(),Expected->Vehicles.Num(),*Save->GetLastDetail().Replace(TEXT("\""),TEXT("'")));
  Test->TestTrue(TEXT("Cross-process static authority evidence written"),FFileHelper::SaveStringToFile(JSON,*(Folder/Name)));
 }
 FAutomationTestBase* Test;FString Mode;int32 Stage=0;double Started=0;float At=0;
 AIslandLandResidence* Residence=nullptr;AActor* Blocker=nullptr;FTransform ResidenceTransform;FName PropertyID;
 TMap<TWeakObjectPtr<UStaticMeshComponent>,FTransform> StaticTransforms;TStrongObjectPtr<UNeonCitySaveGame> Expected;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandSaveCompatibility,"NeonCity.IslandLand.SaveCompatibilityPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandSaveCompatibility::RunTest(const FString&)
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityLandSaveSession="),Mode);
 if(Mode!=TEXT("A") && Mode!=TEXT("B")){AddError(TEXT("Explicit isolated session A or B required"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FLandSaveCompatibility>(this,Mode));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
