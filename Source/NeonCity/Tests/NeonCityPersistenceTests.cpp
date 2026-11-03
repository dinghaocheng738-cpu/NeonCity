#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleComponent.h"
#include "Property/PropertySubsystem.h"
#include "Property/NeonCityStarterApartment.h"
#include "Property/PropertyComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/HealthComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Wanted/WantedSubsystem.h"
#include "Crime/CrimeSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistenceSchema,"NeonCity.Persistence.Schema",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPersistenceSchema::RunTest(const FString& Parameters)
{
 auto* Data=NewObject<UNeonCitySaveGame>(); FString Error;
 TestFalse(TEXT("An uninitialized model is not a valid disk save"),Data->ValidateEnvelope(Error));
 Data->FormatID=TEXT("NeonCity.SaveGame"); Data->SaveID=FGuid::NewGuid();
 TestTrue(TEXT("Current-version envelope accepted"),Data->ValidateEnvelope(Error) && Data->MigrateToCurrent(Error));
 Data->SaveVersion=UNeonCitySaveGame::CurrentVersion+1; TestFalse(TEXT("Future version cannot be silently downgraded"),Data->MigrateToCurrent(Error));
 Data->SaveVersion=0; TestFalse(TEXT("Unreleased old schema fails safely"),Data->MigrateToCurrent(Error)); Data->SaveVersion=UNeonCitySaveGame::CurrentVersion;
 {
  // Version 1 -> 2: positions on the offshore island (and its sea) move with it; nothing else changes.
  auto* Old=NewObject<UNeonCitySaveGame>(); Old->FormatID=TEXT("NeonCity.SaveGame"); Old->SaveID=FGuid::NewGuid(); Old->SaveVersion=1;
  const FVector Mansion(48600,-118800,30480),Mainland(-92000,36000,973);
  Old->Player.Transform.SetLocation(Mansion);
  Old->Vehicles.SetNum(2); Old->Vehicles[0].Transform.SetLocation(FVector(59249,-153126,747)); Old->Vehicles[1].Transform.SetLocation(Mainland);
  Old->Properties.SetNum(1); Old->Properties[0].GarageSlots.SetNum(1); Old->Properties[0].GarageSlots[0].StoredLocation=FVector(57000,-153500,740);
  Old->ResidenceLife.Pet.Transform.SetLocation(FVector(49050,-119500,30480));
  TestTrue(TEXT("Version 1 save migrates"),Old->MigrateToCurrent(Error) && Old->SaveVersion==UNeonCitySaveGame::CurrentVersion);
  TestTrue(TEXT("Island positions follow the moved island"),Old->Player.Transform.GetLocation().Equals(NeonCityCoastalLayout::FromLegacyIsland(Mansion))
   && Old->Vehicles[0].Transform.GetLocation().Equals(NeonCityCoastalLayout::FromLegacyIsland(FVector(59249,-153126,747)))
   && Old->Properties[0].GarageSlots[0].StoredLocation.Equals(NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-153500,740)))
   && Old->ResidenceLife.Pet.Transform.GetLocation().Equals(NeonCityCoastalLayout::FromLegacyIsland(FVector(49050,-119500,30480))));
  TestTrue(TEXT("Mainland positions are untouched"),Old->Vehicles[1].Transform.GetLocation().Equals(Mainland));
  TestTrue(TEXT("A migrated save is not migrated twice"),Old->MigrateToCurrent(Error) && Old->Player.Transform.GetLocation().Equals(NeonCityCoastalLayout::FromLegacyIsland(Mansion)));
 }
 auto* Defaults=NewObject<UNeonCitySaveGame>(); Defaults->Player.Transform.SetLocation(FVector(1,2,100)); Defaults->World.WorldID=TEXT("NeonCity.SouthBeach");
 TArray<FString> Warnings; Data->FillMissingSections(*Defaults,Warnings);
 TestEqual(TEXT("Every absent optional section reported"),Warnings.Num(),7);
 TestTrue(TEXT("Missing values use design defaults"),Data->Player.Transform.Equals(Defaults->Player.Transform) && Data->Economy.Balance==1850);
 Data->Player.Health=101; TestFalse(TEXT("Invalid health rejected before mutation"),Data->ValidateEnvelope(Error)); Data->Player.Health=100;
 Data->Player.Money=1849; TestFalse(TEXT("Conflicting wallet copies rejected"),Data->ValidateEnvelope(Error)); Data->Player.Money=1850;
 TFunction<bool(const FProperty*)> ValuesOnly;
 ValuesOnly=[&](const FProperty* P)
 {
  if(CastField<FObjectPropertyBase>(P) || CastField<FInterfaceProperty>(P) || CastField<FSoftObjectProperty>(P)) return false;
  if(const auto* A=CastField<FArrayProperty>(P)) return ValuesOnly(A->Inner);
  if(const auto* S=CastField<FStructProperty>(P)) for(TFieldIterator<FProperty> It(S->Struct);It;++It) if(!ValuesOnly(*It)) return false;
  return true;
 };
 for(TFieldIterator<FProperty> It(UNeonCitySaveGame::StaticClass());It;++It) TestTrue(FString::Printf(TEXT("Disk field %s contains only values"),*It->GetName()),ValuesOnly(*It));
 return true;
}

