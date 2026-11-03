#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleInteraction.h"
#include "Property/PropertySubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace MainlandSave920
{
const FName StarterID(TEXT("NeonCity.Property.StarterApartment"));
const FName SpiderProfile(TEXT("DA_HeroNacre"));
const FVector ReplacementBay(3799.5,-830.,55.);
// Explicit synthetic legacy garage fixture, outside the new sample geometry.
// It is derived from an actual production checkpoint, not a played parking run.
const FVector LegacyBay(-91350.,38000.,1040.);
constexpr float FixtureCondition=.46f,FixtureRoof=.62f;

bool RequiredSlot(FAutomationTestBase& Test,FString& Slot)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation"))
  || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))
  || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityC920SaveSlot="),Slot)
  || !Slot.StartsWith(TEXT("NeonCity_Test_C920_"),ESearchCase::CaseSensitive)
  || Slot.Len()<=FCString::Strlen(TEXT("NeonCity_Test_C920_")) || Slot.Len()>64)
 {
  Test.AddError(TEXT("Required: isolated standalone, disabled automatic persistence, and a unique -NeonCityC920SaveSlot=NeonCity_Test_C920_<run>; no default slot is permitted."));
  return false;
 }
 for(TCHAR C:Slot)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))
 {Test.AddError(TEXT("The dedicated C920 slot must contain only letters, digits, underscore or hyphen."));return false;}
 if(Slot.EndsWith(TEXT("_Backup")) || Slot.EndsWith(TEXT("_Pending")))
 {Test.AddError(TEXT("Reserved save generation suffix cannot be a test slot."));return false;}
 return true;
}

FString Folder(const FString& Slot)
{return FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC920/SaveCompatibility")/Slot;}

bool DiskHash(const FString& Slot,FString& Hash)
{
 TArray<uint8> Bytes;
 if(!UGameplayStatics::LoadDataFromSlot(Bytes,Slot,0) || Bytes.IsEmpty())return false;
 Hash=FMD5::HashBytes(Bytes.GetData(),Bytes.Num());return true;
}

bool ParkingPolicyHash(FString& Hash)
{
 TArray<uint8> Bytes;
 if(!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/TEXT("Source/NeonCity/Property/PropertySubsystem.cpp")))
  || Bytes.IsEmpty())return false;
 Hash=FMD5::HashBytes(Bytes.GetData(),Bytes.Num());return true;
}

bool StaticSupport(UWorld& World,const FTransform& Pose,bool RequireAll)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(C920SaveActualBaySupport),false);
 for(TActorIterator<APawn> I(&World);I;++I)Query.AddIgnoredActor(*I);
 const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
 int32 Supported=0;
 for(const FVector Offset:{FVector::ZeroVector,FVector(180,80,0),FVector(180,-80,0),FVector(-180,80,0),FVector(-180,-80,0)})
 {
  FHitResult Hit;const FVector Point=Pose.TransformPosition(Offset);
  if(World.LineTraceSingleByObjectType(Hit,Point+FVector(0,0,50),Point-FVector(0,0,500),Objects,Query)
   && Hit.ImpactNormal.Z>.7 && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics())++Supported;
 }
 return RequireAll?Supported==5:Supported>0;
}

