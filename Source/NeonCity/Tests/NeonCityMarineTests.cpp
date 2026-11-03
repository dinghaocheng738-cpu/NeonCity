#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineFeedbackComponent.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineTestArea.h"
#include "Water/CoastalOceanSurface.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
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
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineProfile,"NeonCity.Marine.ProfileAndSchema",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineProfile::RunTest(const FString&)
{
 const auto* Data=GetDefault<UMarineVehicleData>(); TestTrue(TEXT("Speedboat profile valid"),Data->IsValidProfile());
 TestTrue(TEXT("Stable asset ID"),Data->GetPrimaryAssetId().IsValid());
 auto* Bad=NewObject<UMarineVehicleData>(); Bad->Buoyancy=0; TestFalse(TEXT("Nonfloating profile rejected"),Bad->IsValidProfile());
 Bad->Buoyancy=1.8f; Bad->Mass=-1; TestFalse(TEXT("Invalid mass rejected"),Bad->IsValidProfile());
 Bad->Mass=1100;Bad->ThrottleRiseRate=0;TestFalse(TEXT("Nonresponsive throttle profile rejected"),Bad->IsValidProfile());
 auto* Model=NewObject<UNeonCitySaveGame>(); Model->SaveID=FGuid::NewGuid(); Model->FormatID=TEXT("NeonCity.SaveGame");
 TestEqual(TEXT("Additive schema at the current version"),Model->SaveVersion,UNeonCitySaveGame::CurrentVersion);
 FString Error; Model->Marine.SetNum(65); TestFalse(TEXT("Marine save count bounded"),Model->ValidateEnvelope(Error));
 TestTrue(TEXT("Yacht expansion data exists without gameplay"),GetDefault<UYachtVehicleData>()->DeckIDs.IsEmpty());
 return true;
}
namespace NeonCityMarineTests
{
void Key(APlayerController* PC,FKey KeyName,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),KeyName,Event,FPlatformTime::Cycles64())); }
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr; ANeonCityMarineVehicleBase* Boat=nullptr;
 UNeonCitySurfaceMovementComponent* Move=nullptr; UNeonCitySaveSubsystem* Save=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<2 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It) if(It->VehicleID==UMarineWorldSubsystem::SpeedboatID()) Boat=*It;
  Move=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement()); Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  return Boat && Move;
 }
 void PlacePlayer(FVector Position)
 {
  Move->SetMovementMode(MOVE_Falling); Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); Move->StopMovementImmediately();
  Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
 void PlaceBoat(FVector Position,FRotator Rotation=FRotator(0,-90,0))
 {
  Boat->GetMarineMovement()->StopEngine(); Boat->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector); Boat->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Boat->SetActorLocationAndRotation(Position,Rotation,false,nullptr,ETeleportType::TeleportPhysics); Boat->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.75f);
 }
 bool Based() const { return Move->IsMovingOnGround() && Player->GetMovementBaseObject()==Boat->GetHull(); }
 FVector Local() const { return Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()); }
 void LookAtHull()
 {
  FVector Eye; FRotator Rotation; Player->GetActorEyesViewPoint(Eye,Rotation);
  PC->SetControlRotation((Boat->GetActorLocation()-Eye).Rotation());
 }
 void Shot(const TCHAR* Name)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityMarineCapture"))) return;
  const FVector Target=Boat->GetActorLocation()+FVector(0,0,50),Eye=Target+FVector(650,500,360);
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
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Marine")),true);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Marine")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("Marine capture failed: %s"),Name);
  PC->SetViewTarget(PC->GetPawn()); Camera->Destroy();
 }
 bool HUDShot(const TCHAR* Name)
 {
  return FNeonCityVisualEvidence::CaptureHUD(W,Name);
 }
};
class FMarineScenario : public IAutomationLatentCommand
{
public:
 FMarineScenario(FAutomationTestBase* TestIn,int32 KindIn):Test(TestIn),Kind(KindIn) {}
 bool Next(int32 Value) { Stage=Value; Phase=F.W->GetTimeSeconds(); return false; }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Marine timeout kind %d stage %d"),Kind,Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   if(Kind==3 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
    if(auto Window=F.W->GetGameViewport()->GetWindow())Window->Resize(FVector2D(1600,900));
   Test->TestTrue(TEXT("Simulated hull blocks Pawn"),F.Boat->GetHull()->IsSimulatingPhysics() && F.Boat->GetHull()->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
   FWaterSurfaceSample SpawnWater;
   const bool HasSpawnWater=F.W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(F.Boat->GetActorLocation(),SpawnWater);
   Test->TestTrue(TEXT("Spawned afloat in live water with upright hull"),HasSpawnWater && F.Boat->GetMarineMovement()->IsAfloat()
    && FMath::Abs(F.Boat->GetActorLocation().Z-SpawnWater.SurfacePoint.Z)<F.Boat->GetProfile()->HullExtent.Z*2
    && FVector::DotProduct(F.Boat->GetActorUpVector(),FVector::UpVector)>F.Move->GetWalkableFloorZ());
   if(Kind==0 || Kind==3) F.PlacePlayer(FVector(5200,-1000,140));
   if(Kind==1) { F.PlaceBoat(FVector(3500,-5200,-134.5f)); F.PlacePlayer(FVector(3500,-5100,30)); }
   if(Kind==2)
   {
    F.PlaceBoat(FVector(3800,-3350,-134.5f),FRotator::ZeroRotator);
    for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) { Car=*It; break; }
    if(!Test->TestNotNull(TEXT("Existing land car"),Car)) return true;
    Car->SetActorLocation(FVector(2000,-4500,-350),false,nullptr,ETeleportType::TeleportPhysics); Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetDrivingComponent()->SetDriveInput(1);
    F.Boat->GetMarineMovement()->StartEngine();
   }
   return Next(1);
  }
  const float Elapsed=F.W->GetTimeSeconds()-Phase;
  if(Kind==1 || Kind==3) VisualEvidence.Record(F.W,F.PC,Stage);
  if(Kind==0 || Kind==3) return Drive(Elapsed);
  if(Kind==1) return Deck(Elapsed);
  return Damage(Elapsed);
 }
 bool Drive(float Elapsed)
 {
  if(Stage==1)
  {
   const FVector Target(5080,-3350,F.Player->GetActorLocation().Z);
   if(FVector::Dist2D(Target,F.Player->GetActorLocation())>12)
   {
    F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),1);
    if(Elapsed>15) { Test->AddError(FString::Printf(TEXT("Promenade-to-dock walking blocked at %s"),*F.Player->GetActorLocation().ToString())); return true; }
    return false;
   }
   F.Move->StopMovementImmediately();
   Test->TestTrue(TEXT("Walked from promenade across seawall steps onto dock"),F.Move->IsMovingOnGround());
   F.LookAtHull(); auto* Interaction=F.Player->FindComponentByClass<UInteractionComponent>();
   Test->TestTrue(TEXT("Dock focus resolves through interactable interface"),Interaction->RefreshFocusedInteractable()==F.Boat);
   F.Shot(TEXT("A-Dock.png")); Key(F.PC,EKeys::F,IE_Pressed); return Next(2);
  }
  if(Stage==2 && Elapsed>.3f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F possesses speedboat"),F.PC->GetPawn()==F.Boat)) return true;
   Test->TestTrue(TEXT("Marine camera active"),F.Boat->FindComponentByClass<UNeonCityCameraExtensionComponent>()->GetCameraContext()==ENeonCityCameraContext::Marine);
   Test->TestTrue(TEXT("Driver sees marine speed/throttle status"),F.Boat->IsDrivingStatusVisible());
   if(Kind==3)Test->TestTrue(TEXT("Capture actual marine idle HUD"),F.HUDShot(TEXT("Marine-Idle.png")));
   if(Kind==3)
   {
    auto* Feedback=F.Boat->FindComponentByClass<UMarineFeedbackComponent>();
    auto* Audio=Feedback?Feedback->GetEngineAudio():nullptr;
    auto* Wave=Audio?Cast<USoundWave>(Audio->GetSound()):nullptr;
    Test->TestTrue(TEXT("Reviewed spatial engine loop is connected to the boat"),Wave && Wave->bLooping && Wave->NumChannels==1 && Audio->GetAttachParent()==F.Boat->GetHull());
   }
   Origin=F.Boat->GetActorLocation(); Key(F.PC,EKeys::W,IE_Pressed); return Next(Kind==3?20:3);
  }
  if(Stage==20 && Elapsed>.2f)
  {
   const auto T=F.Boat->GetMarineMovement()->GetTelemetry();
   Test->TestTrue(TEXT("Throttle builds progressively rather than setting hull speed"),T.Throttle>.05f && T.Throttle<.8f && T.TargetThrottle==1 && T.SpeedKmh<10);
   return Next(3);
  }
  if(Stage==3 && Elapsed>(Kind==3?12.f:3.f))
  {
   Test->TestTrue(TEXT("Seated driver cannot enter swimming under the hull"),F.Move->MovementMode==MOVE_None);
   Speed=F.Boat->GetSpeed_Implementation();
   Test->TestTrue(TEXT("W drives away from dock"),FVector::Dist2D(Origin,F.Boat->GetActorLocation())>500 && Speed>300);
   if(Kind==3) Test->TestTrue(TEXT("Sustained thrust reaches cruising speed under water drag"),Speed*.036f>30 && Speed*.036f<=F.Boat->GetProfile()->MaxSpeed);
   if(Kind==3)
   {
    auto* Effects=F.Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
    bool PhysicalWake=false;for(TActorIterator<ACoastalOceanSurface> It(F.Boat->GetWorld());It;++It)PhysicalWake|=It->HasPhysicalWakeFor(F.Boat);
    Test->TestTrue(TEXT("Cruise produces physical ocean wake (or local-water foam) and bounded spray"),Effects && (PhysicalWake?Effects->GetActiveWakeCount()==0:Effects->GetActiveWakeCount()>8) && Effects->GetActiveSprayCount()>=2
     && Effects->GetActiveWakeCount()+Effects->GetActiveSprayCount()<=Effects->GetParticleCapacity());
   }
   if(Kind==3)Test->TestTrue(TEXT("Capture actual marine cruise HUD"),F.HUDShot(TEXT("Marine-Cruise.png")));
   if(Kind==3 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))
   {
    auto* Audio=F.Boat->FindComponentByClass<UMarineFeedbackComponent>()->GetEngineAudio();
    Test->TestTrue(TEXT("Audio-enabled run starts real engine playback"),Audio && Audio->IsPlaying());
   }
   Test->AddInfo(FString::Printf(TEXT("Drive travel %.1f speed %.1f"),FVector::Dist2D(Origin,F.Boat->GetActorLocation()),Speed));
   if(Kind==3) { Key(F.PC,EKeys::F,IE_Pressed);return Next(26); }
   Yaw=F.Boat->GetActorRotation().Yaw; Key(F.PC,EKeys::D,IE_Pressed); F.Shot(TEXT("B-Driving.png")); return Next(4);
  }
  if(Stage==4 && Elapsed>1.5f)
  {
   Test->TestTrue(TEXT("D applies rudder yaw"),FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,F.Boat->GetActorRotation().Yaw))>8);
   Key(F.PC,EKeys::D,IE_Released); Key(F.PC,EKeys::W,IE_Released); Speed=F.Boat->GetSpeed_Implementation(); return Next(5);
  }
  if(Stage==5 && Elapsed>2)
  {
   Test->TestTrue(TEXT("Water drag slows released throttle"),F.Boat->GetSpeed_Implementation()<Speed*.9f);
   if(Kind==3)
   {
    const auto T=F.Boat->GetMarineMovement()->GetTelemetry();
    Test->TestTrue(TEXT("Released throttle coasts with retained inertia"),T.State==EMarineDriveState::Coasting && T.Throttle<.01f && T.SpeedKmh>3);
    Test->TestTrue(TEXT("Capture actual marine coast HUD"),F.HUDShot(TEXT("Marine-Coast.png")));
   }
   Key(F.PC,EKeys::SpaceBar,IE_Pressed); return Next(6);
  }
  if(Stage==6 && Elapsed>2)
  {
   Key(F.PC,EKeys::SpaceBar,IE_Released);
   Test->TestTrue(TEXT("Assist brake reaches safe exit speed"),F.Boat->GetSpeed_Implementation()<150);
   if(Kind==3) { Key(F.PC,EKeys::S,IE_Pressed);return Next(21); }
   Test->TestTrue(TEXT("Safe exit point available"),F.Boat->GetMarineInteraction()->FindExitTransform(ExitTransform));
   Key(F.PC,EKeys::F,IE_Pressed); return Next(7);
  }
  if(Stage==7 && Elapsed>.7f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("F restores character possession"),F.PC->GetPawn()==F.Player && !F.Player->IsHidden() && F.Player->GetActorEnableCollision());
   Test->TestTrue(TEXT("Exit onto actual deck floor"),F.Based() && F.Local().Z>120);
   Test->TestTrue(TEXT("Exit leaves empty boat engine off"),!F.Boat->GetMarineMovement()->IsEngineRunning());
   Test->TestEqual(TEXT("Ordinary driving causes no damage"),F.Boat->GetHealthComponent()->GetCurrentHealth(),F.Boat->GetProfile()->Durability);
   Test->TestFalse(TEXT("Marine HUD is removed when character control returns"),F.Boat->IsDrivingStatusVisible());
   if(Kind==3 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))
   {
    auto* Audio=F.Boat->FindComponentByClass<UMarineFeedbackComponent>()->GetEngineAudio();
    Test->TestTrue(TEXT("Engine playback finishes fading after exit"),Audio && !Audio->IsPlaying());
   }
   F.Shot(TEXT("B-SafeExit.png"));
   if(Kind==3) { Direction=F.Boat->GetActorRightVector();Key(F.PC,EKeys::SpaceBar,IE_Pressed);return Next(23); }
   return true;
  }
  if(Stage==21 && Elapsed>3)
  {
   const auto T=F.Boat->GetMarineMovement()->GetTelemetry();
   Test->TestTrue(TEXT("S reverses actual water-relative motion"),T.State==EMarineDriveState::Reverse && T.ForwardSpeedKmh<-3 && T.SignedThrust<-.95f);
   Test->TestTrue(TEXT("Capture actual marine reverse HUD"),F.HUDShot(TEXT("Marine-Reverse.png")));
   Key(F.PC,EKeys::S,IE_Released);Key(F.PC,EKeys::SpaceBar,IE_Pressed);return Next(22);
  }
  if(Stage==26 && Elapsed>.2f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("Unsafe moving exit retains control and explains recovery"),F.PC->GetPawn()==F.Boat && !F.Boat->GetExitFeedback().IsEmpty());
   Test->TestTrue(TEXT("Capture actual marine exit recovery feedback"),F.HUDShot(TEXT("Marine-ExitFeedback.png")));
   Yaw=F.Boat->GetActorRotation().Yaw;Key(F.PC,EKeys::D,IE_Pressed);return Next(4);
  }
  if(Stage==22 && Elapsed>2)
  {
   Test->TestTrue(TEXT("Reverse thrust can brake to a safe stop"),F.Boat->GetSpeed_Implementation()<150);
   Key(F.PC,EKeys::SpaceBar,IE_Released);Key(F.PC,EKeys::F,IE_Pressed);return Next(7);
  }
  if(Stage==23)
  {
   if(Elapsed>.15f)Key(F.PC,EKeys::SpaceBar,IE_Released);
   F.Player->AddMovementInput(Direction,1);
   if(F.Move->IsSwimming()) { Origin=F.Player->GetActorLocation();return Next(24); }
   if(Elapsed>6) { Test->AddError(TEXT("Drive-loop exit did not reach native swimming"));return true; }
  }
  if(Stage==24)
  {
   const FVector Target=F.Boat->GetActorLocation()+Direction*170;
   F.Player->AddMovementInput((Target-F.Player->GetActorLocation()).GetSafeNormal2D(),1);
   Key(F.PC,EKeys::SpaceBar,IE_Pressed);
   if(Elapsed>1 && FVector::Dist2D(F.Player->GetActorLocation(),Target)<60 && F.Boat->GetSpeed_Implementation()<150)
   {
    Key(F.PC,EKeys::SpaceBar,IE_Released);F.LookAtHull();
    Test->TestTrue(TEXT("Return swimmer can focus the same boat"),F.Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==F.Boat);
    Key(F.PC,EKeys::F,IE_Pressed);return Next(25);
   }
   if(Elapsed>18) { Test->AddError(TEXT("Drive-loop swimmer could not return to boat"));return true; }
  }
  if(Stage==25 && Elapsed>.3f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("Continuous drive/swim loop restores boat and one active HUD"),F.PC->GetPawn()==F.Boat && F.Boat->IsDrivingStatusVisible());
   Test->TestTrue(TEXT("Reboarding starts at idle, without stale thrust"),F.Boat->GetMarineMovement()->GetTelemetry().Throttle<.01f);
   TInlineComponentArray<UAudioComponent*> AudioParts(F.Boat);
   Test->TestEqual(TEXT("Repeated entry retains exactly one engine audio component"),AudioParts.Num(),1);
   auto* Effects=F.Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
   Test->TestTrue(TEXT("Stopped boat retires old foam and spray"),Effects && Effects->GetActiveWakeCount()==0 && Effects->GetActiveSprayCount()==0);
   return true;
  }
  return false;
 }
 bool Deck(float Elapsed)
 {
  if(Stage==1 && Elapsed>1)
  {
   if(!Test->TestTrue(TEXT("Native deck base acquired"),F.Based())) { Test->AddInfo(FString::Printf(TEXT("Player %s boat %s mode %d"),*F.Player->GetActorLocation().ToString(),*F.Boat->GetActorLocation().ToString(),int(F.Move->MovementMode))); return true; }
   Local=F.Local(); Origin=F.Boat->GetActorLocation(); Yaw=F.Boat->GetActorRotation().Yaw;
   F.Boat->GetMarineMovement()->StartEngine(); F.Boat->GetMarineMovement()->SetInput(.35f,.35f,0); return Next(2);
  }
  if(Stage==2)
  {
   if(!F.Based())
   {
    Test->AddError(FString::Printf(TEXT("Lost marine base after %.2f: local %s mode %d; releases %d (%s); hull %s / %s"),
     Elapsed,*F.Local().ToString(),int(F.Move->MovementMode),F.Move->GetSurfaceReleaseCount(),*F.Move->GetLastSurfaceReleaseDetails(),
     *F.Boat->GetActorRotation().ToString(),*F.Boat->GetVelocity().ToString()));
    F.HUDShot(TEXT("Marine-DeckBaseFailure.png"));return true;
   }
   if(Elapsed<5) return false;
   Test->TestTrue(TEXT("Deck translated and rotated with physics"),FVector::Dist2D(Origin,F.Boat->GetActorLocation())>350 && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,F.Boat->GetActorRotation().Yaw))>5);
   Test->TestTrue(TEXT("Native based movement keeps local deck position"),FVector::Dist2D(Local,F.Local())<50);
   Test->TestEqual(TEXT("Deck standing is not vehicle impact damage"),F.Player->GetHealthComponent()->GetCurrentHealth(),100.f);
   F.Shot(TEXT("C-MovingDeck.png")); F.Boat->GetMarineMovement()->StopEngine();
   Direction=F.Boat->GetActorRightVector(); Key(F.PC,EKeys::SpaceBar,IE_Pressed); return Next(3);
  }
  if(Stage==3)
  {
   if(Elapsed>.15f) Key(F.PC,EKeys::SpaceBar,IE_Released);
   F.Player->AddMovementInput(Direction,1);
   if(F.Move->IsSwimming())
   {
    Test->TestTrue(TEXT("Water volume selected by native CMC"),F.Player->GetPhysicsVolume()->bWaterVolume);
    Test->TestTrue(TEXT("Jump released boat base"),!F.Player->GetMovementBaseObject());
    Origin=F.Player->GetActorLocation(); F.Shot(TEXT("D-Swimming.png")); return Next(4);
   }
   if(Elapsed>6) { Test->AddError(FString::Printf(TEXT("No swimming: player %s mode %d water %d"),*F.Player->GetActorLocation().ToString(),int(F.Move->MovementMode),F.Player->GetPhysicsVolume()->bWaterVolume)); return true; }
  }
  if(Stage==4)
  {
   const FVector Target=F.Boat->GetActorLocation()+Direction*170;
   const FVector Toward=(Target-F.Player->GetActorLocation()).GetSafeNormal2D(); F.Player->AddMovementInput(Toward,1);
   Key(F.PC,EKeys::SpaceBar,IE_Pressed);
   if(Elapsed>1 && FVector::Dist2D(F.Player->GetActorLocation(),Target)<60 && F.Boat->GetSpeed_Implementation()<150)
   {
    Key(F.PC,EKeys::SpaceBar,IE_Released); F.LookAtHull();
    Test->TestTrue(TEXT("Swimmer moved back toward boat"),FVector::Dist2D(Origin,F.Player->GetActorLocation())>50);
    Test->TestTrue(TEXT("Swimmer can focus boat"),F.Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==F.Boat);
    Key(F.PC,EKeys::F,IE_Pressed); return Next(5);
   }
   if(Elapsed>18) { Test->AddError(FString::Printf(TEXT("Swim return failed player %s boat %s distance %.1f"),*F.Player->GetActorLocation().ToString(),*F.Boat->GetActorLocation().ToString(),FVector::Dist2D(F.Player->GetActorLocation(),Target))); return true; }
  }
  if(Stage==5 && Elapsed>.3f)
  {
   Key(F.PC,EKeys::F,IE_Released); Test->TestTrue(TEXT("F reboards from swimming without animation/softlock"),F.PC->GetPawn()==F.Boat);
   F.Shot(TEXT("D-Reboarded.png")); return true;
  }
  return false;
 }
 bool Damage(float Elapsed)
 {
  if(Stage==1 && Elapsed>1)
  {
   Test->TestTrue(TEXT("Land car deep-water engine stalled"),Car->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Stalled);
   Test->TestTrue(TEXT("Marine floats and engine ignores land water rule"),F.Boat->GetMarineMovement()->IsAfloat() && F.Boat->GetMarineMovement()->IsEngineRunning());
   Test->TestTrue(TEXT("Marine supports combat without equipping weapon"),F.Boat->FindComponentByClass<UCombatComponent>() && F.Boat->GetCombatState_Implementation()==ENeonCityCombatState::Unarmed);
   Car->SetActorLocation(FVector(2000,-4500,-5000),false,nullptr,ETeleportType::TeleportPhysics);
   Before=F.Boat->GetHealthComponent()->GetCurrentHealth();
   F.Boat->GetHull()->SetPhysicsLinearVelocity(FVector(1100,0,0)); return Next(2);
  }
  if(Stage==2 && Elapsed>2)
  {
   Test->TestTrue(TEXT("A car below the swimming volume remains disabled"),Car->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Stalled);
   Test->TestTrue(TEXT("Real Chaos dock contact reduces boat health"),F.Boat->GetHealthComponent()->GetCurrentHealth()<Before);
   Test->TestTrue(TEXT("Unified impact event identifies hull"),F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents().ContainsByPredicate([&](const auto& E){ return E.TargetActor.Get()==F.Boat && E.AppliedDamage>0; }));
   Test->AddInfo(FString::Printf(TEXT("Dock collision health %.1f -> %.1f; hull %s / %s velocity %s; impact sampled %s"),Before,F.Boat->GetHealthComponent()->GetCurrentHealth(),*F.Boat->GetActorLocation().ToString(),
    *F.Boat->GetActorRotation().ToString(),*F.Boat->GetVelocity().ToString(),*F.Boat->FindComponentByClass<UPhysicalImpactComponent>()->GetContactVelocity().ToString()));
   F.HUDShot(TEXT("Marine-DockImpact.png"));
   F.Shot(TEXT("E-DockImpact.png")); return true;
  }
  return false;
 }
 FNeonCityVisualEvidence VisualEvidence{TEXT("B-Boat")};
 FAutomationTestBase* Test; int32 Kind,Stage=0; double Started=0; float Phase=0,Speed=0,Yaw=0,Before=0;
 FVector Origin,Local,Direction; FTransform ExitTransform; FScene F; ANeonCityBaseVehicle* Car=nullptr;
};
class FMarinePersistence : public IAutomationLatentCommand
{
public:
 FMarinePersistence(FAutomationTestBase* T,FString M):Test(T),Mode(MoveTemp(M)) {}
 ACoastalOceanSurface* Ocean() const
 {
  for(TActorIterator<ACoastalOceanSurface> It(F.W);It;++It)return *It;
  return nullptr;
 }
 void VerifySeaState(const UNeonCitySaveGame* Model)
 {
  Test->TestTrue(TEXT("Non-default sea state survives disk serialization"),Model->HasWorld && Model->World.CoastalSeaState==uint8(ECoastalSeaState::Moderate));
  if(auto* Surface=Ocean())Test->TestEqual(TEXT("World restore applies saved sea state"),Surface->GetSeaState(),ECoastalSeaState::Moderate);
  else Test->AddError(TEXT("Restored normal world lost its ocean"));
 }
 void Verify(const FNeonCityMarineSaveData& Record)
 {
  Test->TestTrue(TEXT("Stable boat ID restored"),F.Boat->VehicleID==Record.VehicleID);
  Test->TestTrue(TEXT("Marine transform restored"),F.Boat->GetActorTransform().Equals(Record.Transform,.1f));
  Test->TestEqual(TEXT("Marine ownership restored"),F.Boat->Owned,Record.Owned);
  Test->TestTrue(TEXT("Marine condition restored"),FMath::IsNearlyEqual(F.Boat->GetHealthComponent()->GetCurrentHealth(),Record.Condition*F.Boat->GetProfile()->Durability,.01f));
  Test->TestTrue(TEXT("Restore clears engine and motion"),!F.Boat->GetMarineMovement()->IsEngineRunning() && F.Boat->GetVelocity().IsNearlyZero(.1f));
 }
 void Manifest(UNeonCitySaveGame* Model)
 {
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Marine")),true);
  const auto& R=Model->Marine[0];
  const FString Text=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"vehicleId\":\"%s\",\"condition\":%.6f,\"owned\":%d,\"seaState\":%u,\"transform\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*Model->SaveID.ToString(),*R.VehicleID.ToString(),R.Condition,R.Owned,uint32(Model->World.CoastalSeaState),*R.Transform.ToHumanReadableString().Replace(TEXT("\n"),TEXT(" ")).Replace(TEXT("\r"),TEXT(" ")));
  Test->TestTrue(TEXT("Cross-session marine evidence saved"),FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Marine")/(TEXT("Session")+Mode+TEXT(".json")))));
 }
 bool VerifyWorkingBoat(const UNeonCitySaveGame* Model)
 {
  const auto* Record=Model->Marine.FindByPredicate([](const auto& R){return R.VehicleID==UMarineWorldSubsystem::HarborlineID();});
  ANeonCityMarineVehicleBase* Working=nullptr;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(F.W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Working=*It;
  if(!Test->TestTrue(TEXT("Distinct stable working-boat record and actor"),Record && Working))return false;
  Test->TestTrue(TEXT("Working boat pose, ownership and damage restored"),Working->GetActorTransform().Equals(Record->Transform,.1f)
   && Working->Owned==Record->Owned && FMath::IsNearlyEqual(Working->GetHealthComponent()->GetCurrentHealth(),Record->Condition*1400,.01f));
  Test->TestEqual(TEXT("Working boat preserves its profile identity"),Record->DataAssetID,FName(TEXT("NeonCity.HarborFishingBoat")));
  return true;
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Marine persistence timeout")); return true; }
  if(!F.Ready()) return false;
  if(!Test->TestNotNull(TEXT("Persistence fixture uses the normal authored ocean"),Ocean()))return true;
  F.Save->ConfigureSlot(Mode==TEXT("RoundTrip")?TEXT("NeonCity_Test_Marine_RoundTrip"):TEXT("NeonCity_Test_Marine_CrossSession"));
  if(Mode==TEXT("B"))
  {
   if(!Test->TestTrue(TEXT("Session B finds disk save"),F.Save->DoesSaveExist())) return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection());
   if(!Test->TestTrue(TEXT("Both disk marine records readable"),Model.IsValid() && Model->Marine.Num()==2 && Model->Marine[0].VehicleID==UMarineWorldSubsystem::SpeedboatID())) return true;
   FString Previous; FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Marine/SessionA.json")));
   uint32 PID=0; FParse::Value(*Previous,TEXT("\"processId\":"),PID);
   Test->TestTrue(TEXT("Session B is new process with same save identity"),PID && PID!=FPlatformProcess::GetCurrentProcessId() && Previous.Contains(Model->SaveID.ToString()));
   Ocean()->SetSeaState(ECoastalSeaState::Calm);
   Test->TestTrue(TEXT("Cross-session boat load"),F.Save->LoadGame()); Verify(Model->Marine[0]); VerifyWorkingBoat(Model.Get()); VerifySeaState(Model.Get()); Manifest(Model.Get()); F.Shot(TEXT("F-SessionB.png")); F.Save->DeleteSave(); return true;
  }
  F.Save->DeleteSave(); F.PlaceBoat(FVector(3100,-5400,-134.5f),FRotator(0,22,0));
  F.Boat->Owned=false; F.Boat->GetHealthComponent()->ApplyPersistenceHealth(425,850);
  for(TActorIterator<ANeonCityMarineVehicleBase> It(F.W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())
  { It->SetActorLocation(FVector(5000,-8500,-130),false,nullptr,ETeleportType::TeleportPhysics);It->Owned=false;It->GetHealthComponent()->ApplyPersistenceHealth(700,1400); }
  Ocean()->SetSeaState(ECoastalSeaState::Moderate);
  if(!Test->TestTrue(TEXT("Marine state writes via SaveSubsystem"),F.Save->SaveGame())) { Test->AddInfo(F.Save->GetLastDetail()); return true; }
  TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("Two distinct marine records and unchanged land registry"),Model.IsValid() && Model->HasMarine && Model->Marine.Num()==2
   && Model->Marine[0].VehicleID==UMarineWorldSubsystem::SpeedboatID() && Model->Marine[1].VehicleID==UMarineWorldSubsystem::HarborlineID() && Model->Vehicles.Num()==4)) return true;
  VerifySeaState(Model.Get());
  if(Mode==TEXT("A")) { Manifest(Model.Get()); F.Shot(TEXT("F-SessionA.png")); return true; }
  F.PlaceBoat(FVector(4500,-4500,-134.5f)); F.Boat->Owned=true; F.Boat->GetHealthComponent()->Heal(1000);
  for(TActorIterator<ANeonCityMarineVehicleBase> It(F.W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())
  { It->SetActorLocation(FVector(4500,-9000,-130),false,nullptr,ETeleportType::TeleportPhysics);It->Owned=true;It->GetHealthComponent()->Heal(1400); }
  Ocean()->SetSeaState(ECoastalSeaState::Calm);
  Test->TestTrue(TEXT("Round trip load"),F.Save->LoadGame()); Verify(Model->Marine[0]); VerifyWorkingBoat(Model.Get()); VerifySeaState(Model.Get()); F.Shot(TEXT("F-Restored.png"));
  auto Bad=Model->Marine[0]; Bad.VehicleID=FGuid::NewGuid(); Model->Marine.Add(Bad); Model->Marine[0].Condition=-1;
  Test->TestTrue(TEXT("Write malformed marine fixture"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Bad IDs/condition safely normalize"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Invalid record uses default condition"),F.Boat->GetHealthComponent()->GetCurrentHealth(),850.f);
  Model->HasMarine=false; Model->Marine.Reset();
  Test->TestTrue(TEXT("Write legacy Version 1 fixture"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Legacy Version 1 loads marine defaults"),F.Save->LoadGame());
  Test->TestTrue(TEXT("Default owned boat retains stable ID"),F.Boat->Owned && F.Boat->VehicleID==UMarineWorldSubsystem::SpeedboatID());
  F.Save->DeleteSave(); return true;
 }
 FAutomationTestBase* Test; FString Mode; FScene F; double Started=0;
};
}
#define MARINE_TEST(Class,Name,Kind) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityMarineTests::FMarineScenario>(this,Kind)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
MARINE_TEST(FMarineDriving,"NeonCity.Marine.DockDrivingPIE",0)
MARINE_TEST(FMarineDeck,"NeonCity.Marine.DeckSwimmingPIE",1)
MARINE_TEST(FMarineDamage,"NeonCity.Marine.WaterDamagePIE",2)
MARINE_TEST(FMarineThrottleLoop,"NeonCity.Marine.ThrottleLoopPIE",3)
#undef MARINE_TEST
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineSave,"NeonCity.Marine.PersistencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineSave::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityMarineTests::FMarinePersistence>(this,TEXT("RoundTrip")));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMarineCrossSession,"NeonCity.Marine.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FMarineCrossSession::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityMarineSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode); Commands.Add(Mode); }
}
bool FMarineCrossSession::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityMarineTests::FMarinePersistence>(this,Parameters));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
