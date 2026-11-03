#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandHeroDisplayLight.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Vehicles/VehicleComponent.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Materials/MaterialInterface.h"
#include "SkeletalRenderPublic.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"

namespace McLarenPresentation
{
struct FView { const TCHAR* Name; FVector Eye; FVector Target; };
class FReview : public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* In,bool FixOnly=false,bool LightingOnly=false,bool DriverOnly=false,bool TimeOnly=false,bool NightOnly=false):Test(In),bFixOnly(FixOnly),bLightingOnly(LightingOnly),bDriverOnly(DriverOnly),bTimeOnly(TimeOnly),bNightOnly(NightOnly){}
 void Key(FKey K,bool Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Pose(const FView& View)
 {
  CurrentView=View;const auto& T=Car->GetMesh()->GetComponentTransform();const FVector Eye=T.TransformPosition(View.Eye);
  Camera->SetActorLocationAndRotation(Eye,(T.TransformPosition(View.Target)-Eye).Rotation());PC->SetViewTarget(Camera);At=W->GetTimeSeconds();
 }
 void Dump(const FString& Name)
 {
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("McLarenPresentation");
  IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Diagnostics,*(Dir/Name));
 }
 void ProjectedComponents()
 {
  Diagnostics=TEXT("actor,component,mesh,world_x,world_y,world_z,screen_x,screen_y,extent_x,extent_y,extent_z\n");
  for(TActorIterator<AActor> It(W);It;++It)
  {
   TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
   for(const auto* Part:Parts)if(Part->IsVisible() && !Part->bHiddenInGame && Part->GetStaticMesh() && FVector::Dist(Part->Bounds.Origin,Car->GetActorLocation())<1800)
   {
    FVector2D Screen;if(!PC->ProjectWorldLocationToScreen(Part->Bounds.Origin,Screen))continue;
    const FVector Centre=Part->Bounds.Origin,Extent=Part->Bounds.BoxExtent;
    Diagnostics+=FString::Printf(TEXT("%s,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),*It->GetName(),*Part->GetName(),*Part->GetStaticMesh()->GetPathName(),Centre.X,Centre.Y,Centre.Z,Screen.X,Screen.Y,Extent.X,Extent.Y,Extent.Z);
   }
  }
  Dump(TEXT("projected-components.csv"));
 }
 void DriverBones()
 {
  auto* Driver=Car->FindComponentByClass<UTransportOccupantComponent>()->GetDriverMesh();
  Diagnostics=TEXT("bone,car_x,car_y,car_z\n");
  for(const TCHAR* Name:{TEXT("pelvis"),TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")})
  {
   const FVector BonePosition=Car->GetMesh()->GetComponentTransform().InverseTransformPosition(Driver->GetSocketLocation(Name));
   Diagnostics+=FString::Printf(TEXT("%s,%.3f,%.3f,%.3f\n"),Name,BonePosition.X,BonePosition.Y,BonePosition.Z);
  }
  Dump(TEXT("driver-bones.csv"));
  if(bFixOnly && !bLightingOnly)
  {
   const auto* Seat=Car->FindComponentByClass<UTransportOccupantComponent>();
   const auto& CarTransform=Car->GetMesh()->GetComponentTransform();
   Test->TestTrue(TEXT("Left hand reaches the actual steering rim target within 3cm"),FVector::Dist(CarTransform.InverseTransformPosition(Driver->GetSocketLocation(TEXT("hand_l"))),Seat->LeftHandTarget)<3);
   Test->TestTrue(TEXT("Right hand reaches the actual steering rim target within 3cm"),FVector::Dist(CarTransform.InverseTransformPosition(Driver->GetSocketLocation(TEXT("hand_r"))),Seat->RightHandTarget)<3);
   if(bDriverOnly)
   {
    TArray<FFinalSkinVertex> Vertices;Driver->GetCPUSkinnedVertices(Vertices,0);
    int32 Covered=0,Penetrations=0;float Clearance=TNumericLimits<float>::Max();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(DriverRoofClearance),true);
    for(const auto& Vertex:Vertices)
    {
     const FVector WorldPoint=Driver->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
     if(CarTransform.InverseTransformPosition(WorldPoint).Z<70)continue;
     FHitResult Shell;
     // Trace out of the cabin toward the actual inner lining. The upper
     // exterior skin alone would overlook vertices inside the panel thickness.
     if(Car->GetRoofMechanism()->GetFrontPanel()->LineTraceComponent(Shell,WorldPoint-FVector(0,0,200),WorldPoint+FVector(0,0,200),Q))
     {
      ++Covered;const float Gap=Shell.ImpactPoint.Z-WorldPoint.Z;Clearance=FMath::Min(Clearance,Gap);if(Gap<.5f)++Penetrations;
     }
    }
    Test->TestTrue(TEXT("Actual upper occupant geometry was compared with the closed shell"),Covered>100);
    Test->TestEqual(TEXT("No upper occupant/hat vertex clips the actual hardtop"),Penetrations,0);
    Test->AddInfo(FString::Printf(TEXT("Closed-shell occupant comparison: %d vertices, minimum clearance %.3fcm"),Covered,Clearance));
   }
  }
 }
 bool Finish(const TCHAR* Failure=nullptr)
 {
  if(PC){Key(EKeys::F,false);Key(EKeys::R,false);Key(EKeys::SpaceBar,false);PC->SetViewTarget(PC->GetPawn());}
  if(Camera)Camera->Destroy();
  if(Clock){Clock->SetTimeOfDay(OriginalHour);Clock->SetCycleEnabled(OriginalCycle);}
  if(Failure)Test->AddError(Failure);return true;
 }
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>240)return Finish(TEXT("720S presentation timed out including time-of-day exposure settling"));
  if(!W){W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;}
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(Camera && Car)
  {
   const auto& T=Car->GetMesh()->GetComponentTransform();const FVector Eye=T.TransformPosition(CurrentView.Eye);
   Camera->SetActorLocationAndRotation(Eye,(T.TransformPosition(CurrentView.Target)-Eye).Rotation());
  }
  Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==7 && bTimeOnly && Age>NextParkSample)
  {
   NextParkSample=Age+1;auto* M=Car->GetChaosMovement();
   Test->AddInfo(FString::Printf(TEXT("Native parking t%.2f brake%.3f handbrake%d parked%d sleepThreshold%.3f sleepSlope%.3f awake%d controller%s velocity%s rotation%s"),Age,M->GetBrakeInput(),M->GetHandbrakeInput(),M->IsParked(),M->SleepThreshold,M->SleepSlopeLimit,Car->GetMesh()->IsAnyRigidBodyAwake(),*GetNameSafe(Car->GetController()),*Car->GetVelocity().ToString(),*Car->GetActorRotation().ToString()));
   for(int32 I=0;I<M->Wheels.Num();++I){const auto& S=M->GetWheelState(I);const UChaosVehicleWheel* Wheel=M->Wheels[I].Get();Test->AddInfo(FString::Printf(TEXT("Parking wheel%d %s brakeEnabled%d handbrakeEnabled%d contact%d force%.2f brakeTorque%.2f slip%.2f skid%.2f suspension%.3f point%s"),I,*GetNameSafe(Wheel),Wheel?Wheel->bAffectedByBrake:false,Wheel?Wheel->bAffectedByHandbrake:false,S.bInContact,S.SpringForce,S.BrakeTorque,S.SlipMagnitude,S.SkidMagnitude,S.NormalizedSuspensionLength,*S.ContactPoint.ToString()));}
  }
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(60380,-152870,750))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bRetractable)Car=*It;
   if(!P || !Car || !Car->GetPresentationProfile())return Finish(TEXT("Actual 720S presentation absent"));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;break;}
   if(!Clock)return Finish(TEXT("Native time-of-day clock absent"));OriginalHour=Clock->GetTimeOfDay();OriginalCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);
   // One declared supported starting fixture beside the existing parked car.
   // Only normal F/R input changes possession and roof motion from here.
   const FVector Near=Car->GetActorLocation()-Car->GetActorRightVector()*240;FHitResult Floor;FCollisionQueryParams Q;Q.AddIgnoredActor(Car);Q.AddIgnoredActor(P);
   if(!W->LineTraceSingleByChannel(Floor,Near+FVector(0,0,500),Near-FVector(0,0,500),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return Finish(TEXT("Native supported starting floor absent"));
   P->SetActorLocation(Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==1 && Age>.5)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return Finish(TEXT("Normal F eye trace misses 720S"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.2)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Car)return Finish(TEXT("Actual F boarding failed"));return Next(3);
  }
  if(Stage==3 && Age>.8)
  {
   if(!bTimeOnly){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-Day-NormalDriver.png"));ProjectedComponents();DriverBones();}
   // A parked comparison uses normal held brake input throughout adaptation.
   // Longer night settling must not let the native car roll out of its bay.
   Key(EKeys::SpaceBar,true);ParkedPosition=Car->GetActorLocation();
   if(bLightingOnly)Clock->SetTimeOfDay(17.5);
   if(bFixOnly && !bLightingOnly && !bDriverOnly)
   {
    for(TActorIterator<AIslandHeroDisplayLight> It(W);It;++It)if(FVector::Dist(It->GetActorLocation(),Car->GetActorLocation())<500)
    {
     if(!Test->TestTrue(TEXT("Previously floating optical head has a visible physical stand"),It->GroundStand->IsVisible() && It->GroundStand->GetStaticMesh()))return Finish();
     Test->TestTrue(TEXT("Fixture stand has safe physical collision"),It->GroundStand->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
    }
   }
   FActorSpawnParameters S;S.ObjectFlags=RF_Transient;Camera=W->SpawnActor<ACameraActor>(S);Camera->GetCameraComponent()->FieldOfView=58;Camera->GetCameraComponent()->PostProcessBlendWeight=0;
   if(bTimeOnly){PC->SetViewTarget(Car);Key(EKeys::R,true);return Next(5);}
   Pose(Views[bDriverOnly?2:0]);return Next(bFixOnly?8:4);
  }
  if(Stage==4 && Age>1.2)
  {
   if(!FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("720S-Day-Closed-%s.png"),Views[ViewIndex].Name)))return Finish(TEXT("Actual native close view missing"));
   if(++ViewIndex<Views.Num()){Pose(Views[ViewIndex]);return false;}
   PC->SetViewTarget(Car);Key(EKeys::R,true);return Next(5);
  }
  if(Stage==5 && Age>.12){Key(EKeys::R,false);Pose(MechanismViews[0]);return Next(6);}
  if(Stage==6)
  {
   const float Progress=Car->GetRoofMechanism()->GetProgress();
   if(!bTimeOnly && MotionIndex<3 && Progress>=MotionProgress[MotionIndex])
   {
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("720S-Day-Mechanism-%d.png"),MotionIndex));
    if(++MotionIndex<3)Pose(MechanismViews[MotionIndex]);
   }
   if(Car->GetRoofMechanism()->IsCycling())return false;
   Test->TestEqual(TEXT("Real player roof operation reaches open endpoint"),Car->GetRoofMechanism()->GetProgress(),1.f);
   // Time-only paint/glass/cabin comparisons use the normal parked state.
   // Actual occupied driving/roof input is covered separately by the native
   // roof condition test; do not freeze an occupied physics body for a still.
   if(bTimeOnly){Key(EKeys::SpaceBar,false);Key(EKeys::F,true);return Next(9);}
   ViewIndex=0;Pose(Views[0]);return Next(7);
  }
  if(Stage==7 && Age>(HourIndex==0?1.5f:20.f))
  {
   if(FVector::Dist(Car->GetActorLocation(),ParkedPosition)>25)
   {
    Test->AddInfo(FString::Printf(TEXT("Braked bay drift %.3fcm speed %.3fkm/h from%s to%s"),FVector::Dist(Car->GetActorLocation(),ParkedPosition),Car->GetDrivingComponent()->GetSpeedKmh(),*ParkedPosition.ToString(),*Car->GetActorLocation().ToString()));
    return Finish(TEXT("Braked comparison car left its supported parking bay"));
   }
   const TCHAR* Time=HourIndex==0?TEXT("Day"):HourIndex==1?TEXT("Sunset"):TEXT("Night");
   if(!FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("720S-%s-Open-%s.png"),Time,Views[ViewIndex].Name)))return Finish(TEXT("Native time-of-day image missing"));
   if(++ViewIndex<3){Pose(Views[ViewIndex]);return false;}
   if(++HourIndex<3){ViewIndex=0;Clock->SetTimeOfDay(HourIndex==1?17.5f:22.f);Pose(Views[0]);return false;}
   return Finish();
  }
  if(Stage==9 && Age>.3f)
  {
   Key(EKeys::F,false);
   if(PC->GetPawn()!=P)return Finish(TEXT("Normal F parking exit failed before time-of-day comparison"));
   HourIndex=bNightOnly?2:1;ViewIndex=0;ParkedPosition=Car->GetActorLocation();Clock->SetTimeOfDay(bNightOnly?22.f:17.5f);
   Test->AddInfo(TEXT("Time-of-day scope: normal F/R roof opening and F exit; unoccupied native parked car, no body/velocity overrides. Occupied driving remains a separate native input gate."));
   Pose(Views[0]);return Next(7);
  }
  if(Stage==8 && Age>1.5)
  {
   const int32 Index=bDriverOnly?2:bLightingOnly?ViewIndex:ViewIndex==0?0:2;
   const TCHAR* Time=HourIndex==0?(bLightingOnly?TEXT("Sunset"):TEXT("Day")):TEXT("Night");
   if(!FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("720S-%s-%s-%s.png"),bDriverOnly?TEXT("Driver"):bLightingOnly?TEXT("Lighting"):TEXT("Fit"),Time,Views[Index].Name)))return Finish(TEXT("Changed-fit native comparison missing"));
   if(++ViewIndex<(bDriverOnly?1:bLightingOnly?3:2)){Pose(Views[bLightingOnly?ViewIndex:2]);return false;}
   if(++HourIndex<2){ViewIndex=0;Clock->SetTimeOfDay(22);Pose(Views[bDriverOnly?2:0]);return false;}
   return Finish();
  }
  return false;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 double Start=0;float At=0,OriginalHour=0;bool OriginalCycle=false,bFixOnly=false,bLightingOnly=false,bDriverOnly=false,bTimeOnly=false,bNightOnly=false;int32 Stage=0,ViewIndex=0,MotionIndex=0,HourIndex=0;FString Diagnostics;
 FView CurrentView{TEXT("Initial"),FVector::ZeroVector,FVector::ZeroVector};
 FVector ParkedPosition=FVector::ZeroVector;float NextParkSample=0;
 const TArray<FView> Views={{TEXT("Front"),FVector(490,-430,175),FVector(30,0,45)},{TEXT("Rear"),FVector(-420,-370,180),FVector(-50,0,50)},{TEXT("Cabin"),FVector(85,-180,122),FVector(35,-28,59)},{TEXT("FrontWheel"),FVector(150,-208,32),FVector(133.5,-80.8,12)}};
 const TArray<FView> MechanismViews={{TEXT("Leaf"),FVector(-80,-180,142),FVector(-47,-38,81)},{TEXT("Arms"),FVector(-100,-178,129),FVector(-45,-35,64)},{TEXT("Stow"),FVector(-320,-260,170),FVector(-90,0,62)}};
 const float MotionProgress[3]={.15f,.45f,.74f};FNeonCityVisualEvidence Evidence{TEXT("McLarenPresentation")};
};