class FSaveFlow : public IAutomationLatentCommand
{
public:
 FSaveFlow(FAutomationTestBase* InTest,FString InSlot,bool InRead)
  :Test(InTest),Slot(MoveTemp(InSlot)),bRead(InRead),Started(FPlatformTime::Seconds()){}

 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>90)return Finish(TEXT("C920 isolated save validation exceeded its 90 second bound."));
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!W || !W->HasBegunPlay() || !P || W->GetTimeSeconds()<8)return false;
  if(Stage==0)
  {
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   Vehicles=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
   Properties=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
   Economy=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
   const auto* Site=ANeonCityMainlandSite::Find(W);
   if(!Save || !Vehicles || !Properties || !Economy || !Site || !Site->bRecoverUnsupportedMainlandVehicles)
    return Finish(TEXT("Actual adopted C920 map, opt-in recovery marker and production registries are required."));
   if(!ParkingPolicyHash(PolicyHash))return Finish(TEXT("Actual parking policy source is required for immutable reader validation provenance."));
   // This is the first and only ConfigureSlot: the user/default slot is never read or written.
   if(!Save->ConfigureSlot(Slot,false) || Save->GetSlotName()!=Slot)return Finish(TEXT("Dedicated test slot configuration failed."));
   FNeonCityPropertyRecord Starter;
   if(!Properties->GetProperty(StarterID,Starter) || Starter.GarageSlots.Num()!=1
    || Starter.GarageSlots[0].SlotIndex!=0 || !Starter.GarageSlots[0].Location.Equals(ReplacementBay,.1))
    return Finish(TEXT("Starter property does not expose the actual finished replacement interior bay."));
   if(!StaticSupport(*W,FTransform(FRotator(0,90,0),ReplacementBay),true)
    || StaticSupport(*W,FTransform(FRotator(0,31,0),LegacyBay),false))
    return Finish(TEXT("Required supported current bay / unsupported retired fixture geometry was not observed."));
   if(bRead)return BeginRead();
   return WriteFixture();
  }
  if(Stage==1)
  {
   const double Age=FPlatformTime::Seconds()-LoadedAt;
   auto* Car=Cast<AIslandSupercar>(Vehicles->FindLiveVehicle(Expected.VehicleID));
   if(!Car)return Finish(TEXT("Restored original vehicle disappeared during normal physics settling."));
   ObserveParking(*Car,Age);
   if(Age<2)return false;
   VerifyCar(*Car,false);
   FNeonCityPropertyRecord Starter;
   const bool Found=Properties->GetProperty(StarterID,Starter);
   auto* Parked=Properties->FindParkedVehicle(StarterID,0);
   Test->TestTrue(TEXT("Starter property remains registered after ordinary physics"),Found);
   Test->TestTrue(TEXT("Starter property ownership survives ordinary physics"),Found && Starter.Owned);
   Test->TestEqual(TEXT("Starter property retains exactly one garage slot"),Starter.GarageSlots.Num(),1);
   Test->TestTrue(TEXT("Original garage vehicle link survives ordinary physics and reconciliation"),
    Found && Starter.GarageSlots.Num()==1 && Starter.GarageSlots[0].VehicleID==Expected.VehicleID);
   Test->TestTrue(TEXT("Production garage finder returns the restored original car"),Parked==Car);
   ParkingDiagnostics->SetStringField(TEXT("finder_actor"),GetPathNameSafe(Parked));
   ParkingDiagnostics->SetBoolField(TEXT("finder_is_expected_car"),Parked==Car);
   Test->TestTrue(TEXT("Real restored chassis remains over its supported authored bay"),
    FVector::Dist2D(Car->GetActorLocation(),ReplacementBay)<100 && FMath::Abs(Car->GetActorLocation().Z-ReplacementBay.Z)<140
    && StaticSupport(*W,Car->GetActorTransform(),true));
   Test->TestTrue(TEXT("Mid-cycle roof resumes toward its saved target rather than resetting"),
    Car->GetRoofMechanism()->IsTargetOpen() && Car->GetRoofMechanism()->GetProgress()>Expected.RoofProgress
    && Car->GetRoofMechanism()->GetProgress()<=1);
   Test->TestTrue(TEXT("Load keeps the production player visible, collidable and on foot"),
    PC->GetPawn()==P && !P->IsHidden() && P->GetActorEnableCollision() && P->GetCharacterMovement()->IsMovingOnGround());
   FString CurrentHash;
   Test->TestTrue(TEXT("Reader never rewrites the source generation"),DiskHash(Slot,CurrentHash) && CurrentHash==SourceHash);
   return Finish(TEXT(""));
  }
  return false;
 }

