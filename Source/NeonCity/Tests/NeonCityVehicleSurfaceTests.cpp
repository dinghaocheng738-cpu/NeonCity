#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleData.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Combat/HealthComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfacePolicy,"NeonCity.VehicleSurface.Policy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurfacePolicy::RunTest(const FString& Parameters)
{
 FDynamicStandingSurface P;
 TestTrue(TEXT("Normal jump descent is a safe landing"),NeonCityDynamicSurfaces::IsSafeLanding(FVector(200,0,-480),P));
 TestFalse(TEXT("Fast horizontal collision is not a landing"),NeonCityDynamicSurfaces::IsSafeLanding(FVector(1600,0,-300),P));
 TestFalse(TEXT("Severe fall is not ordinary landing"),NeonCityDynamicSurfaces::IsSafeLanding(FVector(0,0,-1300),P));
 TestFalse(TEXT("Underside upward jump is not landing"),NeonCityDynamicSurfaces::IsSafeLanding(FVector(0,0,500),P));
 TestTrue(TEXT("No provider inferred for ordinary objects"),!NeonCityDynamicSurfaces::IsProvider(nullptr));
 for(const double Rate:{30.,60.,120.})
 {
  FDynamicSurfaceMotionFilter Filter;
  TestTrue(TEXT("Measured one-step deck contact jitter does not eject a rider"),Filter.Update(FVector(11.632,23.143,-42.852),1./Rate).Size()<P.ReleaseAcceleration);
  Filter.Reset();
  TestTrue(TEXT("Collision-sized impulse releases immediately at each frame rate"),Filter.Update(FVector(0,1100,250),1./Rate).Size()>P.ReleaseAcceleration);
  Filter.Reset();
  for(int32 Frame=0;Frame<FMath::CeilToInt(.18*Rate);++Frame)Filter.Update(FVector(3000./Rate,0,0),1./Rate);
  TestTrue(TEXT("Sustained excessive acceleration is still unsafe"),Filter.Acceleration.Size()>P.ReleaseAcceleration);
  Filter.Reset();TestTrue(TEXT("New base/load discards the previous shock"),Filter.Acceleration.IsZero());
 }
 return true;
}
namespace NeonCitySurfaceTests
{
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr; UNeonCitySurfaceMovementComponent* Move=nullptr;
 TArray<ANeonCityBaseVehicle*> Cars;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || !W->HasBegunPlay() || W->GetTimeSeconds()<1 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  Move=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement());
  Cars.Reset(); for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It) if(!Cast<AIslandOffroadVehicle>(*It))Cars.Add(*It);
  Cars.Sort([](const auto& A,const auto& B){ return uint8(A.GetVehicleProfile()->VehicleType)<uint8(B.GetVehicleProfile()->VehicleType); });
  for(TActorIterator<ANeonCityTrafficVehicle> It(W);It;++It) { It->SetActorTickEnabled(false); It->SetActorEnableCollision(false); }
  return Move && Cars.Num()==4;
 }
 void Place(ANeonCityBaseVehicle* Car)
 {
  Move->SetMovementMode(MOVE_Falling); Player->SetActorLocation(FVector(-4220,0,118),false,nullptr,ETeleportType::TeleportPhysics); Move->StopMovementImmediately();
  Car->GetDrivingComponent()->SetDrivingEnabled(false); Car->GetChaosMovement()->ResetVehicle();
  Car->SetActorLocationAndRotation(FVector(-4600,0,150),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
  Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
  Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.75f);
  W->GetSubsystem<UPhysicalImpactSubsystem>()->ResetForLoad();
 }
 bool Based(ANeonCityBaseVehicle* Car) const { return Move->IsMovingOnGround() && Player->GetMovementBaseObject()==Car->GetMesh(); }
 FVector Local(ANeonCityBaseVehicle* Car) const { return Car->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()); }
 void Guide(ANeonCityBaseVehicle* Car,FVector LocalTarget)
 {
  FVector Target=Car->GetActorTransform().TransformPosition(LocalTarget); Target.Z=Player->GetActorLocation().Z;
  FVector Desired=(Target-Player->GetActorLocation())*3.f+Car->GetVelocity(); Desired.Z=0; Desired=Desired.GetClampedToMaxSize(Move->MaxWalkSpeed);
  FVector Actual=Move->Velocity; Actual.Z=0; if(Based(Car)) Actual+=Car->GetVelocity();
  FVector Input=Move->IsFalling()?(Desired-Actual)/100.f:Desired/Move->MaxWalkSpeed; Input.Z=0;
  Player->AddMovementInput(Input.GetSafeNormal(),FMath::Min(1.f,Input.Size()));
 }
 void Jump() { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::SpaceBar,IE_Pressed,FPlatformTime::Cycles64())); }
 void ReleaseJump() { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::SpaceBar,IE_Released,FPlatformTime::Cycles64())); }
 bool NoDamage(ANeonCityBaseVehicle* Car) const
 {
  return Player->GetHealthComponent()->GetCurrentHealth()==100 && !W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents().ContainsByPredicate([&](const auto& E){return E.TargetActor.Get()==Player || E.SourceActor.Get()==Player;})
   && FMath::IsNearlyEqual(Car->GetHealthComponent()->GetCurrentHealth(),Car->GetVehicleProfile()->Durability);
 }
 void Shot(const TCHAR* Name,ANeonCityBaseVehicle* Car)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCitySurfaceCapture"))) return;
  const FVector Target=Car->GetActorLocation()+FVector(0,0,90),Eye=Target+FVector(-430,-650,280);
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation());
  PC->SetViewTarget(Camera); PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0.f);
  bool Captured=false;
  if(auto* Client=W->GetGameViewport(); Client && Client->Viewport)
  {
   Client->Viewport->Draw(false); TArray<FColor> Pixels;
   if(Client->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pixel:Pixels) Pixel.A=255;
    const auto Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("VehicleSurface")),true);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("VehicleSurface")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("Vehicle surface capture failed: %s"),Name);
  PC->SetViewTarget(Player); Camera->Destroy();
 }
};
class FStanding : public IAutomationLatentCommand
{
public:
 FStanding(FAutomationTestBase* T,bool Moving):Test(T),bMoving(Moving) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Surface timeout car=%d stage=%d"),Index,Stage)); return true; }
  if(Stage==0) { if(!F.Ready()) return false; Stage=1; }
  if(Stage==1) { Car=F.Cars[Index]; OriginalTransform=Car->GetActorTransform(); F.Place(Car); Phase=F.W->GetTimeSeconds(); Stage=2; return false; }
  const float Now=F.W->GetTimeSeconds();
  if(bMoving && Stage>=3 && Stage<=6) Car->GetDrivingComponent()->SetDriveInput(FMath::Clamp((150.f-Car->GetChaosMovement()->GetForwardSpeed())/150.f,0.f,.3f));
  if(Stage==2 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Real Chaos chassis blocks Pawn"),Car->GetMesh()->IsSimulatingPhysics() && Car->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
   FHitResult Roof(Car,Car->GetMesh(),Car->GetActorTransform().TransformPosition(FVector(-20,0,124)),FVector::UpVector); Roof.bBlockingHit=true; Roof.BoneName=TEXT("Root");
   Test->TestTrue(TEXT("Upward chassis roof can be floor"),F.Move->IsWalkable(Roof));
   FHitResult Bad=Roof; Bad.BoneName=TEXT("Phys_Wheel_FL"); Test->TestFalse(TEXT("Wheel body cannot be a floor"),F.Move->IsWalkable(Bad));
   Bad=Roof; Bad.ImpactNormal=FVector(0,1,0); Test->TestFalse(TEXT("Vertical door cannot be floor"),F.Move->IsWalkable(Bad));
   Bad=Roof; Bad.ImpactNormal=FVector(.9,0,.4359); Test->TestFalse(TEXT("Excessive floor slope rejected"),F.Move->IsWalkable(Bad));
   const auto* BI=Car->GetMesh()->GetBodyInstance(TEXT("Root"));
   Test->TestTrue(TEXT("No rotation lock or inflated mass"),BI && !BI->bLockXRotation && !BI->bLockYRotation && FMath::IsNearlyEqual(BI->GetBodyMass(),Car->GetVehicleProfile()->Weight,1.f));
   Test->TestTrue(TEXT("Suspension does not treat Pawn as road"),Car->GetChaosMovement()->WheelTraceCollisionResponses.GetResponse(ECC_Pawn)==ECR_Ignore);
   Test->AddInfo(FString::Printf(TEXT("Car %d mass %.1f COM %s root %s jump %.1f push %.1f initial %.1f repulsion %.1f weight %.1f"),Index,Car->GetMesh()->GetMass(),*Car->GetMesh()->GetCenterOfMass().ToString(),*Car->GetActorLocation().ToString(),F.Move->JumpZVelocity,F.Move->PushForceFactor,F.Move->InitialPushForceFactor,F.Move->RepulsionForce,F.Move->StandingDownwardForceScale));
   if(bMoving) { Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetMesh()->SetPhysicsLinearVelocity(FVector(150,0,0)); }
   Landings=F.Move->GetDynamicLandingCount(); F.Jump(); Phase=Now; Stage=3; return false;
  }
  if(Stage==3)
  {
   if(Now-Phase>.15f) F.ReleaseJump();
   F.Guide(Car,FVector(165,0,0));
   if(F.Based(Car) && F.Move->GetDynamicLandingCount()>Landings)
   {
    Test->AddInfo(FString::Printf(TEXT("Hood landing local %s vel %s"),*F.Local(Car).ToString(),*F.Move->Velocity.ToString()));
    Phase=Now; Stage=4; return false;
   }
   if(Now-Phase>3) { Test->AddError(FString::Printf(TEXT("Failed street-to-hood jump: player %s car %s local %s mode %d health %.1f"),*F.Player->GetActorLocation().ToString(),*Car->GetActorLocation().ToString(),*F.Local(Car).ToString(),int32(F.Move->MovementMode),F.Player->GetHealthComponent()->GetCurrentHealth())); return true; }
  }
  if(Stage==4 && Now-Phase>.5f)
  { Landings=F.Move->GetDynamicLandingCount(); F.Jump(); Phase=Now; Stage=5; return false; }
  if(Stage==5)
  {
   if(Now-Phase>.15f) F.ReleaseJump(); F.Guide(Car,FVector(-25,0,0));
   if(F.Based(Car) && F.Move->GetDynamicLandingCount()>Landings && F.Local(Car).Z>200)
   { Origin=Car->GetActorLocation(); LocalOrigin=F.Local(Car); Phase=Now; Stage=6; if(Index==0) F.Shot(bMoving?TEXT("B-Moving-Landing.png"):TEXT("A-Parked-Roof.png"),Car); return false; }
   if(Now-Phase>3) { Test->AddError(FString::Printf(TEXT("Failed hood-to-roof jump: local %s mode %d health %.1f"),*F.Local(Car).ToString(),int32(F.Move->MovementMode),F.Player->GetHealthComponent()->GetCurrentHealth())); return true; }
  }
  if(Stage==6)
  {
   if(!F.Based(Car)) { Test->AddError(FString::Printf(TEXT("Lost base while standing: local %s releases %d"),*F.Local(Car).ToString(),F.Move->GetSurfaceReleaseCount())); return true; }
   MaxRoll=FMath::Max(MaxRoll,FMath::Abs(Car->GetActorRotation().Roll));
   if(Now-Phase<5) return false;
   Test->TestTrue(TEXT("Roof standing five seconds causes no impact damage"),F.NoDamage(Car));
   Test->TestTrue(TEXT("Stable relative roof position"),FVector::Dist2D(LocalOrigin,F.Local(Car))<65);
   Test->TestTrue(TEXT("Normal roof load does not roll vehicle"),MaxRoll<12);
   Test->AddInfo(FString::Printf(TEXT("Car %d standing 5s travel %.1f local drift %.1f max roll %.2f"),Index,FVector::Dist2D(Origin,Car->GetActorLocation()),FVector::Dist2D(LocalOrigin,F.Local(Car)),MaxRoll));
   if(bMoving) { Test->TestTrue(TEXT("Chaos vehicle translated while rider stayed based"),FVector::Dist2D(Origin,Car->GetActorLocation())>250); if(Index==0) F.Shot(TEXT("C-Riding-City.png"),Car); }
   DepartVelocity=F.Move->GetImpartedMovementBaseVelocity();
   if(bMoving) { F.Jump(); Stage=9; } else Stage=7;
   Phase=Now; return false;
  }
  if(Stage==7)
  {
   F.Player->AddMovementInput(FVector(0,1,0),1);
   if(!F.Based(Car) && F.Move->IsMovingOnGround()) { Phase=Now; Stage=8; if(Index==0) F.Shot(TEXT("A-Walked-Off.png"),Car); return false; }
   if(Now-Phase>4) { Test->AddError(TEXT("Walk off did not return to ground")); return true; }
  }
  if(Stage==8 && Now-Phase>.8f)
  {
   Test->TestTrue(TEXT("Walking off does not flip or damage vehicle"),FMath::Abs(Car->GetActorRotation().Roll)<12 && F.NoDamage(Car));
   // Second actual jump from hood/roof; fixture teleport only establishes the new test start.
   // Start clear of the roof. A fixed delay after a near-contact teleport could
   // sample the capsule before Chaos/CMC had acquired the floor under load.
   const FVector P=Car->GetActorTransform().TransformPosition(FVector(-25,0,300));
   F.Player->ConsumeMovementInputVector(); F.Move->SetMovementMode(MOVE_Falling); F.Move->StopMovementImmediately();
   F.Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
   Phase=Now; Stage=10; return false;
  }
  if(Stage==10)
  {
   if(F.Based(Car)) { Test->TestTrue(TEXT("Second roof fixture has acquired native base"),true); F.Jump(); Phase=Now; Stage=9; return false; }
   if(Now-Phase>3) { Test->AddError(TEXT("Second roof fixture failed to acquire native base")); return true; }
   return false;
  }
  if(Stage==9)
  {
   if(F.Move->IsFalling() && !CheckedDeparture)
   {
    Test->TestTrue(TEXT("Jump releases movement base"),F.Move->IsFalling() && !F.Player->GetMovementBaseObject());
    if(bMoving) Test->TestTrue(TEXT("Jump inherits horizontal vehicle velocity"),F.Move->Velocity.X>10 && FMath::Abs(F.Move->Velocity.X-DepartVelocity.X)<F.Move->BrakingDecelerationFalling*(Now-Phase)+20 && F.Move->Velocity.Size2D()<2205);
    Test->AddInfo(FString::Printf(TEXT("Departure base %s player %s"),*DepartVelocity.ToString(),*F.Move->Velocity.ToString()));
    F.ReleaseJump(); CheckedDeparture=true; if(Index==0) F.Shot(TEXT("D-Jump-Departure.png"),Car);
   }
   F.Player->AddMovementInput(FVector(0,1,0),1);
   if(Now-Phase<2) return false;
   Test->TestTrue(TEXT("Jump departure leaves car upright and undamaged"),FMath::Abs(Car->GetActorRotation().Roll)<12 && F.NoDamage(Car));
   Car->GetDrivingComponent()->SetDrivingEnabled(false); Car->GetChaosMovement()->ResetVehicle(); Car->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics);
   if(++Index<4) { Stage=1; CheckedDeparture=false; MaxRoll=0; return false; }
   return true;
  }
  return false;
 }
 FAutomationTestBase* Test; bool bMoving=false,CheckedDeparture=false; FScene F; ANeonCityBaseVehicle* Car=nullptr;
 double Started=0; float Phase=0,MaxRoll=0; int32 Stage=0,Index=0,Landings=0; FVector Origin,LocalOrigin,DepartVelocity; FTransform OriginalTransform;
};