namespace NeonCityPersistenceTests
{
const FName Receipt=TEXT("Persistence.Test.Once");
struct FFixture
{
 UWorld* World=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr; UNeonCityEconomySubsystem* Economy=nullptr; UVehicleOwnershipSubsystem* Vehicles=nullptr;
 UPropertySubsystem* Properties=nullptr; UPhoneSubsystem* Phone=nullptr; UMessageSystem* Messages=nullptr;
 ANeonCityBaseVehicle* Sedan=nullptr; ANeonCityBaseVehicle* Sports=nullptr; ANeonCityStarterApartment* Home=nullptr;
 ANeonCityTransportTest* Delivery=nullptr; ANeonCityDayNightCycle* Clock=nullptr; FName PropertyID; int32 DefaultOwnedCount=0;
 bool Init()
 {
  World=GEditor?GEditor->PlayWorld:nullptr; PC=World?World->GetFirstPlayerController():nullptr;
  if(!PC || !World->HasBegunPlay()) return false;
  Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
  if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround() || World->GetTimeSeconds()<1) return false;
  auto* GI=World->GetGameInstance(); Save=GI->GetSubsystem<UNeonCitySaveSubsystem>(); Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();
  Vehicles=GI->GetSubsystem<UVehicleOwnershipSubsystem>(); Properties=GI->GetSubsystem<UPropertySubsystem>();
  Phone=GI->GetSubsystem<UPhoneSubsystem>(); Messages=GI->GetSubsystem<UMessageSystem>();
  for(TActorIterator<ANeonCityBaseVehicle> It(World);It;++It)
  {
   if(It->GetVehicleProfile()->GetFName()==TEXT("DA_StarterSedan"))Sedan=*It;
   if(It->GetVehicleProfile()->GetFName()==TEXT("DA_SportsCar"))Sports=*It;
  }
  for(TActorIterator<ANeonCityStarterApartment> It(World);It;++It) Home=*It;
  for(TActorIterator<ANeonCityTransportTest> It(World);It;++It) Delivery=*It;
  for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It) Clock=*It;
  if(!Sedan || !Sports || !Home || !Delivery || !Clock) return false;
  DefaultOwnedCount=Vehicles->BuildPersistenceSnapshot(true).FilterByPredicate([](const auto& Record){return Record.Owned;}).Num();
  PropertyID=Home->GetPropertyComponent()->GetPropertyID(); Clock->SetCycleEnabled(false); return true;
 }
 void PlaceCar(ANeonCityBaseVehicle* Car,FTransform Transform)
 {
  Car->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);
  Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
 }
 void Seed(FAutomationTestBase* Test)
 {
  Economy->AddMoney(4150,TEXT("PersistenceTest")); Economy->CreditOnce(100,Receipt);
  Player->GetHealthComponent()->ApplyPersistenceHealth(73,125); Player->GetPlayerDataComponent()->SetStamina(42);
  Player->SetActorTransform(FTransform(FRotator(0,-90,0),FVector(4100,1990,139)),false,nullptr,ETeleportType::TeleportPhysics);
  Player->GetCharacterMovement()->StopMovementImmediately();
  Vehicles->GrantVehicle(Sports); Sedan->GetHealthComponent()->ApplyPersistenceHealth(60,100); Sports->GetHealthComponent()->ApplyPersistenceHealth(42.5f,85);
  PlaceCar(Sports,FTransform(FRotator(0,15,0),FVector(-3800,-1000,100)));
  Test->TestTrue(TEXT("Save fixture registers the existing owned Sedan"),Properties->StoreVehicle(PropertyID,0,Sedan));
  auto Weapons=Player->GetCombatAdapter()->BuildPersistenceSnapshot(true); Weapons[0].Ammo=7; Weapons[0].ReserveAmmo=31; Weapons[0].Equipped=true;
  Player->GetCombatAdapter()->ApplyPersistenceSnapshot(Weapons);
  Test->TestTrue(TEXT("Existing Marcus opportunity accepted before saving"),Phone->ExecuteAction(ANeonCityTransportTest::ReceiptKey(),PC));
  Messages->SendMessage(TEXT("Marcus"),FText::FromString(TEXT("Persistence verification message.")));
  Clock->SetTimeOfDay(19.25f);
 }
 void Mutate()
 {
  Economy->RemoveMoney(1000,TEXT("AfterSave")); Player->GetHealthComponent()->ApplyPersistenceHealth(0,100); Player->GetPlayerDataComponent()->SetStamina(1);
  auto Property=Properties->BuildPersistenceSnapshot(); for(auto& P:Property) { P.Owned=false; P.GarageSlots.Reset(); } Properties->ApplyPersistenceSnapshot(Property);
  Sedan->GetHealthComponent()->ApplyPersistenceHealth(0,100); Sports->GetHealthComponent()->ApplyPersistenceHealth(1,85);
  Messages->ApplyPersistenceSnapshot({}); Clock->SetTimeOfDay(3);
 }
 void Verify(FAutomationTestBase* Test,UNeonCitySaveGame* Expected)
 {
  Test->TestEqual(TEXT("Money restored exactly"),Economy->GetBalance(),int64(6100));
  Test->TestTrue(TEXT("Reward receipts survive load"),Economy->HasCredit(Receipt));
  Test->TestFalse(TEXT("A persisted credit cannot pay twice"),Economy->CreditOnce(100,Receipt));
  Test->TestEqual(TEXT("Health restored without keeping later death"),Player->GetHealthComponent()->GetCurrentHealth(),73.f);
  Test->TestEqual(TEXT("MaxHealth restored"),Player->GetHealthComponent()->GetMaxHealth(),125.f);
  Test->TestFalse(TEXT("Player revived from later unsaved death"),Player->GetHealthComponent()->IsDead());
  Test->TestEqual(TEXT("Stamina restored"),Player->GetPlayerDataComponent()->GetStamina(),42.f);
  Test->TestEqual(TEXT("Vehicle ownership restored"),Vehicles->GetOwnedCount(),Expected?Expected->Vehicles.FilterByPredicate([](const auto& Record){return Record.Owned;}).Num():DefaultOwnedCount+1);
  Test->TestTrue(TEXT("Persistent Sports ID resolves to same authored car"),Vehicles->FindLiveVehicle(Sports->VehicleID)==Sports && Vehicles->IsOwned(Sports->VehicleID));
  Test->TestEqual(TEXT("Sedan condition restored from later destruction"),Sedan->GetHealthComponent()->GetCurrentHealth(),60.f);
  Test->TestEqual(TEXT("Sports condition restored"),Sports->GetHealthComponent()->GetCurrentHealth(),42.5f);
  FNeonCityPropertyRecord Property; Properties->GetProperty(PropertyID,Property);
  Test->TestTrue(TEXT("Property ownership restored"),Property.Owned);
  Test->TestTrue(TEXT("Garage slot matched through stable VehicleID"),Property.GarageSlots.Num()==1 && Property.GarageSlots[0].VehicleID==Sedan->VehicleID);
  auto* Weapon=Player->GetCombatComponent()->GetWeapon();
  Test->TestTrue(TEXT("Owned weapon restored and equipped"),Weapon && Weapon->IsEquipped());
  if(Weapon) { Test->TestEqual(TEXT("Magazine restored"),Weapon->GetAmmo(),7); Test->TestEqual(TEXT("Reserve restored"),Weapon->GetReserveAmmo(),31); }
  Test->TestEqual(TEXT("World clock restored"),Clock->GetTimeOfDay(),19.25f);
  Test->TestTrue(TEXT("Important received message restored"),Messages->GetMessages().ContainsByPredicate([](const auto& M){ return M.Content.ToString()==TEXT("Persistence verification message."); }));
  Test->TestTrue(TEXT("Accepted transport state restored"),Delivery->GetState()==ENeonCityTransportState::Accepted && !Phone->HasAction(ANeonCityTransportTest::ReceiptKey()));
  Test->TestEqual(TEXT("Load resets transient Wanted"),World->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
  Test->TestEqual(TEXT("Load clears transient crime references"),World->GetSubsystem<UCrimeSubsystem>()->GetEvents().Num(),0);
  if(Expected)
  {
   Test->TestTrue(TEXT("Player transform restored"),Player->GetActorTransform().Equals(Expected->Player.Transform,.1));
   const auto* Data=Expected->Vehicles.FindByPredicate([&](const auto& V){ return V.VehicleID==Sports->VehicleID; });
   Test->TestTrue(TEXT("Vehicle position and rotation restored"),Data && Sports->GetActorTransform().Equals(Data->Transform,.1));
  }
 }
};

