#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/BoxComponent.h"
#include "Misc/ScopeExit.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/NacreRoofComponent.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Engine/OverlapResult.h"
#include "InputKeyEventArgs.h"

namespace ClosureDiagnostics862
{
/** Read the retained test input without applying it or changing any save.
 * Shadow attribution uses three local camera fixtures, never traversal proof.
 */
class FProbe final : public IAutomationLatentCommand
{
public:
 explicit FProbe(FAutomationTestBase* In):Test(In){}
 ~FProbe(){Restore();}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  if(Now-Started>100){Test->AddError(TEXT("Bounded closure diagnosis timed out"));Restore();return true;}
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W->HasBegunPlay() || W->GetTimeSeconds()<5)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  if(!Camera.IsValid())
  {
   auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();OldSlot=Save->GetSlotName();SavedSubsystem=Save;
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false)){Test->AddError(TEXT("Owned test input slot unavailable"));return true;}
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
   if(!Model.IsValid()){Test->AddError(TEXT("Integrity-checked test input read failed"));return true;}
   auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
   for(const auto& V:Model->Vehicles)
   {
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("guid"),V.VehicleID.ToString());R->SetStringField(TEXT("profile"),V.DataAssetID.ToString());
    R->SetStringField(TEXT("saved_pose"),V.Transform.ToHumanReadableString());R->SetNumberField(TEXT("saved_z_cm"),V.Transform.GetLocation().Z);
    R->SetNumberField(TEXT("condition"),V.Condition);R->SetBoolField(TEXT("legacy_stored"),V.LegacyFleetStored);R->SetStringField(TEXT("stored_property"),V.StoredPropertyID.ToString());R->SetNumberField(TEXT("migration_version"),V.IslandFleetMigrationVersion);
    for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->VehicleID==V.VehicleID){R->SetStringField(TEXT("live_actor"),I->GetName());R->SetStringField(TEXT("live_default_pose"),I->GetActorTransform().ToHumanReadableString());}
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ClosureSavedSupport),true,PC->GetPawn());
    for(TActorIterator<AIslandSupercar> I(W);I;++I)Q.AddIgnoredActor(*I);
    const FVector P=V.Transform.GetLocation();
    if(W->LineTraceSingleByChannel(Hit,P+FVector(0,0,50),P-FVector(0,0,10000),ECC_Visibility,Q))
    {R->SetStringField(TEXT("support_actor"),Hit.GetActor()->GetName());R->SetStringField(TEXT("support_cm"),Hit.ImpactPoint.ToString());R->SetNumberField(TEXT("saved_origin_above_surface_cm"),P.Z-Hit.ImpactPoint.Z);}
    Rows.Add(MakeShared<FJsonValueObject>(R));
   }
   J->SetArrayField(TEXT("vehicles"),Rows);J->SetStringField(TEXT("scope"),TEXT("Read-only native integrity inspection. No LoadGame, SaveGame, pose or physics writes. Camera-only VSM attribution follows; not gameplay/performance acceptance."));
   FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
   FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));Test->TestTrue(TEXT("Read-only saved vehicle identity receipt"),FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("saved-vehicle-inspection.json"))));
   OldView=PC->GetViewTarget();Controller=PC;Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(90);PC->SetViewTarget(Camera.Get());
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;OldHour=I->GetTimeOfDay();OldCycle=I->bCycleEnabled;I->SetCycleEnabled(false);I->SetTimeOfDay(12);break;}
   for(const TCHAR* Name:{TEXT("r.Shadow.Virtual.NonNanite.NumPageAreaDiagSlots"),TEXT("r.Shadow.Virtual.NonNanite.LargeInstancePageAreaThreshold")})if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)){Previous.Add(Name,C->GetInt());C->Set(FCString::Strstr(Name,TEXT("NumPage"))?-1:256,ECVF_SetByConsole);}
   Place();At=Now;return false;
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityVSMApproach897")))
  {
   // Continuous camera fixture through the measured native623 approach.
   // No player/vehicle pose or input writes; this is attribution, not a drive.
   const double Elapsed=Now-At;
   if(Elapsed<35)
   {
    const FVector Eyes[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(40500,-115200,25950)),NeonCityCoastalLayout::FromLegacyIsland(FVector(42016,-116424,26450)),NeonCityCoastalLayout::FromLegacyIsland(FVector(42655,-116928,26410)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-119000,26700))};
    const FVector Aims[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(43000,-117400,26400)),NeonCityCoastalLayout::FromLegacyIsland(FVector(43000,-117400,26400)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44942,-120659,26347)),NeonCityCoastalLayout::FromLegacyIsland(FVector(45200,-120500,26600))};
    const double T=FMath::Clamp((Elapsed-5)/10.,0.,3.);const int32 K=FMath::Min(2,int32(T));const double Alpha=T-K;
    const FVector Eye=FMath::Lerp(Eyes[K],Eyes[K+1],Alpha),Aim=FMath::Lerp(Aims[K],Aims[K+1],Alpha);
    Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());return false;
   }
   Test->AddInfo(TEXT("VSM897 continuous local camera sweep completed; inspect retained engine page-overlap and overflow diagnostics, not a performance/gameplay PASS"));
   Restore();return true;
  }
  if(Now-At<10)return false;
  if(++View<3){Place();At=Now;return false;}
  Restore();return true;
 }