class FDynamicConsequences : public IAutomationLatentCommand
{
public:
 explicit FDynamicConsequences(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>70) { Test->AddError(FString::Printf(TEXT("Dynamic consequences timeout stage %d"),Stage)); return true; }
  if(Stage==0) { if(!F.Ready()) return false; Car=F.Cars[0]; Original=Car->GetActorTransform(); F.Place(Car); Phase=F.W->GetTimeSeconds(); Stage=1; return false; }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1.5f)
  {
   F.Player->SetActorLocation(Car->GetActorTransform().TransformPosition(FVector(-20,0,240)),false,nullptr,ETeleportType::TeleportPhysics);
   F.Move->SetMovementMode(MOVE_Falling); F.Move->StopMovementImmediately(); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>.6f)
  {
   if(!Test->TestTrue(TEXT("Angular test starts on a real floor base"),F.Based(Car))) return true;
   LocalOrigin=F.Local(Car); StartYaw=Car->GetActorRotation().Yaw;
   Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetDrivingComponent()->SetDriveInput(.45f); Car->GetDrivingComponent()->SetSteerInput(.3f);
   Phase=Now; Stage=3; return false;
  }
  if(Stage==3)
  {
   if(!F.Based(Car)) { Test->AddError(TEXT("Rider lost base during ordinary Chaos steering")); return true; }
   if(Now-Phase<3) return false;
   const float Turn=FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw,Car->GetActorRotation().Yaw));
   Test->TestTrue(TEXT("Actual steering rotates the vehicle"),Turn>2);
   Test->TestTrue(TEXT("Based movement carries rotation as well as translation"),FVector::Dist2D(LocalOrigin,F.Local(Car))<40);
   Test->TestTrue(TEXT("Ordinary turn creates no vehicle impact damage"),F.NoDamage(Car));
   Test->AddInfo(FString::Printf(TEXT("Turn %.2f degrees, roof drift %.2f cm, speed %.1f"),Turn,FVector::Dist2D(LocalOrigin,F.Local(Car)),Car->GetVelocity().Size()));
   Releases=F.Move->GetSurfaceReleaseCount();
   Car->GetMesh()->AddImpulse(FVector(0,1100,250),NAME_None,true); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>.18f)
  {
   Test->TestTrue(TEXT("Violent acceleration releases rider instead of gluing to surface"),F.Move->GetSurfaceReleaseCount()>Releases && F.Move->IsFalling());
   F.Player->SetActorLocation(FVector(-3400,800,118),false,nullptr,ETeleportType::TeleportPhysics); F.Move->StopMovementImmediately();
   Car->GetDrivingComponent()->SetDrivingEnabled(false); Car->GetChaosMovement()->ResetVehicle(); Car->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);
   Car=F.Cars[1]; F.Place(Car); F.Player->SetActorLocation(FVector(-3400,0,118),false,nullptr,ETeleportType::TeleportPhysics);
   Phase=Now; Stage=5; return false;
  }
  if(Stage==5 && Now-Phase>1.5f)
  { Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetMesh()->SetPhysicsLinearVelocity(FVector(1800,0,0)); Phase=Now; Stage=6; return false; }
  if(Stage==6)
  {
   const auto& Events=F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents();
   const auto* E=Events.FindByPredicate([&](const auto& Entry){return Entry.TargetActor.Get()==F.Player;});
   if(!E && Now-Phase<3) return false;
   if(!Test->TestNotNull(TEXT("High speed sports-car contact remains a real impact"),E)) return true;
   Test->TestTrue(TEXT("High speed collision damages and downs player"),E->AppliedDamage>0 && F.Player->GetHealthComponent()->GetCurrentHealth()<=20
    && F.Player->FindComponentByClass<UCharacterImpactComponent>()->GetState()==ECharacterImpactState::Downed);
   Test->AddInfo(FString::Printf(TEXT("Sports impact speed %.1f damage %.1f health %.1f"),E->ImpactSpeed,E->AppliedDamage,F.Player->GetHealthComponent()->GetCurrentHealth()));
   F.Shot(TEXT("E-Sports-Impact.png"),Car); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle* Car=nullptr; FTransform Original;
 FVector LocalOrigin; int32 Stage=0,Releases=0; float Phase=0,StartYaw=0; double Started=0;
};


