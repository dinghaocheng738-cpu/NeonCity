#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftFlightStatusComponent.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/AircraftTestArea.h"
#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "GameFramework/SpringArmComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PhysicsVolume.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWindow.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftProfile,"NeonCity.Aircraft.ProfileAndSchema",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAircraftProfile::RunTest(const FString&)
{
 const auto* Data=GetDefault<UAircraftVehicleData>(); TestTrue(TEXT("Helicopter profile valid"),Data->IsValidProfile());
 TestTrue(TEXT("Stable primary asset ID"),Data->GetPrimaryAssetId().IsValid());
 auto* Bad=NewObject<UAircraftVehicleData>(); Bad->LiftForce=100; TestFalse(TEXT("Cannot configure inadequate lift"),Bad->IsValidProfile());
 Bad->LiftForce=28000; Bad->Mass=-1; TestFalse(TEXT("Reject invalid mass"),Bad->IsValidProfile());
 Bad->Mass=1200;Bad->HoverPower=1;TestFalse(TEXT("Rotor profile leaves a real boost range above hover"),Bad->IsValidProfile());
 Bad->HoverPower=.55f;Bad->PowerResponse=0;TestFalse(TEXT("Nonresponsive engine profile is rejected"),Bad->IsValidProfile());
 auto* Model=NewObject<UNeonCitySaveGame>(); Model->FormatID=TEXT("NeonCity.SaveGame"); Model->SaveID=FGuid::NewGuid();
 TestEqual(TEXT("Compatible additive schema at the current version"),Model->SaveVersion,UNeonCitySaveGame::CurrentVersion);
 Model->Aircraft.SetNum(33); FString Error; TestFalse(TEXT("Aircraft save count bounded"),Model->ValidateEnvelope(Error));
 const auto* CDO=GetDefault<ANeonCityAircraftVehicleBase>();
 TestTrue(TEXT("Shared dynamic surfaces and future driver contracts"),CDO->Implements<UDynamicSurfaceInteractionInterface>() && CDO->Implements<UAircraftDriverInterface>());
 TestEqual(TEXT("Legacy landing damage threshold unchanged"),GetDefault<UPhysicalImpactComponent>()->MinimumFloorImpactSpeed,1000.f);
 return true;
}
namespace NeonCityAircraftTests
{
void Key(APlayerController* PC,FKey KeyName,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),KeyName,Event,FPlatformTime::Cycles64())); }
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr; ANeonCityAircraftVehicleBase* Aircraft=nullptr;
 UNeonCitySurfaceMovementComponent* Move=nullptr; UNeonCitySaveSubsystem* Save=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<2 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  for(TActorIterator<ANeonCityAircraftVehicleBase> It(W);It;++It) if(It->AircraftID==UAircraftWorldSubsystem::HelicopterID()) Aircraft=*It;
  Move=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement()); Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  return Aircraft && Move;
 }
 void PlacePlayer(FVector Position)
 {
  Move->SetMovementMode(MOVE_Falling); Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); Move->StopMovementImmediately();
  Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
 void PlaceAircraft(FVector Position,FRotator Rotation=FRotator(0,-90,0))
 {
  Aircraft->GetAircraftMovement()->StopEngine(); Aircraft->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector); Aircraft->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Aircraft->SetActorLocationAndRotation(Position,Rotation,false,nullptr,ETeleportType::TeleportPhysics); Aircraft->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.75f);
 }
 void LookAtHull()
 {
  FVector Eye; FRotator Rotation; Player->GetActorEyesViewPoint(Eye,Rotation);
  PC->SetControlRotation((Aircraft->GetActorLocation()-Eye).Rotation());
 }
 void Shot(const TCHAR* Name)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityAircraftCapture"))) return;
  const FVector Target=Aircraft->GetActorLocation()+FVector(0,0,50),Eye=Target+(Aircraft->GetActorLocation().Z<800?FVector(100,-750,350):FVector(-650,-850,500));
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation()); PC->SetViewTarget(Camera);
  PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0);
  bool Captured=false;
  if(auto* Client=W->GetGameViewport();Client && Client->Viewport)
  {
   Client->Viewport->Draw(false); TArray<FColor> Pixels;
   if(Client->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pixel:Pixels) Pixel.A=255;
    const auto Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Aircraft")),true);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Aircraft")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("Aircraft capture failed: %s"),Name);
  PC->SetViewTarget(PC->GetPawn()); Camera->Destroy();
 }
};

