#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingFlightComponent.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedWingProfile,"NeonCity.FixedWing.Profile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFixedWingProfile::RunTest(const FString&)
{
 const auto* D=GetDefault<UFixedWingAircraftData>(); TestTrue(TEXT("Valid stable fixed wing profile"),D->IsValidProfile() && D->GetPrimaryAssetId().IsValid());
 auto* Independent=NewObject<UFixedWingAircraftData>();Independent->HoverPower=1;Independent->PowerResponse=0;Independent->LiftForce=0;
 TestTrue(TEXT("Fixed wing validity is independent of rotor power/lift fields"),Independent->IsValidProfile());
 UAircraftVehicleComponent* Isolated=NewObject<UFixedWingFlightComponent>();
 TestFalse(TEXT("Fixed wing rejects the rotor power command"),Isolated->SetPowerInput(1));
 TestTrue(TEXT("Polymorphic instruments expose fixed-wing throttle rather than rotor power"),Isolated->GetTelemetry().FixedWing && Isolated->GetTelemetry().CurrentPower==0);
 auto* Bad=NewObject<UFixedWingAircraftData>(); Bad->StallSpeed=Bad->TakeoffSpeed+1; TestFalse(TEXT("Invalid speed ordering rejected"),Bad->IsValidProfile());
 TestEqual(TEXT("Zero forward airspeed has no lift"),UFixedWingFlightComponent::EvaluateLift(*D,0,1,false),0.f);
 const float L1=UFixedWingFlightComponent::EvaluateLift(*D,20,.1f,false),L2=UFixedWingFlightComponent::EvaluateLift(*D,40,.1f,false);
 TestTrue(TEXT("Lift follows forward speed squared"),FMath::IsNearlyEqual(L2,4*L1));
 TestTrue(TEXT("Stall loses lift"),UFixedWingFlightComponent::EvaluateLift(*D,15,.1f,true)<L1*.4f);
 const auto* Pawn=GetDefault<ANeonCityFixedWingAircraft>();
 TestTrue(TEXT("Reuses Aircraft base and movement subtype"),Pawn->IsA<ANeonCityAircraftVehicleBase>() && Pawn->GetAircraftMovement()->IsA<UFixedWingFlightComponent>());
 TestEqual(TEXT("Three metadata-only hardpoints"),Pawn->GetWeaponHardpoints_Implementation().Num(),3);
 TestTrue(TEXT("Fixed gear stays deployed"),Pawn->IsLandingGearDeployed_Implementation());
 TestFalse(TEXT("LOD simulation not falsely advertised"),Pawn->SupportsDistantSimulation_Implementation());
 return true;
}
namespace NeonCityFixedWingTests
{
void Key(APlayerController* PC,FKey K,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64())); }
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 ANeonCityFixedWingAircraft* Plane=nullptr; UNeonCitySaveSubsystem* Save=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr;
  Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<2 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  for(TActorIterator<AFixedWingTestArea> It(W);It;++It) Plane=It->GetAircraft();
  if(!Plane) Plane=W->SpawnActor<AFixedWingTestArea>()->GetAircraft();
  Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); return Plane!=nullptr;
 }
 void PlayerAt(FVector P)
 {
  Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling); Player->GetCharacterMovement()->StopMovementImmediately();
  Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
 void Fixture(FVector P,FRotator R,FVector Velocity=FVector::ZeroVector)
 {
  Plane->GetFlight()->StopEngine(); Plane->GetHull()->SetPhysicsLinearVelocity(Velocity); Plane->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Plane->SetActorLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics); Plane->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.3f);
 }
 void Control(float T,float Pitch,float Roll=0,float Yaw=0,bool Brake=false)
 { Plane->SubmitAircraftControls_Implementation(PC,T,Pitch,Roll,Yaw,Brake); }
 void Shot(const TCHAR* Name)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityFixedWingCapture"))) return;
  const FVector Target=Plane->GetActorLocation(),Eye=Target+FVector(1000,-1500,800);
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation()); PC->SetViewTarget(Camera);
  PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0);
  bool Captured=false;
  if(auto* Client=W->GetGameViewport();Client && Client->Viewport)
  {
   Client->Viewport->Draw(false); TArray<FColor> Pixels;
   if(Client->Viewport->ReadPixels(Pixels))
   {
    for(auto& P:Pixels) P.A=255;
    const auto Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("FixedWing")),true);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("FixedWing")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("Fixed wing screenshot failed"));
  PC->SetViewTarget(PC->GetPawn()); Camera->Destroy();
 }
 FString Diagnostic() const
 { if(!Plane) return TEXT("Scene not ready"); return FString::Printf(TEXT("pos %s rot %s vel %s air %.1f throttle %.2f lift %.0f state %d hp %.1f"),*Plane->GetActorLocation().ToString(),*Plane->GetActorRotation().ToString(),*Plane->GetVelocity().ToString(),Plane->GetFlight()->GetAirspeed(),Plane->GetFlight()->GetThrottle(),Plane->GetFlight()->GetLift(),int(Plane->GetFlight()->GetFixedWingState()),Plane->GetHealthComponent()->GetCurrentHealth()); }
};
class FFlight : public IAutomationLatentCommand
{
public:
 explicit FFlight(FAutomationTestBase* T):Test(T) {}
 bool Next(int S) { UE_LOG(LogTemp,Display,TEXT("FixedWing transfer/flight completed stage %d"),Stage); Test->AddInfo(FString::Printf(TEXT("Stage %d: %s"),Stage,*F.Diagnostic())); Stage=S; At=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>160) { Test->AddError(FString::Printf(TEXT("Flight timeout %d %s Player %s"),Stage,*F.Diagnostic(),F.Player?*F.Player->GetActorLocation().ToString():TEXT("missing"))); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) if(It->GetClass()==ANeonCityStarterSedan::StaticClass()) { Car=*It; break; }
   if(!Test->TestNotNull(TEXT("Existing car available for airport transfer"),Car)) return true;
   Car->GetChaosMovement()->ResetVehicle(); Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
   Car->SetActorLocationAndRotation(FVector(6200,-400,145),FRotator(0,90,0),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->WakeAllRigidBodies(); Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1); F.PlayerAt(FVector(6000,-400,155)); return Next(100);
  }
  Evidence.Record(F.W,F.PC,Stage);
  const float E=F.W->GetTimeSeconds()-At; auto* Flight=F.Plane->GetFlight();
  if(Stage==100 && E>1)
  {
   if(!Test->TestTrue(TEXT("Player enters existing car at runway access"),Car->GetVehicleInteraction()->Enter(F.Player))) return true;
   Car->GetDrivingComponent()->SetDriveInput(.35f); return Next(101);
  }
  if(Stage==101)
  {
   const float Speed=Car->GetChaosMovement()->GetForwardSpeed();
   const FVector Aim=FVector(6200,Car->GetActorLocation().Y+300,Car->GetActorLocation().Z)-Car->GetActorLocation();
   Car->GetDrivingComponent()->SetSteerInput(FMath::Clamp(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Aim.Rotation().Yaw)*.035f,-.6f,.6f));
   Car->GetDrivingComponent()->SetDriveInput(FMath::Clamp((600.f-Speed)/300.f,0.f,1.f));
   Car->GetDrivingComponent()->SetBrakeInput(FMath::Clamp((Speed-650.f)/300.f,0.f,1.f));
   if(Car->GetActorLocation().Y>6000) { Car->GetDrivingComponent()->SetDriveInput(0); Car->GetDrivingComponent()->SetBrakeInput(1); return Next(102); }
   if(E>30) {
    Test->AddInfo(FString::Printf(TEXT("Car rotation %s contact %s"),*Car->GetActorRotation().ToString(),*Car->GetChaosMovement()->GetWheelState(0).ContactPoint.ToString()));
    Test->AddError(FString::Printf(TEXT("Car did not reach runway: %s speed %.1f engine %d RPM %.1f gear %d throttle %.1f brake %.1f contact %d health %.1f"),*Car->GetActorLocation().ToString(),Car->GetDrivingComponent()->GetSpeedKmh(),int(Car->GetDrivingComponent()->GetEngineState()),Car->GetDrivingComponent()->GetEngineRPM(),Car->GetChaosMovement()->GetCurrentGear(),Car->GetChaosMovement()->GetThrottleInput(),Car->GetChaosMovement()->GetBrakeInput(),Car->GetChaosMovement()->GetWheelState(0).bInContact,Car->GetHealthComponent()->GetCurrentHealth())); return true; }
  }
  if(Stage==102 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
  {
   Test->TestTrue(TEXT("Car arrives runway and player exits"),Car->GetVehicleInteraction()->Exit()); WalkCorner=FVector(F.Player->GetActorLocation().X,Car->GetActorLocation().Y+400,F.Player->GetActorLocation().Z); return Next(105);
  }
  if(Stage==105 || Stage==106)
  {
   const FVector Target(Stage==105?WalkCorner.X:6200,WalkCorner.Y,F.Player->GetActorLocation().Z);
   const float Distance=FVector::Dist2D(Target,F.Player->GetActorLocation());
   if(Distance>20) { F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),FMath::Clamp(Distance/200,.15f,1.f)); return false; }
   return Next(Stage==105?106:103);
  }
  if(Stage==103)
  {
   const FVector Target(15410,6300,F.Player->GetActorLocation().Z);
   if(FVector::Dist2D(Target,F.Player->GetActorLocation())>20) { F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),FMath::Clamp(float(FVector::Dist2D(Target,F.Player->GetActorLocation()))/200,.15f,1.f)); return false; }
   return Next(104);
  }
  if(Stage==104)
  {
   const FVector Target(15380,7000,F.Player->GetActorLocation().Z);
   if(FVector::Dist2D(Target,F.Player->GetActorLocation())>15) { F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),FMath::Clamp(float(FVector::Dist2D(Target,F.Player->GetActorLocation()))/200,.15f,1.f)); return false; }
   F.Player->GetCharacterMovement()->StopMovementImmediately(); return Next(1);
  }
  if(Stage==1 && E>1)
  {
   F.PC->SetControlRotation((F.Plane->GetActorLocation()-F.Player->GetPawnViewLocation()).Rotation());
   Test->TestTrue(TEXT("Interaction detects plane"),F.Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==F.Plane);
   Key(F.PC,EKeys::F,IE_Pressed); return Next(2);
  }
  if(Stage==2 && E>.4f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F enters fixed wing"),F.PC->GetPawn()==F.Plane)) return true;
   Test->TestTrue(TEXT("Engine starts on ground"),Flight->IsEngineRunning() && Flight->IsGrounded());
   Key(F.PC,EKeys::Down,IE_Pressed); return Next(3);
  }
  if(Stage==3 && E>1)
  {
   Test->TestTrue(TEXT("Zero throttle pitch cannot lift off"),Flight->IsGrounded() && F.Plane->GetActorLocation().Z<480);
   Key(F.PC,EKeys::Down,IE_Released); Key(F.PC,EKeys::W,IE_Pressed); return Next(4);
  }
  if(Stage==4 && E>2)
  {
   Test->TestTrue(TEXT("Throttle input accelerates ground taxi"),Flight->GetThrottle()>.5f && Flight->GetAirspeed()>1 && Flight->IsGrounded());
   Test->TestTrue(TEXT("Taxi or takeoff roll state"),Flight->GetFixedWingState()==EFixedWingFlightState::Taxiing || Flight->GetFixedWingState()==EFixedWingFlightState::TakeoffRoll);
   return Next(5);
  }
  if(Stage==5 && Flight->GetAirspeed()>=22.5f)
  { F.Shot(TEXT("A-Taxi.png")); Key(F.PC,EKeys::W,IE_Released); Key(F.PC,EKeys::Down,IE_Pressed); return Next(6); }
  if(Stage==6)
  {
   if(E>1) { Key(F.PC,EKeys::Down,IE_Released); F.Control(1,FMath::Clamp((12.f-F.Plane->GetActorRotation().Pitch)*.08f,-.6f,.6f)); }
   if(F.Plane->GetActorLocation().Z>1800)
   {
    Test->TestFalse(TEXT("At takeoff speed aircraft lifts off"),Flight->IsGrounded());
    Test->TestFalse(TEXT("Midair F exit rejected"),F.Plane->ExitAircraft_Implementation());
    F.Shot(TEXT("A-Takeoff.png")); return Next(7);
   }
   if(E>15) { Test->AddError(TEXT("Takeoff failed ")+F.Diagnostic()); return true; }
  }
  if(Stage==7)
  {
   const FRotator R=F.Plane->GetActorRotation(); F.Control(.55f,FMath::Clamp((5.f-R.Pitch)*.1f,-.5f,.5f),FMath::Clamp((-25.f-R.Roll)*.04f,-.5f,.5f));
   if(E>3)
   { Test->TestTrue(TEXT("Bank control tilts airframe"),FMath::Abs(R.Roll)>15); F.Shot(TEXT("B-Bank.png")); return Next(8); }
  }
  if(Stage==8)
  {
   const FRotator R=F.Plane->GetActorRotation(); F.Control(0,FMath::Clamp((25.f-R.Pitch)*.08f,-.5f,.5f),FMath::Clamp(-R.Roll*.04f,-.5f,.5f));
   if(Flight->GetFixedWingState()==EFixedWingFlightState::Stalling)
   { Test->TestTrue(TEXT("Throttle cut and climb naturally stalls"),Flight->GetAirspeed()<19); F.Shot(TEXT("C-Stall.png")); return Next(9); }
   if(E>25) { Test->AddError(TEXT("Stall not reached ")+F.Diagnostic()); return true; }
  }
  if(Stage==9)
  {
   const FRotator R=F.Plane->GetActorRotation(); F.Control(1,FMath::Clamp((-15.f-R.Pitch)*.1f,-1.f,1.f),FMath::Clamp(-R.Roll*.04f,-.5f,.5f));
   if(Flight->GetAirspeed()>25 && Flight->GetFixedWingState()!=EFixedWingFlightState::Stalling)
   { Test->TestTrue(TEXT("Nose down / throttle recovers stall"),!Flight->IsGrounded()); F.Shot(TEXT("C-Recovered.png")); return true; }
   if(E>18) { Test->AddError(TEXT("Recovery failed ")+F.Diagnostic()); return true; }
  }
  return false;
 }
 FNeonCityVisualEvidence Evidence{TEXT("F-FixedWingFlight")};
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle* Car=nullptr; FVector WalkCorner; int Stage=0; double Started=0; float At=0;
};
class FApproach : public IAutomationLatentCommand
{
public:
 explicit FApproach(FAutomationTestBase* T):Test(T) {}
 bool Next(int S) { Test->AddInfo(FString::Printf(TEXT("Approach %d %s"),Stage,*F.Diagnostic())); Stage=S; At=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Approach timeout ")+F.Diagnostic()); return true; }
  if(Stage==0) { if(!F.Ready()) return false; F.PlayerAt(FVector(15380,7000,520)); return Next(1); }
  const float E=F.W->GetTimeSeconds()-At; auto* Flight=F.Plane->GetFlight();
  if(Stage==1 && E>1)
  {
   if(!Test->TestTrue(TEXT("Board for approach fixture"),F.Plane->EnterAircraft_Implementation(F.Player))) return true;
   F.Fixture(FVector(-4500,0,2400),FRotator(0,0,0),FVector(3500,0,0)); Flight->StartEngine();
   for(TActorIterator<ANeonCityNPCCharacter> It(F.W);It;++It) NPCs.Add(*It,It->GetActorLocation());
   for(TActorIterator<ANeonCityTrafficVehicle> It(F.W);It;++It) Traffic.Add(*It,It->GetActorLocation());
   return Next(2);
  }
  if(Stage==2)
  {
   const FRotator R=F.Plane->GetActorRotation(); F.Control(.4f,FMath::Clamp((0.f-R.Pitch)*.1f,-.5f,.5f));
   // World time can advance across an asset/render stall while Chaos clamps its
   // simulated step. Keep the same physical crossing/altitude requirement and
   // minimum observation, with a bounded arrival window rather than one sample.
   const bool Crossed=F.Plane->GetActorLocation().X>2000 && F.Plane->GetActorLocation().Z>1700;
   if(E>2.5f && (Crossed || E>6.f))
   {
    Test->AddInfo(FString::Printf(TEXT("City crossing elapsed=%.3f %s"),E,*F.Diagnostic()));
    Test->TestTrue(TEXT("Real flight traverses South Beach road"),Crossed);
    int MovingNPC=0,MovingTraffic=0;
    for(const auto& P:NPCs) { Test->TestTrue(TEXT("Pedestrian survives overflight"),IsValid(P.Key) && !P.Key->GetHealthComponent()->IsDead()); if(FVector::Dist2D(P.Key->GetActorLocation(),P.Value)>20) ++MovingNPC; }
    for(const auto& P:Traffic) { Test->TestTrue(TEXT("Traffic retains health"),IsValid(P.Key) && !P.Key->GetHealthComponent()->IsDead()); if(FVector::Dist2D(P.Key->GetActorLocation(),P.Value)>20) ++MovingTraffic; }
    Test->TestTrue(TEXT("Existing pedestrian and traffic behavior continues"),NPCs.Num()>=10 && MovingNPC>0 && MovingTraffic>0);
    F.Shot(TEXT("D-CityOverflight.png"));
    F.Fixture(FVector(21000,7000,1800),FRotator(4,180,0),FVector(-2900,0,-120)); Flight->StartEngine(); return Next(3);
   }
  }
  if(Stage==3)
  {
   const FRotator R=F.Plane->GetActorRotation();
   const float DesiredSink=Flight->GetAltitude()>200?-150.f:-65.f;
   const float DesiredPitch=FMath::Clamp(4.f+(DesiredSink-F.Plane->GetVelocity().Z)*.02f,-5.f,12.f);
   F.Control(.1f,FMath::Clamp((DesiredPitch-R.Pitch)*.12f,-.5f,.5f),FMath::Clamp(-R.Roll*.04f,-.3f,.3f),0,Flight->IsGrounded());
   if(Flight->IsGrounded())
   { F.Control(0,0,0,0,true); F.Shot(TEXT("E-Touchdown.png")); return Next(4); }
   if(E>18) { Test->AddError(TEXT("Approach failed ")+F.Diagnostic()); return true; }
  }
  if(Stage==4)
  {
   F.Control(0,0,0,0,true);
   if(F.Plane->GetVelocity().Size()<50 && E>1)
   {
    Test->TestTrue(TEXT("Normal touchdown and braking preserve health"),F.Plane->GetHealthComponent()->GetCurrentHealth()==700);
    Test->TestTrue(TEXT("Safe landing gear fixed deployed"),F.Plane->IsLandingGearDeployed_Implementation());
    Test->TestTrue(TEXT("Ground low speed permits ordinary exit"),F.Plane->ExitAircraft_Implementation());
    Start=F.Player->GetActorLocation(); return Next(5);
   }
   if(E>12) { Test->AddError(TEXT("Ground braking failed ")+F.Diagnostic()); return true; }
  }
  if(Stage==5)
  {
   F.Player->AddMovementInput(FVector(1,0,0));
   if(E>1.2f)
   {
    Test->TestTrue(TEXT("Character movement restored outside aircraft"),F.PC->GetPawn()==F.Player && !F.Player->IsHidden() && F.Player->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist2D(Start,F.Player->GetActorLocation())>150);
    F.Shot(TEXT("E-Exit.png")); return true;
   }
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int Stage=0; double Started=0; float At=0; FVector Start;
 TMap<ANeonCityNPCCharacter*,FVector> NPCs; TMap<ANeonCityTrafficVehicle*,FVector> Traffic;
};
class FDamageWater : public IAutomationLatentCommand
{
public:
 explicit FDamageWater(FAutomationTestBase* T):Test(T) {}
 bool Next(int S) { Test->AddInfo(FString::Printf(TEXT("Damage %d %s"),Stage,*F.Diagnostic())); Stage=S; At=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Damage timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   F.Fixture(FVector(10000,7000,900),FRotator(-20,180,15),FVector(-4000,0,-1500)); F.Plane->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(0); return Next(1);
  }
  const float E=F.W->GetTimeSeconds()-At; auto* Flight=F.Plane->GetFlight(); auto* Health=F.Plane->GetHealthComponent();
  if(Stage==1 && E>2)
  {
   Test->TestTrue(TEXT("Bad fast/steep landing causes unified damage"),Health->GetCurrentHealth()<699);
   Test->TestTrue(TEXT("Impact evidence includes aircraft"),F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents().ContainsByPredicate([&](const auto& V){return V.TargetActor.Get()==F.Plane && V.AppliedDamage>0;}));
   F.Shot(TEXT("F-BadLanding.png")); Health->ApplyPersistenceHealth(700,700);
   F.Fixture(FVector(1400,800,600),FRotator(0,90,0),FVector(0,4000,0)); F.Plane->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(0); return Next(2);
  }
  if(Stage==2 && E>2)
  {
   Test->TestTrue(TEXT("Low flight into existing city building causes structural damage"),Health->GetCurrentHealth()<=175);
   F.Shot(TEXT("F-BuildingImpact.png")); F.Fixture(FVector(0,7000,4000),FRotator::ZeroRotator,FVector(2400,0,0));
   Health->ApplyPersistenceHealth(50,700); Flight->SetControls(1,1,1,1,false); StartZ=4000; return Next(3);
  }
  if(Stage==3 && E>1)
  {
   Test->TestTrue(TEXT("Disabled aircraft cannot power and falls with inertia"),Flight->GetFixedWingState()==EFixedWingFlightState::Disabled && !Flight->IsEngineRunning() && F.Plane->GetActorLocation().Z<StartZ-200 && F.Plane->GetVelocity().X>1000);
   Health->ApplyPersistenceHealth(700,700); F.Fixture(FVector(2000,-6000,100),FRotator(-5,0,0),FVector(2000,0,-300)); Flight->StartEngine(); return Next(4);
  }
  if(Stage==4 && E>1.5f)
  {
   Test->TestTrue(TEXT("Ditching enters submerged water state"),F.Plane->FindComponentByClass<UAircraftWaterInteraction>()->GetWaterState()==EAircraftWaterState::Submerged);
   Test->TestFalse(TEXT("Water disables engine"),Flight->IsEngineRunning()); Test->TestFalse(TEXT("Water blocks restart"),Flight->StartEngine());
   int Boats=0; for(TActorIterator<ANeonCityMarineVehicleBase> It(F.W);It;++It) ++Boats;
   Test->TestTrue(TEXT("Marine craft still registered"),Boats>=1);
   F.PlayerAt(FVector(1000,-5000,-180)); F.Shot(TEXT("G-Ditching.png")); return Next(5);
  }
  if(Stage==5 && E>.6f)
  { Test->TestTrue(TEXT("Existing character swimming coexists with submerged aircraft"),F.Player->GetCharacterMovement()->IsSwimming()); return true; }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int Stage=0; double Started=0; float At=0,StartZ=0;
};

class FSurfaceEnvelope : public IAutomationLatentCommand
{
public:
 explicit FSurfaceEnvelope(FAutomationTestBase* T):Test(T) {}
 bool Next(int S) { Test->AddInfo(FString::Printf(TEXT("Envelope %d %s"),Stage,*F.Diagnostic())); Stage=S; At=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>80) { Test->AddError(TEXT("Surface / envelope timeout")); return true; }
  if(Stage==0) { if(!F.Ready()) return false; F.PlayerAt(FVector(15000,6750,750)); return Next(1); }
  const float E=F.W->GetTimeSeconds()-At; auto* Flight=F.Plane->GetFlight();
  if(Stage==1 && E>5)
  {
   auto* Move=F.Player->GetCharacterMovement(); const auto* Base=Cast<UPrimitiveComponent>(F.Player->GetMovementBaseObject());
   Test->TestTrue(TEXT("Character stands five seconds on wing through native based movement"),Move->IsMovingOnGround() && Base && Base->GetOwner()==F.Plane);
   Test->TestTrue(TEXT("Standing does not damage or roll aircraft"),F.Plane->GetHealthComponent()->GetCurrentHealth()==700 && FMath::Abs(F.Plane->GetActorRotation().Roll)<5);
   Test->TestTrue(TEXT("Start idle engine while rider stands on wing"),Flight->StartEngine()); return Next(11);
  }
  if(Stage==11 && E>1)
  {
   const auto* Base=Cast<UPrimitiveComponent>(F.Player->GetMovementBaseObject());
   Test->TestTrue(TEXT("Grounded engine idle still supports rider"),F.Player->GetCharacterMovement()->IsMovingOnGround() && Base && Base->GetOwner()==F.Plane);
   Flight->StopEngine(); F.PlayerAt(FVector(15380,7000,550)); return Next(2);
  }
  if(Stage==2 && E>1)
  {
   if(!Test->TestTrue(TEXT("Board after standing on wing"),F.Plane->EnterAircraft_Implementation(F.Player))) return true;
   F.Fixture(FVector(0,7000,14000),FRotator(-25,180,0),FVector(-2800,0,-1300)); Flight->StartEngine();
   Speed=F.Plane->GetVelocity().Size(); return Next(3);
  }
  if(Stage==3)
  {
   F.Control(0,FMath::Clamp((-25.f-F.Plane->GetActorRotation().Pitch)*.1f,-.5f,.5f));
   if(E>2)
   {
    Test->TestTrue(TEXT("Unpowered dive gains airspeed under gravity"),F.Plane->GetVelocity().Size()>Speed+150);
    F.Fixture(FVector(0,7000,14000),FRotator(-35,180,0),FVector(-6000,0,-4000)); Flight->StartEngine(); return Next(4);
   }
  }
  if(Stage==4)
  {
   F.Control(1,FMath::Clamp((-35.f-F.Plane->GetActorRotation().Pitch)*.1f,-.5f,.5f));
   if(E>3)
   {
    Test->TestTrue(TEXT("Drag and speed governor bound high-speed dive"),F.Plane->GetVelocity().Size()<6200 && !F.Plane->GetActorTransform().ContainsNaN());
    Yaw=F.Plane->GetActorRotation().Yaw; F.Control(.4f,0,0,.6f); return Next(5);
   }
  }
  if(Stage==5 && E>1)
  {
   Test->TestTrue(TEXT("Rudder provides auxiliary yaw"),FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,F.Plane->GetActorRotation().Yaw))>2);
   Test->TestTrue(TEXT("Camera speed FOV active"),F.Plane->GetAircraftCamera()->FieldOfView>88);
   return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int Stage=0; double Started=0; float At=0,Speed=0,Yaw=0;
};
class FPersistence : public IAutomationLatentCommand
{
public:
 FPersistence(FAutomationTestBase* T,FString M):Test(T),Mode(MoveTemp(M)) {}
 const FNeonCityAircraftSaveData* Record(UNeonCitySaveGame* Model) { return Model?Model->Aircraft.FindByPredicate([](const auto& R){return R.AircraftID==AFixedWingTestArea::PrototypeID();}):nullptr; }
 void Verify(const FNeonCityAircraftSaveData& R)
 {
  Test->TestTrue(TEXT("Aircraft identity/ownership/condition restored"),F.Plane->AircraftID==R.AircraftID && F.Plane->Owned==R.Owned && FMath::IsNearlyEqual(F.Plane->GetHealthComponent()->GetCurrentHealth(),R.Condition*700,.01f));
  Test->TestTrue(TEXT("Transform restored on legal ground"),F.Plane->GetActorTransform().Equals(R.Transform,.1f) && F.Plane->IsSafeRestoreTransform(F.Plane->GetActorTransform()));
  Test->TestTrue(TEXT("No engine, velocity or controller inputs restored"),!F.Plane->GetFlight()->IsEngineRunning() && F.Plane->GetVelocity().IsNearlyZero(.1f) && F.Plane->GetFlight()->GetThrottle()==0);
 }
 void Manifest(UNeonCitySaveGame* M)
 {
  const auto* R=Record(M); IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("FixedWing")),true);
  FString Fleet; for(const auto& V:M->Vehicles) Fleet+=V.VehicleID.ToString()+FString::Printf(TEXT(":%d:%.3f;"),V.Owned,V.Condition);
  const FString JSON=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"aircraftId\":\"%s\",\"condition\":%.6f,\"owned\":%d,\"fleet\":\"%s\",\"transform\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*M->SaveID.ToString(),*R->AircraftID.ToString(),R->Condition,R->Owned,*Fleet,*R->Transform.ToHumanReadableString().Replace(TEXT("\n"),TEXT(" ")).Replace(TEXT("\r"),TEXT(" ")));
  Test->TestTrue(TEXT("Cross-process manifest written"),FFileHelper::SaveStringToFile(JSON,*(FPaths::ProjectSavedDir()/TEXT("FixedWing")/(TEXT("Session")+Mode+TEXT(".json")))));
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Save timeout")); return true; }
  if(!bReady) { if(!F.Ready()) return false; bReady=true; At=F.W->GetTimeSeconds(); return false; }
  if(F.W->GetTimeSeconds()-At<1) return false;
  F.Save->ConfigureSlot(Mode==TEXT("RoundTrip")?TEXT("NeonCity_Test_FixedWing_RoundTrip"):TEXT("NeonCity_Test_FixedWing_Sessions"));
  if(Mode==TEXT("B"))
  {
   TStrongObjectPtr<UNeonCitySaveGame> M(F.Save->ReadSaveForInspection());
   if(!Test->TestTrue(TEXT("Fresh process reads fixed wing record"),M.IsValid() && Record(M.Get()))) return true;
   Test->TestTrue(TEXT("Session B load succeeds"),F.Save->LoadGame()); Verify(*Record(M.Get()));
   auto* Ownership=F.W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
   for(const auto& V:M->Vehicles) Test->TestEqual(TEXT("Cross-process land vehicle ownership retained"),Ownership->IsOwned(V.VehicleID),V.Owned);
   Manifest(M.Get()); F.Shot(TEXT("H-SessionB.png")); F.Save->DeleteSave(); return true;
  }
  F.Save->DeleteSave(); F.Plane->Owned=false; F.Plane->GetHealthComponent()->ApplyPersistenceHealth(350,700);
  auto* Ownership=F.W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
  for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) if(It->VehicleID.IsValid()) Ownership->GrantVehicle(*It);
  if(!Test->TestTrue(TEXT("Shared SaveSubsystem writes fixed wing and fleet"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
  TStrongObjectPtr<UNeonCitySaveGame> M(F.Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("Additive current version retains helicopter, plane, land and marine"),M.IsValid() && Record(M.Get()) && M->Aircraft.Num()==2 && M->Vehicles.Num()==4 && M->Marine.Num()>0 && M->SaveVersion==UNeonCitySaveGame::CurrentVersion)) return true;
  if(Mode==TEXT("A")) { Manifest(M.Get()); F.Shot(TEXT("H-SessionA.png")); return true; }
  const auto Original=*Record(M.Get()); F.Plane->Owned=true; F.Plane->GetHealthComponent()->Heal(1000);
  Test->TestTrue(TEXT("Round trip succeeds"),F.Save->LoadGame()); Verify(Original);
  for(const auto& V:M->Vehicles) Test->TestEqual(TEXT("Land ownership unchanged by aircraft load"),Ownership->IsOwned(V.VehicleID),V.Owned);
  auto* R=M->Aircraft.FindByPredicate([](const auto& V){return V.AircraftID==AFixedWingTestArea::PrototypeID();});
  for(const FVector Unsafe:{FVector(0,7000,5000),FVector(1400,2400,500),FVector(2000,-5000,-300)})
  {
   R->Transform.SetLocation(Unsafe); Test->TestTrue(TEXT("Unsafe fixture written"),F.Save->WriteTestFixture(M.Get()));
   Test->TestTrue(TEXT("Unsafe saved flight/building/water loads safely"),F.Save->LoadGame());
   Test->TestTrue(TEXT("Fallback stand preserves ownership and condition"),F.Plane->GetActorTransform().Equals(AFixedWingTestArea::StandTransform(),.1f) && !F.Plane->Owned && F.Plane->GetHealthComponent()->GetCurrentHealth()==350);
  }
  auto* Obstacle=F.W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Obstacle); Obstacle->SetRootComponent(Box);
  Box->InitBoxExtent(FVector(350,450,150)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
  Obstacle->SetActorLocation(AFixedWingTestArea::StandTransform().GetLocation());
  auto SafeRecords=M->Aircraft; TArray<FString> Warnings;
  F.W->GetSubsystem<UAircraftWorldSubsystem>()->NormalizePersistenceSnapshot(SafeRecords,Warnings);
  F.W->GetSubsystem<UAircraftWorldSubsystem>()->ApplyPersistenceSnapshot(SafeRecords);
  Test->TestTrue(TEXT("Occupied fallback stand selects a clear supported alternate"),F.Plane->IsSafeRestoreTransform(F.Plane->GetActorTransform()) && FVector::Dist(F.Plane->GetActorLocation(),AFixedWingTestArea::StandTransform().GetLocation())>1000);
  Obstacle->Destroy();
  M->Aircraft.RemoveAll([](const auto& V){return V.AircraftID==AFixedWingTestArea::PrototypeID();});
  Test->TestTrue(TEXT("Pre-fixed-wing Version 1 fixture written"),F.Save->WriteTestFixture(M.Get()));
  Test->TestTrue(TEXT("Legacy save supplies authored fixed-wing default"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Legacy default safe and healthy"),F.Plane->Owned && F.Plane->GetHealthComponent()->GetCurrentHealth()==700);
  F.Save->DeleteSave(); return true;
 }
 FAutomationTestBase* Test; FString Mode; FScene F; double Started=0; bool bReady=false; float At=0;
};

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedWingFlight,"NeonCity.FixedWing.FlightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFixedWingFlight::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFixedWingTests::FFlight>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#define FIXED_TEST(Class,Name,Scenario) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFixedWingTests::Scenario>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
FIXED_TEST(FFixedWingApproach,"NeonCity.FixedWing.ApproachCityPIE",FApproach)
FIXED_TEST(FFixedWingSurface,"NeonCity.FixedWing.SurfaceEnvelopePIE",FSurfaceEnvelope)
FIXED_TEST(FFixedWingDamage,"NeonCity.FixedWing.DamageWaterPIE",FDamageWater)
#undef FIXED_TEST
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FFixedWingSave,"NeonCity.FixedWing.Persistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FFixedWingSave::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityFixedWingSession="),Mode);
 if(Mode!=TEXT("A") && Mode!=TEXT("B")) Mode=TEXT("RoundTrip"); Names.Add(Mode); Commands.Add(Mode);
}
bool FFixedWingSave::RunTest(const FString& Mode)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFixedWingTests::FPersistence>(this,Mode));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