private:
 void ObserveParking(AIslandSupercar& Car,double Age)
 {
  // Observe every normal tick: a transient suspension velocity may release the
  // slot long before the final settled location/finder assertions are evaluated.
  // This records production state only; it never reconciles or changes the car.
  FNeonCityPropertyRecord Record;
  const bool Found=Properties->GetProperty(StarterID,Record);
  const bool Linked=Found && Record.GarageSlots.Num()==1 && Record.GarageSlots[0].VehicleID==Expected.VehicleID;
  const FVector Velocity=Car.GetVelocity();
  PeakSpeed=FMath::Max(PeakSpeed,Velocity.Size());
  auto Sample=MakeShared<FJsonObject>();
  Sample->SetNumberField(TEXT("age_wall_seconds"),Age);
  Sample->SetNumberField(TEXT("world_seconds"),W->GetTimeSeconds());
  Sample->SetBoolField(TEXT("property_found"),Found);
  Sample->SetBoolField(TEXT("property_owned"),Found && Record.Owned);
  Sample->SetNumberField(TEXT("slot_count"),Record.GarageSlots.Num());
  Sample->SetBoolField(TEXT("slot_linked"),Linked);
  Sample->SetStringField(TEXT("slot_vehicle_id"),Record.GarageSlots.Num()==1?Record.GarageSlots[0].VehicleID.ToString():FString());
  Sample->SetStringField(TEXT("car_actor"),Car.GetPathName());
  Sample->SetStringField(TEXT("location_cm"),Car.GetActorLocation().ToString());
  Sample->SetStringField(TEXT("actor_velocity_cm_s"),Velocity.ToString());
  Sample->SetStringField(TEXT("physics_root_velocity_cm_s"),Car.GetMesh()->GetPhysicsLinearVelocity(TEXT("Root")).ToString());
  Sample->SetNumberField(TEXT("speed_cm_s"),Velocity.Size());
  Sample->SetNumberField(TEXT("distance_2d_cm"),FVector::Dist2D(Car.GetActorLocation(),ReplacementBay));
  Sample->SetNumberField(TEXT("height_delta_cm"),Car.GetActorLocation().Z-ReplacementBay.Z);
  Sample->SetBoolField(TEXT("authority"),Car.HasAuthority());
  Sample->SetBoolField(TEXT("same_world"),Car.GetWorld()==Properties->GetWorld());
  Sample->SetBoolField(TEXT("controller_absent"),!Car.GetController());
  Sample->SetBoolField(TEXT("driver_absent"),!Car.GetVehicleInteraction()->GetDriver());
  Sample->SetBoolField(TEXT("health_alive"),!Car.GetHealthComponent()->IsDead());
  if(!ParkingDiagnostics.IsValid())ParkingDiagnostics=MakeShared<FJsonObject>();
  ParkingDiagnostics->SetObjectField(TEXT("final_observation"),Sample);
  ParkingDiagnostics->SetNumberField(TEXT("peak_actor_speed_cm_s"),PeakSpeed);
  if(!Linked && !bObservedLinkLoss)
  {
   bObservedLinkLoss=true;ParkingDiagnostics->SetObjectField(TEXT("first_link_loss"),Sample);
   UE_LOG(LogTemp,Display,TEXT("C920 garage first link loss: age %.4f world %.4f found %d owned %d slots %d velocity %s location %s controller %s driver %s alive %d"),
    Age,W->GetTimeSeconds(),Found,Found && Record.Owned,Record.GarageSlots.Num(),*Velocity.ToString(),*Car.GetActorLocation().ToString(),
    *GetNameSafe(Car.GetController()),*GetNameSafe(Car.GetVehicleInteraction()->GetDriver()),!Car.GetHealthComponent()->IsDead());
  }
  if(Linked)ParkingDiagnostics->SetObjectField(TEXT("last_linked_observation"),Sample);
 }

 bool WriteFixture()
 {
  if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Folder(Slot)/TEXT("SaveWrite.json"))))
   return Finish(TEXT("SaveWrite requires a fresh unique run slot; existing evidence is preserved."));
  bReceiptMayWrite=true;
  if(!P->GetCharacterMovement()->IsMovingOnGround())return Finish(TEXT("Production checkpoint requires the normal grounded player."));
  // Actual production collection, serialization, checksum and detached readback first.
  if(!Save->SaveGame() || Save->GetLastResult()!=ENeonCitySaveResult::Success)
   return Finish(TEXT("Production SaveGame failed: ")+Save->GetLastDetail());
  Fixture.Reset(Save->ReadSaveForInspection());
  if(!Fixture.IsValid() || !Fixture->HasVehicles || !Fixture->HasProperties || !Fixture->HasEconomy)
   return Finish(TEXT("Detached integrity-checked production checkpoint is incomplete."));
  ProductionSaveID=Fixture->SaveID;
  const auto* Site=ANeonCityMainlandSite::Find(W);
  if(!Site)return Finish(TEXT("Authored mainland fleet provenance is missing."));
  AIslandSupercar* Spider=nullptr;int32 Count=0;
  for(TActorIterator<AIslandSupercar> I(W);I;++I)
   if(I->GetLevel()==W->PersistentLevel && I->GetVehicleProfile() && I->GetVehicleProfile()->GetFName()==SpiderProfile
    && !Site->FleetProfileReplacements.ContainsByPredicate([&](const auto& Entry){return Entry.VehicleID==I->VehicleID;}))
   {Spider=*I;++Count;}
  if(Count!=1 || !Spider || !Spider->bRetractable || !Spider->GetRoofMechanism()->IsEnabled())
   return Finish(TEXT("Exactly one unchanged original mainland Nacre, excluding explicitly replaced fleet IDs, is required."));
  auto* Vehicle=Fixture->Vehicles.FindByPredicate([&](const auto& V){return V.VehicleID==Spider->VehicleID;});
  auto* Property=Fixture->Properties.FindByPredicate([](const auto& V){return V.PropertyID==StarterID;});
  if(!Vehicle || !Property || Property->GarageSlots.Num()!=1 || Vehicle->DataAssetID!=SpiderProfile)
   return Finish(TEXT("Production checkpoint lost its authored car identity or starter property."));
  // Fault injection edits this detached model only; no live car/roof/wallet poses are written.
  for(auto& SavedProperty:Fixture->Properties)for(auto& Garage:SavedProperty.GarageSlots)
   if(Garage.VehicleID==Vehicle->VehicleID)Garage.VehicleID.Invalidate();
  Property->Owned=true;auto& Garage=Property->GarageSlots[0];
  Garage.SlotIndex=0;Garage.VehicleID=Vehicle->VehicleID;Garage.StoredLocation=LegacyBay;
  Vehicle->Owned=true;Vehicle->StoredPropertyID=StarterID;Vehicle->GarageSlot=0;
  Vehicle->Transform=FTransform(FRotator(0,31,0),LegacyBay);
  Vehicle->Condition=FixtureCondition;Vehicle->RoofProgress=FixtureRoof;Vehicle->RoofTargetOpen=true;
  Vehicle->LegacyFleetStored=false;
  Fixture->SaveID=FGuid::NewGuid();Expected=*Vehicle;
  FString Error;
  if(!Fixture->ValidateEnvelope(Error) || !Save->WriteTestFixture(Fixture.Get()))
   return Finish(TEXT("Explicit isolated legacy fixture write failed: ")+Error);
  const FGuid FixtureID=Fixture->SaveID;
  Fixture.Reset(Save->ReadSaveForInspection());
  if(!Fixture.IsValid() || Fixture->SaveID!=FixtureID || !ReadExpected() || !DiskHash(Slot,SourceHash))
   return Finish(TEXT("Legacy fixture failed detached disk readback."));
  Test->TestTrue(TEXT("Fixture keeps the actual original mainland Nacre GUID and profile"),
   Expected.VehicleID==Spider->VehicleID && Expected.DataAssetID==Spider->GetVehicleProfile()->GetFName());
  Test->TestTrue(TEXT("Fault injection does not move or damage the live car"),
   FVector::Dist2D(Spider->GetActorLocation(),LegacyBay)>10000);
  return Finish(TEXT(""));
 }

 bool BeginRead()
 {
  if(IFileManager::Get().FileExists(*(Folder(Slot)/TEXT("SaveRead.json"))))
  {
   FString PreviousText,PreviousPolicyHash;TSharedPtr<FJsonObject> Previous;bool PreviousPass=false;
   if(!FFileHelper::LoadFileToString(PreviousText,*(Folder(Slot)/TEXT("SaveRead.json")))
    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PreviousText),Previous)
    || !Previous.IsValid() || !Previous->TryGetBoolField(TEXT("passed"),PreviousPass))
    return Finish(TEXT("Unreadable reader evidence is preserved."));
   if(PreviousPass && (!Previous->TryGetStringField(TEXT("parking_policy_source_md5"),PreviousPolicyHash)
     || PreviousPolicyHash.IsEmpty() || PreviousPolicyHash==PolicyHash))
    return Finish(TEXT("Prior successful unchanged/unversioned reader evidence is preserved; only a known changed parking policy may invalidate cached PASS."));
   // A focused reader retry after a code repair may reuse the immutable writer.
   // Preserve the failed receipt before recording the new reader process.
   if(!FFileHelper::SaveStringToFile(PreviousText,*(Folder(Slot)/
      FString::Printf(TEXT("SaveRead-%sBefore-%u.json"),PreviousPass?TEXT("Pass"):TEXT("Failed"),FPlatformProcess::GetCurrentProcessId()))))
    return Finish(TEXT("Could not preserve the prior reader evidence."));
  }
  bReceiptMayWrite=true;
  if(!Save->DoesSaveExist())return Finish(TEXT("SaveRead requires the completed SaveWrite slot from an earlier OS process."));
  FString Text;TSharedPtr<FJsonObject> Receipt;
  if(!FFileHelper::LoadFileToString(Text,*(Folder(Slot)/TEXT("SaveWrite.json")))
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Receipt) || !Receipt.IsValid())
   return Finish(TEXT("Prior writer receipt is absent or unreadable."));
  bool Passed=false;FString RecordedSlot,SaveID,VehicleID,RecordedHash;double PID=0;
  if(!Receipt->TryGetBoolField(TEXT("passed"),Passed) || !Passed
   || !Receipt->TryGetStringField(TEXT("slot"),RecordedSlot) || RecordedSlot!=Slot
   || !Receipt->TryGetNumberField(TEXT("process_id"),PID) || PID<=0 || uint32(PID)==FPlatformProcess::GetCurrentProcessId()
   || !Receipt->TryGetStringField(TEXT("save_id"),SaveID)
   || !Receipt->TryGetStringField(TEXT("vehicle_id"),VehicleID)
   || !Receipt->TryGetStringField(TEXT("source_md5"),RecordedHash))
   return Finish(TEXT("Reader requires a successful exact-slot writer from a different OS process."));
  WriterPID=uint32(PID);
  Fixture.Reset(Save->ReadSaveForInspection());
  if(!Fixture.IsValid() || Fixture->SaveID.ToString()!=SaveID || !ReadExpected()
   || Expected.VehicleID.ToString()!=VehicleID || !DiskHash(Slot,SourceHash) || SourceHash!=RecordedHash)
   return Finish(TEXT("Reader did not reopen the exact validated writer fixture generation."));
  if(!Save->LoadGame() || Save->GetLastResult()!=ENeonCitySaveResult::Success)
   return Finish(TEXT("Production LoadGame failed: ")+Save->GetLastDetail());
  RestoreDetail=Save->GetLastDetail();
  Test->TestTrue(TEXT("Production coordinator explicitly reports the supported garage relocation"),
   RestoreDetail.Contains(TEXT("Restored 1 owned mainland garage vehicle(s) to their supported authored replacement bays.")));
  auto* Car=Cast<AIslandSupercar>(Vehicles->FindLiveVehicle(Expected.VehicleID));
  if(!Car)return Finish(TEXT("Production load did not restore the original live retractable-roof car."));
  VerifyCar(*Car,true);VerifyOtherDomains();
  FNeonCityPropertyRecord Starter;
  Test->TestTrue(TEXT("Original starter property ownership and slot0 link survive coordinator migration"),
   Properties->GetProperty(StarterID,Starter) && Starter.Owned && Starter.GarageSlots.Num()==1
   && Starter.GarageSlots[0].VehicleID==Expected.VehicleID && Starter.GarageSlots[0].Location.Equals(ReplacementBay,.1));
  Test->TestTrue(TEXT("Real restored car is placed at the authored bay before any physics tick"),
   Car->GetActorLocation().Equals(ReplacementBay,.1)
   && FMath::Abs(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,90))<.01);
  const auto LiveValues=Vehicles->BuildPersistenceSnapshot();
  const auto* Migrated=LiveValues.FindByPredicate([&](const auto& V){return V.VehicleID==Expected.VehicleID;});
  Test->TestTrue(TEXT("Vehicle registry retains owned identity, profile, condition and live deployment"),
   Migrated && Migrated->Owned && !Migrated->LegacyFleetStored && Migrated->DataAssetID==Expected.DataAssetID
   && Migrated->VehicleType==Expected.VehicleType && FMath::IsNearlyEqual(Migrated->Condition,Expected.Condition,.0001f));
  LoadedAt=FPlatformTime::Seconds();Stage=1;return false;
 }

 bool ReadExpected()
 {
  const auto* Property=Fixture->Properties.FindByPredicate([](const auto& V){return V.PropertyID==StarterID;});
  if(!Property || !Property->Owned || Property->GarageSlots.Num()!=1)return false;
  const auto& Garage=Property->GarageSlots[0];
  const auto* Vehicle=Fixture->Vehicles.FindByPredicate([&](const auto& V){return V.VehicleID==Garage.VehicleID;});
  if(!Vehicle || !Vehicle->Owned || Vehicle->DataAssetID!=SpiderProfile || Vehicle->StoredPropertyID!=StarterID
   || Vehicle->GarageSlot!=0 || Garage.SlotIndex!=0 || !Garage.StoredLocation.Equals(LegacyBay,.1)
   || !Vehicle->Transform.GetLocation().Equals(LegacyBay,.1) || Vehicle->LegacyFleetStored
   || !FMath::IsNearlyEqual(Vehicle->Condition,FixtureCondition,.0001f)
   || !FMath::IsNearlyEqual(Vehicle->RoofProgress,FixtureRoof,.0001f) || !Vehicle->RoofTargetOpen)return false;
  Expected=*Vehicle;return true;
 }

 void VerifyCar(AIslandSupercar& Car,bool ExactRoof)
 {
  Test->TestEqual(TEXT("Original real car GUID retained"),Car.VehicleID,Expected.VehicleID);
  Test->TestTrue(TEXT("Original profile and roof mechanism retained"),Car.GetVehicleProfile()
   && Car.GetVehicleProfile()->GetFName()==Expected.DataAssetID && Car.bRetractable && Car.GetRoofMechanism()->IsEnabled());
  Test->TestTrue(TEXT("Real car condition is retained after restore"),
   FMath::IsNearlyEqual(Car.GetDamageState()->GetConditionFraction(),Expected.Condition,.0001f));
  Test->TestTrue(TEXT("Original vehicle remains owned and present"),Vehicles->IsOwned(Expected.VehicleID)
   && !Vehicles->IsLegacyFleetStored(Expected.VehicleID) && Vehicles->FindLiveVehicle(Expected.VehicleID)==&Car);
  int32 Copies=0;for(TActorIterator<ANeonCityBaseVehicle> I(W);I;++I)if(I->VehicleID==Expected.VehicleID)++Copies;
  Test->TestEqual(TEXT("Exactly one live actor has the original vehicle identity"),Copies,1);
  if(ExactRoof)Test->TestTrue(TEXT("Exact saved mid-cycle roof progress and target apply before a tick"),
   FMath::IsNearlyEqual(Car.GetRoofMechanism()->GetProgress(),Expected.RoofProgress,.0001f)
   && Car.GetRoofMechanism()->IsTargetOpen()==Expected.RoofTargetOpen
   && Car.GetRoofMechanism()->IsCycling() && Car.GetRoofMechanism()->IsComponentTickEnabled());
 }

 void VerifyOtherDomains()
 {
  const auto Current=Economy->BuildPersistenceSnapshot();const auto& Saved=Fixture->Economy;
  Test->TestEqual(TEXT("Unmodified cash survives the fixture migration"),Current.Balance,Saved.Balance);
  Test->TestEqual(TEXT("Unmodified bank balance survives the fixture migration"),Current.BankBalance,Saved.BankBalance);
  Test->TestTrue(TEXT("Unmodified active loan survives the fixture migration"),
   FNeonCityLoanData::StaticStruct()->CompareScriptStruct(&Current.ActiveLoan,&Saved.ActiveLoan,0));
  Test->TestEqual(TEXT("Unmodified transaction history count retained"),Current.Transactions.Num(),Saved.Transactions.Num());
  for(int32 I=0;I<FMath::Min(Current.Transactions.Num(),Saved.Transactions.Num());++I)
   Test->TestTrue(TEXT("Unmodified transaction values retained"),
    FTransactionData::StaticStruct()->CompareScriptStruct(&Current.Transactions[I],&Saved.Transactions[I],0));
  for(FName ID:Saved.CreditReceipts)Test->TestTrue(TEXT("Original one-time reward receipts retained"),Economy->HasCredit(ID));
  Test->TestTrue(TEXT("Unmodified player health and stamina retained"),
   FMath::IsNearlyEqual(P->GetHealthComponent()->GetCurrentHealth(),Fixture->Player.Health,.001f)
   && FMath::IsNearlyEqual(P->GetPlayerDataComponent()->GetStamina(),Fixture->Player.Stamina,.001f));
  Test->TestTrue(TEXT("Supported production player checkpoint remains unchanged"),
   P->GetActorTransform().Equals(Fixture->Player.Transform,.1));
  for(const auto& Property:Fixture->Properties)
   Test->TestEqual(TEXT("Other property ownership is unaffected"),Properties->IsOwned(Property.PropertyID),Property.Owned);
 }

 bool Finish(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  if(!bReceiptMayWrite)return true;
  auto J=MakeShared<FJsonObject>();
  J->SetBoolField(TEXT("passed"),Error.IsEmpty() && !Test->HasAnyErrors());
  J->SetStringField(TEXT("failure"),Error);J->SetStringField(TEXT("slot"),Slot);
  J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
  J->SetNumberField(TEXT("writer_process_id"),WriterPID);
  J->SetBoolField(TEXT("independent_reader_process"),bRead && WriterPID!=0 && WriterPID!=FPlatformProcess::GetCurrentProcessId());
  J->SetStringField(TEXT("save_id"),Fixture.IsValid()?Fixture->SaveID.ToString():FString());
  J->SetStringField(TEXT("production_checkpoint_id"),ProductionSaveID.ToString());
  J->SetStringField(TEXT("vehicle_id"),Expected.VehicleID.ToString());
  J->SetStringField(TEXT("profile"),Expected.DataAssetID.ToString());J->SetStringField(TEXT("source_md5"),SourceHash);
  J->SetStringField(TEXT("restore_detail"),RestoreDetail);
  J->SetStringField(TEXT("parking_policy_source_md5"),PolicyHash);
  if(ParkingDiagnostics.IsValid())J->SetObjectField(TEXT("parking_diagnostics"),ParkingDiagnostics);
  J->SetStringField(TEXT("scope"),TEXT("Production SaveGame and checksum-validated detached readback, followed by an explicit synthetic old-mainland linked-garage fixture. Separate-process production LoadGame verifies real car GUID/profile/condition/roof, ownership, slot link and supported replacement parking; cash/bank/loan/history/player state checked unchanged. This is migration validation, not evidence of a player-driven parking route."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  IFileManager::Get().MakeDirectory(*Folder(Slot),true);
  Test->TestTrue(TEXT("Isolated cross-process evidence receipt written"),
   FFileHelper::SaveStringToFile(Text,*(Folder(Slot)/(bRead?TEXT("SaveRead.json"):TEXT("SaveWrite.json")))));
  return true;
 }

 FAutomationTestBase* Test;FString Slot;bool bRead,bReceiptMayWrite=false;double Started=0,LoadedAt=0;int32 Stage=0;uint32 WriterPID=0;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;UVehicleOwnershipSubsystem* Vehicles=nullptr;
 UPropertySubsystem* Properties=nullptr;UNeonCityEconomySubsystem* Economy=nullptr;
 TStrongObjectPtr<UNeonCitySaveGame> Fixture;FNeonCityVehicleSaveData Expected;
 FGuid ProductionSaveID;FString SourceHash,RestoreDetail,PolicyHash;
 TSharedPtr<FJsonObject> ParkingDiagnostics;double PeakSpeed=0.;bool bObservedLinkLoss=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandSaveWrite920,
 "NeonCity.Phase205c.MainlandC920.Standalone.SaveWrite",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandSaveWrite920::RunTest(const FString&)
{
 FString Slot;if(!MainlandSave920::RequiredSlot(*this,Slot))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSave920::FSaveFlow(this,Slot,false));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandSaveRead920,
 "NeonCity.Phase205c.MainlandC920.Standalone.SaveRead",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandSaveRead920::RunTest(const FString&)
{
 FString Slot;if(!MainlandSave920::RequiredSlot(*this,Slot))return false;
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSave920::FSaveFlow(this,Slot,true));return true;
}
#endif