class FFlightScenario : public IAutomationLatentCommand
{
public:
 explicit FFlightScenario(FAutomationTestBase* InTest,bool PowerLoop=false):Test(InTest),bPowerLoop(PowerLoop) {}
 bool Next(int32 Value) { Stage=Value; Phase=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>(bPowerLoop?300:200)) { Test->AddError(FString::Printf(TEXT("Flight timeout stage %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   if(bPowerLoop && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
    if(auto Window=F.W->GetGameViewport()->GetWindow())Window->Resize(FVector2D(1600,900));
   Test->TestTrue(TEXT("Helicopter spawned on pad with physics"),F.Aircraft->GetHull()->IsSimulatingPhysics() && F.Aircraft->GetAircraftMovement()->IsGrounded());
   F.PlacePlayer(FVector(-3000,1600,150)); return Next(1);
  }
  Evidence.Record(F.W,F.PC,Stage);
  const float E=F.W->GetTimeSeconds()-Phase; auto* Flight=F.Aircraft->GetAircraftMovement();
  if(Stage==1)
  {
   const FVector Target(-3000,2135,F.Player->GetActorLocation().Z);
   if(FVector::Dist2D(Target,F.Player->GetActorLocation())>10)
   { F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),1); if(E>12) { Test->AddError(TEXT("Helipad walking obstructed")); return true; } return false; }
   F.Move->StopMovementImmediately(); F.LookAtHull();
   Test->TestTrue(TEXT("Interaction focuses helicopter"),F.Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==F.Aircraft);
   F.Shot(TEXT("A-Helipad.png")); Key(F.PC,EKeys::F,IE_Pressed); return Next(2);
  }
  if(Stage==2 && E>.3f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F possesses aircraft"),F.PC->GetPawn()==F.Aircraft)) return true;
   Test->TestTrue(TEXT("Engine starts and grounded state retained"),Flight->IsEngineRunning() && Flight->GetFlightState()==EAircraftFlightState::Grounded);
   Test->TestTrue(TEXT("Pilot sees live flight instruments"),F.Aircraft->GetFlightStatus()->IsVisible());
   if(bPowerLoop)Test->TestTrue(TEXT("Normal helicopter HUD includes real weapon ammunition"),F.Aircraft->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("Ammo")));
   Test->TestFalse(TEXT("Unrelated controller cannot adjust rotor power"),F.Aircraft->SubmitPowerInput(nullptr,1));
   Test->TestTrue(TEXT("Aircraft camera context active"),F.Aircraft->FindComponentByClass<UNeonCityCameraExtensionComponent>()->GetCameraContext()==ENeonCityCameraContext::Aircraft);
   Test->TestFalse(TEXT("Unrelated controller rejected by future driver port"),F.Aircraft->SubmitFlightInput_Implementation(nullptr,FVector::OneVector,1));
   Start=F.Aircraft->GetActorLocation(); Arm=F.Aircraft->FindComponentByClass<USpringArmComponent>()->TargetArmLength;
   Key(F.PC,EKeys::SpaceBar,IE_Pressed); return Next(3);
  }
  if(Stage==3 && E>5)
  {
   Test->TestTrue(TEXT("Space takes off above city roofs"),F.Aircraft->GetActorLocation().Z-Start.Z>1800 && Flight->GetFlightState()==EAircraftFlightState::TakingOff);
   Test->TestTrue(TEXT("Height increases third person camera distance"),F.Aircraft->FindComponentByClass<USpringArmComponent>()->TargetArmLength>Arm+50);
   Test->TestTrue(TEXT("Seated character remains disabled"),F.Move->MovementMode==MOVE_None);
   Test->TestFalse(TEXT("Midair ordinary exit refused"),F.Aircraft->ExitAircraft_Implementation());
   F.Save->ConfigureSlot(TEXT("NeonCity_Test_Aircraft_Flight"));
   Test->TestFalse(TEXT("Midair occupied save refused"),F.Save->SaveGame());
   if(bPowerLoop) { Key(F.PC,EKeys::F,IE_Pressed); }
   F.Shot(TEXT("B-Takeoff.png")); Key(F.PC,EKeys::SpaceBar,IE_Released); return Next(4);
  }
  if(Stage==4 && E>2)
  {
   if(bPowerLoop)
   {
    Key(F.PC,EKeys::F,IE_Released);
    Test->TestTrue(TEXT("Airborne F keeps pilot seated and explains safe recovery"),F.PC->GetPawn()==F.Aircraft && F.Aircraft->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("Land and stop")));
    Test->TestTrue(TEXT("Capture actual airborne exit feedback"),FNeonCityVisualEvidence::CaptureHUD(F.W,TEXT("Flight-ExitFeedback.png")));
   }
   Start=F.Aircraft->GetActorLocation(); return Next(5);
  }
  if(Stage==5 && E>5)
  {
   Test->TestTrue(TEXT("Five-second hover holds altitude within 50cm"),FMath::Abs(F.Aircraft->GetActorLocation().Z-Start.Z)<50);
   Test->TestTrue(TEXT("Hover drift within 80cm"),FVector::Dist2D(Start,F.Aircraft->GetActorLocation())<80);
   Test->TestTrue(TEXT("Hover state"),Flight->GetFlightState()==EAircraftFlightState::Hovering);
   Test->AddInfo(FString::Printf(TEXT("Hover height drift %.2f, horizontal drift %.2f"),F.Aircraft->GetActorLocation().Z-Start.Z,FVector::Dist2D(Start,F.Aircraft->GetActorLocation())));
   F.Shot(TEXT("D-Hover.png")); Start=F.Aircraft->GetActorLocation(); Key(F.PC,EKeys::W,IE_Pressed); return Next(6);
  }
  if(Stage==6 && E>2)
  {
   Test->TestTrue(TEXT("W forward flight"),FVector::DotProduct(F.Aircraft->GetActorLocation()-Start,FVector(0,-1,0))>500 && Flight->GetFlightState()==EAircraftFlightState::Flying);
   if(bPowerLoop)return Next(30);
   Key(F.PC,EKeys::W,IE_Released); return Next(7);
  }
  if(Stage==30 && E>6)
  {
   const auto T=Flight->GetTelemetry();CruiseSpeed=T.SpeedKmh;
   Test->TestTrue(TEXT("Normal rotor thrust builds real cruising velocity"),CruiseSpeed>45 && T.CurrentPower>=F.Aircraft->GetProfile()->HoverPower-.01f);
   Test->TestTrue(TEXT("Capture actual cruise instruments"),FNeonCityVisualEvidence::CaptureHUD(F.W,TEXT("Flight-Cruise.png")));
   Key(F.PC,EKeys::LeftShift,IE_Pressed);return Next(31);
  }
  if(Stage==31 && E>.3f)
  {
   const auto T=Flight->GetTelemetry();
   Test->TestTrue(TEXT("Shift increases target with a measured engine response lag"),T.TargetPower>T.CurrentPower+.01f && T.CurrentPower>F.Aircraft->GetProfile()->HoverPower);
   return Next(32);
  }
  if(Stage==32 && E>6)
  {
   const auto T=Flight->GetTelemetry();FastSpeed=T.SpeedKmh;
   Test->TestTrue(TEXT("Sustained power produces faster physical flight"),T.CurrentPower>.95f && FastSpeed>CruiseSpeed*1.2f && FastSpeed<=F.Aircraft->GetProfile()->MaxSpeed+5);
   Test->AddInfo(FString::Printf(TEXT("Cruise %.2f km/h, fast %.2f km/h, power %.3f"),CruiseSpeed,FastSpeed,T.CurrentPower));
   Test->TestTrue(TEXT("Capture actual fast flight instruments"),FNeonCityVisualEvidence::CaptureHUD(F.W,TEXT("Flight-Fast.png")));
   Key(F.PC,EKeys::LeftShift,IE_Released);Key(F.PC,EKeys::LeftControl,IE_Pressed);return Next(33);
  }
  if(Stage==33 && E>1.2f)
  {
   const auto T=Flight->GetTelemetry();
   Test->TestTrue(TEXT("Controlled deceleration retains inertia rather than snapping to zero speed"),T.SpeedKmh<FastSpeed*.9f && T.SpeedKmh>5);
   Test->TestTrue(TEXT("Capture actual controlled deceleration"),FNeonCityVisualEvidence::CaptureHUD(F.W,TEXT("Flight-Slow.png")));
   return Next(35);
  }
  if(Stage==35 && E>4)
  {
   const auto T=Flight->GetTelemetry();
   Test->TestTrue(TEXT("Ctrl decelerates even while forward input remains held"),T.SpeedKmh<3 && T.CurrentPower<.62f && T.EngineRunning);
   Key(F.PC,EKeys::LeftControl,IE_Released);Key(F.PC,EKeys::W,IE_Released);return Next(34);
  }
  if(Stage==34 && E>2)
  {
   const auto T=Flight->GetTelemetry();
   Test->TestTrue(TEXT("Powered deceleration returns to safe hover without dropping the airframe"),T.SpeedKmh<2 && Flight->GetFlightState()==EAircraftFlightState::Hovering && FMath::Abs(F.Aircraft->GetActorLocation().Z-Start.Z)<75);
   Key(F.PC,EKeys::Tab,IE_Pressed);return Next(40);
  }
  if(Stage==40 && E>.3f)
  {
   Key(F.PC,EKeys::Tab,IE_Released);
   auto* Mount=F.Aircraft->FindComponentByClass<UVehicleCombatComponent>();auto* Weapon=Mount?Mount->GetManager()->GetWeapon():nullptr;
   Test->TestTrue(TEXT("Tab selects the normal helicopter rocket mount and updates HUD"),Weapon && Weapon->GetVehicleData()->WeaponType==EVehicleWeaponType::Rocket
    && F.Aircraft->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(Weapon->GetData()->DisplayName.ToString()));
   return Next(41);
  }
  if(Stage==41 && E>.2f) { Key(F.PC,EKeys::Tab,IE_Pressed);return Next(42); }
  if(Stage==42 && E>.3f)
  {
   Key(F.PC,EKeys::Tab,IE_Released);
   auto* Mount=F.Aircraft->FindComponentByClass<UVehicleCombatComponent>();auto* Weapon=Mount?Mount->GetManager()->GetWeapon():nullptr;
   Test->TestTrue(TEXT("Missile selection is visible with real capacity"),Weapon && Weapon->GetVehicleData()->WeaponType==EVehicleWeaponType::Missile
    && F.Aircraft->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(Weapon->GetData()->DisplayName.ToString()));
   Key(F.PC,EKeys::V,IE_Pressed);Key(F.PC,EKeys::RightMouseButton,IE_Pressed);return Next(43);
  }
  if(Stage==43 && E>.4f)
  {
   auto* Mount=F.Aircraft->FindComponentByClass<UVehicleCombatComponent>();
   Test->TestTrue(TEXT("Arm/lock input reaches the existing combat system and pilot HUD"),Mount && Mount->IsArmed() && Mount->IsLockRequested()
    && F.Aircraft->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("ARMED")));
   Test->TestTrue(TEXT("Capture actual selected-missile HUD"),FNeonCityVisualEvidence::CaptureHUD(F.W,TEXT("Flight-MissileHUD.png")));
   Key(F.PC,EKeys::V,IE_Released);Key(F.PC,EKeys::RightMouseButton,IE_Released);return Next(44);
  }
  if(Stage==44 && E>.2f) { Key(F.PC,EKeys::V,IE_Pressed);Key(F.PC,EKeys::Tab,IE_Pressed);return Next(45); }
  if(Stage==45 && E>.3f)
  {
   Key(F.PC,EKeys::V,IE_Released);Key(F.PC,EKeys::Tab,IE_Released);
   Test->TestFalse(TEXT("Pilot can return weapons to safe before landing"),F.Aircraft->FindComponentByClass<UVehicleCombatComponent>()->IsArmed());
   return Next(7);
  }
  if(Stage==7 && E>2) { Start=F.Aircraft->GetActorLocation(); Key(F.PC,EKeys::E,IE_Pressed); return Next(8); }
  if(Stage==8 && E>2)
  {
   Test->TestTrue(TEXT("E right strafe over city"),F.Aircraft->GetActorLocation().X-Start.X>500); F.Shot(TEXT("C-CityFlight.png"));
   Key(F.PC,EKeys::E,IE_Released); return Next(9);
  }
  if(Stage==9 && E>2) { Start=F.Aircraft->GetActorLocation(); Key(F.PC,EKeys::Q,IE_Pressed); return Next(10); }
  if(Stage==10 && E>2)
  {
   Test->TestTrue(TEXT("Q left strafe"),Start.X-F.Aircraft->GetActorLocation().X>500); Key(F.PC,EKeys::Q,IE_Released); return Next(11);
  }
  if(Stage==11 && E>2) { Start=F.Aircraft->GetActorLocation(); Key(F.PC,EKeys::S,IE_Pressed); return Next(12); }
  if(Stage==12 && E>2)
  {
   Test->TestTrue(TEXT("S reverse flight"),F.Aircraft->GetActorLocation().Y-Start.Y>500); Key(F.PC,EKeys::S,IE_Released); Yaw=F.Aircraft->GetActorRotation().Yaw; Key(F.PC,EKeys::D,IE_Pressed); return Next(13);
  }
  if(Stage==13 && E>1.5f)
  {
   Test->TestTrue(TEXT("D yaw turn"),FMath::FindDeltaAngleDegrees(Yaw,F.Aircraft->GetActorRotation().Yaw)>35); Key(F.PC,EKeys::D,IE_Released);
   ViewYaw=F.PC->GetControlRotation().Yaw;
   F.PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,20.f,.016f,1,FPlatformTime::Cycles64())); return Next(14);
  }
  if(Stage==14 && E>.3f)
  {
   Test->TestTrue(TEXT("Mouse changes view independently"),FMath::Abs(FMath::FindDeltaAngleDegrees(ViewYaw,F.PC->GetControlRotation().Yaw))>1); return Next(15);
  }
  if(Stage==15)
  {
   // Automated pilot uses only the public driver port to return; no transform/velocity writes.
   const FVector Offset=FVector(-3000,2450,F.Aircraft->GetActorLocation().Z)-F.Aircraft->GetActorLocation();
   const FVector V=F.Aircraft->GetVelocity(); const FVector Flat(V.X,V.Y,0);
   const FVector Desired=Offset.GetClampedToMaxSize(bPowerLoop?2000:800)*.8f;
   const FVector Accel=(Desired-Flat)*1.6f+Flat*F.Aircraft->GetProfile()->AirResistance;
   const FRotator Heading(0,F.Aircraft->GetActorRotation().Yaw,0);
   FVector Command=Heading.UnrotateVector(Accel)/(F.Aircraft->GetProfile()->Acceleration*100); Command.Z=0;
   F.Aircraft->SubmitFlightInput_Implementation(F.PC,Command,0);
   if(Offset.Size()<20 && Flat.Size()<25)
   { F.Aircraft->SubmitFlightInput_Implementation(F.PC,FVector::ZeroVector,0); Key(F.PC,EKeys::C,IE_Pressed); return Next(16); }
   if(E>(bPowerLoop?50:35)) { Test->AddError(FString::Printf(TEXT("Return approach failed: %s"),*Offset.ToString())); return true; }
  }
  if(Stage==16)
  {
   if(!Flight->IsGrounded() && E>1) bLanding|=Flight->GetFlightState()==EAircraftFlightState::Landing;
   if(Flight->IsGrounded())
   {
    if(GroundAt==0) GroundAt=F.W->GetTimeSeconds();
    if(F.W->GetTimeSeconds()-GroundAt<.8f) return false;
    Key(F.PC,EKeys::C,IE_Released); Test->TestTrue(TEXT("Landing state observed"),bLanding);
    Test->TestTrue(TEXT("Safe landing Grounded"),Flight->GetFlightState()==EAircraftFlightState::Grounded);
    Test->TestEqual(TEXT("Normal flight/landing causes no damage"),F.Aircraft->GetHealthComponent()->GetCurrentHealth(),600.f);
    FTransform Exit;
    if(!F.Aircraft->GetAircraftInteraction()->FindExitTransform(Exit)) Test->AddInfo(FString::Printf(TEXT("Exit diagnostic position %s velocity %s rotation %s grounded %d"),*F.Aircraft->GetActorLocation().ToString(),*F.Aircraft->GetVelocity().ToString(),*F.Aircraft->GetActorRotation().ToString(),Flight->IsGrounded()));
    Test->TestTrue(TEXT("Clear external exit exists"),F.Aircraft->GetAircraftInteraction()->FindExitTransform(Exit));
    auto* Obstacle=F.W->SpawnActor<AActor>(); auto* Block=NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Block); Block->InitBoxExtent(FVector(600,600,130));
    Block->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Block->SetCollisionResponseToAllChannels(ECR_Ignore); Block->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Block->RegisterComponent(); Obstacle->SetActorLocation(F.Aircraft->GetActorLocation());
    Test->TestFalse(TEXT("Blocked exits do not eject player into geometry"),F.Aircraft->ExitAircraft_Implementation());
    Test->TestTrue(TEXT("Blocked exit retains possession"),F.PC->GetPawn()==F.Aircraft);
    Obstacle->Destroy();
    Test->TestTrue(TEXT("Grounded pilot can save using safe external player position"),F.Save->SaveGame()); F.Save->DeleteSave();
    F.Shot(TEXT("E-Landed.png")); Key(F.PC,EKeys::F,IE_Pressed); return Next(17);
   }
   if(E>25) { Test->AddError(FString::Printf(TEXT("Descent failed: %s"),*F.Aircraft->GetActorLocation().ToString())); return true; }
  }
  if(Stage==17 && E>.8f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("F returns possession/visibility/collision"),F.PC->GetPawn()==F.Player && !F.Player->IsHidden() && F.Player->GetActorEnableCollision());
   Test->TestTrue(TEXT("Exit stands on static floor outside cabin"),F.Move->IsMovingOnGround() && FVector::Dist2D(F.Player->GetActorLocation(),F.Aircraft->GetActorLocation())>220);
   Test->TestFalse(TEXT("Unoccupied engine stopped"),Flight->IsEngineRunning());
   Test->TestFalse(TEXT("Flight HUD leaves with the pilot"),F.Aircraft->GetFlightStatus()->IsVisible());
   Test->TestTrue(TEXT("Unoccupied aircraft resets transient power"),Flight->GetTelemetry().CurrentPower==0 && Flight->GetTelemetry().TargetPower==0);
   F.Shot(TEXT("E-Exit.png")); return true;
  }
  return false;
 }
 FNeonCityVisualEvidence Evidence{TEXT("E-HelicopterFlight")};
 FAutomationTestBase* Test; FScene F; int32 Stage=0; double Started=0; float Phase=0,Yaw=0,Arm=0,ViewYaw=0,GroundAt=0,CruiseSpeed=0,FastSpeed=0; FVector Start; bool bLanding=false,bPowerLoop=false;
};
class FPowerScenario : public FFlightScenario
{ public:explicit FPowerScenario(FAutomationTestBase* Test):FFlightScenario(Test,true) {} };
class FAircraftDamage : public IAutomationLatentCommand
{
public:
 explicit FAircraftDamage(FAutomationTestBase* InTest):Test(InTest) {}
 bool Next(int32 Value) { Stage=Value; Phase=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Aircraft damage timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   Test->TestTrue(TEXT("Aircraft exposes unarmed combat"),F.Aircraft->GetCombatState_Implementation()==ENeonCityCombatState::Unarmed);
   F.PlaceAircraft(FVector(-3000,2450,250)); F.Aircraft->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(0);
   F.Aircraft->GetHull()->SetPhysicsLinearVelocity(FVector(0,0,-700)); return Next(1);
  }
  const float E=F.W->GetTimeSeconds()-Phase; auto* Flight=F.Aircraft->GetAircraftMovement(); auto* Health=F.Aircraft->GetHealthComponent();
  if(Stage==1 && E>1.5f)
  {
   Test->TestTrue(TEXT("Low-altitude real ground contact damages hull"),Health->GetCurrentHealth()<600 && Health->GetCurrentHealth()>500);
   Test->AddInfo(FString::Printf(TEXT("Low crash health %.1f"),Health->GetCurrentHealth()));
   F.PlaceAircraft(FVector(-3000,2650,180),FRotator(0,90,0)); Health->ApplyPersistenceHealth(600,600);
   F.Aircraft->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(0); F.Aircraft->GetHull()->SetPhysicsLinearVelocity(FVector(0,3200,0)); return Next(2);
  }
  if(Stage==2 && E>2)
  {
   Test->TestTrue(TEXT("High-speed barrier collision produces critical/disabled damage"),Health->GetCurrentHealth()<=150);
   Test->TestTrue(TEXT("Disabled stops powered flight"),Flight->GetCondition()==EAircraftCondition::Disabled && !Flight->IsEngineRunning());
   Test->TestTrue(TEXT("Shared impact evidence exists"),F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents().ContainsByPredicate([&](const auto& Event){return Event.TargetActor.Get()==F.Aircraft && Event.AppliedDamage>0;}));
   Test->TestTrue(TEXT("Crash remains finite and angular speed bounded"),!F.Aircraft->GetActorTransform().ContainsNaN() && F.Aircraft->GetVelocity().Size()<6001 && F.Aircraft->GetHull()->GetPhysicsAngularVelocityInRadians().Size()<4.1f);
   Test->AddInfo(FString::Printf(TEXT("High crash health %.1f"),Health->GetCurrentHealth())); F.Shot(TEXT("F-Impact.png"));
   Health->ApplyPersistenceHealth(600,600); F.PlaceAircraft(FVector(-3000,2450,1400)); Flight->StartEngine(); return Next(3);
  }
  if(Stage==3 && E>1)
  { StartZ=F.Aircraft->GetActorLocation().Z; Flight->StopEngine(); return Next(4); }
  if(Stage==4 && E>.8f)
  {
   Test->TestTrue(TEXT("Engine off permits physical descent"),F.Aircraft->GetActorLocation().Z<StartZ-150 && Flight->GetFlightState()==EAircraftFlightState::Disabled);
   F.PlaceAircraft(FVector(2500,-5500,-250)); Health->ApplyPersistenceHealth(600,600); return Next(5);
  }
  if(Stage==5 && E>.3f)
  {
   auto* Water=F.Aircraft->FindComponentByClass<UAircraftWaterInteraction>();
   Test->TestTrue(TEXT("Aircraft has distinct Submerged state"),Water && Water->GetWaterState()==EAircraftWaterState::Submerged);
   Test->TestFalse(TEXT("Submerged aircraft cannot start powered flight"),Flight->StartEngine());
   Test->TestTrue(TEXT("No car driving component on aircraft"),!F.Aircraft->FindComponentByClass<UVehicleComponent>());
   F.Shot(TEXT("F-Water.png")); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int32 Stage=0; double Started=0; float Phase=0,StartZ=0;
};
class FAircraftPersistence : public IAutomationLatentCommand
{
public:
 FAircraftPersistence(FAutomationTestBase* T,FString M):Test(T),Mode(MoveTemp(M)) {}
 void Verify(const FNeonCityAircraftSaveData& Record)
 {
  Test->TestTrue(TEXT("Stable aircraft ID restored"),F.Aircraft->AircraftID==Record.AircraftID);
  Test->TestTrue(TEXT("Aircraft transform restored"),F.Aircraft->GetActorTransform().Equals(Record.Transform,.1f));
  Test->TestEqual(TEXT("Aircraft ownership restored"),F.Aircraft->Owned,Record.Owned);
  Test->TestTrue(TEXT("Aircraft condition restored"),FMath::IsNearlyEqual(F.Aircraft->GetHealthComponent()->GetCurrentHealth(),Record.Condition*F.Aircraft->GetProfile()->Durability,.01f));
  Test->TestTrue(TEXT("Restore clears engine and motion"),!F.Aircraft->GetAircraftMovement()->IsEngineRunning() && F.Aircraft->GetVelocity().IsNearlyZero(.1f));
  const auto Power=F.Aircraft->GetAircraftMovement()->GetTelemetry();
  Test->TestTrue(TEXT("Load clears transient rotor power and target"),Power.CurrentPower==0 && Power.TargetPower==0);
 }
 void Manifest(UNeonCitySaveGame* Model)
 {
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Aircraft")),true);
  const auto& R=Model->Aircraft[0];
  const FString Text=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"vehicleId\":\"%s\",\"condition\":%.6f,\"owned\":%d,\"transform\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*Model->SaveID.ToString(),*R.AircraftID.ToString(),R.Condition,R.Owned,*R.Transform.ToHumanReadableString().Replace(TEXT("\n"),TEXT(" ")).Replace(TEXT("\r"),TEXT(" ")));
  Test->TestTrue(TEXT("Cross-session aircraft evidence saved"),FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Aircraft")/(TEXT("Session")+Mode+TEXT(".json")))));
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Aircraft persistence timeout")); return true; }
  if(!F.Ready()) return false;
  F.Save->ConfigureSlot(Mode==TEXT("RoundTrip")?TEXT("NeonCity_Test_Aircraft_RoundTrip"):TEXT("NeonCity_Test_Aircraft_CrossSession"));
  if(Mode==TEXT("B"))
  {
   if(!Test->TestTrue(TEXT("Session B finds disk save"),F.Save->DoesSaveExist())) return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection());
   if(!Test->TestTrue(TEXT("Disk aircraft record readable"),Model.IsValid() && Model->Aircraft.Num()==1)) return true;
   FString Previous; FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Aircraft/SessionA.json")));
   uint32 PID=0; FParse::Value(*Previous,TEXT("\"processId\":"),PID);
   Test->TestTrue(TEXT("Session B is new process with same save identity"),PID && PID!=FPlatformProcess::GetCurrentProcessId() && Previous.Contains(Model->SaveID.ToString()));
   Test->TestTrue(TEXT("Cross-session aircraft load"),F.Save->LoadGame()); Verify(Model->Aircraft[0]); Manifest(Model.Get()); F.Shot(TEXT("G-SessionB.png")); F.Save->DeleteSave(); return true;
  }
  const auto GroundBefore=F.W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>()->BuildPersistenceSnapshot();
  if(!Test->TestTrue(TEXT("Ordinary world has a populated ground vehicle registry"),!GroundBefore.IsEmpty()))return true;
  F.Save->DeleteSave(); F.PlaceAircraft(FVector(-3050,2520,135),FRotator(0,-85,0));
  F.Aircraft->Owned=false; F.Aircraft->GetHealthComponent()->ApplyPersistenceHealth(300,600);
  if(!Test->TestTrue(TEXT("Aircraft state writes via SaveSubsystem"),F.Save->SaveGame())) { Test->AddInfo(F.Save->GetLastDetail()); return true; }
  TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("One distinct aircraft record and unchanged land registry count"),Model.IsValid() && Model->HasAircraft && Model->Aircraft.Num()==1 && Model->Vehicles.Num()==GroundBefore.Num())) return true;
  for(const auto& Before:GroundBefore)
  {
   const auto* Saved=Model->Vehicles.FindByPredicate([&](const auto& Record){return Record.VehicleID==Before.VehicleID;});
   if(!Test->TestTrue(TEXT("Aircraft save preserves each ground vehicle identity, ownership and state"),Saved
    && Saved->VehicleType==Before.VehicleType && Saved->DataAssetID==Before.DataAssetID && Saved->Owned==Before.Owned
    && Saved->Transform.Equals(Before.Transform,.1f) && FMath::IsNearlyEqual(Saved->Condition,Before.Condition,.0001f)))return true;
  }
  if(Mode==TEXT("A")) { Manifest(Model.Get()); F.Shot(TEXT("G-SessionA.png")); return true; }
  F.PlaceAircraft(FVector(-3000,2350,135)); F.Aircraft->Owned=true; F.Aircraft->GetHealthComponent()->Heal(1000);
  Test->TestTrue(TEXT("Round trip load"),F.Save->LoadGame()); Verify(Model->Aircraft[0]); F.Shot(TEXT("G-Restored.png"));
  auto Bad=Model->Aircraft[0]; Bad.AircraftID=FGuid::NewGuid(); Model->Aircraft.Add(Bad); Model->Aircraft[0].Condition=-1;
  Test->TestTrue(TEXT("Write malformed aircraft fixture"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Bad IDs/condition safely normalize"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Invalid record uses default condition"),F.Aircraft->GetHealthComponent()->GetCurrentHealth(),600.f);
  Model->Aircraft.SetNum(1); Model->Aircraft[0].Condition=.5f;
  Model->Aircraft[0].Transform.SetLocation(FVector(1400,2400,500));
  Test->TestTrue(TEXT("Write obstructed aircraft fixture"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Aircraft inside building safely uses authored default"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Restored aircraft is back at unobstructed helipad"),FVector::Dist2D(F.Aircraft->GetActorLocation(),FVector(-3000,2450,137))<1);
  Model->HasAircraft=false; Model->Aircraft.Reset();
  Test->TestTrue(TEXT("Write legacy Version 1 fixture"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Legacy Version 1 loads aircraft defaults"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Default owned aircraft retains stable ID"),F.Aircraft->Owned && F.Aircraft->AircraftID==UAircraftWorldSubsystem::HelicopterID());
  F.Save->DeleteSave(); return true;
 }
 FAutomationTestBase* Test; FString Mode; FScene F; double Started=0;
};
}

#define AIRCRAFT_TEST(Class,Name,Scenario) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityAircraftTests::Scenario>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
AIRCRAFT_TEST(FAircraftFlight,"NeonCity.Aircraft.FlightPIE",FFlightScenario)
AIRCRAFT_TEST(FAircraftPowerLoop,"NeonCity.Aircraft.PowerLoopPIE",FPowerScenario)
AIRCRAFT_TEST(FAircraftImpact,"NeonCity.Aircraft.DamageWaterPIE",FAircraftDamage)
#undef AIRCRAFT_TEST
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftSave,"NeonCity.Aircraft.PersistencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAircraftSave::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityAircraftTests::FAircraftPersistence>(this,TEXT("RoundTrip")));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FAircraftCrossSession,"NeonCity.Aircraft.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FAircraftCrossSession::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityAircraftSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode); Commands.Add(Mode); }
}
bool FAircraftCrossSession::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityAircraftTests::FAircraftPersistence>(this,Parameters));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
