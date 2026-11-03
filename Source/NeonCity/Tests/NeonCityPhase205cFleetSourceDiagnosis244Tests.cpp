#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetSourceDiagnosis244,"NeonCity.Phase205c.Garage.StoredSourceDiagnosis244",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetSourceDiagnosis244::RunTest(const FString&)
{
 const FString SaveDir=FPaths::ProjectSavedDir()/TEXT("SaveGames");
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(SaveDir/TEXT("NeonCity_Test_*.sav")),true,false);Files.Sort();
 TArray<TSharedPtr<FJsonValue>> Sources;
 for(const auto& File:Files)
 {
  if(File.Contains(TEXT("Backup")))continue;
  const FString Slot=FPaths::GetBaseFilename(File);TArray<uint8> Before,After;
  if(!FFileHelper::LoadFileToArray(Before,*(SaveDir/File))){AddError(TEXT("Retained source bytes unreadable: ")+Slot);continue;}
  const auto* Model=Cast<UNeonCitySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
  if(!Model){AddWarning(TEXT("Optional retained slot is not NeonCity SaveGame: ")+Slot);continue;}
  auto Source=MakeShared<FJsonObject>();Source->SetStringField(TEXT("slot"),Slot);
  Source->SetBoolField(TEXT("has_vehicles"),Model->HasVehicles);Source->SetNumberField(TEXT("raw_vehicle_count"),Model->Vehicles.Num());
  TArray<TSharedPtr<FJsonValue>> Vehicles;
  for(const auto& Data:Model->Vehicles)
  {
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),Data.VehicleID.ToString(EGuidFormats::Digits));
   Row->SetStringField(TEXT("profile"),Data.DataAssetID.ToString());Row->SetNumberField(TEXT("type"),Data.VehicleType);
   Row->SetBoolField(TEXT("owned"),Data.Owned);Row->SetBoolField(TEXT("legacy_stored"),Data.LegacyFleetStored);
   Row->SetNumberField(TEXT("migration_version"),Data.IslandFleetMigrationVersion);Row->SetNumberField(TEXT("condition"),Data.Condition);
   Row->SetStringField(TEXT("stored_property"),Data.StoredPropertyID.ToString());Row->SetStringField(TEXT("position_cm"),Data.Transform.GetLocation().ToString());
   Vehicles.Add(MakeShared<FJsonValueObject>(Row));
  }
  Source->SetArrayField(TEXT("raw_vehicles"),Vehicles);
  const bool Unchanged=FFileHelper::LoadFileToArray(After,*(SaveDir/File)) && Before==After;
  Source->SetBoolField(TEXT("source_bytes_unchanged"),Unchanged);if(!Unchanged)AddError(TEXT("Read-only source changed: ")+Slot);
  Sources.Add(MakeShared<FJsonValueObject>(Source));
 }
 auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("scope"),TEXT("Read only raw retained genuine save records. No F7 restore, normalization, world/input/ownership/vehicle/profile/condition/storage/reward/save writes. Collection success is not retrieval gameplay PASS."));
 Receipt->SetArrayField(TEXT("sources"),Sources);FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("FleetSourceDiagnosis244");
 IFileManager::Get().MakeDirectory(*Dir,true);TestTrue(TEXT("Read-only raw fleet evidence saved"),FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("receipt.json"))));
 return !HasAnyErrors();
}
#endif
