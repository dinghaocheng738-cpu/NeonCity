#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Combat/HealthComponent.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Explosion/ExplosionPresentationSubsystem.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "InputCoreTypes.h"

namespace NeonCityWestLakeHelipad
{
const FName PadTag(TEXT("WL.Helipad"));

/** Normal PIE at the lakeside residence: the helipad beside the house carries the player's armed helicopter at
 * rest on its H; from the cockpit, a missile fired with no lock flies straight down the aim line into a parked car
 * and a stack of loose crates 32 m ahead, and the blast throws them physically (the car lifts and tumbles, the
 * crates scatter), throws ground debris and presents the fireball. Pass -NeonCityHelipadCapture for stills. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(FString::Printf(TEXT("Helipad check timed out at stage %d"),Stage));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  ACharacter* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(Stage==0)
  {
   if(!Player || W->GetTimeSeconds()<4 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   // the pad's floor: the tagged actor that collides (the deck mesh itself only draws)
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->ActorHasTag(PadTag) && It->GetStaticMeshComponent()->IsCollisionEnabled())Pad=*It;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)if(It->ActorHasTag(PadTag))Heli=*It;
   if(!Test->TestNotNull(TEXT("residence helipad deck"),Pad.Get()) || !Test->TestNotNull(TEXT("helicopter on the residence helipad"),Heli.Get()))return true;
   if(Heli->GetHull()->GetPhysicsLinearVelocity().Size()>5 && W->GetTimeSeconds()<12)return false;          // settle on the skids
   const FBox Deck=Pad->GetComponentsBoundingBox();
   const FVector H=Heli->GetActorLocation();
   Test->TestTrue(TEXT("helicopter identity authored and owned"),Heli->AircraftID.IsValid() && Heli->Owned);
   Test->TestTrue(FString::Printf(TEXT("helicopter on the pad centre (%.0f cm off)"),FVector::Dist2D(H,Deck.GetCenter())),FVector::Dist2D(H,Deck.GetCenter())<200);
   // the hull box (the only collision) must sit on the floor, not sink into it
   const float HullBottom=H.Z-Heli->GetHull()->GetScaledBoxExtent().Z;
   Test->TestTrue(FString::Printf(TEXT("helicopter hull rests on the deck floor (hull bottom %.1f cm from the floor top)"),HullBottom-Deck.Max.Z),FMath::Abs(HullBottom-Deck.Max.Z)<6);
   Test->TestTrue(FString::Printf(TEXT("helicopter at rest (%.1f cm/s)"),Heli->GetHull()->GetPhysicsLinearVelocity().Size()),Heli->GetHull()->GetPhysicsLinearVelocity().Size()<10);
   Test->TestTrue(FString::Printf(TEXT("helipad is beside the home (%.0f m from the arrival point)"),FVector::Dist2D(Deck.GetCenter(),Player->GetActorLocation())/100),FVector::Dist2D(Deck.GetCenter(),Player->GetActorLocation())<5000);
   Shot(TEXT("A-Helipad.png"),Deck.GetCenter(),Deck.GetCenter()+Heli->GetActorForwardVector()*2400+Heli->GetActorRightVector()*900+FVector(0,0,900));
   // targets: a parked car broadside and a crate stack, 32 m off the nose on open lawn
   const FVector Fwd=Heli->GetActorForwardVector().GetSafeNormal2D();
   const FVector Probe=H+Fwd*3200;FHitResult G;FCollisionQueryParams Q(TEXT("HelipadTarget"),false,Heli.Get());
   if(!Test->TestTrue(TEXT("open ground ahead of the helicopter"),W->LineTraceSingleByChannel(G,Probe+FVector(0,0,2000),Probe-FVector(0,0,3000),ECC_Visibility,Q)))return true;
   Ground=G.ImpactPoint;
   FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
   Car=W->SpawnActor<ANeonCityStarterSedan>(Ground+FVector(0,0,90),FRotator(0,Fwd.Rotation().Yaw+90,0),P);
   UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
   const FVector Side=FVector::CrossProduct(FVector::UpVector,Fwd);
   for(int32 I=0;I<6;++I)
   {
    // in the open 4 m short of the car and 3 m left of the line of fire (the missile rail is on the right):
    // thrown away from the car, never into it, and never in the missile's path
    const FVector Spot=Ground-Fwd*400-Side*(300+(I%3-1)*80)+FVector(0,0,40+(I/3)*80);
    auto* Crate=W->SpawnActor<AStaticMeshActor>(Spot,FRotator::ZeroRotator,P);
    Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Crate->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Crate->SetActorScale3D(FVector(.7f));Crate->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("PhysicsActor"));
    Crate->GetStaticMeshComponent()->SetSimulatePhysics(true);Crate->GetStaticMeshComponent()->SetMassOverrideInKg(NAME_None,40,true);
    Crates.Add(Crate);
   }
   // walk up to the cabin door, then board the normal way
   Player->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocation(H+Heli->GetActorRightVector()*260+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(1);
  }
  const double E=FPlatformTime::Seconds()-At;
  if(Stage==1)
  {
   if(E<1)return false;
   if(!Test->TestTrue(TEXT("helicopter boarded"),Heli->EnterAircraft_Implementation(Player)))return true;
   return Next(5);
  }
  if(Stage==5)
  {
   if(E<2.5)return false;                                                            // car on its wheels, crates stacked
   if(!Test->TestNotNull(TEXT("target car spawned"),Car.Get()))return true;
   CarStart=Car->GetActorTransform();for(auto& C:Crates)CrateStart.Add(C.IsValid()?C->GetActorLocation():FVector::ZeroVector);
   CarHealth=Car->GetHealthComponent()->GetCurrentHealth();
   auto* Combat=Heli->FindComponentByClass<UVehicleCombatComponent>();
   if(!Test->TestNotNull(TEXT("helicopter weapons installed"),Combat))return true;
   Test->TestTrue(TEXT("missile rail selected"),Combat->GetManager()->Select(TEXT("SideMount.Right")));
   Combat->SetArmed(true);Combat->LockTarget(false);
   Explosions=W->GetSubsystem<UNeonCityExplosionSubsystem>();Presentation=W->GetSubsystem<UNeonCityExplosionPresentationSubsystem>();
   Test->TestNotNull(TEXT("explosion presentation runs in the game world"),Presentation.Get());
   Detonations=Explosions->GetDetonationCount();Presented=Presentation.IsValid()?Presentation->GetPresentedCount():0;
   const FVector Muzzle=Combat->GetManager()->GetPoint()->GetComponentLocation();
   const FVector Dir=Car->GetActorLocation()+FVector(0,0,40)-Muzzle;
   Combat->AimWeapon(Dir);PC->SetControlRotation(Dir.Rotation());
   if(!Test->TestTrue(TEXT("missile fires with no lock (straight shot)"),Combat->FireWeapon()))return true;
   Missile=Cast<AMissileProjectile>(Combat->GetManager()->GetWeapon()->GetLastProjectile());
   Test->TestTrue(TEXT("dumb-fired missile is unguided"),Missile.IsValid() && !Missile->HasGuidance());
   return Next(2);
  }
  if(Stage==2)
  {
   if(Missile.IsValid() && E>.15f && !bExhaustSeen){bExhaustSeen=Missile->HasExhaust();Shot(TEXT("B-MissileInFlight.png"),Missile->GetActorLocation(),Missile->GetActorLocation()+FVector::CrossProduct(FVector::UpVector,Missile->GetActorForwardVector())*900+FVector(0,0,300));}
   if(Explosions->GetDetonationCount()==Detonations){if(E>8){Test->AddError(TEXT("missile never detonated"));return true;}return false;}
   const auto& Last=Explosions->GetRecentExplosions().Last();
   Test->TestEqual(TEXT("missile blast profile"),Last.DataID,FName(TEXT("NeonCity.MissileExplosion")));
   Test->TestTrue(TEXT("missile exhaust lit in flight"),bExhaustSeen);
   Test->TestTrue(FString::Printf(TEXT("blast at the target (%.0f cm from the car)"),FVector::Dist(Last.Center,CarStart.GetLocation())),FVector::Dist(Last.Center,CarStart.GetLocation())<500);
   Test->TestTrue(FString::Printf(TEXT("blast pushed the loose crates (%d bodies)"),Explosions->GetLastLooseBodyCount()),Explosions->GetLastLooseBodyCount()>=3);
   Test->TestTrue(TEXT("blast presented"),Presentation.IsValid() && Presentation->GetPresentedCount()>Presented);
   Test->TestTrue(TEXT("ground debris thrown"),Presentation.IsValid() && Presentation->GetActiveDebrisCount()>0);
   BlastAt=FPlatformTime::Seconds();return Next(3);
  }
  if(Stage==3)
  {
   if(Car.IsValid())
   {
    if(LaunchVelocity.IsZero())LaunchVelocity=Car->GetMesh()->GetPhysicsLinearVelocity();
    PeakRise=FMath::Max(PeakRise,Car->GetActorLocation().Z-CarStart.GetLocation().Z);
    PeakSpin=FMath::Max(PeakSpin,float(FMath::RadiansToDegrees(Car->GetMesh()->GetPhysicsAngularVelocityInRadians().Size())));
   }
   // the blast as it develops: fireball, lift, smoke column
   static const double Times[]={.05,.3,.8,1.6};static const TCHAR* Names[]={TEXT("C1-Blast.png"),TEXT("C2-Blast.png"),TEXT("C3-Blast.png"),TEXT("C4-Blast.png")};
   if(BlastShots<4 && E>Times[BlastShots]){Shot(Names[BlastShots],CarStart.GetLocation(),CarStart.GetLocation()-Heli->GetActorForwardVector()*1600+Heli->GetActorRightVector()*1000+FVector(0,0,500));++BlastShots;}
   if(E<4)return false;
   Shot(TEXT("D-Aftermath.png"),CarStart.GetLocation(),CarStart.GetLocation()-Heli->GetActorForwardVector()*1300+Heli->GetActorRightVector()*1100+FVector(0,0,700));
   if(Car.IsValid())
   {
    const float Moved=FVector::Dist(Car->GetActorLocation(),CarStart.GetLocation());
    const float Turned=FMath::RadiansToDegrees(CarStart.GetRotation().AngularDistance(Car->GetActorQuat()));
    Test->AddInfo(FString::Printf(TEXT("car: launch %s cm/s, moved %.0f cm, peak rise %.0f cm, peak spin %.0f deg/s, turned %.0f deg, health %.0f -> %.0f"),*LaunchVelocity.ToCompactString(),Moved,PeakRise,PeakSpin,Turned,CarHealth,Car->GetHealthComponent()->GetCurrentHealth()));
    // believable, not cartoon: a couple of metres up and a few car lengths away at most
    Test->TestTrue(TEXT("car launch stays physical (under 6 m up, 25 m away)"),PeakRise<600 && Moved<2500);
    Test->TestTrue(TEXT("car thrown into the air"),PeakRise>80);
    Test->TestTrue(TEXT("car tumbled"),PeakSpin>60 || Turned>20);
    Test->TestTrue(TEXT("car damaged"),Car->GetHealthComponent()->GetCurrentHealth()<CarHealth);
   }
   int32 Scattered=0;
   for(int32 I=0;I<Crates.Num();++I)if(Crates[I].IsValid() && FVector::Dist(Crates[I]->GetActorLocation(),CrateStart[I])>200)++Scattered;
   Test->TestTrue(FString::Printf(TEXT("crates scattered (%d of %d moved over 2 m)"),Scattered,Crates.Num()),Scattered>=4);
   Test->TestTrue(TEXT("helicopter and pilot unharmed by their own shot"),Heli->GetHealthComponent()->GetCurrentHealth()>=Heli->GetHealthComponent()->GetMaxHealth()*.99f);
   return true;
  }
  return true;
 }
private:
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 void Shot(const TCHAR* Name,FVector Focus,FVector Eye)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityHelipadCapture")) || !W || !PC)return;
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());AActor* Previous=PC->GetViewTarget();
  PC->SetViewTarget(Camera);PC->PlayerCameraManager->SetGameCameraCutThisFrame();PC->PlayerCameraManager->UpdateCamera(0);
  bool Done=false;
  if(auto* V=W->GetGameViewport();V && V->Viewport)
  {
   V->Viewport->Draw(false);TArray<FColor> Pixels;
   if(V->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pxl:Pixels)Pxl.A=255;TArray64<uint8> PNG;const auto Size=V->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/Helipad");IFileManager::Get().MakeDirectory(*Dir,true);
    Done=FFileHelper::SaveArrayToFile(PNG,*(Dir/Name));
   }
  }
  if(!Done)UE_LOG(LogTemp,Warning,TEXT("Helipad still %s not captured"),Name);
  PC->SetViewTarget(Previous);Camera->Destroy();
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;
 double Started=0,At=0,BlastAt=0;int32 Stage=0,Detonations=0,Presented=0;float CarHealth=0,PeakRise=0,PeakSpin=0;bool bExhaustSeen=false;int32 BlastShots=0;
 FVector Ground=FVector::ZeroVector,LaunchVelocity=FVector::ZeroVector;FTransform CarStart;TArray<FVector> CrateStart;
 TWeakObjectPtr<AStaticMeshActor> Pad;TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;TWeakObjectPtr<ANeonCityStarterSedan> Car;
 TArray<TWeakObjectPtr<AStaticMeshActor>> Crates;TWeakObjectPtr<AMissileProjectile> Missile;
 TWeakObjectPtr<UNeonCityExplosionSubsystem> Explosions;TWeakObjectPtr<UNeonCityExplosionPresentationSubsystem> Presentation;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeHelipadPIE,"NeonCity.WestLake.HelipadMissilePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeHelipadPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeHelipad::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
namespace NeonCityWestLakeHelipad
{
/** Normal PIE takeoff from the residence helipad: walk to the helicopter, board, hold Space through the player's
 * real input path (key events into the player controller) and the helicopter climbs off the deck; release and it
 * holds a hover. Logs the flight state every half second so a failed takeoff shows why. */
class FTakeoff:public IAutomationLatentCommand
{
public:
 explicit FTakeoff(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>200){Test->AddError(FString::Printf(TEXT("Takeoff check timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  ACharacter* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(Stage==0)
  {
   if(!Player || W->GetTimeSeconds()<4 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)if(It->ActorHasTag(PadTag))Heli=*It;
   if(!Test->TestNotNull(TEXT("helicopter on the residence helipad"),Heli.Get()))return true;
   if(Heli->GetHull()->GetPhysicsLinearVelocity().Size()>5 && W->GetTimeSeconds()<12)return false;
   Player->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocation(Heli->GetActorLocation()+Heli->GetActorRightVector()*260+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(1);
  }
  const double E=FPlatformTime::Seconds()-At;auto* Move=Heli.IsValid()?Heli->GetAircraftMovement():nullptr;
  if(!Move){Test->AddError(TEXT("helicopter lost"));return true;}
  if(Stage==1)
  {
   if(E<1)return false;
   if(!Test->TestTrue(TEXT("helicopter boarded"),Heli->EnterAircraft_Implementation(Player)))return true;
   return Next(2);
  }
  if(Stage==2)
  {
   if(E<1.5)return false;                                                             // seated, mapping applied
   Test->TestTrue(TEXT("player controls the helicopter"),PC->GetPawn()==Heli.Get());
   StartZ=Heli->GetActorLocation().Z;Key(PC,EKeys::SpaceBar,IE_Pressed);LastLog=0;return Next(3);
  }
  if(Stage==3)
  {
   Key(PC,EKeys::SpaceBar,IE_Repeat);
   if(E-LastLog>=.5){LastLog=E;Log(TEXT("space held"),E);}
   if(E<4)return false;
   Key(PC,EKeys::SpaceBar,IE_Released);
   const float Rise=Heli->GetActorLocation().Z-StartZ;
   Test->TestTrue(FString::Printf(TEXT("Space lifts the helicopter off the deck (rose %.0f cm in 4 s)"),Rise),Rise>500);
   HoverZ=Heli->GetActorLocation().Z;return Next(4);
  }
  if(Stage==4)
  {
   if(E-LastLog>=.5){LastLog=E;Log(TEXT("released"),E);}
   if(E<3)return false;
   const float Drift=Heli->GetActorLocation().Z-HoverZ;
   Test->TestTrue(FString::Printf(TEXT("released, it holds a hover (drifted %.0f cm in 3 s)"),Drift),Move->IsEngineRunning() && FMath::Abs(Drift)<300);
   return true;
  }
  return true;
 }
private:
 static void Key(APlayerController* PC,FKey K,EInputEvent Ev){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Ev,FPlatformTime::Cycles64()));}
 void Log(const TCHAR* Phase,double E)
 {
  auto* Move=Heli->GetAircraftMovement();
  Test->AddInfo(FString::Printf(TEXT("%s +%.1fs: engine %d grounded %d state %d power %.2f fuel %.2f alt %.0f cm z %+.0f v %s ctrl %s"),Phase,E,
   Move->IsEngineRunning(),Move->IsGrounded(),int32(Move->GetFlightState()),Move->GetCurrentPower(),Move->GetFuelFraction(),Move->GetAltitude(),
   Heli->GetActorLocation().Z-StartZ,*Heli->GetHull()->GetPhysicsLinearVelocity().ToCompactString(),*GetNameSafe(Heli->GetController())));
 }
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 FAutomationTestBase* Test;double Started=0,At=0,LastLog=0;int32 Stage=0;float StartZ=0,HoverZ=0;
 TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeHelipadTakeoffPIE,"NeonCity.WestLake.HelipadTakeoffPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeHelipadTakeoffPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeHelipad::FTakeoff(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
