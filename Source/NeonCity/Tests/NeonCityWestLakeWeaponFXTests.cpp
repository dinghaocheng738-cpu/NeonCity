#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "Combat/CombatComponent.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/VehicleAimPresentation.h"
#include "VehicleCombat/LockOnSystem.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Explosion/ExplosionSubsystem.h"

namespace NeonCityWestLakeWeaponFX
{
const FName PadTag(TEXT("WL.Helipad"));

/** The helicopter's weapons as the player sees and feels them, from the cockpit on the residence helipad:
 *  - sight: armed, the impact mark sits on a parked car 40 m ahead and reads ON TARGET, the red laser designator
 *    shows; the machine-gun burst puts visible tracers in the air, muzzle reports and round strikes
 *    (impact effects) on the car, and the rounds' momentum rocks it;
 *  - rockets: off the rail slow, then the motor lights and drives them to full speed;
 *  - missile: holding lock, the seeker closes on the car and locks; the guided missile is tracked in flight
 *    and detonates on the car;
 *  - rounds fired into the lake strike the water surface (splashes), not the lake bed.
 * Stills with the HUD to Saved/WestLake/WeaponFX/ when run with -NeonCityWeaponFXCapture. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(FString::Printf(TEXT("Weapon FX check timed out at stage %d"),Stage));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  const double E=W?W->GetTimeSeconds()-At:0;                                       // game time: the seeker, motor and burst run on it
  if(Stage==0)
  {
   ACharacter* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
   if(!Player || W->GetTimeSeconds()<4 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)if(It->ActorHasTag(PadTag))Heli=*It;
   if(!Test->TestNotNull(TEXT("helicopter on the residence helipad"),Heli.Get()))return true;
   if(Heli->GetHull()->GetPhysicsLinearVelocity().Size()>5 && W->GetTimeSeconds()<12)return false;
   const FVector H=Heli->GetActorLocation(),Fwd=Heli->GetActorForwardVector().GetSafeNormal2D();
   FHitResult G;FCollisionQueryParams Q(TEXT("WeaponFXTarget"),false,Heli.Get());
   const FVector Probe=H+Fwd*4000;
   if(!Test->TestTrue(TEXT("open ground ahead"),W->LineTraceSingleByChannel(G,Probe+FVector(0,0,2000),Probe-FVector(0,0,3000),ECC_Visibility,Q)))return true;
   FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
   Car=W->SpawnActor<ANeonCityStarterSedan>(G.ImpactPoint+FVector(0,0,90),FRotator(0,Fwd.Rotation().Yaw+90,0),P);
   // a second, untouched car for the missile (the gun burst may wreck the first)
   const FVector Probe2=H+Fwd*4500+FVector::CrossProduct(FVector::UpVector,Fwd)*1200;FHitResult G2;
   if(W->LineTraceSingleByChannel(G2,Probe2+FVector(0,0,2000),Probe2-FVector(0,0,3000),ECC_Visibility,Q))
    Car2=W->SpawnActor<ANeonCityStarterSedan>(G2.ImpactPoint+FVector(0,0,90),FRotator(0,Fwd.Rotation().Yaw,0),P);
   Player->SetActorLocation(H+Heli->GetActorRightVector()*260+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(1);
  }
  auto* Combat=Heli.IsValid()?Heli->FindComponentByClass<UVehicleCombatComponent>():nullptr;
  auto* Aim=Heli.IsValid()?Heli->FindComponentByClass<UVehicleAimPresentationComponent>():nullptr;
  auto* Effects=W?W->GetSubsystem<UNeonCityWeaponEffectsSubsystem>():nullptr;
  if(Stage==1)
  {
   if(E<1)return false;
   if(!Test->TestTrue(TEXT("helicopter boarded"),Heli->EnterAircraft_Implementation(Cast<ACharacter>(PC->GetPawn()))))return true;
   return Next(2);
  }
  if(!Combat || !Effects || !Car.IsValid()){Test->AddError(TEXT("weapons, effects or target missing"));return true;}
  if(Stage==2)
  {
   if(E<2.5)return false;                                                            // car settled on its wheels
   if(!Test->TestNotNull(TEXT("aim presentation installed on the helicopter"),Aim))return true;
   Test->TestTrue(TEXT("machine gun selected"),Combat->GetManager()->Select(TEXT("Nose")));
   Combat->SetArmed(false);Combat->LockTarget(false);
   LookAt(Car->GetActorLocation()+FVector(0,0,30));
   return Next(20);
  }
  if(Stage==20)
  {
   // weapons safe: no sight, no laser; the Hellfires hang on their rack all the same
   LookAt(Car->GetActorLocation()+FVector(0,0,30));
   if(E<.6)return false;
   Test->TestFalse(TEXT("no sight while the weapons are safe"),Aim->GetLastState().bVisible);
   Test->TestFalse(TEXT("no laser while the weapons are safe"),Aim->IsLaserVisible());
   const AVehicleMissileLauncher* Rack=Launcher();
   Test->TestTrue(FString::Printf(TEXT("four Hellfires on the rack (%d)"),Rack?Rack->GetMissilesOnRails():-1),Rack && Rack->GetMissilesOnRails()==4);
   if(Rack && !bShotRack){bShotRack=true;SideShot(TEXT("0-HellfireRack.png"),Rack->GetActorLocation(),Rack->GetActorLocation()+Heli->GetActorRightVector()*650+Heli->GetActorForwardVector()*250+FVector(0,0,120));}
   Combat->SetArmed(true);                                                           // V
   CarStart=Car->GetActorLocation();Impacts=Effects->GetImpactCount();Reports=Effects->GetReportCount();
   return Next(3);
  }
  if(Stage==3)
  {
   LookAt(Car->GetActorLocation()+FVector(0,0,30));
   if(E<1.2)return false;                                                            // camera swings round; sight settles
   const auto& S=Aim->GetLastState();
   Test->TestTrue(TEXT("sight shown while flying armed"),S.bVisible && S.bArmed);
   Test->TestTrue(FString::Printf(TEXT("impact mark on screen at %.0f m"),S.RangeMeters),S.bImpactOnScreen && S.RangeMeters>10 && S.RangeMeters<80);
   Test->TestTrue(TEXT("sight reads ON TARGET over the car"),S.bOnTarget);
   Test->TestFalse(TEXT("no laser designator for the gun"),Aim->IsLaserVisible());
   Test->TestTrue(FString::Printf(TEXT("aim point on the car (%.0f cm off)"),FVector::Dist(Aim->GetAimPoint(),Car->GetActorLocation())),FVector::Dist(Aim->GetAimPoint(),Car->GetActorLocation())<400);
   return Next(4);
  }
  if(Stage==4)
  {
   LookAt(Car->GetActorLocation()+FVector(0,0,30));
   Combat->FireWeapon();                                                             // trigger held
   if(auto* Round=Combat->GetManager()->GetWeapon()->GetLastProjectile();Round && !bTracer)
   {
    TInlineComponentArray<UStaticMeshComponent*> Parts(Round);
    for(auto* Part:Parts)if(Part->GetFName()==TEXT("Tracer"))bTracer=Part->IsVisible() && Part->GetMaterial(0) && Part->GetMaterial(0)->GetName().Contains(TEXT("Tracer"));
   }
   if(E>.5 && !bShotA){Shot(TEXT("A-GunBurst.png"));bShotA=true;}
   if(E>.9 && !bSideA){bSideA=true;const FVector M=(Heli->GetActorLocation()+Car->GetActorLocation())*.5f;
    SideShot(TEXT("A2-GunBurstSide.png"),M,M+FVector::CrossProduct(FVector::UpVector,(Car->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D())*1800+FVector(0,0,450));}
   if(E>1.3 && !bSideB){bSideB=true;SideShot(TEXT("A3-Strikes.png"),Car->GetActorLocation(),Car->GetActorLocation()-(Car->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D()*900+FVector::CrossProduct(FVector::UpVector,(Car->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D())*500+FVector(0,0,250));}
   PeakCarSpeed=FMath::Max(PeakCarSpeed,float(Car->GetVelocity().Size()));
   if(auto* Body=Cast<UPrimitiveComponent>(Car->GetRootComponent()))PeakCarSpin=FMath::Max(PeakCarSpin,float(Body->GetPhysicsAngularVelocityInDegrees().Size()));
   if(E<2)return false;
   Test->TestTrue(TEXT("tracer streak on the rounds"),bTracer);
   Test->TestTrue(FString::Printf(TEXT("muzzle reports presented (%d)"),Effects->GetReportCount()-Reports),Effects->GetReportCount()-Reports>=8);
   Test->TestTrue(FString::Printf(TEXT("round strikes presented (%d)"),Effects->GetImpactCount()-Impacts),Effects->GetImpactCount()-Impacts>=5);
   Test->TestTrue(FString::Printf(TEXT("rounds' momentum reaches the car body (peak %.0f cm/s, %.1f deg/s)"),PeakCarSpeed,PeakCarSpin),PeakCarSpeed>2 || PeakCarSpin>1);
   Test->TestTrue(TEXT("hit marker for strikes on the car"),Effects->GetLastConfirmedHit(PC)>0);
   // rocket pod: slow out of the tube, motor, full speed - into open lawn beside the car (the car stays the missile's target)
   Test->TestTrue(TEXT("rocket pod selected"),Combat->GetManager()->Select(TEXT("SideMount.Left")));
   RocketAim=Car->GetActorLocation()+FVector::CrossProduct(FVector::UpVector,(Car->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D())*-1500;
   LookAt(RocketAim);
   return Next(5);
  }
  if(Stage==5)
  {
   LookAt(RocketAim);
   if(E<.6)return false;
   if(!Rocket.IsValid())
   {
    if(!Test->TestTrue(TEXT("rocket fires"),Combat->FireWeapon()))return true;
    Rocket=Cast<ARocketProjectile>(Combat->GetManager()->GetWeapon()->GetLastProjectile());
    if(!Test->TestTrue(TEXT("rocket in flight"),Rocket.IsValid()))return true;
    RocketV0=FMath::Max(1.f,float(Rocket->GetVelocity().Size()));RocketAt=W->GetTimeSeconds();
    return false;
   }
   if(W->GetTimeSeconds()-RocketAt<.4)return false;
   const float V1=Rocket.IsValid()?Rocket->GetVelocity().Size():0;
   Test->TestTrue(FString::Printf(TEXT("rocket motor lit and accelerating (%.0f -> %.0f cm/s)"),RocketV0,V1),Rocket.IsValid() && Rocket->IsMotorBurning() && V1>RocketV0*1.8f);
   Test->TestTrue(TEXT("missile rail selected"),Combat->GetManager()->Select(TEXT("SideMount.Right")));
   if(Car2.IsValid())LookAt(Car2->GetActorLocation()+FVector(0,0,30));
   Combat->LockTarget(true);
   return Next(6);
  }
  if(Stage==6)
  {
   if(!Test->TestTrue(TEXT("second target car"),Car2.IsValid()))return true;
   LookAt(Car2->GetActorLocation()+FVector(0,0,30));
   const auto& S=Aim->GetLastState();
   if(S.bSeeker && S.bCandidate && !S.bLocked && S.LockProgress>.3f && !bShotB){Shot(TEXT("B-Locking.png"));bShotB=true;}
   if(Combat->GetLockSystem()->State!=EVehicleLockState::Locked){if(E>6){Test->AddError(TEXT("seeker never locked the car"));return true;}return false;}
   Test->TestTrue(TEXT("seeker shown with lock brackets on the car"),S.bSeeker && S.bCandidate);
   Test->TestTrue(TEXT("laser designates the locked car"),Aim->IsLaserVisible());
   if(!bShotC){bShotC=true;Shot(TEXT("C-Locked.png"));return false;}
   Detonations=W->GetSubsystem<UNeonCityExplosionSubsystem>()->GetDetonationCount();
   if(!Test->TestTrue(TEXT("guided missile fires on the lock"),Combat->FireWeapon()))return true;
   Missile=Cast<AMissileProjectile>(Combat->GetManager()->GetWeapon()->GetLastProjectile());
   Test->TestTrue(TEXT("missile guided"),Missile.IsValid() && Missile->HasGuidance());
   return Next(7);
  }
  if(Stage==7)
  {
   if(!bRackAfter){bRackAfter=true;const AVehicleMissileLauncher* Rack=Launcher();
    Test->TestTrue(FString::Printf(TEXT("three Hellfires left on the rack (%d)"),Rack?Rack->GetMissilesOnRails():-1),Rack && Rack->GetMissilesOnRails()==3);}
   if(E>.45 && !bShotD && Missile.IsValid()){Shot(TEXT("D-MissileTracked.png"));bShotD=true;Test->TestTrue(TEXT("own missile tracked on the sight"),Aim->GetLastState().bRoundOnScreen);}
   if(E>.75 && !bSideD && Missile.IsValid()){bSideD=true;SideShot(TEXT("D2-MissileSide.png"),Missile->GetActorLocation(),Missile->GetActorLocation()+FVector::CrossProduct(FVector::UpVector,Missile->GetActorForwardVector()).GetSafeNormal2D()*1100+FVector(0,0,300));}
   if(W->GetSubsystem<UNeonCityExplosionSubsystem>()->GetDetonationCount()==Detonations){if(E>8){Test->AddError(TEXT("missile never detonated"));return true;}return false;}
   Combat->LockTarget(false);
   // last: a burst into the lake
   Test->TestTrue(TEXT("machine gun reselected"),Combat->GetManager()->Select(TEXT("Nose")));
   Water=FindWater();
   if(!Test->TestTrue(TEXT("lake surface in reach"),!Water.IsZero()))return true;
   Impacts=Effects->GetImpactCount();
   return Next(8);
  }
  if(Stage==8)
  {
   LookAt(Water);
   if(E<1.2)return false;
   if(!bAimWater){bAimWater=true;Test->TestTrue(TEXT("sight reads WATER over the lake"),Aim->GetLastState().bWater);}
   Combat->FireWeapon();
   if(E>1.6 && !bShotE){Shot(TEXT("E-WaterBurst.png"));bShotE=true;}
   if(E>2.1 && !bSideE){bSideE=true;SideShot(TEXT("E2-Splashes.png"),Water,Water+(Heli->GetActorLocation()-Water).GetSafeNormal2D()*1200+FVector::CrossProduct(FVector::UpVector,(Water-Heli->GetActorLocation()).GetSafeNormal2D())*900+FVector(0,0,500));}
   if(E<3)return false;
   Test->TestTrue(FString::Printf(TEXT("rounds splash on the lake surface (%d)"),Effects->GetImpactCount()-Impacts),Effects->GetImpactCount()-Impacts>=4);
   // laser designation: the Hellfire on a patch of open lawn, no vehicle or person anywhere near it
   Test->TestTrue(TEXT("missile rail reselected"),Combat->GetManager()->Select(TEXT("SideMount.Right")));
   {
    // well away from both cars (outside the seeker's field around them): open lawn to one side
    const FVector Dir=(Car->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(-60,FVector::UpVector);
    const FVector P=Heli->GetActorLocation()+Dir*12000;FHitResult G;FCollisionQueryParams Q(TEXT("LaserLawn"),false,Heli.Get());
    LaserAim=W->LineTraceSingleByChannel(G,P+FVector(0,0,2000),P-FVector(0,0,3000),ECC_Visibility,Q)?G.ImpactPoint:P;
   }
   // up to a firing hover first: from the pad the missile would drop off its rail into the lawn
   Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);
   Heli->SetActorLocation(Heli->GetActorLocation()+FVector(0,0,2500),false,nullptr,ETeleportType::TeleportPhysics);
   Combat->LockTarget(true);
   return Next(9);
  }
  if(Stage==9)
  {
   LookAt(LaserAim);
   if(E<.3)return false;
   const auto& S=Aim->GetLastState();
   if(Combat->GetLockSystem()->State!=EVehicleLockState::Locked){if(E>6){Test->AddError(FString::Printf(TEXT("the laser never locked the ground (candidate %d, seeker %d)"),S.bCandidate,S.bSeeker));return true;}return false;}
   Test->TestTrue(TEXT("seeker holds the laser spot (LASE LOCK), not SEARCH"),Combat->IsDesignatingPoint() && S.bCandidate && S.bLocked && S.bPointLock);
   if(!bShotF){bShotF=true;Shot(TEXT("F-LaseLock.png"));return false;}
   Spot=Combat->GetLockSystem()->GetCandidate()->GetActorLocation();
   Detonations=W->GetSubsystem<UNeonCityExplosionSubsystem>()->GetDetonationCount();
   if(!Test->TestTrue(TEXT("guided missile fires on the laser spot"),Combat->FireWeapon()))return true;
   return Next(10);
  }
  if(Stage==10)
  {
   LookAt(LaserAim);                                                                // the gunner keeps the laser on the spot
   if(E>.6 && !bShotG){bShotG=true;Shot(TEXT("G-LasedMissile.png"));}

   auto* Explosions=W->GetSubsystem<UNeonCityExplosionSubsystem>();
   if(Explosions->GetDetonationCount()==Detonations){if(E>8){Test->AddError(TEXT("the lased missile never detonated"));return true;}return false;}
   const FVector Blast=Explosions->GetRecentExplosions().Last().Center;
   Test->TestTrue(FString::Printf(TEXT("blast on the laser spot, inside the warhead's lethal radius (%.0f cm off)"),FVector::Dist(Blast,Spot)),FVector::Dist(Blast,Spot)<600);
   Combat->LockTarget(false);
   return true;
  }
  return true;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 const AVehicleMissileLauncher* Launcher() const
 {
  const auto* Combat=Heli.IsValid()?Heli->FindComponentByClass<UVehicleCombatComponent>():nullptr;if(!Combat)return nullptr;
  for(const auto& Mount:Combat->GetManager()->GetMounts())
   if(const AVehicleMissileLauncher* Rack=Mount.Combat?Cast<AVehicleMissileLauncher>(Mount.Combat->GetWeapon()):nullptr)return Rack;
  return nullptr;
 }
 void LookAt(const FVector& Target)
 {
  FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);
  PC->SetControlRotation((Target-Eye).Rotation());
 }
 FVector FindWater() const
 {
  // the lake lies off the residence's glass front: search a fan ahead and to both sides for water
  FCollisionQueryParams Q(TEXT("WeaponFXWater"),false,Heli.Get());
  for(float Yaw=0;Yaw<360;Yaw+=15)for(float D:{6000.f,9000.f,12000.f})
  {
   const FVector P=Heli->GetActorLocation()+FRotator(0,Yaw,0).Vector()*D;FHitResult G;
   if(!W->LineTraceSingleByChannel(G,P+FVector(0,0,3000),P-FVector(0,0,3000),ECC_Visibility,Q))continue;
   // the lake: ground well below the shore with open sky above (the water plane has no collision)
   if(G.ImpactPoint.Z<Heli->GetActorLocation().Z-200)return FVector(P.X,P.Y,-50);
  }
  return FVector::ZeroVector;
 }
 void Shot(const TCHAR* Name)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityWeaponFXCapture")))return;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/WeaponFX");IFileManager::Get().MakeDirectory(*Dir,true);
  FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);                        // with the HUD (sight, seeker)
 }
 void SideShot(const TCHAR* Name,FVector Focus,FVector Eye)
 {
  // scene only, from a spectator camera beside the action (tracers, flashes, strikes, splashes)
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityWeaponFXCapture")) || !W || !PC)return;
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());AActor* Previous=PC->GetViewTarget();
  PC->SetViewTarget(Camera);PC->PlayerCameraManager->SetGameCameraCutThisFrame();PC->PlayerCameraManager->UpdateCamera(0);
  if(auto* V=W->GetGameViewport();V && V->Viewport)
  {
   V->Viewport->Draw(false);TArray<FColor> Pixels;
   if(V->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pxl:Pixels)Pxl.A=255;TArray64<uint8> PNG;const auto Size=V->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/WeaponFX");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveArrayToFile(PNG,*(Dir/Name));
   }
  }
  PC->SetViewTarget(Previous);Camera->Destroy();
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;
 double Started=0,At=0,RocketAt=0;int32 Stage=0,Impacts=0,Reports=0,Detonations=0;float PeakCarSpeed=0,PeakCarSpin=0,RocketV0=0;
 bool bSideA=false,bSideB=false,bSideD=false,bSideE=false;
 bool bTracer=false,bShotA=false,bShotB=false,bShotC=false,bShotD=false,bShotE=false,bAimWater=false,bShotRack=false,bRackAfter=false,bShotF=false,bShotG=false;FVector Spot=FVector::ZeroVector,LaserAim=FVector::ZeroVector;
 FVector RocketAim=FVector::ZeroVector,CarStart=FVector::ZeroVector,Water=FVector::ZeroVector;
 TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;TWeakObjectPtr<ANeonCityStarterSedan> Car,Car2;
 TWeakObjectPtr<ARocketProjectile> Rocket;TWeakObjectPtr<AMissileProjectile> Missile;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeWeaponFXPIE,"NeonCity.WestLake.WeaponFXPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeWeaponFXPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeWeaponFX::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