private:
 void Place()
 {
  const FVector Eyes[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(42016,-116424,26450)),NeonCityCoastalLayout::FromLegacyIsland(FVector(42655,-116928,26410)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-119000,26700))};
  const FVector Aims[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(43000,-117400,26400)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44942,-120659,26347)),NeonCityCoastalLayout::FromLegacyIsland(FVector(45200,-120500,26600))};
  Camera->SetActorLocation(Eyes[View]);Camera->SetActorRotation((Aims[View]-Eyes[View]).Rotation());
  Test->AddInfo(FString::Printf(TEXT("CLOSURE862_VSM_CAMERA %d eye=%s"),View,*Eyes[View].ToString()));
 }
 void Restore()
 {
  if(Controller.IsValid() && OldView.IsValid())Controller->SetViewTarget(OldView.Get());
  if(Camera.IsValid()){Camera->Destroy();Camera.Reset();}
  if(Clock.IsValid()){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);Clock.Reset();}
  for(const auto& Pair:Previous)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))C->Set(Pair.Value,ECVF_SetByConsole);Previous.Reset();
  if(SavedSubsystem.IsValid() && !OldSlot.IsEmpty()){SavedSubsystem->ConfigureSlot(OldSlot,false);SavedSubsystem.Reset();}
 }
 FAutomationTestBase* Test;double Started=0,At=0;int32 View=0;float OldHour=12;bool OldCycle=false;FString OldSlot;
 TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<AActor> OldView;TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;TWeakObjectPtr<UNeonCitySaveSubsystem> SavedSubsystem;TMap<FString,int32> Previous;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClosureDiagnostics862,"NeonCity.Phase205c.Estate.Standalone.ClosureDiagnostics862",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FClosureDiagnostics862::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Isolated read-only native diagnosis only"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(ClosureDiagnostics862::FProbe(this));return true;
}
namespace ClosureDiagnostics862
{
static const FString RecoverySlot=TEXT("NeonCity_Test_RetiredEstate869");
static void Key(APlayerController* PC,FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
static bool Receipt(const TCHAR* Name,const TSharedRef<FJsonObject>& J)
{
 FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);
 FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*(Dir/Name));
}
class FRecoveryProbe final : public IAutomationLatentCommand
{
public:
 explicit FRecoveryProbe(FAutomationTestBase* In):Test(In){}
 ~FRecoveryProbe(){if(Controller.IsValid())Key(Controller.Get(),EKeys::F6,false);if(SaveSubsystem.IsValid() && !OriginalSlot.IsEmpty())SaveSubsystem->ConfigureSlot(OriginalSlot,false);}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  auto* W=NeonCityValidationWorld();auto* Region=W?ANeonCityCoastalRegion::Find(W):nullptr;
  if(Now-Started>45){Test->AddError(TEXT("Retired vehicle recovery fixture timed out"));return true;}
  if(!W || W->GetTimeSeconds()<5 || !Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();auto* Vehicles=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
  if(Stage)
  {
   if(Stage==3 && Now-At>.2){Key(Controller.Get(),EKeys::F6,false);Stage=4;At=Now;return false;}
   if(Now-At<2)return false;
   if(Stage==4)
   {
    if(Save->IsOperationInProgress())return false;
    TStrongObjectPtr<UNeonCitySaveGame> Saved(Save->ReadSaveForInspection());
    const auto* V=Saved.IsValid()?Saved->Vehicles.FindByPredicate([&](const auto& D){return D.VehicleID==Expected.VehicleID;}):nullptr;
    Test->TestTrue(TEXT("Normal F6 saves retrieved identity, condition and recovery version without changing other source files"),V && V->Owned && !V->LegacyFleetStored && V->IslandFleetMigrationVersion==3 && FMath::Abs(V->Condition-Expected.Condition)<.002);
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetStringField(TEXT("slot"),RecoverySlot);J->SetStringField(TEXT("recovered_guid"),Expected.VehicleID.ToString());J->SetNumberField(TEXT("original_condition"),Expected.Condition);J->SetStringField(TEXT("original_obsolete_pose_preserved_in_source"),Expected.Transform.ToHumanReadableString());J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
    J->SetStringField(TEXT("scope"),TEXT("Real legacy LoadGame, owned-storage recovery, existing production DeployStoredVehicle at actual authored bay, native settling, normal F6 into one fresh test slot. No source/player save writes. Direct retrieval function is disclosed; normal F panel selection is cached separately."));
    Test->TestTrue(TEXT("Recovery/save receipt written"),Receipt(TEXT("retired-restoration.json"),J));return true;
   }
   if(Stage==1)
   {
    if(!Test->TestTrue(TEXT("Real LoadGame stores unsupported old car without spawning a falling duplicate"),Vehicles->IsLegacyFleetStored(Expected.VehicleID) && !Vehicles->FindLiveVehicle(Expected.VehicleID)))return true;
    auto* Properties=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
    FString Candidates;
    for(TActorIterator<AIslandGarageControl> I(W);I;++I)if(I->Action==EIslandGarageAction::RetrieveStored)
    {
     FNeonCityPropertyRecord Property;const bool Found=Properties->GetProperty(I->PropertyID,Property);
     Candidates+=FString::Printf(TEXT("%s property=%s found=%d owned=%d slot=%d count=%d "),*I->GetName(),*I->PropertyID.ToString(),Found,Property.Owned,I->SlotIndex,Property.GarageSlots.Num());
     if(!Found || !Property.Owned || !Property.GarageSlots.IsValidIndex(I->SlotIndex))continue;
     Bay=I->RetrievalBayLocal*I->GetActorTransform();Bay.SetLocation(Property.GarageSlots[I->SlotIndex].Location);
     TArray<FOverlapResult> Blocks;FCollisionQueryParams Q(SCENE_QUERY_STAT(RetiredRetrievalBayProbe),false);
     W->OverlapMultiByChannel(Blocks,Bay.GetLocation()+FVector(0,0,60),Bay.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(FVector(255,115,58)),Q);
     Candidates+=TEXT("bay=")+Bay.GetLocation().ToString()+TEXT(" blocks=");for(const auto& B:Blocks)if(B.bBlockingHit)Candidates+=GetNameSafe(B.GetActor())+TEXT("/")+GetNameSafe(B.GetComponent())+TEXT("|");
     Retrieved=Vehicles->DeployStoredVehicle(Expected.VehicleID,Bay);if(Retrieved.IsValid())break;
    }
    Test->AddInfo(TEXT("Actual native retrieval candidates: ")+(Candidates.IsEmpty()?TEXT("NONE"):Candidates));
    if(!Test->TestTrue(TEXT("Existing production retrieval deploys recovered ID into an actual clear owned bay"),Retrieved.IsValid()))return true;
    Stage=2;At=Now;return false;
   }
   const auto* Car=Cast<AIslandSupercar>(Retrieved.Get());
   Test->TestTrue(TEXT("Retrieved car retains ownership, identity, condition and roof after native settling"),Car && Vehicles->IsOwned(Expected.VehicleID) && !Vehicles->IsLegacyFleetStored(Expected.VehicleID) && FMath::Abs(Car->GetDamageState()->GetConditionFraction()-Expected.Condition)<.002 && FMath::Abs(Car->GetRoofMechanism()->GetProgress()-Expected.RoofProgress)<.002);
   Test->TestTrue(TEXT("Retrieved car stays in the supported authored bay"),Car && FVector::Dist2D(Car->GetActorLocation(),Bay.GetLocation())<40 && FMath::Abs(Car->GetActorLocation().Z-Bay.GetLocation().Z)<100);
   if(Test->HasAnyErrors())return true;
   if(!Save->ConfigureSlot(RecoverySlot,false) || Save->DoesSaveExist()){Test->AddError(TEXT("Fresh recovery slot required; preserve retained generations"));return true;}
   Controller=W->GetFirstPlayerController();Key(Controller.Get(),EKeys::F6,true);Stage=3;At=Now;return false;
  }
  OriginalSlot=Save->GetSlotName();SaveSubsystem=Save;
  if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false)){Test->AddError(TEXT("Retained native input required"));return true;}
  TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());Save->ConfigureSlot(OriginalSlot,false);
  if(!Model.IsValid()){Test->AddError(TEXT("Integrity-checked native input required"));return true;}
  const auto* Source=Model->Vehicles.FindByPredicate([](const auto& V){return V.VehicleID.ToString(EGuidFormats::Digits)==TEXT("8BD22EB34C404BB67EF8ECAC243B9DB9");});
  if(!Test->TestNotNull(TEXT("Actual retained obsolete-estate car fixture"),Source))return true;
  TArray<FNeonCityVehicleSaveData> Values={*Source};TArray<FString> Warnings;Vehicles->NormalizePersistenceSnapshot(Values,Warnings);
  const auto* Recovered=Values.FindByPredicate([&](const auto& V){return V.VehicleID==Source->VehicleID;});
  if(!Test->TestNotNull(TEXT("Unsupported owned car remains in collection"),Recovered))return true;
  Test->TestTrue(TEXT("Obsolete estate car is stored instead of falling30m on restore"),Recovered->LegacyFleetStored);
  Test->TestTrue(TEXT("Recovery preserves saved identity, original pose, condition and roof"),Recovered->Owned==Source->Owned && Recovered->Transform.Equals(Source->Transform) && Recovered->Condition==Source->Condition && Recovered->RoofProgress==Source->RoofProgress && Recovered->RoofTargetOpen==Source->RoofTargetOpen);
  // Inside the same obsolete court envelope, a real temporary collision
  // platform must prevent recovery. This exercises the support branch rather
  // than merely returning at the geographic gate. No native asset is edited.
  AActor* Platform=W->SpawnActor<AActor>();ON_SCOPE_EXIT{if(IsValid(Platform))Platform->Destroy();};
  auto* Support=NewObject<UBoxComponent>(Platform);Platform->SetRootComponent(Support);
  Support->SetBoxExtent(FVector(500,500,10));Support->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Support->SetCollisionResponseToAllChannels(ECR_Ignore);Support->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);Support->RegisterComponent();
  Platform->SetActorLocation(Source->Transform.GetLocation()-FVector(0,0,60));
  Values={*Source};Warnings.Reset();Vehicles->NormalizePersistenceSnapshot(Values,Warnings);
  const auto* Elevated=Values.FindByPredicate([&](const auto& V){return V.VehicleID==Source->VehicleID;});
  Test->TestTrue(TEXT("Real support inside old court prevents recovery and preserves pose"),Elevated && !Elevated->LegacyFleetStored && Elevated->Transform.Equals(Source->Transform));
  Platform->Destroy();Platform=nullptr;
  auto Supported=*Source;Supported.Transform=FTransform(FRotator(0,22.9,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(57073.127565,-153874.853382,704.681907)));Supported.IslandFleetMigrationVersion=1;Values={Supported};Warnings.Reset();Vehicles->NormalizePersistenceSnapshot(Values,Warnings);
  const auto* Kept=Values.FindByPredicate([&](const auto& V){return V.VehicleID==Supported.VehicleID;});
  Test->TestTrue(TEXT("A supported player-moved retired car keeps its exact saved pose"),Kept && !Kept->LegacyFleetStored && Kept->Transform.Equals(Supported.Transform));
  auto Other=*Source;FGuid::ParseExact(TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8"),EGuidFormats::Digits,Other.VehicleID);Other.DataAssetID=TEXT("DA_HeroNacre");Other.IslandFleetMigrationVersion=2;Values={Other};Warnings.Reset();Vehicles->NormalizePersistenceSnapshot(Values,Warnings);
  const auto* Unrelated=Values.FindByPredicate([&](const auto& V){return V.VehicleID==Other.VehicleID;});
  Test->TestTrue(TEXT("Recovery never rewrites unrelated car at the same coordinate"),Unrelated && !Unrelated->LegacyFleetStored && Unrelated->Transform.Equals(Other.Transform));
  if(Test->HasAnyErrors())return true;
  Expected=*Source;
  if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->LoadGame()){Test->AddError(TEXT("Production LoadGame failed: ")+Save->GetLastDetail());return true;}
  Stage=1;At=Now;return false;
 }
private:FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0;FString OriginalSlot;FNeonCityVehicleSaveData Expected;FTransform Bay;TWeakObjectPtr<UNeonCitySaveSubsystem> SaveSubsystem;TWeakObjectPtr<ANeonCityBaseVehicle> Retrieved;TWeakObjectPtr<APlayerController> Controller;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRetiredEstateRecovery863,"NeonCity.Phase205c.Estate.Standalone.RetiredEstateRecovery863",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FRetiredEstateRecovery863::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Isolated native validation only"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(ClosureDiagnostics862::FRecoveryProbe(this));return true;
}
namespace ClosureDiagnostics862
{
class FColdRecovery final : public IAutomationLatentCommand
{
public:
 explicit FColdRecovery(FAutomationTestBase* In):Test(In){}
 ~FColdRecovery(){if(Save.IsValid() && !OriginalSlot.IsEmpty())Save->ConfigureSlot(OriginalSlot,false);}
 bool Update()override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  if(Now-Started>45){Test->AddError(TEXT("Cold recovery timeout"));return true;}
  auto* W=NeonCityValidationWorld();auto* Region=W?ANeonCityCoastalRegion::Find(W):nullptr;
  if(!W || W->GetTimeSeconds()<5 || !Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  if(!Loaded)
  {
   FString Text;TSharedPtr<FJsonObject> Prior;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/RootRetiredClosure869/retired-restoration.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Prior) || !Prior->GetBoolField(TEXT("passed")) || uint32(Prior->GetNumberField(TEXT("process_id")))==FPlatformProcess::GetCurrentProcessId()){Test->AddError(TEXT("Successful A receipt from a different OS process required"));return true;}
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();OriginalSlot=Save->GetSlotName();
   if(!Save->ConfigureSlot(RecoverySlot,false)){Test->AddError(TEXT("Dedicated cold source unavailable"));return true;}
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());FGuid ID;FGuid::Parse(Prior->GetStringField(TEXT("recovered_guid")),ID);
   const auto* V=Model.IsValid()?Model->Vehicles.FindByPredicate([&](const auto& D){return D.VehicleID==ID;}):nullptr;
   if(!V || !V->Owned || V->LegacyFleetStored){Test->AddError(TEXT("Real A save has no deployed recovered identity"));return true;}
   Expected=*V;FishCount=Model->Fishing.Fish.Num();OwnedCount=0;for(const auto& D:Model->Vehicles)OwnedCount+=D.Owned?1:0;
   if(!Save->LoadGame()){Test->AddError(TEXT("Production cold LoadGame failed"));return true;}
   Loaded=true;At=Now;return false;
  }
  if(Now-At<3)return false;
  auto* Vehicles=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();const auto* Car=Cast<AIslandSupercar>(Vehicles->FindLiveVehicle(Expected.VehicleID));
  Test->TestTrue(TEXT("Cold restored retrieved car remains owned/live with unchanged condition, roof and bay"),Car && Vehicles->IsOwned(Expected.VehicleID) && !Vehicles->IsLegacyFleetStored(Expected.VehicleID) && FMath::Abs(Car->GetDamageState()->GetConditionFraction()-Expected.Condition)<.002 && FMath::Abs(Car->GetRoofMechanism()->GetProgress()-Expected.RoofProgress)<.002 && FVector::Dist(Car->GetActorLocation(),Expected.Transform.GetLocation())<45);
  Test->TestEqual(TEXT("Cold owned collection count retained"),Vehicles->GetOwnedCount(),OwnedCount);
  TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());Test->TestTrue(TEXT("Cold source generation retains genuine fishing inventory"),Model.IsValid() && Model->Fishing.Fish.Num()==FishCount);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetStringField(TEXT("slot"),RecoverySlot);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("scope"),TEXT("Independent OS process, native integrity read and production LoadGame only. Recovered deployed car identity/ownership/condition/roof/position and total owned collection checked; no save write."));Test->TestTrue(TEXT("Cold recovery receipt written"),Receipt(TEXT("retired-restoration-cold.json"),J));return true;
 }
private:FAutomationTestBase* Test;double Started=0,At=0;bool Loaded=false;int32 OwnedCount=0,FishCount=0;FString OriginalSlot;TWeakObjectPtr<UNeonCitySaveSubsystem> Save;FNeonCityVehicleSaveData Expected;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRetiredEstateCold869,"NeonCity.Phase205c.Estate.Standalone.RetiredEstateCold869",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FRetiredEstateCold869::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Isolated native validation only"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(ClosureDiagnostics862::FColdRecovery(this));return true;
}
#endif