class FDeparture : public IAutomationLatentCommand
{
public:
 explicit FDeparture(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("50 km/h departure timeout")); return true; }
  if(Stage==0) { if(!F.Ready()) return false; Car=F.Cars[0]; F.Place(Car); Phase=F.W->GetTimeSeconds(); Stage=1; return false; }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1.5f)
  {
   F.Player->SetActorLocation(Car->GetActorTransform().TransformPosition(FVector(-20,0,240)),false,nullptr,ETeleportType::TeleportPhysics);
   F.Move->SetMovementMode(MOVE_Falling); F.Move->StopMovementImmediately(); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>.6f)
  {
   if(!Test->TestTrue(TEXT("High speed departure fixture is natively based"),F.Based(Car))) return true;
   // Set the vehicle's initial physical velocity once, never the rider's. Jump uses Enhanced Input.
   Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetMesh()->SetPhysicsLinearVelocity(FVector(1388.89,0,0));
   const auto Inherited=F.Move->GetImpartedMovementBaseVelocity();
   Test->TestTrue(TEXT("50 km/h base contributes real horizontal point velocity"),FMath::IsNearlyEqual(Inherited.X,1388.89f,5.f));
   F.Jump(); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && F.Move->IsFalling())
  {
   Test->TestTrue(TEXT("50 km/h jump inherits inertia and normal vertical jump"),F.Move->Velocity.X>1000 && F.Move->Velocity.X<1450 && F.Move->Velocity.Z>350);
   Test->TestTrue(TEXT("Jump clears base without reciprocal impulse"),!F.Player->GetMovementBaseObject() && FMath::Abs(Car->GetActorRotation().Roll)<5 && F.NoDamage(Car));
   Test->AddInfo(FString::Printf(TEXT("50 km/h departure actual velocity %s"),*F.Move->Velocity.ToString()));
   F.ReleaseJump(); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>.3f)
  {
   Test->TestTrue(TEXT("Inertia persists while original air braking remains active"),F.Move->Velocity.X>200 && F.Move->Velocity.X<1389);
   Test->TestTrue(TEXT("Vehicle remains upright after high-speed departure"),FMath::Abs(Car->GetActorRotation().Roll)<8 && F.NoDamage(Car));
   return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle* Car=nullptr; int32 Stage=0; float Phase=0; double Started=0;
};


