#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Property/PropertySubsystem.h"
#include "Property/PropertyComponent.h"
#include "Property/NeonCityStarterApartment.h"
#include "Property/NeonCityGarageMarker.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Phone/PhoneComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Combat/HealthComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyRegistryTest,"NeonCity.Property.RegistryInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPropertyRegistryTest::RunTest(const FString& Parameters)
{
 auto* GI=NewObject<UGameInstance>(); auto* Registry=NewObject<UPropertySubsystem>(GI);
 auto* Data=NewObject<UNeonCityPropertyData>();
 TestFalse(TEXT("Empty catalogue data rejected"),Registry->RegisterProperty(Data));
 Data->PropertyID=TEXT("Test.Home"); Data->DisplayName=FText::FromString(TEXT("Home")); Data->Price=45000; Data->GarageSlotCount=1; Data->bInitiallyOwned=true;
 TestTrue(TEXT("Valid starter home registered"),Registry->RegisterProperty(Data));
 TestTrue(TEXT("Idempotent registration"),Registry->RegisterProperty(Data));
 TestTrue(TEXT("Starter ownership present"),Registry->IsOwned(Data->PropertyID));
 TestEqual(TEXT("Home identity"),Registry->GetHomePropertyID(),Data->PropertyID);
 auto* Duplicate=DuplicateObject<UNeonCityPropertyData>(Data,GI);
 TestFalse(TEXT("Different asset cannot overwrite stable identity"),Registry->RegisterProperty(Duplicate));
 Duplicate->PropertyID=TEXT("Test.Invalid"); Duplicate->Price=-1;
 TestFalse(TEXT("Negative price rejected"),Registry->RegisterProperty(Duplicate));
 Duplicate->Price=0; Duplicate->GarageSlotCount=9;
 TestFalse(TEXT("Unbounded garage rejected"),Registry->RegisterProperty(Duplicate));
 Duplicate->GarageSlotCount=0; Duplicate->Type=static_cast<ENeonCityPropertyType>(255);
 TestFalse(TEXT("Invalid property type rejected"),Registry->RegisterProperty(Duplicate));
 TestFalse(TEXT("Invalid slot release rejected"),Registry->ReleaseVehicle(Data->PropertyID,3));
 TestFalse(TEXT("Unknown property is not owned"),Registry->IsOwned(TEXT("Missing")));
 FTransform Spawn; TestFalse(TEXT("Catalogue alone does not invent world spawn"),Registry->GetHomeSpawnTransform(Spawn));
 auto Values=Registry->GetProperties(); Values[0].Owned=false;
 TestTrue(TEXT("Returned values cannot mutate registry"),Registry->IsOwned(Data->PropertyID));
 TestEqual(TEXT("Rejected operations leave single record"),Registry->GetProperties().Num(),1);
 return true;
}
namespace NeonCityPropertyTests
{
static void Key(APlayerController* PC,FKey Value,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64())); }
static void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityHomeCapture"))) return;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase12")),true);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase12")/Name,true,false);
}
class FVerify : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(FString::Printf(TEXT("Home PIE timeout stage %d"),Stage)); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W->HasBegunPlay()) return false;
  const float Now=W->GetTimeSeconds(); auto* GI=W->GetGameInstance();
  auto* Registry=GI->GetSubsystem<UPropertySubsystem>(); auto* Phone=PC->FindComponentByClass<UPhoneComponent>();
  auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>(); auto* Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();
  auto Next=[&](int32 Value){ UE_LOG(LogTemp,Display,TEXT("Home stage %d -> %d"),Stage,Value); Stage=Value; Phase=Now; return false; };
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn()); if(!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
   for(TActorIterator<ANeonCityStarterApartment> It(W);It;++It) { if(Home.IsValid()) { Test->AddError(TEXT("Multiple starter homes")); return true; } Home=*It; }
   for(TActorIterator<ANeonCityGarageMarker> It(W);It;++It) Marker=*It;
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It) if(It->bStartsOwned) Car=*It; else Loaned=*It;
   if(!Test->TestTrue(TEXT("Authored home, garage and cars exist"),Home.IsValid() && Marker.IsValid() && Car.IsValid() && Loaned.IsValid())) return true;
   ID=Home->GetPropertyComponent()->GetPropertyID(); FNeonCityPropertyRecord Record;
   Test->TestTrue(TEXT("Starter home catalogue entry"),Registry->GetProperty(ID,Record));
   Test->TestTrue(TEXT("Starter apartment starts owned"),Record.Owned);
   Test->TestEqual(TEXT("Catalogue price"),Record.Price,int64(45000));
   Test->TestEqual(TEXT("One garage slot"),Record.GarageSlots.Num(),1);
   FTransform Spawn; Test->TestTrue(TEXT("Home spawn interface available"),Registry->GetHomeSpawnTransform(Spawn));
   Test->TestTrue(TEXT("Default PlayerStart places same template-derived character at home"),Home->IsInsideInterior_Implementation(Player.Get()) && FVector::Dist(Player->GetActorLocation(),Spawn.GetLocation())<20);
   AnimClass=Player->GetMesh()->GetAnimClass(); Test->TestNotNull(TEXT("Template animation blueprint retained"),AnimClass.Get());
   Money=Economy->GetBalance(); Test->TestEqual(TEXT("Starter home does not charge player"),Money,int64(1850));
   Test->TestTrue(TEXT("Save system now supports disk persistence"),Save->IsPersistenceImplemented());
   Test->TestTrue(TEXT("Sleep request reserved at home"),Home->RequestHomeService_Implementation(Player.Get(),ENeonCityHomeService::Sleep));
   Test->TestTrue(TEXT("Save point request reserved at home"),Home->RequestHomeService_Implementation(Player.Get(),ENeonCityHomeService::SavePoint));
   Origin=Player->GetActorLocation(); Shot(TEXT("StarterApartment.png")); return Next(1);
  }
  if(Stage==1)
  {
   Player->DoMove(0,1);
   if(Now-Phase<.5f) return false;
   Test->AddInfo(FString::Printf(TEXT("Home startup move: elapsed %.3f, world %.3f, displacement %.2f, velocity %s, mode %d, ignored %d, control %s"),
    Now-Phase,Now,FVector::Dist2D(Player->GetActorLocation(),Origin),*Player->GetVelocity().ToString(),int32(Player->GetCharacterMovement()->MovementMode),PC->IsMoveInputIgnored(),*PC->GetControlRotation().ToString()));
   Test->TestTrue(TEXT("Template movement works inside home"),FVector::Dist2D(Player->GetActorLocation(),Origin)>40);
   Player->GetCharacterMovement()->StopMovementImmediately(); Key(PC,EKeys::P,IE_Pressed); return Next(2);
  }
  if(Stage==2 && Now-Phase>.2f) { Key(PC,EKeys::P,IE_Released); Key(PC,EKeys::H,IE_Pressed); return Next(3); }
  if(Stage==3 && Now-Phase>.3f)
  {
   Key(PC,EKeys::H,IE_Released);
   Test->TestTrue(TEXT("H opens Real Estate via phone routing"),Phone->IsPhoneOpen() && Phone->GetActiveApp()==ENeonCityPhoneApp::Properties);
   const FString Text=Phone->GetDisplayedText().ToString();
   Test->TestTrue(TEXT("Name, price, location, ownership and garage are visible"),Text.Contains(TEXT("Starter Apartment")) && Text.Contains(TEXT("45000")) && Text.Contains(TEXT("Ocean Apartments")) && Text.Contains(TEXT("Owned")) && Text.Contains(TEXT("0 / 1")));
   Test->TestFalse(TEXT("Property page cannot accept a delivery"),Phone->AcceptLatestMessage());
   Key(PC,EKeys::Enter,IE_Pressed); Shot(TEXT("RealEstate.png")); return Next(4);
  }
  if(Stage==4 && Now-Phase>.6f)
  {
   Key(PC,EKeys::Enter,IE_Released); Test->TestEqual(TEXT("Browsing preserves cash"),Economy->GetBalance(),Money);
   Key(PC,EKeys::P,IE_Pressed); return Next(5);
  }
  if(Stage==5 && Now-Phase>.2f)
  {
   Key(PC,EKeys::P,IE_Released); Place(FVector(4100,1750,139)); return Next(6);
  }
  if(Stage==6)
  {
   if(Now-Phase<.7f) { Player->DoMove(0,1); return false; }
   Player->GetCharacterMovement()->StopMovementImmediately();
   Test->TestTrue(TEXT("At the inner door panel the resident remains inside"),Home->IsInsideInterior_Implementation(Player.Get()));
   Aim(PC,Home->GetActorLocation()+FVector(0,0,120)); Key(PC,EKeys::F,IE_Pressed); return Next(7);
  }
  if(Stage==7 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("F exits home through existing interaction"),!Home->IsInsideInterior_Implementation(Player.Get()) && Player->GetActorLocation().Y<1400);
   Test->TestFalse(TEXT("Outside sleep request rejected"),Home->RequestHomeService_Implementation(Player.Get(),ENeonCityHomeService::Sleep));
   Test->TestTrue(TEXT("Unobstructed entry is eligible before blocking destination"),Home->CanEnterInterior_Implementation(Player.Get()));
   // A blocked destination must reject the transition without moving the pawn.
   auto* Block=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Block); Block->SetRootComponent(Box); Box->SetBoxExtent(FVector(70)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
   Block->SetActorLocation((Home->EntryLocal*Home->GetActorTransform()).GetLocation());
   const FVector Before=Player->GetActorLocation();
   Test->TestFalse(TEXT("Occupied entry destination rejected"),Home->EnterInterior_Implementation(Player.Get()));
   Test->TestTrue(TEXT("Failed transition preserves location"),Player->GetActorLocation().Equals(Before)); Block->Destroy();
   Shot(TEXT("HomeEntrance.png")); return Next(8);
  }
  if(Stage==8 && Now-Phase>.5f) { Aim(PC,Home->GetActorLocation()+FVector(0,0,120)); Key(PC,EKeys::F,IE_Pressed); return Next(9); }
  if(Stage==9 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("F enters the physical apartment"),Home->IsInsideInterior_Implementation(Player.Get()));
   Test->TestTrue(TEXT("Same pawn, controller and animation after home transitions"),PC->GetPawn()==Player.Get() && Player->GetMesh()->GetAnimClass()==AnimClass.Get());
   Place(FVector(6000,1300,139)); return Next(10);
  }
  if(Stage==10 && Now-Phase>.5f)
  {
   Test->TestFalse(TEXT("Remote apartment transition denied"),Home->EnterInterior_Implementation(Player.Get()));
   Test->TestFalse(TEXT("Cannot register unowned car"),Registry->StoreVehicle(ID,0,Loaned.Get()));
   Test->TestFalse(TEXT("Invalid garage slot denied"),Registry->StoreVehicle(ID,1,Car.Get()));
   Place(FVector(4230,1390,139)); return Next(16);
  }
  if(Stage==16 && Now-Phase>.5f) { Aim(PC,Marker->GetActorLocation()+FVector(0,0,70)); Key(PC,EKeys::F,IE_Pressed); return Next(11); }
  if(Stage==11 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released); FNeonCityPropertyRecord Record; Registry->GetProperty(ID,Record);
   Test->TestTrue(TEXT("F at garage registers owned Sedan"),Record.GarageSlots[0].VehicleID==Car->VehicleID);
   Test->TestTrue(TEXT("Repeat storage is idempotent"),Registry->StoreVehicle(ID,0,Car.Get()));
   auto Snapshot=Save->BuildPropertySnapshot();
   Test->TestTrue(TEXT("Property save boundary includes ownership and garage identity"),Snapshot.Num()==1 && Snapshot[0].Owned && Snapshot[0].GarageSlots[0].VehicleID==Car->VehicleID);
   Test->TestTrue(TEXT("Vehicle remains visible and present"),!Car->IsHidden() && !Car->IsActorBeingDestroyed());
   Phone->ShowRealEstate(); Test->TestTrue(TEXT("Phone reflects garage change"),Phone->GetDisplayedText().ToString().Contains(TEXT("1 / 1"))); Shot(TEXT("GarageRecord.png")); return Next(12);
  }
  if(Stage==12 && Now-Phase>.6f)
  {
   Phone->TogglePhone(); Test->TestTrue(TEXT("Explicit garage release"),Registry->ReleaseVehicle(ID,0));
   Test->TestTrue(TEXT("Released slot can register same vehicle again"),Registry->StoreVehicle(ID,0,Car.Get()));
   Place(Car->GetActorLocation()-Car->GetActorRightVector()*210+FVector(0,0,70)); return Next(13);
  }
  if(Stage==13 && Now-Phase>.5f) { Aim(PC,Car->GetActorLocation()+FVector(0,0,90)); Key(PC,EKeys::F,IE_Pressed); return Next(14); }
  if(Stage==14 && Now-Phase>.8f)
  {
   Key(PC,EKeys::F,IE_Released); FNeonCityPropertyRecord Record; Registry->GetProperty(ID,Record);
   Test->TestTrue(TEXT("Stored car remains enterable"),PC->GetPawn()==Car.Get());
   Test->TestFalse(TEXT("Occupied vehicle clears garage assignment"),Record.GarageSlots[0].VehicleID.IsValid());
   Test->TestFalse(TEXT("Cannot store occupied vehicle"),Registry->StoreVehicle(ID,0,Car.Get()));
   Test->TestEqual(TEXT("Property operations preserve ownership count"),GI->GetSubsystem<UVehicleOwnershipSubsystem>()->GetOwnedCount(),1);
   Key(PC,EKeys::F,IE_Pressed); return Next(15);
  }
  if(Stage==15 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released); Test->TestTrue(TEXT("Leaving stored car restores character"),PC->GetPawn()==Player.Get());
   Test->TestEqual(TEXT("Property flow leaves cash unchanged"),Economy->GetBalance(),Money);
   Test->TestEqual(TEXT("Four future save domains retained"),Save->GetPlannedDomains().Num(),4);
   return true;
  }
  return false;
 }
 void Place(FVector Location) { Player->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
 void Aim(APlayerController* PC,FVector Target) { FVector Eye; FRotator R; Player->GetActorEyesViewPoint(Eye,R); PC->SetControlRotation((Target-Eye).Rotation()); }
 FAutomationTestBase* Test; double Started=0; float Phase=0; int32 Stage=0; int64 Money=0; FName ID; FVector Origin;
 TWeakObjectPtr<ANeonCityPlayerCharacter> Player; TWeakObjectPtr<ANeonCityStarterApartment> Home; TWeakObjectPtr<ANeonCityGarageMarker> Marker;
 TWeakObjectPtr<ANeonCityBaseVehicle> Car,Loaned; TWeakObjectPtr<UClass> AnimClass;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyHomePIE,"NeonCity.Property.HomePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPropertyHomePIE::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityPropertyTests::FVerify(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
