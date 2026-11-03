#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "HAL/PlatformProcess.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace GarageStoredNative
{
// 244 read-only audit proves this genuine F6 contains one stored old Kestrel
// and one already deployed old Aster. Never force a player's live car to storage.
static const FString OriginalSlot=TEXT("NeonCity_Test_Aquarium02");
static TArray<FGuid> DefaultIDs()
{
 return {FGuid(0x5FF78036,0x45833C8D,0x593E77A5,0x0B006245),
  FGuid(0x7F242340,0x469FEA4D,0x1A7D21A5,0xD5DED892),FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8)};
}
static TArray<FGuid> LegacyIDs()
{
 return {FGuid(0x3071B435,0x4B42495A,0x92AE5FA3,0x467F35A0),FGuid(0x8BD22EB3,0x4C404BB6,0x7EF8ECAC,0x243B9DB9)};
}

/** Minimum actual retrieval/save/cold-restore reproduction. The starting
 * owned collection comes from retained normal F6 data, never a save fixture. */
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* InTest,bool Cold):Test(InTest),bCold(Cold),Evidence(Cold?TEXT("GarageStoredCold"):TEXT("GarageStoredSave")) {}

 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>100)return End(TEXT("Stored-car minimum native sequence timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityGarageStoredRun="),Suffix);
   Suffix=FPaths::MakeValidFileName(Suffix.IsEmpty()?TEXT("02"):Suffix);
   NewSlot=TEXT("NeonCity_Test_GarageStored")+Suffix;
   SourceDir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("GarageStored")+Suffix);
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   Vehicles=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
   Properties=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
   for(TActorIterator<AIslandGarageControl> I(W);I;++I)
    if(I->Action==EIslandGarageAction::RetrieveStored && I->PropertyID==TEXT("Property.Island.Aurelia"))
    {if(Control)return End(TEXT("Ambiguous native Retrieve control"));Control=*I;}
   if(!Control || !Save || !Vehicles || !Properties || !UGameplayStatics::LoadDataFromSlot(OriginalBytes,OriginalSlot,0))
    return End(TEXT("Retained genuine roof checkpoint or real garage control unavailable"));
   if(!Save->ConfigureSlot(NewSlot,false))return End(TEXT("Scoped stored-car slot unavailable"));
   if(bCold)
   {
    FString Text;TSharedPtr<FJsonObject> Prior;
    if(!Save->DoesSaveExist() || !FFileHelper::LoadFileToString(Text,*(SourceDir/TEXT("source-receipt.json")))
     || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Prior)
     || !Prior->GetBoolField(TEXT("passed")) || Prior->GetStringField(TEXT("slot"))!=NewSlot)
     return End(TEXT("Genuine successful A retrieval checkpoint and receipt required"));
    SourcePID=uint32(Prior->GetNumberField(TEXT("process_id")));
    if(SourcePID==FPlatformProcess::GetCurrentProcessId())return End(TEXT("B must run in a genuinely different OS process from A"));
    if(!FGuid::Parse(Prior->GetStringField(TEXT("deployed_id")),ChosenID)
     || !FGuid::Parse(Prior->GetStringField(TEXT("other_retained_deployed_id")),OtherID)
     || !UGameplayStatics::LoadDataFromSlot(ColdBytes,NewSlot,0))return End(TEXT("A checkpoint identity/readback missing"));
    const auto* Model=Save->ReadSaveForInspection();if(!Model)return End(TEXT("A save cannot be read without injection"));
    Baseline=Model->Vehicles;SavedPlayer=Model->Player.Transform;
    const auto* Chosen=Baseline.FindByPredicate([&](const auto& D){return D.VehicleID==ChosenID;});
    if(!Chosen || !Chosen->Owned || Chosen->LegacyFleetStored)return End(TEXT("A did not save an actual deployed owned car"));
    Bay=Chosen->Transform;Key(EKeys::F7,true);return Next(1);
   }
   if(Save->DoesSaveExist() || IFileManager::Get().DirectoryExists(*SourceDir))
    return End(TEXT("Preserve existing A evidence; select a fresh GarageStoredRun suffix"));
   if(!Save->ConfigureSlot(OriginalSlot,false) || !Save->DoesSaveExist())return End(TEXT("Original genuine F6 slot missing"));
   Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>.25){Key(EKeys::F7,false);return Next(2);}
  if(Stage==2 && Age>.6)
  {
   if(Save->IsOperationInProgress())return false;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || P->IsHidden() || PC->GetPawn()!=P
    || !P->GetActorEnableCollision() || !P->GetCharacterMovement()->IsMovingOnGround())
    return End(TEXT("Genuine F7 did not restore normal on-foot owned collection"));
   if(!Properties->GetProperty(Control->PropertyID,PropertyBefore) || !PropertyBefore.Owned
    || PropertyBefore.GarageSlots.Num()!=2 || !PropertyBefore.GarageSlots.IsValidIndex(Control->SlotIndex))
    return End(TEXT("Existing two owned property anchors unavailable"));
   if(bCold)
   {
    if(FVector::Distance(P->GetActorLocation(),SavedPlayer.GetLocation())>10 || !Check(true))
     return End(Failure.IsEmpty()?TEXT("Cold F7 moved saved player approach"):Failure);
    const auto* Chosen=Baseline.FindByPredicate([&](const auto& D){return D.VehicleID==ChosenID;});
    if(!Chosen || FVector::Dist2D(Vehicles->FindLiveVehicle(ChosenID)->GetActorLocation(),Chosen->Transform.GetLocation())>30)
     return End(TEXT("Genuine F7 moved deployed old car from its saved bay position"));
    Bay=Control->RetrievalBayLocal*Control->GetActorTransform();
    Bay.SetLocation(PropertyBefore.GarageSlots[Control->SlotIndex].Location);
    Journal(TEXT("genuine_cold_F7"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageStored-Cold-Genuine-F7.png"));
    return Next(5);
   }
   Baseline=Vehicles->BuildPersistenceSnapshot();
   ChosenID=Control->FindStoredOwnedVehicle();
   if(ChosenID!=LegacyIDs()[0] || !Vehicles->IsOwned(ChosenID)
    || !Vehicles->IsLegacyFleetStored(ChosenID) || Vehicles->FindLiveVehicle(ChosenID))
    return End(TEXT("Real Retrieve selection must match the genuinely stored old Kestrel in audited Aquarium02"));
   for(const auto ID:LegacyIDs())if(ID!=ChosenID)OtherID=ID;
   if(!Check(false))return End(Failure);
   Bay=Control->RetrievalBayLocal*Control->GetActorTransform();
   Bay.SetLocation(PropertyBefore.GarageSlots[Control->SlotIndex].Location);
   FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageStoredActualEmptyBay),false);
   TArray<FOverlapResult> Blocks;W->OverlapMultiByChannel(Blocks,Bay.GetLocation()+FVector(0,0,60),Bay.GetRotation(),
    ECC_Pawn,FCollisionShape::MakeBox(FVector(255,115,58)),Q);
   for(const auto& B:Blocks)if(B.bBlockingHit)BayBlockers+=GetNameSafe(B.GetActor())+TEXT("/")+GetNameSafe(B.GetComponent())+TEXT("|");
   if(!BayBlockers.IsEmpty())return End(TEXT("Actual unchanged retrieval bay is obstructed: ")+BayBlockers);
   // One explicitly permitted player-only start. All floor, wall, furnishings,
   // cars, IDs, profiles, ownership, health and slot anchors remain untouched.
   const FVector Near=Control->GetActorLocation()+Control->GetActorForwardVector()*125;
   FCollisionQueryParams FloorQ(SCENE_QUERY_STAT(GarageStoredActualPlayerStart),false,P);FHitResult Floor;
   if(!W->LineTraceSingleByChannel(Floor,Near+FVector(0,0,150),Near-FVector(0,0,250),ECC_Pawn,FloorQ) || Floor.ImpactNormal.Z<.7)
    return End(TEXT("Real Retrieve control has no supported standing approach"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,
    FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),FloorQ))
    return End(TEXT("Real Retrieve control standing capsule obstructed"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   ++PlayerFixtures;Journal(TEXT("genuine_source_F7"));return Next(3);
  }
  if(Stage==3 && Age>.3)
  {
   if(!Focus())return End(TEXT("Actual normal F prompt cannot reach Retrieve control"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageStored-Actual-Retrieve-Prompt.png"));Key(EKeys::F,true);++RetrievePresses;return Next(4);
  }
  if(Stage==4 && Age>.3)
  {
   Key(EKeys::F,false);if(!Check(true))return End(Failure);
   if(FVector::Dist2D(Vehicles->FindLiveVehicle(ChosenID)->GetActorLocation(),Bay.GetLocation())>30)
    return End(TEXT("Normal F did not retrieve the original car into the actual authored bay"));
   Journal(TEXT("actual_F_retrieved"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageStored-Actual-F-Retrieved.png"));return Next(5);
  }
  if(Stage==5 && Age>.3)
  {
   if(!Check(true))return End(Failure);
   if(Control->FindStoredOwnedVehicle().IsValid())return End(TEXT("Retrieval unexpectedly left another eligible stored legacy car; audited source has only one"));
   FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageStoredActualOccupiedBay),false);
   TArray<FOverlapResult> Blocks;W->OverlapMultiByChannel(Blocks,Bay.GetLocation()+FVector(0,0,60),Bay.GetRotation(),
    ECC_Pawn,FCollisionShape::MakeBox(FVector(255,115,58)),Q);
   bool ActualCarBlock=false;for(const auto& B:Blocks)if(B.bBlockingHit && B.GetActor()==Vehicles->FindLiveVehicle(ChosenID))ActualCarBlock=true;
   if(!ActualCarBlock)return End(TEXT("Retrieved car does not physically occupy the original bay gate"));
   if(!Focus())return End(TEXT("Actual Retrieve F prompt lost on repeat"));
   BeforeRepeated=Vehicles->BuildPersistenceSnapshot();Key(EKeys::F,true);++RetrievePresses;return Next(6);
  }
  if(Stage==6 && Age>.3)
  {
   Key(EKeys::F,false);if(!Check(true))return End(Failure);
   const auto After=Vehicles->BuildPersistenceSnapshot();
   for(const auto ID:LegacyIDs())
   {
    const auto* Before=BeforeRepeated.FindByPredicate([&](const auto& D){return D.VehicleID==ID;});
    const auto* Now=After.FindByPredicate([&](const auto& D){return D.VehicleID==ID;});
    if(!Before || !Now || Now->LegacyFleetStored!=Before->LegacyFleetStored
     || Now->DataAssetID!=Before->DataAssetID || FMath::Abs(Now->Condition-Before->Condition)>.002)
     return End(TEXT("Occupied-bay repeated normal F copied, swallowed or changed an old car"));
   }
   RepeatedRejected=true;Journal(TEXT("occupied_bay_actual_F_rejected"));
   FNeonCityVisualEvidence::CaptureHUD(W,bCold?TEXT("GarageStored-Cold-Repeat-Rejected.png"):TEXT("GarageStored-Repeat-Rejected.png"));
   if(bCold)return End(TEXT(""));
   if(!Save->ConfigureSlot(NewSlot,false) || Save->DoesSaveExist())return End(TEXT("New isolated A save slot was unexpectedly occupied"));
   Key(EKeys::F6,true);return Next(7);
  }
  if(Stage==7 && Age>.3){Key(EKeys::F6,false);return Next(8);}
  if(Stage==8 && Age>.4)
  {
   if(Save->IsOperationInProgress())return false;
   const auto* Model=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Check(true))return End(TEXT("Actual F6 could not preserve retrieved/stored collection: ")+Failure);
   const auto* Deployed=Model->Vehicles.FindByPredicate([&](const auto& D){return D.VehicleID==ChosenID;});
   const auto* Stored=Model->Vehicles.FindByPredicate([&](const auto& D){return D.VehicleID==OtherID;});
   if(!Deployed || !Stored || !Deployed->Owned || Deployed->LegacyFleetStored || !Stored->Owned || Stored->LegacyFleetStored)
    return End(TEXT("Genuine F6 did not preserve the retrieved Kestrel and the already deployed owned Aster"));
   for(const auto ID:DefaultIDs())if(!Model->Vehicles.ContainsByPredicate([&](const auto& D){return D.VehicleID==ID && D.Owned;}))
    return End(TEXT("Actual F6 lost an original default fleet identity"));
   SavedPlayer=Model->Player.Transform;Saved=true;Journal(TEXT("actual_F6_fresh_slot"));return End(TEXT(""));
  }
  return false;
 }

private:
 bool Check(bool Deployed)
 {
  Failure.Reset();const auto Current=Vehicles->BuildPersistenceSnapshot();
  auto Fail=[&](const FString& Why){Failure=Why;return false;};
  int Owned=0;for(const auto& D:Baseline)if(D.Owned)++Owned;
  if(Vehicles->GetOwnedCount()!=Owned)return Fail(TEXT("Owned collection count changed"));
  TSet<FGuid> Seen;int Defaults=0;
  for(TActorIterator<ANeonCityBaseVehicle> I(W);I;++I)
  {
   if(!I->VehicleID.IsValid())continue;
   if(Seen.Contains(I->VehicleID))return Fail(TEXT("Duplicate actual vehicle GUID: ")+I->VehicleID.ToString());
   Seen.Add(I->VehicleID);if(I->ActorHasTag(TEXT("NeonCity.IslandDefaultFleet")))++Defaults;
  }
  if(Defaults!=3)return Fail(TEXT("Authored default fleet count changed"));
  for(const auto ID:DefaultIDs())if(!Seen.Contains(ID) || !Vehicles->IsOwned(ID))return Fail(TEXT("Original default fleet ID/ownership lost"));
  for(const auto& Before:Baseline)if(Before.Owned)
  {
   const auto* Now=Current.FindByPredicate([&](const auto& D){return D.VehicleID==Before.VehicleID;});
   if(!Now || !Now->Owned || Now->VehicleType!=Before.VehicleType || Now->DataAssetID!=Before.DataAssetID
    || FMath::Abs(Now->Condition-Before.Condition)>.002)return Fail(TEXT("Original owned ID/profile/condition changed: ")+Before.VehicleID.ToString());
   if(Before.VehicleID!=ChosenID && Now->LegacyFleetStored!=Before.LegacyFleetStored)
    return Fail(TEXT("Other owned stored/deployed state changed"));
  }
  if(!Vehicles->IsOwned(OtherID) || Vehicles->IsLegacyFleetStored(OtherID) || !Vehicles->FindLiveVehicle(OtherID))
   return Fail(TEXT("Already deployed old Aster was lost, copied or forced into storage"));
  const auto* OtherBefore=Baseline.FindByPredicate([&](const auto& D){return D.VehicleID==OtherID;});
  const auto* OtherNow=Current.FindByPredicate([&](const auto& D){return D.VehicleID==OtherID;});
  if(!OtherBefore || !OtherNow || OtherBefore->LegacyFleetStored
   || FVector::Dist(OtherNow->Transform.GetLocation(),OtherBefore->Transform.GetLocation())>30)
   return Fail(TEXT("Already deployed old Aster was moved during unrelated retrieval"));
  auto* Live=Vehicles->FindLiveVehicle(ChosenID);
  if(Deployed)
  {
   const auto* Before=Baseline.FindByPredicate([&](const auto& D){return D.VehicleID==ChosenID;});
   if(!Live || Vehicles->IsLegacyFleetStored(ChosenID) || !Before || !Vehicles->IsOwned(ChosenID)
    || !Live->GetVehicleProfile() || Live->GetVehicleProfile()->GetFName()!=Before->DataAssetID
    || uint8(Live->GetVehicleProfile()->VehicleType)!=Before->VehicleType
    || FMath::Abs(Live->GetDamageState()->GetConditionFraction()-Before->Condition)>.002)
    return Fail(TEXT("Same-GUID original profile and condition retrieval/restore failed"));
  }
  else if(Live || !Vehicles->IsLegacyFleetStored(ChosenID))return Fail(TEXT("Source old car was already deployed"));
  FNeonCityPropertyRecord Property;
  if(!Properties->GetProperty(Control->PropertyID,Property) || Property.GarageSlots.Num()!=PropertyBefore.GarageSlots.Num())
   return Fail(TEXT("Original garage anchors lost"));
  for(int I=0;I<Property.GarageSlots.Num();++I)
   if(!Property.GarageSlots[I].Location.Equals(PropertyBefore.GarageSlots[I].Location,.001)
    || Property.GarageSlots[I].SlotIndex!=PropertyBefore.GarageSlots[I].SlotIndex)
    return Fail(TEXT("Original garage anchor moved"));
  return true;
 }

 bool Focus()
 {
  PC->SetControlRotation((Control->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
  return P->GetInteractionComponent()->RefreshFocusedInteractable()==Control;
 }
 bool Next(int NewStage){Stage=NewStage;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down)
 {
  bool& Held=Keys.FindOrAdd(K);
  if(Held!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}
 }
 void Journal(const FString& StageName)
 {
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),StageName);
  Row->SetStringField(TEXT("actual_player_cm"),P->GetActorLocation().ToString());
  TArray<TSharedPtr<FJsonValue>> Records;
  for(const auto& D:Vehicles->BuildPersistenceSnapshot())if(D.Owned)
  {
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("id"),D.VehicleID.ToString());R->SetStringField(TEXT("profile"),D.DataAssetID.ToString());
   R->SetNumberField(TEXT("condition"),D.Condition);R->SetBoolField(TEXT("legacy_stored"),D.LegacyFleetStored);
   R->SetStringField(TEXT("actual_position_cm"),D.Transform.GetLocation().ToString());
   R->SetBoolField(TEXT("actual_live_actor"),Vehicles->FindLiveVehicle(D.VehicleID)!=nullptr);Records.Add(MakeShared<FJsonValueObject>(R));
  }
  Row->SetArrayField(TEXT("owned_vehicles"),Records);Stages.Add(MakeShared<FJsonValueObject>(Row));
 }

 bool End(FString Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));
  TArray<uint8> After;const bool OriginalPreserved=!OriginalBytes.IsEmpty()
   && UGameplayStatics::LoadDataFromSlot(After,OriginalSlot,0) && After==OriginalBytes;
  if(!OriginalBytes.IsEmpty() && !OriginalPreserved)Error+=TEXT(" Retained original checkpoint bytes changed.");
  const bool ColdPreserved=!bCold || (!ColdBytes.IsEmpty() && UGameplayStatics::LoadDataFromSlot(After,NewSlot,0) && After==ColdBytes);
  if(bCold && !ColdBytes.IsEmpty() && !ColdPreserved)Error+=TEXT(" B changed genuine A checkpoint bytes.");
  if(Error.IsEmpty() && (!RepeatedRejected || (!bCold && !Saved)))Error=TEXT("Minimum retrieval/refusal/save stages incomplete");
  if(!Error.IsEmpty())Test->AddError(Error);
  auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("passed"),Error.IsEmpty());R->SetStringField(TEXT("failure"),Error);
  R->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());R->SetNumberField(TEXT("source_process_id"),SourcePID);
  R->SetBoolField(TEXT("independent_cold_process"),bCold && SourcePID!=FPlatformProcess::GetCurrentProcessId());
  R->SetStringField(TEXT("original_slot"),OriginalSlot);R->SetStringField(TEXT("slot"),NewSlot);
  R->SetStringField(TEXT("deployed_id"),ChosenID.ToString());R->SetStringField(TEXT("other_retained_deployed_id"),OtherID.ToString());
  R->SetStringField(TEXT("repeat_refusal_reason"),TEXT("No remaining stored legacy car; live owned cars must never be copied or moved. Physically occupied bay is observed, but this is not the separate eligible-second-car clearance test."));
  R->SetStringField(TEXT("bay_transform"),Bay.ToHumanReadableString());R->SetNumberField(TEXT("player_start_fixtures"),PlayerFixtures);
  R->SetNumberField(TEXT("normal_retrieve_F_presses"),RetrievePresses);R->SetBoolField(TEXT("repeated_occupied_F_rejected"),RepeatedRejected);
  R->SetBoolField(TEXT("original_checkpoint_bytes_preserved"),OriginalPreserved);R->SetBoolField(TEXT("cold_checkpoint_bytes_preserved"),ColdPreserved);
  R->SetBoolField(TEXT("actual_fresh_F6"),Saved);R->SetStringField(TEXT("initial_native_bay_blockers"),BayBlockers);R->SetArrayField(TEXT("native_stage_records"),Stages);
  R->SetStringField(TEXT("scope"),bCold?TEXT("Different OS process from A. Normal F7 reads A's unmodified genuine F6: same retrieved Kestrel GUID/profile/condition, already deployed old Aster stays live, three stable owned default IDs and two property anchors. Repeated normal F with no remaining eligible stored car must not copy or move any live car. Zero player/vehicle/save/ownership/condition/slot fixtures. Cached roof/drive/garage controls not repeated."):TEXT("Normal F7 of genuine Aquarium02, independently audited by244; no save injection or source overwrite. One supported player-only start to existing Retrieve control. Normal F deploys the genuinely stored old Kestrel into the unchanged clear bay, leaving the old owned Aster already deployed. Repeated F with no remaining stored car cannot duplicate either. Genuine F6 writes a fresh isolated slot. All owned profiles/conditions/unique IDs and two property anchors verified. No vehicle/ownership/health/time/slot injection. Eligible-second-car occupied-bay refusal is a separate unvalidated scope, not claimed here."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/(bCold?TEXT("GarageStoredCold"):TEXT("GarageStoredSave"));
  IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
  if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json"))))Test->AddError(TEXT("GarageStored native receipt write failed"));
  if(!bCold && Error.IsEmpty())
  {
   IFileManager::Get().MakeDirectory(*SourceDir,true);
   if(!FFileHelper::SaveStringToFile(Text,*(SourceDir/TEXT("source-receipt.json"))))Test->AddError(TEXT("A cold-process source receipt write failed"));
  }
  return true;
 }

 FAutomationTestBase* Test=nullptr;bool bCold=false;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandGarageControl* Control=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;UVehicleOwnershipSubsystem* Vehicles=nullptr;UPropertySubsystem* Properties=nullptr;
 FNeonCityPropertyRecord PropertyBefore;TArray<FNeonCityVehicleSaveData> Baseline,BeforeRepeated;
 FGuid ChosenID,OtherID;FTransform Bay,SavedPlayer;FString NewSlot,SourceDir,Failure,BayBlockers;
 TArray<uint8> OriginalBytes,ColdBytes;TMap<FKey,bool> Keys;TArray<TSharedPtr<FJsonValue>> Stages;
 FNeonCityVisualEvidence Evidence;double Started=0;float At=0;int Stage=0,PlayerFixtures=0,RetrievePresses=0;
 uint32 SourcePID=0;bool RepeatedRejected=false,Saved=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarageStoredGenuineSave,"NeonCity.Phase205c.GarageStored.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGarageStoredGenuineSave::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(GarageStoredNative::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarageStoredGenuineCold,"NeonCity.Phase205c.GarageStored.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGarageStoredGenuineCold::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(GarageStoredNative::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