class FBaseSave : public IAutomationLatentCommand
{
public:
 explicit FBaseSave(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Based load timeout")); if(Save) Save->DeleteSave(); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; Save=F.W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); Save->ConfigureSlot(TEXT("NeonCity_Test_VehicleSurface")); Save->DeleteSave();
   if(!Test->TestTrue(TEXT("Create isolated save before roof scenario"),Save->CreateNewGameSave())) return true;
   SavedPosition=F.Player->GetActorLocation(); Car=F.Cars[0]; F.Place(Car); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1.5f)
  {
   F.Player->SetActorLocation(Car->GetActorTransform().TransformPosition(FVector(-20,0,240)),false,nullptr,ETeleportType::TeleportPhysics);
   F.Move->SetMovementMode(MOVE_Falling); F.Move->StopMovementImmediately(); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>.6f)
  {
   Test->TestTrue(TEXT("Load begins while standing on vehicle"),F.Based(Car));
   Test->TestTrue(TEXT("Existing SaveSubsystem loads while based"),Save->LoadGame());
   Test->TestFalse(TEXT("Old vehicle base is cleared before restoring actor transforms"),F.Based(Car));
   Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>.6f)
  {
   Test->TestTrue(TEXT("Saved player is not carried by vehicle restoration"),FVector::Dist2D(SavedPosition,F.Player->GetActorLocation())<10 && F.Move->IsMovingOnGround());
   Test->TestTrue(TEXT("Clear test save"),Save->DeleteSave()); Test->TestFalse(TEXT("Test save removed"),Save->DoesSaveExist()); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; UNeonCitySaveSubsystem* Save=nullptr; ANeonCityBaseVehicle* Car=nullptr;
 FVector SavedPosition; int32 Stage=0; double Started=0; float Phase=0;
};

}
#define SURFACE_PIE(Class,Name,Moving) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString& Parameters) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCitySurfaceTests::FStanding>(this,Moving)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
SURFACE_PIE(FParkedSurfaces,"NeonCity.VehicleSurface.ParkedPIE",false)
SURFACE_PIE(FMovingSurfaces,"NeonCity.VehicleSurface.MovingPIE",true)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceConsequences,"NeonCity.VehicleSurface.ConsequencesPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurfaceConsequences::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCitySurfaceTests::FDynamicConsequences>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceDeparture,"NeonCity.VehicleSurface.DeparturePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurfaceDeparture::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCitySurfaceTests::FDeparture>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceSave,"NeonCity.VehicleSurface.LoadWhileBasedPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSurfaceSave::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCitySurfaceTests::FBaseSave>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#undef SURFACE_PIE
#endif
