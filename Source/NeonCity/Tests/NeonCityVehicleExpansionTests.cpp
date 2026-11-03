#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Phone/AutoMarketSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
namespace NeonCityFleetTests
{
static void Key(APlayerController* PC,FKey Value,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64())); }
static void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityFleetCapture"))) return;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase11")),true);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase11")/Name,true,false);
}
class FVerify : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Fleet PIE timeout stage=%d model=%d"),Stage,Index)); return true; }
  if(!PC || !W->HasBegunPlay()) return false;
  const float Now=W->GetTimeSeconds();
  auto* GI=W->GetGameInstance(); auto* Registry=GI->GetSubsystem<UVehicleOwnershipSubsystem>();
  auto* Phone=PC->FindComponentByClass<UPhoneComponent>();
  auto Next=[&](int32 Value){ UE_LOG(LogTemp,Display,TEXT("Fleet stage %d -> %d model=%d t=%.2f"),Stage,Value,Index,Now); Stage=Value; Phase=Now; return false; };
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn()); if(!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
   if(auto* Coast=ANeonCityCoastalRegion::Find(W);Coast && !Coast->IsInitialRegionReady())return false;
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)
    if(Cast<AIslandOffroadVehicle>(*It)){OtherOwned+=Registry->IsOwned(It->VehicleID)?1:0;}else Cars.Add(*It);
   Cars.Sort([](const auto& A,const auto& B){ return uint8(A->GetVehicleProfile()->VehicleType)<uint8(B->GetVehicleProfile()->VehicleType); });
   if(!Test->TestEqual(TEXT("Four authored drivable cars"),Cars.Num(),4)) return true;
   TSet<FGuid> IDs; TSet<const UVehicleData*> Profiles;
   for(int32 I=0;I<Cars.Num();++I)
   {
    auto* C=Cars[I].Get(); const auto* Data=C->GetVehicleProfile();
    Test->TestTrue(TEXT("Vehicle identity valid"),C->VehicleID.IsValid()); IDs.Add(C->VehicleID); Profiles.Add(Data);
    Test->TestEqual(TEXT("All four types present"),uint8(Data->VehicleType),uint8(I));
    Test->TestTrue(TEXT("Independent profile valid"),Data->IsValidProfile());
    Test->TestEqual(TEXT("Durability initializes shared health"),C->GetHealthComponent()->GetMaxHealth(),Data->Durability);
    Test->TestTrue(TEXT("Weight configures real physics body"),FMath::IsNearlyEqual(C->GetMesh()->GetBodyInstance()->GetBodyMass(),Data->Weight,2.f));
    Parking.Add(C->GetActorTransform());
   }
   Test->TestEqual(TEXT("Unique persisted vehicle IDs"),IDs.Num(),4); Test->TestEqual(TEXT("Four separate DataAssets"),Profiles.Num(),4);
   Test->TestEqual(TEXT("Only original city Sedan starts owned; island fleet remains independent"),Registry->GetOwnedCount(),1+OtherOwned);
   Test->TestTrue(TEXT("Original identity is in owned registry"),Registry->IsOwned(Cars[0]->VehicleID));
   Cars[0]->GetHealthComponent()->SetCurrentHealth(90);
   Test->TestTrue(TEXT("Repeated live registration is idempotent"),Registry->RegisterLiveVehicle(Cars[0].Get()));
   Test->TestEqual(TEXT("Repeated registration never heals stale saved condition"),Cars[0]->GetHealthComponent()->GetCurrentHealth(),90.f);
   Cars[0]->GetHealthComponent()->Heal(10);
   Test->TestFalse(TEXT("Registry restore cannot overwrite a live fleet"),Registry->RestoreRegistrySnapshot({}));
   Test->TestEqual(TEXT("Read-only market loads four models"),GI->GetSubsystem<UAutoMarketSubsystem>()->GetVehicleProfiles().Num(),4);
   MoneyBefore=GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance();
   Key(PC,EKeys::P,IE_Pressed); return Next(1);
  }
  if(Stage==1 && Now-Phase>.2f) { Key(PC,EKeys::P,IE_Released); Key(PC,EKeys::V,IE_Pressed); return Next(2); }
  if(Stage==2 && Now-Phase>.3f)
  {
   Key(PC,EKeys::V,IE_Released);
   Test->TestTrue(TEXT("V opens AutoMarket through phone app routing"),Phone->IsPhoneOpen() && Phone->GetActiveApp()==ENeonCityPhoneApp::Vehicles);
   Test->TestTrue(TEXT("Market displays vehicle properties"),Phone->GetDisplayedText().ToString().Contains(TEXT("Durability")) && Phone->GetDisplayedText().ToString().Contains(TEXT("100 km/h")));
   Test->TestFalse(TEXT("Market cannot accidentally accept delivery"),Phone->AcceptLatestMessage());
   Key(PC,EKeys::Right,IE_Pressed); return Next(3);
  }
  if(Stage==3 && Now-Phase>.25f)
  {
   Key(PC,EKeys::Right,IE_Released);
   Test->TestTrue(TEXT("Arrow browses Sports profile"),Phone->GetDisplayedText().ToString().Contains(TEXT("Sports Car")) && Phone->GetDisplayedText().ToString().Contains(TEXT("140 km/h")));
   Shot(TEXT("AutoMarket.png")); return Next(4);
  }
  if(Stage==4 && Now-Phase>.5f && !FScreenshotRequest::IsScreenshotRequested()) { Key(PC,EKeys::P,IE_Pressed); return Next(5); }
  if(Stage==5 && Now-Phase>.2f)
  {
   Key(PC,EKeys::P,IE_Released); Test->TestFalse(TEXT("Phone closes"),Phone->IsPhoneOpen());
   Test->TestEqual(TEXT("Catalogue browsing does not spend money"),GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance(),MoneyBefore);
   return Next(40);
  }
  if(Stage==10)
  {
   auto* Car=Cars[Index].Get();
   // Deterministic identical approach on the existing road. Subsequent motion is actual Chaos input.
   Place(Car,FVector(-3500,0,150),FRotator::ZeroRotator);
   Player->SetActorLocation(FVector(-3500,-210,104),false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately();
   return Next(11);
  }
  auto* Car=Cars.IsValidIndex(Index)?Cars[Index].Get():nullptr;
  if(!Car) return false;
  // Validate actual authored parking access before the controlled flat-road comparison.
  if(Stage==40)
  {
   Player->SetActorLocation(Car->GetActorLocation()-Car->GetActorRightVector()*210+FVector(0,0,70),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately(); return Next(41);
  }
  if(Stage==41 && Now-Phase>.5f)
  {
   FVector Eye; FRotator R; Player->GetActorEyesViewPoint(Eye,R);
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,90)-Eye).Rotation()); Key(PC,EKeys::F,IE_Pressed); return Next(42);
  }
  if(Stage==42 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   Test->AddInfo(FString::Printf(TEXT("Bay model %d: car=%s player=%s speed=%.2f health=%.1f possessed=%d"),Index,*Car->GetActorLocation().ToString(),*Player->GetActorLocation().ToString(),Car->GetVelocity().Size(),Car->GetHealthComponent()->GetCurrentHealth(),PC->GetPawn()==Car));
   if(!Test->TestTrue(TEXT("F enters each model in its authored bay"),PC->GetPawn()==Car)) return true;
   return Next(46);
  }
  if(Stage==46)
  {
   // Preserve live traffic: the same safe-gap policy as the original Sedan driving test.
   const FVector Exit(Car->GetActorLocation().X,0,89);
   for(TActorIterator<ANeonCityTrafficVehicle> It(W);It;++It)
    if(FVector::Dist2D(It->GetActorLocation(),Exit)<1200)
    {
     if(Now-Phase>20) { Test->AddError(TEXT("No safe traffic gap at fleet parking exit")); return true; }
     return false;
    }
   Key(PC,EKeys::W,IE_Pressed); return Next(43);
  }
  if(Stage==43)
  {
   if(Car->GetActorLocation().Y>100 && Now-Phase<6) return false;
   Test->AddInfo(FString::Printf(TEXT("Bay exit %d: location=%s speed=%.2f km/h elapsed=%.2f"),Index,*Car->GetActorLocation().ToString(),Car->GetDrivingComponent()->GetSpeedKmh(),Now-Phase));
   Shot(*FString::Printf(TEXT("Parking-Exit-%d.png"),Index));
   Test->TestTrue(TEXT("Each model drives from parking across driveway to road"),Car->GetActorLocation().Y<100);
   Test->TestEqual(TEXT("Normal parking exit causes no damage"),Car->GetHealthComponent()->GetCurrentHealth(),Car->GetHealthComponent()->GetMaxHealth());
   Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::SpaceBar,IE_Pressed); return Next(44);
  }
  if(Stage==44 && Now-Phase>2.f)
  { Key(PC,EKeys::SpaceBar,IE_Released); Key(PC,EKeys::F,IE_Pressed); return Next(45); }
  if(Stage==45 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Parking drive restores character"),PC->GetPawn()==Player.Get())) return true;
   Car->SetActorTransform(Parking[Index],false,nullptr,ETeleportType::TeleportPhysics); Zero(Car);
   if(++Index<4) return Next(40);
   Index=0; return Next(10);
  }
  if(Stage==11 && Now-Phase>1.1f)
  {
   FVector Eye; FRotator R; Player->GetActorEyesViewPoint(Eye,R);
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,90)-Eye).Rotation()); Key(PC,EKeys::F,IE_Pressed); return Next(12);
  }
  if(Stage==12 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F enters each actual vehicle"),PC->GetPawn()==Car)) return true;
   Test->TestTrue(TEXT("Engine running on possession"),Car->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Running);
   Test->TestTrue(TEXT("Driving loaned models does not confer ownership"),Registry->GetOwnedCount()==1+OtherOwned);
   Origin=Car->GetActorLocation(); Key(PC,EKeys::W,IE_Pressed); return Next(13);
  }
  if(Stage==13 && Now-Phase>2.5f)
  {
   const float Speed=Car->GetDrivingComponent()->GetSpeedKmh(); Speeds.Add(Speed);
   Test->AddInfo(FString::Printf(TEXT("Model %d: %.2f km/h after 2.5s W, travel %.1f cm, mass %.1f kg"),Index,Speed,FVector::Dist2D(Origin,Car->GetActorLocation()),Car->GetMesh()->GetBodyInstance()->GetBodyMass()));
   Test->TestTrue(TEXT("Each profile physically accelerates and travels"),Speed>3 && FVector::Dist2D(Origin,Car->GetActorLocation())>100);
   int32 Contacts=0; for(int32 I=0;I<4;++I) Contacts+=Car->GetChaosMovement()->GetWheelState(I).bInContact;
   Test->TestTrue(TEXT("Chaos wheel contacts preserved"),Contacts>=2);
   Test->TestTrue(TEXT("Live engine RPM interface"),Car->GetDrivingComponent()->GetEngineRPM()>0);
   Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::D,IE_Pressed); return Next(14);
  }
  if(Stage==14 && Now-Phase>.25f)
  {
   Test->TestTrue(TEXT("Each model steers physical front wheels"),Car->GetChaosMovement()->Wheels[0]->GetSteerAngle()>.5f);
   Key(PC,EKeys::D,IE_Released); Key(PC,EKeys::SpaceBar,IE_Pressed); return Next(15);
  }
  if(Stage==15 && Now-Phase>2.f)
  {
   Test->TestTrue(TEXT("Each model brakes to rest"),FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.5f);
   Key(PC,EKeys::SpaceBar,IE_Released); Key(PC,EKeys::F,IE_Pressed); return Next(16);
  }
  if(Stage==16 && Now-Phase>.3f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F restores original character for every model"),PC->GetPawn()==Player.Get())) return true;
   Test->TestTrue(TEXT("Parked engine off"),Car->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Off);
   Car->SetActorTransform(Parking[Index],false,nullptr,ETeleportType::TeleportPhysics); Zero(Car);
   if(++Index<4) return Next(10);
   Test->TestTrue(TEXT("Sports accelerates faster than Sedan"),Speeds[1]>Speeds[0]+1.f);
   Test->TestTrue(TEXT("Sports accelerates faster than SUV and Pickup"),Speeds[1]>Speeds[2]+2.f && Speeds[1]>Speeds[3]+2.f);
   Test->TestTrue(TEXT("Explicit grant records SUV"),Registry->GrantVehicle(Cars[2].Get()));
   Test->TestFalse(TEXT("Duplicate ownership grant rejected"),Registry->GrantVehicle(Cars[2].Get()));
   Test->TestEqual(TEXT("Exactly two owned city vehicles plus island fleet"),Registry->GetOwnedCount(),2+OtherOwned);
   Index=0; return Next(30);
  }
  if(Stage==30)
  {
   Place(Car,FVector(-1800,0,150),FRotator::ZeroRotator);
   Player->SetActorLocation(FVector(-1800,-210,104),false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately();
   Wall=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall.Get()); Wall->SetRootComponent(Box);
   Box->SetBoxExtent(FVector(40,220,170)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent(); Wall->SetActorLocation(FVector(-600,0,170));
   return Next(31);
  }
  if(Stage==31 && Now-Phase>1.f)
  {
   if(!Test->TestTrue(TEXT("Re-enter collision-test Sedan"),Car->GetVehicleInteraction()->Enter(Player.Get()))) return true;
   HealthBefore=Car->GetHealthComponent()->GetCurrentHealth();
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector(1800,0,0)); Car->GetMesh()->WakeAllRigidBodies(); Key(PC,EKeys::W,IE_Pressed); return Next(32);
  }
  if(Stage==32)
  {
   if(Car->GetDamageState()->GetCollisionDamageCount()==0 && Now-Phase<4) return false;
   Test->TestTrue(TEXT("Real rigid-body collision generates damage"),Car->GetDamageState()->GetCollisionDamageCount()>0 && Car->GetHealthComponent()->GetCurrentHealth()<HealthBefore);
   Test->TestFalse(TEXT("Moderate collision is survivable"),Car->GetHealthComponent()->IsDead());
   Test->TestTrue(TEXT("Collision feedback reports actual health loss"),Car->GetDamageState()->GetRecentDamage()>0);
   Test->AddInfo(FString::Printf(TEXT("Collision health %.1f -> %.1f, impacts %d"),HealthBefore,Car->GetHealthComponent()->GetCurrentHealth(),Car->GetDamageState()->GetCollisionDamageCount()));
   Shot(TEXT("Collision-Feedback.png")); Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::SpaceBar,IE_Pressed); return Next(33);
  }
  if(Stage==33 && Now-Phase>2.f)
  {
   Key(PC,EKeys::SpaceBar,IE_Released);
   auto* Health=Car->GetHealthComponent(); Health->SetCurrentHealth(50);
   Test->TestTrue(TEXT("Damaged state at half durability"),Car->GetDamageState()->GetCondition()==ENeonCityVehicleCondition::Damaged);
   FNeonCityDamageEvent E; E.DamageAmount=30; E.DamageType=ENeonCityDamageType::Ballistic; E.SourceActor=Player.Get(); E.Instigator=PC;
   Test->TestEqual(TEXT("Weapon damage uses shared DamageSystem"),W->GetSubsystem<UDamageSystem>()->ApplyDamage(Car,E),30.f);
   Test->TestTrue(TEXT("Critical state limits drive power"),Car->GetDamageState()->GetCondition()==ENeonCityVehicleCondition::Critical && Car->GetDamageState()->GetDrivePowerMultiplier()<.5f);
   E.DamageAmount=100; W->GetSubsystem<UDamageSystem>()->ApplyDamage(Car,E);
   Test->TestTrue(TEXT("Destroyed vehicle Actor and identity retained"),IsValid(Car) && Car->GetHealthComponent()->IsDead() && Car->GetDamageState()->GetCondition()==ENeonCityVehicleCondition::Destroyed);
   return Next(34);
  }
  if(Stage==34 && Now-Phase>.5f)
  {
   Test->TestTrue(TEXT("Destroyed engine stalled"),Car->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Stalled);
   Test->TestEqual(TEXT("Destroyed vehicle cannot be healed back implicitly"),Car->GetHealthComponent()->Heal(100),0.f);
   auto Snapshot=GI->GetSubsystem<UNeonCitySaveSubsystem>()->BuildVehicleSnapshot();
   Test->TestEqual(TEXT("Save interface exports only owned records"),Snapshot.Num(),2+OtherOwned);
   const auto* Record=Snapshot.FindByPredicate([&](const auto& R){return R.VehicleID==Car->VehicleID;});
   Test->TestTrue(TEXT("Snapshot preserves identity, type, live location and destroyed condition"),Record && Record->VehicleType==ENeonCityVehicleType::Sedan && Record->Condition==0 && Record->Location.Equals(Car->GetActorLocation(),1));
   Test->TestTrue(TEXT("Save subsystem declares implemented disk persistence"),GI->GetSubsystem<UNeonCitySaveSubsystem>()->IsPersistenceImplemented());
   Shot(TEXT("Vehicle-Destroyed.png")); return Next(35);
  }
  if(Stage==35 && Now-Phase>.6f && !FScreenshotRequest::IsScreenshotRequested())
  {
   Car->GetVehicleInteraction()->RestoreDriver(true); Wall->Destroy();
   Test->AddInfo(TEXT("Four Chaos models, actual F/W/D/Space driving, readonly phone catalogue, impact and ballistic health, engine/condition feedback and stable ownership snapshots verified.")); return true;
  }
  return false;
 }