class FRoundTrip : public IAutomationLatentCommand
{
public:
 explicit FRoundTrip(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(FString::Printf(TEXT("Persistence round trip timeout stage %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Init()) return false;
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_RoundTrip")); F.Save->DeleteSave();
   Test->TestFalse(TEXT("Deleted slot does not exist"),F.Save->DoesSaveExist());
   F.Economy->AddMoney(999,TEXT("BeforeNewGame")); F.Vehicles->GrantVehicle(F.Sports);
   if(!Test->TestTrue(TEXT("New Game writes formal SaveGame"),F.Save->CreateNewGameSave())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   Test->TestTrue(TEXT("New save exists on disk"),F.Save->DoesSaveExist());
   Test->TestEqual(TEXT("New Game uses design wallet, not current cash"),F.Economy->GetBalance(),int64(1850));
   Test->TestEqual(TEXT("New Game restores authored vehicle ownership"),F.Vehicles->GetOwnedCount(),F.DefaultOwnedCount);
   Test->TestEqual(TEXT("New Game restores default time"),F.Clock->GetTimeOfDay(),10.f);
   Test->TestTrue(TEXT("Starter apartment default ownership"),F.Properties->IsOwned(F.PropertyID));
   Stage=1; Phase=F.World->GetTimeSeconds(); return false;
  }
  if(Stage==1 && F.World->GetTimeSeconds()-Phase>1)
  { F.Seed(Test); Stage=2; Phase=F.World->GetTimeSeconds(); return false; }
  if(Stage==2 && F.World->GetTimeSeconds()-Phase>1)
  {
   if(!Test->TestTrue(TEXT("Modified game saved"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   Expected.Reset(F.Save->ReadSaveForInspection()); if(!Test->TestNotNull(TEXT("Disk model readable"),Expected.Get())) return true;
   Test->TestEqual(TEXT("Disk SaveVersion"),Expected->SaveVersion,UNeonCitySaveGame::CurrentVersion);
   F.Mutate(); if(!Test->TestTrue(TEXT("Reload succeeds"),F.Save->LoadGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   F.Verify(Test,Expected.Get());
   Test->TestTrue(TEXT("Repeated load is idempotent"),F.Save->LoadGame()); F.Verify(Test,Expected.Get());
   Test->TestTrue(TEXT("Delete removes all generations"),F.Save->DeleteSave()); Test->TestFalse(TEXT("DoesSaveExist is false after delete"),F.Save->DoesSaveExist());
   Test->TestTrue(TEXT("Missing save safely creates a new game"),F.Save->LoadGame());
   Test->TestEqual(TEXT("Missing-save flow resets design cash"),F.Economy->GetBalance(),int64(1850));
   Test->TestTrue(TEXT("Missing-save flow writes a slot"),F.Save->DoesSaveExist());
   Test->TestFalse(TEXT("NewGame refuses to overwrite existing save"),F.Save->CreateNewGameSave());
   F.Save->DeleteSave(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FFixture F; int32 Stage=0; double Started=0; float Phase=0; TStrongObjectPtr<UNeonCitySaveGame> Expected;
};

class FRecovery : public IAutomationLatentCommand
{
public:
 explicit FRecovery(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>40) { Test->AddError(TEXT("Recovery PIE not ready")); return true; }
  if(!F.Init()) return false;
  F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_Recovery")); F.Save->DeleteSave();
  if(!Test->TestTrue(TEXT("Recovery baseline created"),F.Save->CreateNewGameSave())) { Test->AddError(F.Save->GetLastDetail()); return true; }
  TStrongObjectPtr<UNeonCitySaveGame> Original(F.Save->ReadSaveForInspection());
  F.Economy->AddMoney(25,TEXT("First")); Test->TestTrue(TEXT("First generation"),F.Save->SaveGame());
  F.Economy->AddMoney(25,TEXT("Second")); Test->TestTrue(TEXT("Second generation with backup"),F.Save->SaveGame());
  Test->TestTrue(TEXT("Inject damaged main file"),F.Save->WriteTestFixture(Original.Get(),true));
  Test->TestTrue(TEXT("Corrupt primary recovers backup"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Latest valid prior balance recovered"),F.Economy->GetBalance(),int64(1875));
  Original->SaveVersion=99; F.Save->WriteTestFixture(Original.Get());
  Test->TestFalse(TEXT("Unsupported version refuses load even with backup"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Version failure has explicit result"),F.Save->GetLastResult()==ENeonCitySaveResult::UnsupportedVersion);
  Test->TestEqual(TEXT("Version failure leaves live wallet unchanged"),F.Economy->GetBalance(),int64(1875));
  Test->TestFalse(TEXT("Failed version cannot be overwritten by Save"),F.Save->SaveGame());
  F.Save->DeleteSave(); Original->SaveVersion=1;
  Original->HasPhone=Original->HasWeapons=Original->HasVehicles=Original->HasProperties=Original->HasWorld=false;
  F.Save->WriteTestFixture(Original.Get()); Test->TestTrue(TEXT("Absent legacy sections use defaults"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Missing vehicle section restores authored fleet"),F.Vehicles->GetOwnedCount(),F.DefaultOwnedCount);
  Test->TestTrue(TEXT("Missing phone section restores offer"),F.Phone->HasAction(ANeonCityTransportTest::ReceiptKey()));
  F.Save->DeleteSave(); Original.Reset(F.Save->ReadSaveForInspection()); // Missing lookup is intentionally safe.
  Test->TestTrue(TEXT("Create clean identity fixture"),F.Save->CreateNewGameSave()); Original.Reset(F.Save->ReadSaveForInspection());
  FNeonCityVehicleSaveData InvalidVehicle; Original->Vehicles.Add(InvalidVehicle);
  FNeonCityPropertySaveData Unknown; Unknown.PropertyID=TEXT("Missing.Property"); Unknown.Owned=true; Original->Properties.Add(Unknown);
  F.Save->WriteTestFixture(Original.Get()); Test->TestTrue(TEXT("Invalid VehicleID and unknown PropertyID are isolated"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Skipped records are reported"),F.Save->GetLastDetail().Contains(TEXT("skipped")));
  Test->TestTrue(TEXT("Known property remains valid"),F.Properties->IsOwned(F.PropertyID));
  Test->TestEqual(TEXT("Invalid vehicle did not spawn or gain ownership"),F.Vehicles->GetOwnedCount(),F.DefaultOwnedCount);
  F.Save->DeleteSave(); F.Save->WriteTestFixture(Original.Get(),true);
  const int64 Before=F.Economy->GetBalance(); Test->TestFalse(TEXT("No recovery generation fails safely"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Corrupt slot does not mutate money"),F.Economy->GetBalance(),Before);
  Test->TestTrue(TEXT("Corrupt file retained for diagnosis"),F.Save->DoesSaveExist());
  Test->TestFalse(TEXT("Invalid slot traversal rejected"),F.Save->ConfigureSlot(TEXT("../NeonCity_Autosave_01")));
  F.Save->DeleteSave(); Test->TestFalse(TEXT("Recovery test cleaned every generation"),F.Save->DoesSaveExist()); return true;
 }
 FAutomationTestBase* Test; FFixture F; double Started=0;
};


class FAutosave : public IAutomationLatentCommand
{
public:
 explicit FAutosave(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();
  if(!Started) Started=Now;
  if(Now-Started>75) { Test->AddError(FString::Printf(TEXT("Autosave timeout stage %d: %s"),Stage,F.Save?*F.Save->GetLastDetail():TEXT("no world"))); return true; }
  auto Next=[&](int32 Value){ Stage=Value; Phase=Now; return false; };
  if(Stage==0)
  {
   if(!F.Init()) return false;
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_Autosave")); F.Save->DeleteSave();
   if(!Test->TestTrue(TEXT("Autosave baseline created"),F.Save->CreateNewGameSave())) return true;
   Baseline=F.Save->ReadSaveForInspection()->SaveID;
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_Autosave"),true);
   F.Economy->AddMoney(50,TEXT("HomeAutosave"));
   F.Player->SetActorLocation(FVector(4100,1300,139),false,nullptr,ETeleportType::TeleportPhysics);
   F.Player->GetCharacterMovement()->StopMovementImmediately(); return Next(1);
  }
  if(Stage==1 && Now-Phase>.7)
  {
   if(!Test->TestTrue(TEXT("Entering existing home requests autosave"),F.Home->EnterInterior_Implementation(F.Player))) return true;
   return Next(2);
  }
  if(Stage==2 && Now-Phase>1)
  {
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection());
   if(!Test->TestNotNull(TEXT("Home autosave readable"),Data.Get())) return true;
   Test->TestTrue(TEXT("Home event created a new disk generation"),Data->SaveID!=Baseline);
   Test->TestEqual(TEXT("Home autosave includes latest cash"),Data->Economy.Balance,int64(1900));
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_Autosave"),true);
   Test->TestTrue(TEXT("Accept existing delivery"),F.Phone->ExecuteAction(ANeonCityTransportTest::ReceiptKey(),F.PC));
   F.Player->SetActorLocation(F.Sedan->GetActorLocation()-F.Sedan->GetActorRightVector()*210+FVector(0,0,70),false,nullptr,ETeleportType::TeleportPhysics);
   F.Player->GetCharacterMovement()->StopMovementImmediately(); return Next(3);
  }
  if(Stage==3 && Now-Phase>.7)
  {
   if(!Test->TestTrue(TEXT("Driver enters existing delivery Sedan"),F.Sedan->GetVehicleInteraction()->Enter(F.Player))) return true;
   F.PlaceCar(F.Sedan,FTransform(F.Delivery->GetActorRotation(),F.Delivery->GetActorLocation()+FVector(0,0,70))); return Next(4);
  }
  if(Stage==4 && Now-Phase>3)
  {
   if(F.Delivery->GetState()!=ENeonCityTransportState::Completed) return false;
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection());
   if(!Data || !Data->Economy.CreditReceipts.Contains(ANeonCityTransportTest::ReceiptKey())) return false;
   Test->TestEqual(TEXT("Delivery completion autosaves credited cash"),Data->Economy.Balance,int64(2250));
   Test->TestTrue(TEXT("Completed delivery state saved"),Data->Phone.Opportunities.Num()==1 && Data->Phone.Opportunities[0].State==2);
   Test->TestTrue(TEXT("Load while parked safely restores the driver on foot"),F.Save->LoadGame());
   Test->TestTrue(TEXT("Character possession restored"),F.PC->GetPawn()==F.Player);
   Test->TestFalse(TEXT("Completed delivery cannot pay again after load"),F.Economy->CreditOnce(350,ANeonCityTransportTest::ReceiptKey()));
   Test->TestTrue(TEXT("Completed action stays unavailable"),!F.Phone->HasAction(ANeonCityTransportTest::ReceiptKey()));
   Baseline=Data->SaveID; return Next(5);
  }
  if(Stage==5 && Now-Phase>.5)
  {
   F.Player->GetHealthComponent()->ApplyPersistenceHealth(0,100);
   Test->TestFalse(TEXT("Unsafe dead player cannot autosave"),F.Save->RequestAutosave(ENeonCityAutosaveReason::SavePoint));
   F.Player->GetHealthComponent()->ApplyPersistenceHealth(100,100);
   F.Economy->AddMoney(10,TEXT("CoalescedAutosave"));
   for(int32 I=0;I<5;++I) Test->TestTrue(TEXT("Repeated safe requests accepted and coalesced"),F.Save->RequestAutosave(ENeonCityAutosaveReason::SavePoint));
   return Next(6);
  }
  if(Stage==6 && Now-Phase>1)
  {
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection());
   Test->TestTrue(TEXT("Cooldown prevents repeated immediate disk writes"),Data && Data->SaveID==Baseline); return Next(7);
  }
  if(Stage==7 && Now-Phase>16)
  {
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection());
   Test->TestTrue(TEXT("One delayed generation contains latest state"),Data && Data->SaveID!=Baseline && Data->Economy.Balance==2260);
   F.Economy->RemoveMoney(100,TEXT("DeferredLoadTest")); F.PC->UnPossess();
   Test->TestTrue(TEXT("Load defers when player is not available"),F.Save->LoadGame());
   Test->TestTrue(TEXT("Deferred result explicitly reported"),F.Save->GetLastResult()==ENeonCitySaveResult::Deferred);
   Test->TestEqual(TEXT("Deferred load has not applied wallet early"),F.Economy->GetBalance(),int64(2160));
   F.PC->Possess(F.Player); return Next(8);
  }
  if(Stage==8 && Now-Phase>.5)
  {
   Test->TestTrue(TEXT("Deferred load completes after player is ready"),F.Save->GetLastResult()==ENeonCitySaveResult::Success && !F.Save->IsOperationInProgress());
   Test->TestEqual(TEXT("Deferred load restores wallet in order"),F.Economy->GetBalance(),int64(2260));
   F.Save->DeleteSave(); Test->TestFalse(TEXT("Deletion prevents later queued autosave recreation"),F.Save->RequestAutosave(ENeonCityAutosaveReason::SavePoint)); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FFixture F; int32 Stage=0; double Started=0,Phase=0; FGuid Baseline;
};

class FCrossSession : public IAutomationLatentCommand
{
public:
 FCrossSession(FAutomationTestBase* InTest,FString InMode):Test(InTest),Mode(MoveTemp(InMode)) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Cross-session timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Init()) return false;
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase13_CrossSession"));
   if(Mode==TEXT("A"))
   { F.Save->DeleteSave(); if(!Test->TestTrue(TEXT("Session A creates new game"),F.Save->CreateNewGameSave())) { Test->AddError(F.Save->GetLastDetail()); return true; } Stage=1; Phase=F.World->GetTimeSeconds(); return false; }
   Test->TestTrue(TEXT("Session B finds Session A disk file"),F.Save->DoesSaveExist());
   TStrongObjectPtr<UNeonCitySaveGame> Expected(F.Save->ReadSaveForInspection()); if(!Test->TestNotNull(TEXT("Session A model readable in new process"),Expected.Get())) return true;
   FString Manifest; FFileHelper::LoadFileToString(Manifest,*(FPaths::ProjectSavedDir()/TEXT("Phase13/SessionA.json")));
   uint32 PreviousPID=0; FParse::Value(*Manifest,TEXT("\"processId\":"),PreviousPID);
   Test->TestTrue(TEXT("Session B runs in a different operating-system process"),PreviousPID!=0 && PreviousPID!=FPlatformProcess::GetCurrentProcessId());
   Test->TestTrue(TEXT("Session B reads the exact Session A save identity"),Manifest.Contains(Expected->SaveID.ToString()));
   F.Mutate(); if(!Test->TestTrue(TEXT("Session B loads disk state"),F.Save->LoadGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   F.Verify(Test,Expected.Get()); WriteManifest(TEXT("SessionB.json"),Expected.Get());
   F.Save->DeleteSave(); Test->TestFalse(TEXT("Cross-session fixture removed after successful verification"),F.Save->DoesSaveExist()); return true;
  }
  if(Stage==1 && F.World->GetTimeSeconds()-Phase>1) { F.Seed(Test); Stage=2; Phase=F.World->GetTimeSeconds(); return false; }
  if(Stage==2 && F.World->GetTimeSeconds()-Phase>1)
  {
   if(!Test->TestTrue(TEXT("Session A saves modified world to disk"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection()); if(!Data.IsValid()) return true;
   F.Verify(Test,Data.Get()); WriteManifest(TEXT("SessionA.json"),Data.Get()); return true;
  }
  return false;
 }
 void WriteManifest(const TCHAR* Name,UNeonCitySaveGame* Data)
 {
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Phase13"); IFileManager::Get().MakeDirectory(*Folder,true);
  const FString Text=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"money\":%lld,\"ownedVehicles\":%d,\"sedanId\":\"%s\",\"sportsId\":\"%s\",\"health\":%.2f,\"stamina\":%.2f,\"timeOfDay\":%.2f,\"propertyId\":\"%s\",\"deliveryState\":%d}"),FPlatformProcess::GetCurrentProcessId(),*Data->SaveID.ToString(),F.Economy->GetBalance(),F.Vehicles->GetOwnedCount(),*F.Sedan->VehicleID.ToString(),*F.Sports->VehicleID.ToString(),F.Player->GetHealthComponent()->GetCurrentHealth(),F.Player->GetPlayerDataComponent()->GetStamina(),F.Clock->GetTimeOfDay(),*F.PropertyID.ToString(),int32(F.Delivery->GetState()));
  Test->TestTrue(TEXT("Cross-session evidence written"),FFileHelper::SaveStringToFile(Text,*(Folder/Name)));
 }
 FAutomationTestBase* Test; FString Mode; FFixture F; int32 Stage=0; double Started=0; float Phase=0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistenceRoundTrip,"NeonCity.Persistence.RoundTripPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPersistenceRoundTrip::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPersistenceTests::FRoundTrip>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistenceRecovery,"NeonCity.Persistence.RecoveryPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPersistenceRecovery::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPersistenceTests::FRecovery>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistenceAutosave,"NeonCity.Persistence.AutosavePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPersistenceAutosave::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPersistenceTests::FAutosave>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPersistenceCrossSession,"NeonCity.Persistence.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FPersistenceCrossSession::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityPersistenceSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode); Commands.Add(Mode); }
}
bool FPersistenceCrossSession::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPersistenceTests::FCrossSession>(this,Parameters));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
