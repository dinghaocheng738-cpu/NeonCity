#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleCombatTestArea.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/TargetingComponent.h"
#include "VehicleCombat/LockOnSystem.h"
#include "VehicleCombat/ExplosionDamageComponent.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingFlightComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleCombatProfiles,"NeonCity.VehicleCombat.Profiles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleCombatProfiles::RunTest(const FString&)
{
 for(FName ID:{FName(TEXT("NeonCity.MountedMachineGun")),FName(TEXT("NeonCity.AircraftCannon")),FName(TEXT("NeonCity.RocketPod")),FName(TEXT("NeonCity.GuidedMissile"))})
 { const auto* D=UVehicleWeaponManager::Profile(ID); TestTrue(TEXT("Four explicit native profile factories valid"),D && D->IsValidProfile()); }
 TestFalse(TEXT("Arbitrary save class ID refused"),bool(UVehicleWeaponManager::ResolveWeapon(TEXT("PIE_WeaponInjected"))));
 auto* Bad=NewObject<UVehicleWeaponData>(); Bad->ExplosionRadius=100000; TestFalse(TEXT("Large explosion refused"),Bad->IsValidProfile());
 TestEqual(TEXT("Center falloff full"),UExplosionDamageComponent::Falloff(0,500),1.f);
 TestEqual(TEXT("Edge falloff zero"),UExplosionDamageComponent::Falloff(500,500),0.f);
 TestTrue(TEXT("Monotonic distance attenuation"),UExplosionDamageComponent::Falloff(100,500)>UExplosionDamageComponent::Falloff(400,500));
 return true;
}
namespace NeonCityVehicleCombatTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 ANeonCityBaseVehicle* Car=nullptr; ANeonCityBaseVehicle* Pickup=nullptr;
 ANeonCityMarineVehicleBase* Boat=nullptr; ANeonCityAircraftVehicleBase* Heli=nullptr; ANeonCityFixedWingAircraft* Plane=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<2 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  W->SpawnActor<AVehicleCombatTestArea>();
  for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)
  { if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::SUV) Car=*It; if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Pickup) Pickup=*It; }
  for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It) if(It->VehicleID==UMarineWorldSubsystem::SpeedboatID()) Boat=*It;
  for(TActorIterator<ANeonCityAircraftVehicleBase> It(W);It;++It) { if(auto* P=Cast<ANeonCityFixedWingAircraft>(*It)) Plane=P; else Heli=*It; }
  Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); return Car && Pickup && Boat && Heli && Plane;
 }
 UVehicleCombatComponent* Combat(AActor* A) const { return A->FindComponentByClass<UVehicleCombatComponent>(); }
 void PlayerAt(FVector P)
 {
  Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
 void PlaceCar()
 {
  Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);
  Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
  Car->SetActorLocationAndRotation(FVector(-12000,7000,540),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
  PlayerAt(FVector(-12000,6740,510));
 }
 ACombatTestTarget* Target(FVector P,float HP=1000)
 { auto* T=W->SpawnActor<ACombatTestTarget>(P,FRotator::ZeroRotator); T->GetHealthComponent()->ApplyPersistenceHealth(HP,HP); return T; }
 void Aim(UVehicleCombatComponent* C,AActor* T)
 { const FVector Direction=T->GetActorLocation()-C->GetManager()->GetPoint()->GetComponentLocation(); C->AimWeapon(Direction); PC->SetControlRotation(Direction.Rotation()); }
 void Shot(const TCHAR* Name,AActor* Focus)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityVehicleCombatCapture"))) return;
  const FVector P=Focus->GetActorLocation(),Eye=P+FVector(1000,-1400,800);
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(P-Eye).Rotation()); PC->SetViewTarget(Camera); PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0);
  bool Done=false;
  if(auto* V=W->GetGameViewport();V && V->Viewport)
  {
   V->Viewport->Draw(false); TArray<FColor> Pixels;
   if(V->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pxl:Pixels) Pxl.A=255; TArray64<uint8> PNG; const auto Size=V->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("VehicleCombat")),true);
    Done=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("VehicleCombat")/Name));
   }
  }
  if(!Done) UE_LOG(LogTemp,Error,TEXT("Vehicle combat screenshot failed"));
  PC->SetViewTarget(PC->GetPawn()); Camera->Destroy();
 }
};
class FGroundBoat : public IAutomationLatentCommand
{
public:
 explicit FGroundBoat(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(FString::Printf(TEXT("Ground/boat stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; F.PlaceCar(); return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  auto* C=F.Combat(F.Car); auto* M=C->GetManager();
  if(Stage==1 && Elapsed>.8f)
  {
   Test->TestTrue(TEXT("SUV roof mount installed"),M->GetPoint()->HardpointID==TEXT("RoofMount"));
   Test->TestNotNull(TEXT("Pickup shares same combat adapter"),F.Combat(F.Pickup));
   Test->TestFalse(TEXT("Roof mount rejects missile"),M->Equip(TEXT("RoofMount"),TEXT("NeonCity.GuidedMissile")));
   if(!Test->TestTrue(TEXT("Enter SUV through interaction"),F.Car->GetVehicleInteraction()->Enter(F.Player))) return true;
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f) { StartPosition=F.Car->GetActorLocation(); Key(F.PC,EKeys::W,IE_Pressed); return Next(3); }
  if(Stage==3 && Elapsed>1.2f)
  {
   Key(F.PC,EKeys::W,IE_Released); Key(F.PC,EKeys::SpaceBar,IE_Pressed);
   Test->TestTrue(TEXT("Armed-capable SUV retains real driving"),FVector::Distance(StartPosition,F.Car->GetActorLocation())>25);
   return Next(4);
  }
  if(Stage==4 && Elapsed>1.5f && F.Car->GetVelocity().Size()<100)
  {
   Key(F.PC,EKeys::SpaceBar,IE_Released); Victim=F.Target(M->GetPoint()->GetComponentLocation()+FVector(1800,0,0));
   F.Aim(C,Victim); Test->TestFalse(TEXT("Safe weapon cannot fire"),C->FireWeapon()); Key(F.PC,EKeys::V,IE_Pressed); return Next(5);
  }
  if(Stage==5 && Elapsed>.15f)
  { Key(F.PC,EKeys::V,IE_Released); Before=M->GetWeapon()->GetAmmo(); Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(6); }
  if(Stage==6 && Elapsed>.55f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released);
   Test->TestTrue(TEXT("Actual LMB consumes ammo"),M->GetWeapon()->GetAmmo()<Before);
   Test->TestTrue(TEXT("Mounted bullets damage target through shared health"),Victim->GetHealthComponent()->GetCurrentHealth()<1000);
   Test->TestTrue(TEXT("Shot rate bounded"),M->GetWeapon()->GetShotCount()<=6);
   Test->TestTrue(TEXT("Heat feedback populated"),M->GetWeapon()->GetHeat()>0);
   F.Shot(TEXT("A-SUV.png"),F.Car); Key(F.PC,EKeys::R,IE_Pressed); return Next(7);
  }
  if(Stage==7 && Elapsed>2.1f)
  {
   Key(F.PC,EKeys::R,IE_Released); Test->TestEqual(TEXT("Reload fills magazine"),M->GetWeapon()->GetAmmo(),40);
   Test->TestTrue(TEXT("Reload spends finite reserve"),M->GetWeapon()->GetReserveAmmo()<160);
   if(!Test->TestTrue(TEXT("SUV normal exit retained"),F.Car->GetVehicleInteraction()->Exit())) return true;
   F.PlayerAt(F.Boat->GetActorLocation()+FVector(260,0,120)); return Next(8);
  }
  if(Stage==8 && Elapsed>.2f)
  { if(!Test->TestTrue(TEXT("Boat entered through marine interface"),F.Boat->EnterVehicle_Implementation(F.Player))) return true; return Next(9); }
  if(Stage==9 && Elapsed>.3f)
  {
   Key(F.PC,EKeys::V,IE_Pressed);return Next(10);
  }
  if(Stage==10 && Elapsed>.2f)
  {
   Key(F.PC,EKeys::V,IE_Released);
   Test->TestTrue(TEXT("Marine V selects helm camera"),F.Boat->IsHelmCamera());
   Test->TestFalse(TEXT("Camera toggle never arms boat weapon"),F.Combat(F.Boat)->IsArmed());
   Key(F.PC,EKeys::B,IE_Pressed);return Next(11);
  }
  if(Stage==11 && Elapsed>.2f)
  {
   Key(F.PC,EKeys::B,IE_Released);
   auto* B=F.Combat(F.Boat);Test->TestTrue(TEXT("Marine B arms weapon independently"),B->IsArmed());B->AimWeapon(F.Boat->GetActorForwardVector());
   Test->TestTrue(TEXT("Speedboat uses same fire interface"),B->FireWeapon());
   Test->TestEqual(TEXT("Boat round consumed"),B->GetManager()->GetWeapon()->GetAmmo(),39);
   Test->TestTrue(TEXT("Boat driver control still accepted"),F.Boat->SubmitDriverInput_Implementation(F.PC,.3f,0,0));
   F.Shot(TEXT("Boat-Hardpoint.png"),F.Boat); return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int Stage=0,Before=0; FVector StartPosition; ACombatTestTarget* Victim=nullptr;
};
class FAircraft : public IAutomationLatentCommand
{
public:
 explicit FAircraft(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>90) { Test->AddError(FString::Printf(TEXT("Aircraft combat stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; F.PlayerAt(F.Heli->GetActorLocation()+FVector(260,0,50)); return Next(1); }
  const float E=F.W->GetTimeSeconds()-At;
  if(Stage==1 && E>.5f)
  { if(!Test->TestTrue(TEXT("Player boards existing helicopter"),F.Heli->EnterAircraft_Implementation(F.Player))) return true; return Next(2); }
  if(Stage==2 && E>.4f)
  {
   auto* C=F.Combat(F.Heli); C->SetArmed(true); F.Heli->SubmitFlightInput_Implementation(F.PC,FVector(0,0,.6f),0); StartZ=F.Heli->GetActorLocation().Z; return Next(3);
  }
  if(Stage==3 && E>2)
  {
   Test->TestTrue(TEXT("Pilot has climbed under flight controls"),F.Heli->GetActorLocation().Z>StartZ+80);
   auto* C=F.Combat(F.Heli); Victim=F.Target(F.Heli->GetActorLocation()+FVector(0,-1800,0)); F.Aim(C,Victim);
   if(!Test->TestTrue(TEXT("Helicopter shared machine gun fires"),C->FireWeapon())) return true;
   return Next(4);
  }
  if(Stage==4 && E>.25f)
  {
   Test->TestTrue(TEXT("Helicopter bullet hits target"),Victim->GetHealthComponent()->GetCurrentHealth()<1000); F.Shot(TEXT("B-HelicopterGun.png"),F.Heli);
   auto* C=F.Combat(F.Heli); C->GetManager()->Select(TEXT("SideMount.Left"));
   auto* P=C->GetManager()->GetPoint(); RocketTarget=F.Target(P->GetComponentLocation()+P->GetForwardVector()*1500);
   C->AimWeapon(P->GetForwardVector()); Test->TestTrue(TEXT("Unguided rocket fired from side mount"),C->FireWeapon());
   Rocket=C->GetManager()->GetWeapon()->GetLastProjectile(); Test->TestTrue(TEXT("Rocket has no guidance"),Rocket.IsValid() && !Rocket->HasGuidance()); return Next(5);
  }
  if(Stage==5 && E>.8f)
  {
   Test->TestTrue(TEXT("Rocket impact radial damage applied"),RocketTarget->GetHealthComponent()->GetCurrentHealth()<1000); F.Shot(TEXT("C-HelicopterRocket.png"),F.Heli);
   // Isolated transition fixture: return this aircraft to its authored safe stand, then use normal exit/entry.
   F.Heli->GetAircraftMovement()->StopEngine(); F.Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);
   F.Heli->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); F.Heli->SetActorLocationAndRotation(FVector(-3000,2450,137),FRotator(0,-90,0),false,nullptr,ETeleportType::TeleportPhysics);
   F.Heli->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1); return Next(6);
  }
  if(Stage==6 && E>1)
  {
   if(!Test->TestTrue(TEXT("Helicopter safely exits"),F.Heli->ExitAircraft_Implementation())) return true;
   F.PlayerAt(F.Plane->GetActorLocation()+FVector(380,0,50)); return Next(7);
  }
  if(Stage==7 && E>.25f)
  { if(!Test->TestTrue(TEXT("Fixed wing interaction retains pilot"),F.Plane->EnterAircraft_Implementation(F.Player))) return true; return Next(8); }
  if(Stage==8 && E>.4f)
  {
   // High-speed airborne fixture; subsequent flight and projectiles use their actual simulation.
   F.Plane->SetActorLocationAndRotation(FVector(0,7000,4000),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   F.Plane->GetHull()->SetPhysicsLinearVelocity(FVector(4000,0,0)); F.Plane->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
   F.Plane->GetFlight()->SetControls(.6f,0,0,0,false); F.Plane->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.3f);
   auto* C=F.Combat(F.Plane); C->SetArmed(true); return Next(9);
  }
  if(Stage==9 && E>.4f)
  {
   auto* C=F.Combat(F.Plane); auto* P=C->GetManager()->GetPoint(); Victim=F.Target(P->GetComponentLocation()+P->GetForwardVector()*2500);
   Test->TestTrue(TEXT("Aircraft cannon fires at speed"),C->FireWeapon()); auto* Bullet=C->GetManager()->GetWeapon()->GetLastProjectile();
   Test->TestNotNull(TEXT("Cannon projectile exists"),Bullet);
   if(Bullet)
   {
    Test->TestTrue(TEXT("Projectile originates at nose, not character center"),FVector::Dist(Bullet->GetActorLocation(),P->GetComponentLocation())<5);
    Test->TestTrue(TEXT("Aircraft velocity added to muzzle velocity"),Bullet->GetFlightVelocity().X>46000);
   }
   return Next(10);
  }
  if(Stage==10 && E>.3f)
  {
   Test->TestTrue(TEXT("High-speed cannon hits target"),Victim->GetHealthComponent()->GetCurrentHealth()<1000);
   Test->TestTrue(TEXT("Fixed wing remains airborne and moving"),F.Plane->GetVelocity().Size()>3000 && !F.Plane->GetFlight()->IsGrounded());
   F.Shot(TEXT("D-AircraftCannon.png"),F.Plane); return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0,StartZ=0; int Stage=0; ACombatTestTarget* Victim=nullptr; ACombatTestTarget* RocketTarget=nullptr; TWeakObjectPtr<ABulletProjectile> Rocket;
};

class FLockMissile : public IAutomationLatentCommand
{
public:
 explicit FLockMissile(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>75) { Test->AddError(FString::Printf(TEXT("Lock/missile stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; F.PlayerAt(F.Heli->GetActorLocation()+FVector(260,0,50)); return Next(1); }
  const float E=F.W->GetTimeSeconds()-At; auto* C=F.Combat(F.Heli); auto* L=C->GetLockSystem();
  if(Stage==1 && E>.5f)
  {
   if(!Test->TestTrue(TEXT("Helicopter occupied for targeting"),F.Heli->EnterAircraft_Implementation(F.Player))) return true;
   return Next(2);
  }
  if(Stage==2 && E>.4f)
  {
   F.Heli->SetActorLocationAndRotation(FVector(0,7000,800),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   F.Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector); F.Heli->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
   F.Heli->GetAircraftMovement()->SetInput(FVector(0,0,.1f),0);
   // Actual parked Chaos vehicle on the existing runway, distinct from the piloted helicopter.
   F.Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); F.Car->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
   F.Car->SetActorLocationAndRotation(FVector(2800,7000,545),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
   F.Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);
   F.Car->GetHealthComponent()->ApplyPersistenceHealth(500,500);
   C->GetManager()->Select(TEXT("SideMount.Right")); C->SetArmed(true); F.Aim(C,F.Car); Key(F.PC,EKeys::RightMouseButton,IE_Pressed); return Next(3);
  }
  if(Stage==3 && E>.5f)
  {
   F.Heli->GetAircraftMovement()->SetInput(FVector::ZeroVector,0);
   Test->TestTrue(TEXT("Locking takes time; no instant lock"),L->State==EVehicleLockState::Locking);
   Test->TestFalse(TEXT("Missile launch before lock rejected"),C->FireWeapon()); return Next(4);
  }
  if(Stage==4 && E>1.4f)
  {
   Test->TestTrue(TEXT("LOS vehicle target locked after dwell"),L->GetLockedTarget()==F.Car);
   F.Shot(TEXT("E-LockedVehicle.png"),F.Heli);
   Wall=F.W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->InitBoxExtent(FVector(50,650,650)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
   Wall->SetActorLocation(FVector(1400,7000,900));
   auto* WallVisual=NewObject<UStaticMeshComponent>(Wall); WallVisual->SetupAttachment(Box); WallVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); WallVisual->SetRelativeScale3D(FVector(1,13,13)); WallVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision); WallVisual->RegisterComponent(); return Next(5);
  }
  if(Stage==5 && E>.35f)
  {
   Test->AddInfo(FString::Printf(TEXT("Occlusion lock state %d armed %d target %s heli %s"),int(L->State),C->IsArmed(),*GetNameSafe(L->GetCandidate()),*F.Heli->GetActorLocation().ToString()));
   Test->TestTrue(TEXT("Occlusion loses existing lock"),L->State==EVehicleLockState::Lost);
   Test->TestFalse(TEXT("Wall blocks launch"),C->FireWeapon());
   Test->TestFalse(TEXT("Wall blocks target inspection"),C->GetTargeting()->Inspect(F.Car,C->GetManager()->GetPoint()->GetComponentLocation(),FVector::ForwardVector,12000,25).Visible);
   F.Shot(TEXT("F-LostLOS.png"),Wall); Wall->Destroy(); return Next(6);
  }
  if(Stage==6 && E>1.8f)
  {
   Test->AddInfo(FString::Printf(TEXT("Reacquire state %d target %s progress %f heli %s"),int(L->State),*GetNameSafe(L->GetCandidate()),L->GetProgress(),*F.Heli->GetActorLocation().ToString()));
   if(!Test->TestTrue(TEXT("Lock reacquired with new dwell"),L->GetLockedTarget()==F.Car)) return true;
   if(!Test->TestTrue(TEXT("Guided missile launches"),C->FireWeapon())) return true;
   Missile=Cast<AMissileProjectile>(C->GetManager()->GetWeapon()->GetLastProjectile());
   Test->TestTrue(TEXT("Missile starts with guidance"),Missile.IsValid() && Missile->HasGuidance()); LastDir=Missile->GetFlightVelocity().GetSafeNormal(); LastTime=F.W->GetTimeSeconds();
   F.Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0,200,0)); return Next(7);
  }
  if(Stage==7)
  {
   if(Missile.IsValid())
   {
    const FVector Dir=Missile->GetFlightVelocity().GetSafeNormal(); const float Dt=F.W->GetTimeSeconds()-LastTime;
    const float Degrees=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir,LastDir),-1.f,1.f)));
    Test->TestTrue(TEXT("Missile turn rate is bounded by 45 degrees per second"),Degrees<=45*Dt+.6f);
    LastDir=Dir; LastTime=F.W->GetTimeSeconds(); return false;
   }
   Test->TestTrue(TEXT("Guidance hits moving ground vehicle and applies explosion damage"),F.Car->GetHealthComponent()->GetCurrentHealth()<500);
   F.Shot(TEXT("G-VehicleDamage.png"),F.Car); Key(F.PC,EKeys::RightMouseButton,IE_Released);
   // Exercise target loss after launch separately from lock acquisition, with a bounded-fuel projectile.
   const auto* D=UVehicleWeaponManager::Profile(TEXT("NeonCity.GuidedMissile"));
   LostTarget=F.Target(FVector(6000,7000,5000)); auto* Pool=F.W->GetSubsystem<UVehicleProjectilePool>();
   auto* P=Pool->Acquire(AMissileProjectile::StaticClass(),FTransform(FVector(3000,7000,5000)),F.Heli);
   FNeonCityDamageEvent Event; Event.DamageAmount=120; Event.SourceActor=F.Heli; Event.Instigator=F.PC;
   P->LaunchVehicle(FVector::ForwardVector,FVector::ZeroVector,*D,Event,LostTarget); LostMissile=Cast<AMissileProjectile>(P);
   LostTarget->Destroy(); return Next(8);
  }
  if(Stage==8 && E>.15f)
  { Test->TestTrue(TEXT("Destroyed target produces finite unguided continuation"),LostMissile.IsValid() && !LostMissile->HasGuidance() && LostMissile->GetFlightVelocity().Size()>0); return true; }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0,LastTime=0; int Stage=0; FVector LastDir;
 AActor* Wall=nullptr; ACombatTestTarget* LostTarget=nullptr; TWeakObjectPtr<AMissileProjectile> Missile,LostMissile;
};
class FExplosionCrime : public IAutomationLatentCommand
{
public:
 explicit FExplosionCrime(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Explosion/crime timeout")); return true; }
  if(!Stage) { if(!F.Ready()) return false; F.PlaceCar(); return Next(1); }
  const float E=F.W->GetTimeSeconds()-At; auto* C=F.Combat(F.Car);
  if(Stage==1 && E>.8f)
  {
   if(!Test->TestTrue(TEXT("Player enters crime test SUV"),F.Car->GetVehicleInteraction()->Enter(F.Player))) return true;
   return Next(2);
  }
  if(Stage==2 && E>.3f)
  {
   C->SetArmed(true); auto* Point=C->GetManager()->GetPoint();
   const FVector Origin=Point->GetComponentLocation();
   NPC=F.W->SpawnActor<ANeonCityNPCCharacter>(Origin+FVector(1300,0,0),FRotator(0,180,0)); NPC->GetCharacterMovement()->DisableMovement();
   F.Aim(C,NPC); Test->TestTrue(TEXT("Mounted weapon attacks NPC"),C->FireWeapon()); return Next(3);
  }
  if(Stage==3 && E>.2f)
  {
   Test->TestTrue(TEXT("Mounted projectile damages NPC"),NPC->GetHealthComponent()->GetCurrentHealth()<100);
   Test->TestTrue(TEXT("NPC damage crime retains driver instigator"),F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().ContainsByPredicate([&](const auto& V){return V.CrimeType==ENeonCityCrimeType::DamageNPC && V.Victim==NPC && V.Instigator==F.PC;}));
   NPC->Destroy();
   Officer=F.W->SpawnActor<APoliceCharacter>(C->GetManager()->GetPoint()->GetComponentLocation()+FVector(1500,0,0),FRotator(0,180,0));
   Officer->GetCharacterMovement()->DisableMovement(); F.Aim(C,Officer); return Next(4);
  }
  if(Stage==4 && E>.3f)
  { Test->TestTrue(TEXT("Player vehicle weapon attacks police"),C->FireWeapon()); return Next(5); }
  if(Stage==5 && E>.35f)
  {
   Test->TestTrue(TEXT("PoliceAttack offense recorded"),F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().ContainsByPredicate([&](const auto& V){return V.Offense==EPoliceOffense::PoliceAttack && V.Victim==Officer;}));
   Test->TestTrue(TEXT("Police witness elevates Wanted without balance changes"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()>0);
   const FVector Center(8000,7000,4000);
   Near=F.Target(Center+FVector(100,0,0)); Edge=F.Target(Center+FVector(0,350,0)); Outside=F.Target(Center+FVector(-550,0,0));
   auto* Host=F.W->SpawnActor<AActor>(); auto* Explosion=NewObject<UExplosionDamageComponent>(Host); Explosion->RegisterComponent();
   FNeonCityDamageEvent Payload; Payload.DamageAmount=100; Payload.SourceActor=C->GetManager()->GetWeapon(); Payload.Instigator=F.PC;
   Test->TestTrue(TEXT("Driver exits before radial player-damage fixture"),F.Car->GetVehicleInteraction()->Exit());
   F.PlayerAt(Center+FVector(0,-200,0)); F.Player->GetHealthComponent()->ApplyPersistenceHealth(100,100);
   auto* Prop=F.W->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),FTransform(Center+FVector(-250,0,0)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Prop->Configure(ECityPhysicsProp::Sign); Prop->FinishSpawning(FTransform(Center+FVector(-250,0,0)));
   auto* Sheltered=F.Target(Center+FVector(0,0,350));
   auto* Blocker=F.W->SpawnActor<AActor>(); auto* Shield=NewObject<UBoxComponent>(Blocker); Blocker->SetRootComponent(Shield);
   Shield->InitBoxExtent(FVector(100,100,20)); Shield->SetCollisionProfileName(TEXT("BlockAll")); Shield->RegisterComponent(); Blocker->SetActorLocation(Center+FVector(0,0,175));
   Test->TestEqual(TEXT("Only four visible recipients inside radius damaged"),Explosion->Detonate(Center,500,Payload,F.Car),4);
   Test->TestEqual(TEXT("Player shares radial health pipeline"),F.Player->GetHealthComponent()->GetCurrentHealth(),40.f);
   Test->TestEqual(TEXT("Wall shields explosion target"),Sheltered->GetHealthComponent()->GetCurrentHealth(),1000.f);
   Test->TestTrue(TEXT("Explosion damage to public prop records PropertyDamage"),F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().ContainsByPredicate([&](const auto& V){return V.CrimeType==ENeonCityCrimeType::PropertyDamage && V.Victim==Prop && V.Instigator==F.PC;}));
   Test->TestEqual(TEXT("Near target 80 damage"),Near->GetHealthComponent()->GetCurrentHealth(),920.f);
   Test->TestEqual(TEXT("Edge target 30 damage"),Edge->GetHealthComponent()->GetCurrentHealth(),970.f);
   Test->TestEqual(TEXT("Outside radius unaffected"),Outside->GetHealthComponent()->GetCurrentHealth(),1000.f);
   Test->TestEqual(TEXT("Explosion resolves once"),Explosion->Detonate(Center,500,Payload,F.Car),0);
   F.Shot(TEXT("Explosion-Falloff.png"),Near); return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int Stage=0;
 ANeonCityNPCCharacter* NPC=nullptr; APoliceCharacter* Officer=nullptr; ACombatTestTarget* Near=nullptr; ACombatTestTarget* Edge=nullptr; ACombatTestTarget* Outside=nullptr;
};
class FLimits : public IAutomationLatentCommand
{
public:
 explicit FLimits(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>45) { Test->AddError(TEXT("Projectile bounds timeout")); return true; }
  if(!Stage)
  {
   if(!F.Ready()) return false;
   Pool=F.W->GetSubsystem<UVehicleProjectilePool>(); UVehicleWeaponData* D=NewObject<UVehicleWeaponData>(); D->Lifetime=.2f;
   FNeonCityDamageEvent Event; Event.DamageAmount=5; Event.SourceActor=F.Car;
   for(int32 I=0;I<UVehicleProjectilePool::MaxActive;++I)
   {
    auto* P=Pool->Acquire(ABulletProjectile::StaticClass(),FTransform(FVector(I*20,0,12000)),F.Car);
    if(!Test->TestNotNull(TEXT("Within budget allocation"),P)) return true;
    if(!Test->TestNotNull(TEXT("Projectile allocated"),P)) return true;
    Test->TestTrue(TEXT("Valid bounded projectile launched"),P->LaunchVehicle(FVector::UpVector,FVector::ZeroVector,*D,Event));
   }
   Test->TestNull(TEXT("Active projectile cap enforced"),Pool->Acquire(ABulletProjectile::StaticClass(),FTransform::Identity,F.Car));
   At=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  if(Stage==1 && F.W->GetTimeSeconds()-At>.4f)
  {
   Test->TestEqual(TEXT("Lifetime returns all actors"),Pool->GetActiveCount(),0);
   UVehicleWeaponData* D=NewObject<UVehicleWeaponData>(); D->Range=100; D->Lifetime=5;
   auto* P=Pool->Acquire(ABulletProjectile::StaticClass(),FTransform(FVector(0,0,12000)),F.Car); FNeonCityDamageEvent Event; Event.DamageAmount=5; Event.SourceActor=F.Car;
   if(!Test->TestNotNull(TEXT("Projectile allocated"),P)) return true;
    Test->TestTrue(TEXT("Valid bounded projectile launched"),P->LaunchVehicle(FVector::UpVector,FVector::ZeroVector,*D,Event)); DistanceProjectile=P; At=F.W->GetTimeSeconds(); Stage=2; return false;
  }
  if(Stage==2 && F.W->GetTimeSeconds()-At>.2f)
  {
   Test->TestFalse(TEXT("Distance recycle happens before lifetime"),DistanceProjectile.IsValid());
   F.Car->GetMesh()->SetSimulatePhysics(false); F.Car->SetActorLocation(FVector(0,7000,8000)); F.Car->GetHealthComponent()->ApplyPersistenceHealth(100,100);
   F.Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(0);
   const FVector Start=F.Car->GetActorLocation()+FVector(2000,0,80);
   auto* P=Pool->Acquire(ABulletProjectile::StaticClass(),FTransform(Start),F.Heli); if(!Test->TestNotNull(TEXT("Vehicle damage projectile allocated"),P)) return true;
   FNeonCityDamageEvent Event; Event.DamageAmount=14; Event.SourceActor=F.Heli; Event.Instigator=F.PC;
   Test->TestTrue(TEXT("Vehicle damage projectile launches"),P->LaunchVehicle((F.Car->GetActorLocation()+FVector(0,0,80)-Start).GetSafeNormal(),FVector::ZeroVector,*UVehicleWeaponManager::Profile(TEXT("NeonCity.MountedMachineGun")),Event));
   At=F.W->GetTimeSeconds(); Stage=3; return false;
  }
  if(Stage==3 && F.W->GetTimeSeconds()-At>.3f)
  { Test->TestEqual(TEXT("Bullet damages vehicle exactly once, no road-crash damage"),F.Car->GetHealthComponent()->GetCurrentHealth(),86.f); return true; }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int Stage=0; UVehicleProjectilePool* Pool=nullptr; TWeakObjectPtr<ABulletProjectile> DistanceProjectile;
};

class FPersistence : public IAutomationLatentCommand
{
public:
 FPersistence(FAutomationTestBase* T,FString M):Test(T),Mode(MoveTemp(M)) {}
 FString Signature(const TArray<FVehicleWeaponSaveData>& Records)
 { FString S; for(const auto& R:Records) S+=FString::Printf(TEXT("%s/%s/%s/%d/%d/%d;"),*R.VehicleID.ToString(),*R.HardpointID.ToString(),*R.WeaponID.ToString(),R.Ammo,R.Reserve,R.Selected); return S; }
 void Verify(UNeonCitySaveGame* Model)
 {
  const auto Live=F.W->GetSubsystem<UVehicleWeaponRegistry>()->BuildSnapshot();
  Test->TestEqual(TEXT("All mounted weapons, rounds and selections restored"),Signature(Live),Signature(Model->VehicleWeapons));
  for(const auto& R:Model->Vehicles) if(R.VehicleID==F.Car->VehicleID)
   Test->TestTrue(TEXT("Vehicle position and condition restored with loadout"),F.Car->GetActorTransform().Equals(R.Transform,.1f) && FMath::IsNearlyEqual(F.Car->GetHealthComponent()->GetCurrentHealth(),R.Condition*F.Car->GetHealthComponent()->GetMaxHealth(),.01f));
  for(AActor* A:{static_cast<AActor*>(F.Car),static_cast<AActor*>(F.Boat),static_cast<AActor*>(F.Heli),static_cast<AActor*>(F.Plane)})
   Test->TestFalse(TEXT("Load never resumes armed trigger or lock"),F.Combat(A)->IsArmed());
 }
 void Manifest(UNeonCitySaveGame* Model)
 {
  FString Position;
  for(const auto& R:Model->Vehicles) if(R.VehicleID==F.Car->VehicleID) Position=R.Transform.ToHumanReadableString().Replace(TEXT("\r"),TEXT(" ")).Replace(TEXT("\n"),TEXT(" "));
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("VehicleCombat")),true);
  const FString Json=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"loadout\":\"%s\",\"vehicleTransform\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*Model->SaveID.ToString(),*Signature(Model->VehicleWeapons),*Position);
  Test->TestTrue(TEXT("Session manifest written"),FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("VehicleCombat")/(TEXT("Session")+Mode+TEXT(".json")))));
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Vehicle weapon save timeout")); return true; }
  if(!bReady) { if(!F.Ready()) return false; bReady=true; At=F.W->GetTimeSeconds(); return false; }
  if(F.W->GetTimeSeconds()-At<1) return false;
  F.Save->ConfigureSlot(Mode==TEXT("RoundTrip")?TEXT("NeonCity_Test_VehicleCombat_RoundTrip"):TEXT("NeonCity_Test_VehicleCombat_Sessions"));
  auto* Registry=F.W->GetSubsystem<UVehicleWeaponRegistry>();
  if(Mode==TEXT("B"))
  {
   if(!Test->TestTrue(TEXT("Session B sees prior disk save"),F.Save->DoesSaveExist())) return true;
   TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection()); if(!Model.IsValid()) return true;
   FString Previous; FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("VehicleCombat/SessionA.json")));
   uint32 PID=0; FParse::Value(*Previous,TEXT("\"processId\":"),PID);
   Test->TestTrue(TEXT("Distinct OS process reads same save generation"),PID && PID!=FPlatformProcess::GetCurrentProcessId() && Previous.Contains(Model->SaveID.ToString()));
   if(!Test->TestTrue(TEXT("Session B loads successfully"),F.Save->LoadGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   Verify(Model.Get()); Manifest(Model.Get()); F.Shot(TEXT("H-SessionB.png"),F.Car); F.Save->DeleteSave(); return true;
  }
  F.Save->DeleteSave(); auto Records=Registry->BuildSnapshot(); Test->TestEqual(TEXT("Nine loadouts across five platforms"),Records.Num(),9);
  for(auto& R:Records) { R.Ammo=1; R.Reserve=17; } Registry->ApplySnapshot(Records);
  F.Car->GetHealthComponent()->ApplyPersistenceHealth(65,100);
  if(!Test->TestTrue(TEXT("Vehicle weapons saved through central SaveSubsystem"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
  TStrongObjectPtr<UNeonCitySaveGame> Model(F.Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("Additive section serialized at the current version"),Model.IsValid() && Model->HasVehicleWeapons && Model->VehicleWeapons.Num()==9 && Model->SaveVersion==UNeonCitySaveGame::CurrentVersion)) return true;
  if(Mode==TEXT("A")) { Manifest(Model.Get()); F.Shot(TEXT("H-SessionA.png"),F.Car); return true; }
  for(auto& R:Records) { R.Ammo=0; R.Reserve=0; } Registry->ApplySnapshot(Records); F.Car->GetHealthComponent()->Heal(1000);
  Test->TestTrue(TEXT("Central load restores vehicle and weapon state"),F.Save->LoadGame()); Verify(Model.Get());
  Model->HasVehicleWeapons=false; Model->VehicleWeapons.Reset(); Test->TestTrue(TEXT("Legacy fixture written"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Released save lacking loadout section loads"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Legacy load gets authored magazine"),F.Combat(F.Car)->GetManager()->GetWeapon()->GetAmmo(),40);
  Model->HasVehicleWeapons=true; Model->VehicleWeapons=Registry->BuildSnapshot(); const auto Duplicate=Model->VehicleWeapons[0]; Model->VehicleWeapons.Add(Duplicate);
  FVehicleWeaponSaveData Bad; Bad.VehicleID=TEXT("PIE_TransientPawn_1"); Bad.HardpointID=TEXT("Root"); Bad.WeaponID=TEXT("InjectedClass"); Model->VehicleWeapons.Add(Bad);
  Model->VehicleWeapons[1].Ammo=-5;
  Test->TestTrue(TEXT("Invalid IDs and ammo fixture written"),F.Save->WriteTestFixture(Model.Get()));
  Test->TestTrue(TEXT("Malformed entries isolated safely"),F.Save->LoadGame());
  Test->TestEqual(TEXT("Only nine valid authored mounts remain"),Registry->BuildSnapshot().Num(),9);
  // A destroyed vehicle retains its loadout records without re-enabling fire or health.
  auto DeadRecords=Registry->BuildSnapshot(); for(auto& R:DeadRecords) if(R.VehicleID==F.Combat(F.Car)->GetManager()->GetVehicleID()) { R.Ammo=7; R.Reserve=9; }
  F.Car->GetHealthComponent()->ApplyPersistenceHealth(0,100); Registry->ApplySnapshot(DeadRecords);
  Test->TestEqual(TEXT("Dead vehicle restores ammunition without enabling weapon"),F.Combat(F.Car)->GetManager()->GetWeapon()->GetAmmo(),7);
  Test->TestTrue(TEXT("Dead host remains dead and weapon unequipped"),F.Car->GetHealthComponent()->IsDead() && !F.Combat(F.Car)->GetManager()->GetWeapon()->IsEquipped());
  F.Shot(TEXT("H-RoundTrip.png"),F.Car); F.Save->DeleteSave(); return true;
 }
 FAutomationTestBase* Test; FString Mode; FScene F; double Started=0; float At=0; bool bReady=false;
};
}
#define VEHICLE_COMBAT_TEST(Class,Name,Scenario) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityVehicleCombatTests::Scenario>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
VEHICLE_COMBAT_TEST(FVehicleGroundBoat,"NeonCity.VehicleCombat.GroundBoatPIE",FGroundBoat)
VEHICLE_COMBAT_TEST(FVehicleAircraft,"NeonCity.VehicleCombat.AircraftPIE",FAircraft)
VEHICLE_COMBAT_TEST(FVehicleLock,"NeonCity.VehicleCombat.LockMissilePIE",FLockMissile)
VEHICLE_COMBAT_TEST(FVehicleExplosion,"NeonCity.VehicleCombat.ExplosionCrimePIE",FExplosionCrime)
VEHICLE_COMBAT_TEST(FVehicleProjectileLimits,"NeonCity.VehicleCombat.ProjectileLimitsPIE",FLimits)
#undef VEHICLE_COMBAT_TEST
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FVehicleCombatPersistence,"NeonCity.VehicleCombat.Persistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FVehicleCombatPersistence::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{ FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityVehicleCombatSession="),Mode); if(Mode!=TEXT("A") && Mode!=TEXT("B")) Mode=TEXT("RoundTrip"); Names.Add(Mode); Commands.Add(Mode); }
bool FVehicleCombatPersistence::RunTest(const FString& Mode)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityVehicleCombatTests::FPersistence>(this,Mode));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