/** Two declared local fixtures; gravity, F/R possession and roof movement are native. */
class FContactReview : public IAutomationLatentCommand
{
public:
 explicit FContactReview(FAutomationTestBase* In):Test(In){}
 void Key(FKey K,bool Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool Finish(const TCHAR* Error=nullptr)
 {
  if(PC){Key(EKeys::F,false);Key(EKeys::R,false);PC->SetViewTarget(PC->GetPawn());}
  if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(Hour);Clock->SetCycleEnabled(Cycle);}
  if(Error)Test->AddError(Error);return true;
 }
 void Pose(FVector Eye,FVector Target)
 {
  const FTransform T=Car->GetMesh()->GetComponentTransform();
  Camera->SetActorLocationAndRotation(T.TransformPosition(Eye),(T.TransformPosition(Target)-T.TransformPosition(Eye)).Rotation());PC->SetViewTarget(Camera);At=W->GetTimeSeconds();
 }
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>110)return Finish(TEXT("720S contact review timed out"));
  if(!W){W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;}
  if(!PC || W->GetTimeSeconds()<4)return false;
  Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(60380,-152870,750))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bRetractable)Car=*It;
   if(!P || !Car || !Car->GetPresentationProfile())return Finish(TEXT("Required actual 720S absent"));
   Roof=Car->GetRoofMechanism();if(Roof->IsCycling() || Roof->GetProgress()!=0)return Finish(TEXT("Authored closed roof fixture required"));
   FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);const FTransform T=Car->GetMesh()->GetComponentTransform();
   if(!W->LineTraceSingleByChannel(Hit,T.TransformPosition(FVector(10,0,150)),T.TransformPosition(FVector(10,0,70)),ECC_Pawn,Q) || Hit.GetComponent()!=Roof->GetFrontPanel())return Finish(TEXT("Closed visible shell has no actual pawn collision"));
   P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+100),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return Next(1);
  }
  if(Stage==1 && Age>1.7)
  {
   auto* Move=P->GetCharacterMovement();FDynamicStandingSurface Surface;
   Test->TestTrue(TEXT("Real gravity landing remains on the closed roof"),Move->IsMovingOnGround() && Move->CurrentFloor.HitResult.GetComponent()==Roof->GetFrontPanel());
   Test->TestTrue(TEXT("Closed roof uses the existing dynamic standing contract"),NeonCityDynamicSurfaces::Query(Move->CurrentFloor.HitResult,P,Surface));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-Closed-Roof-NativeLanding.png"));
   const FVector Near=Car->GetActorLocation()-Car->GetActorRightVector()*240;FHitResult Floor;FCollisionQueryParams Q;Q.AddIgnoredActor(Car);Q.AddIgnoredActor(P);
   if(!W->LineTraceSingleByChannel(Floor,Near+FVector(0,0,500),Near-FVector(0,0,500),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return Finish(TEXT("Local supported entry fixture absent"));
   P->SetActorLocation(Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);return Next(2);
  }
  if(Stage==2 && Age>.5)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return Finish(TEXT("Normal F contact misses new car"));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3 && Age>.2){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return Finish(TEXT("Normal boarding failed"));return Next(4);}
  if(Stage==4 && Age>.7)
  {
   const FVector Velocity=Car->GetMesh()->GetPhysicsLinearVelocity();
   // A single bounded native-physics fixture reproduces sideways speed. It
   // changes no input mapping or production state and restores before ticking.
   Car->GetMesh()->SetPhysicsLinearVelocity(Car->GetActorRightVector()*500);
   const bool Accepted=Roof->RequestToggle();Car->GetMesh()->SetPhysicsLinearVelocity(Velocity);
   if(!Test->TestFalse(TEXT("13.5km/h lateral motion cannot start the roof"),Accepted))return Finish();
   int32 Found=0;
   for(TActorIterator<AIslandHeroDisplayLight> It(W);It;++It)if(FVector::Dist(It->GetActorLocation(),Car->GetActorLocation())<500)
   {
    ++Found;auto* Stand=It->GroundStand.Get();const FBox StandBox=Stand->CalcBounds(Stand->GetComponentTransform()).GetBox();
    Test->TestTrue(TEXT("Native fixture has a visible collidable stand"),Stand->IsVisible() && Stand->GetStaticMesh() && Stand->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(StandGroundContact),true,*It);Q.AddIgnoredActor(Car);Q.AddIgnoredActor(P);FHitResult Floor;
    const FVector Bottom(StandBox.GetCenter().X,StandBox.GetCenter().Y,StandBox.Min.Z);
    Test->TestTrue(TEXT("Manufactured foot actually contacts visible stationary ground"),W->LineTraceSingleByChannel(Floor,Bottom+FVector(0,0,10),Bottom-FVector(0,0,30),ECC_Visibility,Q) && FMath::Abs(Floor.ImpactPoint.Z-Bottom.Z)<1 && Floor.GetComponent() && Floor.GetComponent()->IsVisible() && Floor.GetComponent()->Mobility!=EComponentMobility::Movable);
    const float Join=StandBox.Max.Z-It->Fixture->CalcBounds(It->Fixture->GetComponentTransform()).GetBox().Min.Z;
    Test->TestTrue(TEXT("Stand socket physically joins optical head"),Join>=0 && Join<3);
   }
   Test->TestTrue(TEXT("The repaired nearby fixture is present"),Found>0);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-Roof-Controls-NormalHUD.png"));Key(EKeys::R,true);return Next(5);
  }
  if(Stage==5 && Age>.12){Key(EKeys::R,false);return Next(6);}
  if(Stage==6 && !Roof->IsCycling() && Age>1)
  {
   if(!Test->TestEqual(TEXT("Actual player R reaches open endpoint"),Roof->GetProgress(),1.f))return Finish();
   FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);const FTransform T=Car->GetMesh()->GetComponentTransform();
   Test->TestFalse(TEXT("Open cockpit has no invisible closed roof ceiling"),W->LineTraceSingleByChannel(Hit,T.TransformPosition(FVector(10,0,150)),T.TransformPosition(FVector(10,0,80)),ECC_Pawn,Q));
   bool Red=false;
   TInlineComponentArray<UStaticMeshComponent*> Parts(Car);
   for(const auto* Part:Parts)if(Part->GetStaticMesh() && Part->GetStaticMesh()->GetName()==TEXT("SM_McLaren720S_FixedBody"))
    for(int32 I=0;I<Part->GetStaticMesh()->GetStaticMaterials().Num();++I)
     if(Part->GetStaticMesh()->GetStaticMaterials()[I].MaterialSlotName.ToString().Contains(TEXT("Red_car_lights")))Red=Part->GetMaterial(I) && Part->GetMaterial(I)->GetName()==TEXT("M_Hero_RedLED");
   Test->TestTrue(TEXT("Tail lamp retains its dedicated red emissive material"),Red);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;break;}
   if(!Clock)return Finish(TEXT("Native clock absent"));Hour=Clock->GetTimeOfDay();Cycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(17.5);
   FActorSpawnParameters S;S.ObjectFlags=RF_Transient;Camera=W->SpawnActor<ACameraActor>(S);Camera->GetCameraComponent()->FieldOfView=58;Camera->GetCameraComponent()->PostProcessBlendWeight=0;
   Pose(FVector(490,-430,175),FVector(30,0,45));return Next(7);
  }
  if(Stage==7 && Age>1.5){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-FixedGlass-Sunset-Front.png"));Pose(FVector(85,-180,122),FVector(35,-28,59));return Next(8);}
  if(Stage==8 && Age>1.5){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-FixedGlass-Sunset-Cabin.png"));Clock->SetTimeOfDay(22);Pose(FVector(-420,-370,180),FVector(-50,0,50));return Next(9);}
  if(Stage==9 && Age>1.5){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("720S-RestoredTailLamp-Night-Rear.png"));return Finish();}
  return false;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;UNacreRoofComponent* Roof=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 double Start=0;float At=0,Hour=0;bool Cycle=false;int32 Stage=0;FNeonCityVisualEvidence Evidence{TEXT("McLarenContact")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SPresentation,"NeonCity.Phase205cUpgrade.McLarenPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SPresentation::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SPresentationFit,"NeonCity.Phase205cUpgrade.McLarenPresentationFitPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SPresentationFit::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SContact,"NeonCity.Phase205cUpgrade.McLarenContactPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SContact::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FContactReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SRepairedLighting,"NeonCity.Phase205cUpgrade.McLarenRepairedLightingPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SRepairedLighting::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SStandalonePresentation,"NeonCity.Phase205c.Estate.Standalone.McLarenPresentation",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool F720SStandalonePresentation::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SStandaloneTimeOfDay,"NeonCity.Phase205c.Estate.Standalone.McLarenTimeOfDay",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool F720SStandaloneTimeOfDay::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,false,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SStandaloneNightExposure,"NeonCity.Phase205c.Estate.Standalone.McLarenNightExposure",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool F720SStandaloneNightExposure::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,false,false,false,true,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SNightExposurePIE,"NeonCity.Phase205cUpgrade.McLarenNightExposurePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SNightExposurePIE::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,false,false,false,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SDriverClearance,"NeonCity.Phase205cUpgrade.McLarenDriverClearancePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SDriverClearance::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(McLarenPresentation::FReview(this,true,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
