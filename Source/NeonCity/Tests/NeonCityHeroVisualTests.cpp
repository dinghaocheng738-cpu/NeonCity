#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "PhysicsEngine/PhysicsAsset.h"
namespace
{
class FHeroVisualReview : public IAutomationLatentCommand
{
public:
 explicit FHeroVisualReview(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>260){Test->AddError(FString::Printf(TEXT("Hero review timeout %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds();
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-155000,700))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   int32 Count=0;Cars.SetNumZeroed(2);for(TActorIterator<AIslandSupercar> It(W);It;++It){++Count;const int32 I=It->bRoadster?1:0;if(!Cars[I])Cars[I]=*It;}
   if(!Test->TestEqual(TEXT("Four hero replacements present in ordinary island map"),Count,4) || !Cars[0] || !Cars[1])return true;
   for(auto* Car:Cars)Test->TestTrue(TEXT("Fast sports profile passes validation"),Car->VehicleData && Car->VehicleData->IsValidProfile());
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);Stage=FParse::Param(FCommandLine::Get(),TEXT("NeonCityHeroQuick"))?8:1;At=Now;Place(W,0);return false;
  }
  if(Stage>=1 && Stage<=8)
  {
   if(Now-At<12)return false;
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Hero-Car-%02d.png"),Stage));
   if(Stage<8){Place(W,Stage);++Stage;At=Now;return false;}
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(12);
   const FVector P=Cars[0]->GetActorLocation();Player->SetActorLocation(P-Cars[0]->GetActorForwardVector()*650+FVector(0,0,130),false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(Cars[0]->GetActorRotation());Player->GetCharacterMovement()->StopMovementImmediately();Stage=9;At=Now;return false;
  }
  if(Stage==9 && Now-At>2)
  {
   auto* Dive=Player->FindComponentByClass<UDivingComponent>();if(!Test->TestTrue(TEXT("New dive equipment equips through production component"),Dive->Equip()))return true;
   const FVector P=Player->GetActorLocation();Camera->SetActorLocation(P+Player->GetActorForwardVector()*340+Player->GetActorRightVector()*80+FVector(0,0,30));Camera->SetActorRotation((P-Camera->GetActorLocation()).Rotation());Stage=10;At=Now;return false;
  }
  if(Stage==10 && Now-At>4)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Dive-Standing.png"));Player->FindComponentByClass<UDivingComponent>()->Unequip();
   if(!Test->TestTrue(TEXT("New flight equipment equips through production component"),Player->FindComponentByClass<UWingsuitComponent>()->Equip()))return true;
   Stage=11;At=Now;return false;
  }
  if(Stage==11 && Now-At>4)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Standing.png"));Player->FindComponentByClass<UWingsuitComponent>()->Unequip();
   auto* Car=Cars[0];Player->SetActorLocation(Car->GetActorLocation()-Car->GetActorRightVector()*240+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);Stage=12;At=Now;return false;
  }
  if(Stage==12 && Now-At>1)
  {
   if(!Test->TestTrue(TEXT("Production entry accepts hero chassis"),Cars[0]->GetVehicleInteraction()->Enter(Player)))return true;
   PC->SetViewTarget(Cars[0]);Origin=Cars[0]->GetActorLocation();Cars[0]->GetDrivingComponent()->SetDriveInput(1);Stage=13;At=Now;return false;
  }
  if(Stage==13)Cars[0]->GetDrivingComponent()->SetDriveInput(1);
  if(Stage==13 && Now-At>1.2)
  {
   auto* M=Cars[0]->GetChaosMovement();Test->AddInfo(FString::Printf(TEXT("Hero drive: %.1f cm, %.1f kmh, throttle %.2f, brake %.2f, gear %d, rpm %.1f, torque %.1f, sleeping %d"),FVector::Dist2D(Origin,Cars[0]->GetActorLocation()),Cars[0]->GetDrivingComponent()->GetSpeedKmh(),M->GetThrottleInput(),M->GetBrakeInput(),M->GetCurrentGear(),M->GetEngineRotationSpeed(),M->EngineSetup.MaxTorque,!Cars[0]->GetMesh()->IsAnyRigidBodyAwake()));
   for(int32 I=0;I<M->Wheels.Num();++I)Test->AddInfo(FString::Printf(TEXT("Hero wheel %d contact %d suspension %.2f drive torque %.2f"),I,M->GetWheelState(I).bInContact,M->Wheels[I]->GetSuspensionOffset(),M->GetWheelState(I).DriveTorque));
   for(int32 I=0;I<M->Wheels.Num();++I){const auto& S=M->GetWheelState(I);Test->AddInfo(FString::Printf(TEXT("Wheel %d spring %.1f slip %.2f brake %.1f point %s local %s"),I,S.SpringForce,S.SlipMagnitude,S.BrakeTorque,*S.ContactPoint.ToString(),*Cars[0]->GetActorTransform().InverseTransformPosition(S.ContactPoint).ToString()));}
   Test->AddInfo(FString::Printf(TEXT("Chassis world %s root bone %s physics %s"),*Cars[0]->GetActorLocation().ToString(),*Cars[0]->GetMesh()->GetSocketTransform(TEXT("Root"),RTS_Component).ToString(),*GetNameSafe(Cars[0]->GetMesh()->GetPhysicsAsset())));
   Test->TestTrue(TEXT("Hero car moves using Chaos"),FVector::Dist2D(Origin,Cars[0]->GetActorLocation())>65);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Car-Driving.png"));Cars[0]->GetDrivingComponent()->SetDriveInput(0);Cars[0]->GetDrivingComponent()->SetBrakeInput(1);Stage=14;At=Now;return false;
  }
  if(Stage==14 && Now-At>2){if(!Test->TestTrue(TEXT("Hero entry/drive/brake/exit remains usable"),Cars[0]->GetVehicleInteraction()->Exit()))return true;
   const FVector P=Cars[0]->GetActorLocation();Player->SetActorLocation(P-Cars[0]->GetActorForwardVector()*650+FVector(0,0,130),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();PC->SetViewTarget(Camera);Stage=15;At=Now;return false;}
  if(Stage==15 && Now-At>2)
  {
   if(GearView==0 && !Player->FindComponentByClass<UDivingComponent>()->Equip()){Test->AddError(TEXT("Dive visual review cannot equip"));return true;}
   if(GearView==9 && !Player->FindComponentByClass<UWingsuitComponent>()->Equip()){Test->AddError(TEXT("Flight visual review cannot equip"));return true;}
   PlaceGear(W,GearView);Stage=16;At=Now;return false;
  }
  if(Stage==16 && Now-At>3)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Hero-%s-Light%d-Distance%d.png"),GearView<9?TEXT("Dive"):TEXT("Flight"),(GearView%9)/3,GearView%3));
   if(++GearView==18){Player->FindComponentByClass<UWingsuitComponent>()->Unequip();return true;}
   Stage=15;At=Now-2;return false;
  }
  return false;
 }
 void PlaceGear(UWorld* W,int32 I)
 {
  const FVector Focus=Player->GetActorLocation();const float Distance=TArray<float>{350,900,2400}[I%3];
  FCollisionQueryParams Query(SCENE_QUERY_STAT(HeroGarmentView),true,Player);FHitResult Hit;FVector Eye;bool Clear=false;
  for(int32 Angle=0;Angle<16;++Angle)
  {
   Eye=Focus+FRotator(0,Player->GetActorRotation().Yaw+Angle*22.5,0).RotateVector(FVector(Distance,80,Distance*.32));
   if(!W->LineTraceSingleByChannel(Hit,Focus,Eye,ECC_Visibility,Query)){Clear=true;break;}
  }
  Test->TestTrue(TEXT("Distance garment review is not hidden behind scenery"),Clear);
  Camera->SetActorLocation(Eye);Camera->SetActorRotation((Focus-Eye).Rotation());
  const float Hour=TArray<float>{12,17.5,22}[(I%9)/3];
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(Hour);
 }
 void Place(UWorld* W,int32 I)
 {
  auto* Car=Cars[I%2];const FVector P=Car->GetActorLocation()+FVector(0,0,35);FVector Offset;
  switch(I/2){case 0:Offset=FVector(430,-475,175);break;case 1:Offset=FVector(-460,-480,185);break;case 2:Offset=FVector(440,470,190);break;default:Offset=FVector(-470,480,220);break;}
  FVector Eye=P+Car->GetActorRotation().RotateVector(Offset);
  FCollisionQueryParams Query(SCENE_QUERY_STAT(HeroReviewView),true,Car);Query.AddIgnoredActor(Player);
  FHitResult Occlusion;
  if(W->LineTraceSingleByChannel(Occlusion,P,Eye,ECC_Visibility,Query))
  {
   Offset.Y*=-1;Eye=P+Car->GetActorRotation().RotateVector(Offset);
   if(W->LineTraceSingleByChannel(Occlusion,P,Eye,ECC_Visibility,Query)){Offset.X=0;Offset.Z=320;Eye=P+Car->GetActorRotation().RotateVector(Offset);}
  }
  Camera->SetActorLocation(Eye);Camera->SetActorRotation((P-Eye).Rotation());
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(I<4?12:I<6?17.5:22);}
 }
 FAutomationTestBase* Test;double Started=0;float At=0;int32 Stage=0,GearView=0;TArray<AIslandSupercar*> Cars;ANeonCityPlayerCharacter* Player=nullptr;ACameraActor* Camera=nullptr;FVector Origin=FVector::ZeroVector;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeroVisualPIE,"NeonCity.Polish.HeroVisualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHeroVisualPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FHeroVisualReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