private:
 static void Zero(ANeonCityBaseVehicle* Car) { Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); Car->GetMesh()->WakeAllRigidBodies(); }
 static void Place(ANeonCityBaseVehicle* Car,FVector P,FRotator R) { Car->SetActorLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics); Zero(Car); }
 FAutomationTestBase* Test; double Started=0; int32 Stage=0,Index=0,OtherOwned=0; float Phase=0,HealthBefore=0; int64 MoneyBefore=0; FVector Origin;
 TArray<TWeakObjectPtr<ANeonCityBaseVehicle>> Cars; TArray<FTransform> Parking; TArray<float> Speeds;
 TWeakObjectPtr<ANeonCityPlayerCharacter> Player; TWeakObjectPtr<AActor> Wall;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityFleet,"NeonCity.Vehicle.ExpansionPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityFleet::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFleetTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityVehicleRegistry,"NeonCity.Vehicle.RegistryInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityVehicleRegistry::RunTest(const FString& Parameters)
{
 auto* GI=NewObject<UGameInstance>();
 auto* Registry=NewObject<UVehicleOwnershipSubsystem>(GI);
 FNeonCityOwnedVehicle Original; Original.VehicleID=FGuid::NewGuid(); Original.VehicleType=ENeonCityVehicleType::SUV; Original.Location=FVector(100,200,30); Original.Condition=.4f;
 TestTrue(TEXT("Valid snapshot restores registry before world binding"),Registry->RestoreRegistrySnapshot({Original}));
 TestEqual(TEXT("Value snapshot round trip condition"),Registry->BuildSaveSnapshot()[0].Condition,.4f);
 TestFalse(TEXT("Duplicate ID rejected atomically"),Registry->RestoreRegistrySnapshot({Original,Original}));
 auto Invalid=Original; Invalid.Condition=-1; TestFalse(TEXT("Negative condition rejected"),Registry->RestoreRegistrySnapshot({Invalid}));
 Invalid=Original; Invalid.Condition=2; TestFalse(TEXT("Condition overflow rejected"),Registry->RestoreRegistrySnapshot({Invalid}));
 Invalid=Original; Invalid.VehicleID.Invalidate(); TestFalse(TEXT("Missing identity rejected"),Registry->RestoreRegistrySnapshot({Invalid}));
 Invalid=Original; Invalid.VehicleType=static_cast<ENeonCityVehicleType>(255); TestFalse(TEXT("Unsupported model rejected"),Registry->RestoreRegistrySnapshot({Invalid}));
 TestTrue(TEXT("Rejected import retains original registry"),Registry->IsOwned(Original.VehicleID) && Registry->GetOwnedCount()==1);
 auto* Profile=NewObject<UVehicleData>(); TestTrue(TEXT("Default tuning valid"),Profile->IsValidProfile()); Profile->Weight=-1; TestFalse(TEXT("Invalid physical mass rejected"),Profile->IsValidProfile());
 return true;
}
#endif
