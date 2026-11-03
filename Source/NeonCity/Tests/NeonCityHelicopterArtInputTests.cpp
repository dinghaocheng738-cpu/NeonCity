#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftFlightStatusComponent.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/LockOnSystem.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Widgets/SWindow.h"

namespace NeonCityHelicopterArtInput
{
void Key(APlayerController* PC,FKey K,EInputEvent E) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
class FScenario:public IAutomationLatentCommand
{
public:
 explicit FScenario(FAutomationTestBase* T):Test(T),Evidence(TEXT("D-HelicopterMissile")) {}
 bool Next(int32 S) { Stage=S;At=W->GetTimeSeconds();return false; }
 bool Update() override
 {
  if(!Start) Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>110) { Test->AddError(FString::Printf(TEXT("Helicopter input review timed out stage %d"),Stage));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr; PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<3) return false;
  if(!Stage)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player) return false;
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
    if(auto Window=W->GetGameViewport()->GetWindow())Window->Resize(FVector2D(1600,900));
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It) Heli=*It;
   for(TActorIterator<ANeonCityStarterSedan> It(W);It;++It) if(!It->IsA<ANeonCityPrototypeVehicle>()) { Car=*It;break; }
   if(!Test->TestNotNull(TEXT("Ordinary map helicopter"),Heli) || !Test->TestNotNull(TEXT("Existing Chaos target car"),Car)) return true;
   Combat=Heli->FindComponentByClass<UVehicleCombatComponent>();
   if(!Test->TestNotNull(TEXT("Normal helicopter has loadout without combat test-area install"),Combat)) return true;
   bool Runway=false;for(TActorIterator<AFixedWingTestArea> It(W);It;++It) Runway=true;
   if(!Runway) W->SpawnActor<AFixedWingTestArea>();
   // One-time fixture staging before the input sequence. No aircraft transform,
   // velocity, input-port, weapon fire or lock calls are used during flight.
   Heli->SetActorLocationAndRotation(FVector(-10000,7000,480),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);Heli->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
   Heli->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);
   Car->SetActorLocationAndRotation(FVector(-6800,7000,450),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);
   Car->GetHealthComponent()->ApplyPersistenceHealth(500,500);
   Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(FVector(-9740,6790,530),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
   return Next(1);
  }
  const float E=W->GetTimeSeconds()-At; Evidence.Record(W,PC,Stage);
  if(Stage==1 && E>.9f)
  {
   FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);PC->SetControlRotation((Heli->GetActorLocation()-Eye).Rotation());
   if(!Test->TestTrue(TEXT("Interaction sees real helicopter hull"),Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==Heli)) return true;
   Key(PC,EKeys::F,IE_Pressed);return Next(2);
  }
  if(Stage==2 && E>.3f)
  {
   Key(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F entered the normal helicopter"),PC->GetPawn()==Heli)) return true;
   Test->TestFalse(TEXT("Normal loadout initially safe"),Combat->IsArmed());
   Key(PC,EKeys::SpaceBar,IE_Pressed);return Next(3);
  }
  if(Stage==3 && E>1.1f) { Key(PC,EKeys::SpaceBar,IE_Released);Key(PC,EKeys::V,IE_Pressed);Key(PC,EKeys::Tab,IE_Pressed);return Next(4); }
  if(Stage==4 && E>.20f) { Key(PC,EKeys::V,IE_Released);Key(PC,EKeys::Tab,IE_Released);return Next(5); }
  if(Stage==5 && E>.20f) { Key(PC,EKeys::Tab,IE_Pressed);return Next(6); }
  if(Stage>=6 && Stage<=13)
  {
   const FVector Aim=Car->GetActorLocation()-Combat->GetManager()->GetPoint()->GetComponentLocation();PC->SetControlRotation(Aim.Rotation());
  }
  if(Stage==6 && E>.35f)
  {
   Key(PC,EKeys::Tab,IE_Released);
   Test->TestTrue(TEXT("V armed through input"),Combat->IsArmed());
   if(!Test->TestEqual(TEXT("Tab selected missile hardpoint"),Combat->GetManager()->GetPoint()->HardpointID,FName(TEXT("SideMount.Right")))) return true;
   Test->TestTrue(TEXT("No target requested: Searching"),Combat->GetLockSystem()->State==EVehicleLockState::Searching);
   Key(PC,EKeys::RightMouseButton,IE_Pressed);return Next(7);
  }
  if(Stage==7 && E>.55f)
  {
   Test->TestTrue(TEXT("RMB entered timed Locking"),Combat->GetLockSystem()->State==EVehicleLockState::Locking);
   Test->TestTrue(TEXT("Pilot instruments show timed lock progress"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("LOCKING")));
   Test->TestTrue(TEXT("Capture actual locking HUD"),FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Flight-Locking.png")));return Next(8);
  }
  if(Stage==8 && E>1.5f)
  {
   if(!Test->TestTrue(TEXT("Real camera/LOS target Locked"),Combat->GetLockSystem()->GetLockedTarget()==Car)) return true;
   Test->TestTrue(TEXT("Pilot instruments show confirmed lock"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("LOCKED")));
   Test->TestTrue(TEXT("Capture actual locked HUD"),FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Flight-Locked.png")));
   Wall=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Box->InitBoxExtent(FVector(45,800,1000));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Wall->SetActorLocation(FVector(-8500,7000,800));
   auto* Face=NewObject<UStaticMeshComponent>(Wall);Face->SetupAttachment(Box);Face->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Face->SetRelativeScale3D(FVector(.9,16,20));Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);Face->RegisterComponent();return Next(9);
  }
  if(Stage==9 && E>.40f)
  {
   Test->TestTrue(TEXT("Blocked LOS loses lock"),Combat->GetLockSystem()->State==EVehicleLockState::Lost);
   Test->TestTrue(TEXT("Lost lock is visible to the pilot"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("LOST")));
   BeforeAmmo=Combat->GetManager()->GetWeapon()->GetAmmo();Key(PC,EKeys::LeftMouseButton,IE_Pressed);return Next(10);
  }
  if(Stage==10 && E>.25f)
  {
   Key(PC,EKeys::LeftMouseButton,IE_Released);Test->TestEqual(TEXT("LMB cannot fire missile through occluder"),Combat->GetManager()->GetWeapon()->GetAmmo(),BeforeAmmo);Wall->Destroy();return Next(11);
  }
  if(Stage==11 && E>1.8f)
  {
   if(!Test->TestTrue(TEXT("RMB reacquires with a new dwell"),Combat->GetLockSystem()->GetLockedTarget()==Car)) return true;
   LaunchPoint=Combat->GetManager()->GetPoint()->GetComponentLocation();Key(PC,EKeys::LeftMouseButton,IE_Pressed);return Next(12);
  }
  if(Stage==12)
  {
   Missile=Cast<AMissileProjectile>(Combat->GetManager()->GetWeapon()->GetLastProjectile());
   if(Missile.IsValid())
   {
    Key(PC,EKeys::LeftMouseButton,IE_Released);
    Test->TestTrue(TEXT("Missile leaves actual right mount"),FVector::Distance(Missile->GetActorLocation(),LaunchPoint)<450);
    Test->TestTrue(TEXT("Input-launched missile has guidance"),Missile->HasGuidance());
    Test->TestEqual(TEXT("One input launch consumes one round"),Combat->GetManager()->GetWeapon()->GetAmmo(),BeforeAmmo-1);
    LastDirection=Missile->GetFlightVelocity().GetSafeNormal();LastTime=W->GetTimeSeconds();Car->GetMesh()->SetPhysicsLinearVelocity(FVector(0,120,0));return Next(13);
   }
   if(E>1) { Test->AddError(TEXT("LMB did not launch configured missile"));return true; }
  }
  if(Stage==13)
  {
   if(Missile.IsValid())
   {
    const FVector Direction=Missile->GetFlightVelocity().GetSafeNormal();const float Dt=W->GetTimeSeconds()-LastTime;
    const float Angle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction,LastDirection),-1.f,1.f)));
    Test->TestTrue(TEXT("Tracking respects 45 degree/sec limit"),Angle<=45*Dt+.65f);LastDirection=Direction;LastTime=W->GetTimeSeconds();
    if(E>8) { Test->AddError(TEXT("Missile did not impact in bounded sequence"));return true; }return false;
   }
   Test->TestTrue(TEXT("Impact/explosion reduced target vehicle health"),Car->GetHealthComponent()->GetCurrentHealth()<500);
   Key(PC,EKeys::RightMouseButton,IE_Released);return Next(14);
  }
  if(Stage==14 && E>1.5f)
  {
   auto* Weapon=Combat->GetManager()->GetWeapon();
   const FString Ammo=FString::Printf(TEXT("Ammo %d / %d"),Weapon->GetAmmo(),Weapon->GetReserveAmmo());
   Test->TestTrue(TEXT("After real impact HUD retains the actual remaining ammunition"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(Ammo));
   Test->TestTrue(TEXT("Capture actual post-launch instruments"),FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Flight-AfterMissile.png")));return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;ANeonCityHelicopterPrototype* Heli=nullptr;ANeonCityStarterSedan* Car=nullptr;UVehicleCombatComponent* Combat=nullptr;
 AActor* Wall=nullptr;TWeakObjectPtr<AMissileProjectile> Missile;int32 Stage=0,BeforeAmmo=0;float At=0,LastTime=0;double Start=0;FVector LaunchPoint,LastDirection;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHelicopterArtInput,"NeonCity.FullArt.HelicopterMissileInputPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHelicopterArtInput::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityHelicopterArtInput::FScenario>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
