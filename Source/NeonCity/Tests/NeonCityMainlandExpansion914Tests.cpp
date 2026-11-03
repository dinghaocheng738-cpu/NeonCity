#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Adventure/NacreRoofComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/IConsoleManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "RenderTimer.h"
#include "ProfilingDebugging/MiscTrace.h"

namespace Mainland914
{
const TCHAR* ProfilePath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland914/Cars/DA_HeroCarrera914");
void Receipt(const FString& Name,const TSharedRef<FJsonObject>& J)
{
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/Mainland914/Validation")/FPaths::MakeValidFileName(Label);
 IFileManager::Get().MakeDirectory(*Dir,true);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(Name+TEXT(".json"))));
}
bool Allowed(FAutomationTestBase* T)
{
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return true;
 T->AddError(TEXT("Mainland diagnostics require isolated automatic-persistence-disabled world"));return false;
}
void Key(APlayerController* PC,FKey K,bool Down)
{if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
bool Focus(APlayerController* PC,ANeonCityPlayerCharacter* Player,AActor* Target)
{
 FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);auto* Interaction=Player->GetInteractionComponent();FVector Origin,Extent;Target->GetActorBounds(true,Origin,Extent);
 // Actual visible target surfaces inside the unchanged normal player trace.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(MainlandNormalFocus),false,Player);
 for(float X:{-.8f,-.4f,0.f,.4f,.8f})for(float Y:{-.8f,-.4f,0.f,.4f,.8f})for(float Z:{-.5f,0.f,.5f,.8f})
 {
  FVector Aim=Origin+Extent*FVector(X,Y,Z),D=(Aim-Eye).GetSafeNormal();FHitResult H;
  if(Player->GetWorld()->LineTraceSingleByChannel(H,Eye,Eye+D*Interaction->InteractionDistance,Interaction->TraceChannel,Q) && H.GetActor()==Target)
  {PC->SetControlRotation(D.Rotation());return Interaction->RefreshFocusedInteractable()==Target;}
 }
 return false;
}
class FSurvey:public IAutomationLatentCommand
{
public:explicit FSurvey(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>60)return End(TEXT("Physical world startup timeout"));
  UWorld* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<5)return false;auto* PC=W->GetFirstPlayerController();if(!PC)return false;
  auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return End(TEXT("Native navigation missing"));Nav->RefreshRoadGuide();int32 NewRoads=0,Total=0;float MaxError=0;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(MainlandPhysicalRoad),false);for(TActorIterator<APawn> I(W);I;++I)Q.AddIgnoredActor(*I);
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
  {
   ++Total;if(!I->RoadID.ToString().StartsWith(TEXT("Mainland914.")))continue;++NewRoads;
   if(I->VehicleCount!=0 || !I->bBidirectional)return End(TEXT("New district must preserve ambient fleet cap and bidirectional guide"));auto* S=I->GetRoadSpline();
   for(float Along=0;Along<=S->GetSplineLength();Along+=200)
   {
    const FVector P=S->GetLocationAtDistanceAlongSpline(Along,ESplineCoordinateSpace::World),R=S->GetRightVectorAtDistanceAlongSpline(Along,ESplineCoordinateSpace::World);
    for(float Offset:{-180.f,0.f,180.f})
    {
     const FVector Probe=P+R*Offset;FHitResult H;bool Hit=W->LineTraceSingleByChannel(H,Probe+FVector(0,0,50),Probe-FVector(0,0,150),ECC_Pawn,Q);++Probes;
     float Error=Hit?FMath::Abs(H.ImpactPoint.Z-8):9999;MaxError=FMath::Max(MaxError,Error);
     if(!Hit || Error>3 || H.ImpactNormal.Z<.98)return End(FString::Printf(TEXT("Physical lane failed %s at %.1f offset%.1f hit%s z%.2f"),*I->RoadID.ToString(),Along,Offset,*GetNameSafe(H.GetActor()),H.ImpactPoint.Z));
    }
   }
   TArray<FVector> Drive;float Limit=0;bool Loop=false;const FVector Goal=S->GetLocationAtDistanceAlongSpline(S->GetSplineLength()*.6f,ESplineCoordinateSpace::World);
   if(!Nav->GetRoadGuide().FindDriveRoute(FVector(-5500,-280,89),FVector(-1,0,0),&Goal,Drive,Limit,Loop))return End(TEXT("Existing city cannot route to ")+I->RoadID.ToString());++Connected;
  }
  // Physical old city boundary opening and the actual accessible showroom gap.
  for(const TPair<FVector,FVector>& Segment:{TPair<FVector,FVector>(FVector(-6000,-180,110),FVector(-7000,-180,110)),TPair<FVector,FVector>(FVector(-14100,1200,130),FVector(-14100,1800,130))})
  {
   FHitResult H;if(W->SweepSingleByChannel(H,Segment.Key,Segment.Value,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q))return End(TEXT("Entrance or west road still obstructed by ")+GetNameSafe(H.GetActor()));
  }
  FHitResult Pane;
  if(!W->LineTraceSingleByChannel(Pane,FVector(-15000,1400,200),FVector(-15000,1600,200),ECC_Pawn,Q))return End(TEXT("Visible showroom pane lacks physical collision"));
  if(NewRoads!=4 || Connected!=4 || Total>32)return End(TEXT("Four district roads/joins and bounded global road registry required"));
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("floor_probes"),Probes);J->SetNumberField(TEXT("maximum_floor_error_cm"),MaxError);J->SetNumberField(TEXT("connected_roads_from_original_city"),Connected);J->SetNumberField(TEXT("total_loaded_roads"),Total);J->SetBoolField(TEXT("passed"),true);Receipt(TEXT("PhysicalGraph"),J);return true;
 }
private:bool End(FString Error){Test->AddError(Error);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),false);J->SetStringField(TEXT("failure"),Error);Receipt(TEXT("PhysicalGraph"),J);return true;}FAutomationTestBase* Test;int32 Probes=0,Connected=0;double Started=0;
};

class FDrive:public IAutomationLatentCommand
{
public:explicit FDrive(FAutomationTestBase* T,bool InParking=false):Test(T),Parking(InParking),Metrics(InParking?TEXT("Mainland914-Parking"):TEXT("Mainland914-Drive")){}
 bool Update()override
 {
  if(!StartWall)StartWall=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-StartWall>180)return End(TEXT("Continuous mainland driving timeout"));if(!PC || W->GetTimeSeconds()<5)return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->GetVehicleProfile()->GetFName()==TEXT("DA_HeroCarrera914")){if(Car)return End(TEXT("Duplicate new coupe identity"));Car=*I;}
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Car || !Player)return End(TEXT("Authored coupe/player absent"));Initial=Car->GetDamageState()->GetConditionFraction();
   // One disclosed independent fixture setup, before entry/driving starts.
   // Every subsequent metre is normal Chaos/public driving input; no pose writes.
   if(!Parking){Car->SetActorLocationAndRotation(FVector(-5300,-280,65),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);}
   Player->SetActorLocation(Parking?Car->GetActorLocation()+FVector(0,210,90):FVector(-5300,-70,145),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return Next(1);
  }
  Metrics.Sample(W,Stage==1?TEXT("SetupWarmup"):Stage==3?TEXT("ContinuousCoastalDrive"):TEXT("BrakingAndExit"));
  if(Car && (Car->GetDamageState()->GetConditionFraction()<Initial-.001 || FMath::Abs(Car->GetActorRotation().Roll)>18))return End(TEXT("Car damage or unstable physical support"));
  if(Stage==1 && Age>3)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Focus(PC,Player,Car))return End(TEXT("Normal player car trace/approach failed"));Key(PC,EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(PC,EKeys::F,false);if(PC->GetPawn()!=Car || Car->GetDrivingComponent()->GetEngineState()!=ENeonCityEngineState::Running)return End(TEXT("Real F/engine entry failed"));return Next(3);
  }
  if(Stage==3)
  {
   const FVector P=Car->GetActorLocation();Peak=FMath::Max(Peak,Car->GetDrivingComponent()->GetSpeedKmh());
   if(Age>8 && Peak<.3f){auto* M=Car->GetChaosMovement();const auto& Output=M->PhysicsVehicleOutput();FString Wheels;for(int32 I=0;I<M->Wheels.Num();++I){const auto& S=M->GetWheelState(I);Wheels+=FString::Printf(TEXT(" wheel%d contact%d spring%.1f drive%.1f brake%.1f point%s"),I,S.bInContact,S.SpringForce,S.DriveTorque,S.BrakeTorque,*S.ContactPoint.ToString());}return End(FString::Printf(TEXT("Native drive blocked: throttle%.3f brake%.3f gear%d RPM%.1f torque%.1f parked%d handbrake%d interlock%d%s"),M->GetThrottleInput(),M->GetBrakeInput(),M->GetCurrentGear(),Output?Output->EngineRPM:0,Output?Output->TransmissionTorque:0,M->IsParked(),M->GetHandbrakeInput(),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked(),*Wheels));}
   if(Parking)
   {
    const FVector Targets[]={FVector(-15400,900,0),FVector(-15700,300,0),FVector(-16600,-180,0)};
    const FVector Delta=Targets[ParkingPoint]-P;
    auto* D=Car->GetDrivingComponent();
    if(Delta.Size2D()<140){if(++ParkingPoint==3){D->SetDriveInput(0);D->SetSteerInput(0);D->SetBrakeInput(1);return Next(4);}}
    const float Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,(Targets[ParkingPoint]-P).Rotation().Yaw);D->SetSteerInput(FMath::Clamp(Error/35.f,-.9f,.9f));D->SetDriveInput(FMath::Clamp((9-D->GetSpeedKmh())/8.f,0.f,.65f));D->SetBrakeInput(FMath::Clamp((D->GetSpeedKmh()-10)/6.f,0.f,.4f));
    if(P.Z<0 || P.Z>180 || P.Y>1500 || P.Y<-650)return End(TEXT("Authored parking departure lost support/clearance"));return false;
   }
   if(P.Z<0 || P.Z>150 || P.Y<-380 || P.Y>100)return End(TEXT("Continuous route left supported coastal lane"));
   const FVector Goal(P.X-800,-180,P.Z);const float Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,(Goal-P).Rotation().Yaw);
   auto* D=Car->GetDrivingComponent();D->SetSteerInput(FMath::Clamp(Error/32.f,-.4f,.4f));D->SetBrakeInput(0);D->SetDriveInput(D->GetSpeedKmh()<43?.55f:.05f);
   if(P.X<-32500){D->SetDriveInput(0);D->SetSteerInput(0);D->SetBrakeInput(1);return Next(4);}return false;
  }
  if(Stage==4 && Car->GetDrivingComponent()->GetSpeedKmh()<.5)
  {if(Peak<(Parking?3:25))return End(TEXT("Native coupe acceleration insufficient"));Key(PC,EKeys::F,true);return Next(5);}
  if(Stage==5 && Age>.3){Key(PC,EKeys::F,false);if(PC->GetPawn()!=Player)return End(TEXT("Normal F exit failed"));return Next(6);}
  if(Stage==6 && Age>1.5)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Coupe exit lacks road support"));
   if(Parking)return End(TEXT(""));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();FString Slot;
   if(!Save || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityMainlandSlot="),Slot) || !Slot.StartsWith(TEXT("NeonCity_Test_Mainland914")) || !Save->ConfigureSlot(Slot,false) || Save->DoesSaveExist())return End(TEXT("Fresh isolated mainland save slot required"));
   if(!Save->SaveGame())return End(TEXT("Real mainland save failed: ")+Save->GetLastDetail());return Next(7);
  }
  if(Stage==7)
  {if(Save->IsOperationInProgress())return false;if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Mainland save incomplete: ")+Save->GetLastDetail());return End(TEXT(""));}
  return false;
 }
private:
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(FString Error)
 {
  Key(PC,EKeys::F,false);if(Car){Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);if(!Error.IsEmpty() && PC && PC->GetPawn()==Car)Car->GetVehicleInteraction()->Exit();}Metrics.Flush();if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("parking_waypoint"),ParkingPoint);J->SetStringField(TEXT("final_car_pose"),Car?Car->GetActorTransform().ToString():TEXT("absent"));J->SetNumberField(TEXT("peak_kmh"),Peak);J->SetNumberField(TEXT("initial_condition"),Initial);J->SetNumberField(TEXT("final_condition"),Car?Car->GetDamageState()->GetConditionFraction():0);J->SetStringField(TEXT("scope"),Parking?TEXT("Authored native coupe placement unchanged, independent nearby player start, real F and public Chaos drive/steer/brake through fitted forecourt onto coastal road, supported F exit; no car pose writes."):TEXT("Independent original-city start fixture, then real F, normal Chaos drive/steer/brake for272m without pose writes, supported F exit and genuine isolated disk save."));Receipt(Parking?TEXT("AuthoredParking"):TEXT("ContinuousDrive"),J);return true;
 }
 FAutomationTestBase* Test;bool Parking=false;int32 ParkingPoint=0;FNeonCityLandFrameMetrics Metrics;UWorld* W=nullptr;APlayerController* PC=nullptr;AIslandSupercar* Car=nullptr;ANeonCityPlayerCharacter* Player=nullptr;UNeonCitySaveSubsystem* Save=nullptr;int32 Stage=0;float At=0,Peak=0,Initial=0;double StartWall=0;
};

class FHarbour:public IAutomationLatentCommand
{
public:explicit FHarbour(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(FPlatformTime::Seconds()-Started>55)return End(TEXT("New harbour interaction timeout"));if(!PC || W->GetTimeSeconds()<5)return false;const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   for(TActorIterator<ANeonCityMotorYacht> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Mainland914.Yacht0")))Boat=*I;Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Boat || !Player)return End(TEXT("New marina yacht/player absent"));
   Player->SetActorLocation(FVector(-24200,-4300,130),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return Next(1);
  }
  if(Stage==1 && Age>3)
  {const bool Ground=Player->GetCharacterMovement()->IsMovingOnGround(),Afloat=Boat->GetMarineMovement()->IsAfloat(),Moored=Boat->GetMarineMovement()->IsMoored(),Focused=Focus(PC,Player,Boat);if(!Ground || !Afloat || !Moored || !Focused)return End(FString::Printf(TEXT("New berth: ground%d afloat%d moored%d focus%d player%s boat%s"),Ground,Afloat,Moored,Focused,*Player->GetActorLocation().ToString(),*Boat->GetActorLocation().ToString()));Key(PC,EKeys::F,true);return Next(2);}
  if(Stage==2 && Age>.3){Key(PC,EKeys::F,false);if(PC->GetPawn()!=Boat || !Boat->GetMarineMovement()->IsEngineRunning())return End(TEXT("Normal marina F/engine failed"));return Next(20);}
  if(Stage==20 && Age>.3){Key(PC,EKeys::F,true);return Next(3);}
  if(Stage==3 && Age>.3){Key(PC,EKeys::F,false);if(PC->GetPawn()!=Player)return End(TEXT("New berth normal F shore exit failed"));return Next(4);}
  if(Stage==4 && Age>1)
  {auto* Base=Player->GetMovementBase();if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Base || Base->GetOwner()==Boat)return End(TEXT("New berth F exit did not choose supported pontoon"));return Next(5);}
  if(Stage==5)
  {
   const FVector Goals[]={FVector(-24200,-3400,0),FVector(-23000,-3400,0),FVector(-23000,-1900,0)};const FVector Delta=Goals[Waypoint]-Player->GetActorLocation();
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Harbour walking lost support"));
   if(Delta.Size2D()<75){if(++Waypoint==3)return End(TEXT(""));}else Player->AddMovementInput(Delta.GetSafeNormal2D());
  }
  return false;
 }
private:bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(FString Error){Key(PC,EKeys::F,false);if(!Error.IsEmpty())Test->AddError(Error);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("scope"),TEXT("New physical berth: real normal-range F/engine/F supported pontoon exit, then ordinary character movement along pontoon and graded gangway. Existing yacht sea driving and island cold-save results cached."));Receipt(TEXT("HarbourInteraction"),J);return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityMotorYacht* Boat=nullptr;ANeonCityPlayerCharacter* Player=nullptr;double Started=0;float At=0;int32 Stage=0,Waypoint=0;
};

class FViews:public IAutomationLatentCommand
{
public:explicit FViews(FAutomationTestBase* T,bool InMetrics,bool InIdentity=false):Test(T),OnlyMetrics(InMetrics),Identity(InIdentity),Metrics(TEXT("Mainland914-Fixed")){if(OnlyMetrics && FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerfViewLimit="),ViewLimit))ViewLimit=FMath::Clamp(ViewLimit,1,15);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(FPlatformTime::Seconds()-Started>250)return End(TEXT("Mainland views timeout"));if(!PC || W->GetTimeSeconds()<5)return false;
  if(!Camera)
  {
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;break;}if(!Clock)return End(TEXT("Shared lighting clock absent"));OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);
   for(const TCHAR* N:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage")})if(auto* C=IConsoleManager::Get().FindConsoleVariable(N)){OldPolicy.Add(N,C->GetFloat());C->Set(FCString::Strcmp(N,TEXT("r.ScreenPercentage"))==0?100:3,ECVF_SetByConsole);}
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);Place();return false;
  }
  const float Age=W->GetTimeSeconds()-At;const float Settle=Identity?(View==0?12:4):(View%5==0?12:4);
  if(OnlyMetrics){Metrics.Sample(W,Age<Settle?TEXT("Warmup"):*FString::Printf(TEXT("View%d"),View));if(Age<Settle+8)return false;}
  else{if(Age<Settle)return false;const FString File=Identity?FString::Printf(TEXT("Riviera915-%02d.jpg"),View):FString::Printf(TEXT("Mainland914-%02d.jpg"),View);if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,File))return End(TEXT("Actual rendered comparison failed"));}
  if(++View==(Identity?20:ViewLimit))return End(TEXT(""));Place();return false;
 }
private:
 void Place()
 {
  if(Identity)
  {
   const FVector Centres[]={FVector(-30700,2500,28),FVector(-27200,2500,28),FVector(-17700,2500,28),FVector(-30700,11100,28),FVector(-27200,11100,28),FVector(-17700,11200,28),FVector(-14500,11200,28),FVector(-30700,5300,28),FVector(-20700,2600,28),FVector(-20700,5800,28),FVector(-27200,5600,28),FVector(-17500,5600,28),FVector(-14500,5600,28),FVector(-33500,11100,28),FVector(-20700,11200,28),FVector(-25000,11200,28)};
   const float Yaws[]={0,90,0,180,90,0,180,180,0,90,0,0,90,0,0,90};
   FVector Eye,Aim;
   if(View<16)
   {
    const bool Med=View==0 || View==1 || View==2 || View==5 || View==7;
    const FVector Offset=Med?FVector(1250,2100,420):FVector(1000,1600,380);
    Eye=Centres[View]+FRotator(0,Yaws[View],0).RotateVector(Offset);Aim=Centres[View]+FVector(0,0,Med?650:550);
   }
   else
   {
    const FVector StreetEyes[]={FVector(-33300,-350,250),FVector(-32500,13900,250),FVector(-32600,7900,250),FVector(-23600,2400,250)};
    const FVector StreetAims[]={FVector(-27000,-200,260),FVector(-26000,13900,350),FVector(-27000,7900,330),FVector(-23600,-2200,180)};
    Eye=StreetEyes[View-16];Aim=StreetAims[View-16];
   }
   Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Clock->SetTimeOfDay(12);At=W->GetTimeSeconds();return;
  }
  const FVector Eyes[]={FVector(-39500,-14000,16500),FVector(-28600,-2700,450),FVector(-14100,800,260),FVector(-25700,-4700,400)};
  const FVector Aims[]={FVector(-23000,4500,600),FVector(-27000,2300,650),FVector(-14100,2900,160),FVector(-24000,-3400,100)};
  FVector Eye=Eyes[View%5==4?2:View%5],Aim=Aims[View%5==4?2:View%5];
  if(View%5==4)for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->GetVehicleProfile()->GetFName()==TEXT("DA_HeroCarrera914")){Eye=I->GetActorTransform().TransformPosition(FVector(650,-450,180));Aim=I->GetActorLocation()+FVector(0,0,60);break;}
  Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Clock->SetTimeOfDay(View/5==0?12:View/5==1?17.8f:22);At=W->GetTimeSeconds();TRACE_BOOKMARK(TEXT("Mainland915FixedView%d"),View);
 }
 bool End(FString Error)
 {
  if(!Metrics.Flush() && Error.IsEmpty())Error=TEXT("Final frame metrics write failed");if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}for(const auto& E:OldPolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*E.Key))C->Set(E.Value,ECVF_SetByConsole);if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("views"),View);J->SetBoolField(TEXT("metrics_without_readback"),OnlyMetrics);Receipt(Identity?TEXT("AddressVisuals"):OnlyMetrics?TEXT("FixedPerformance"):TEXT("DaySunsetNight"),J);return true;
 }
 FAutomationTestBase* Test;bool OnlyMetrics=false,Identity=false,OldCycle=true;FNeonCityLandFrameMetrics Metrics;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;TWeakObjectPtr<AActor> OldView;TMap<FString,float> OldPolicy;float OldHour=12,At=0;double Started=0;int32 View=0,ViewLimit=15;
};

class FCold:public IAutomationLatentCommand
{
public:explicit FCold(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>55){Test->AddError(TEXT("Mainland cold restore timeout"));return true;}UWorld* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<5)return false;
  auto* S=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Requested)
  {FString Slot;if(!S || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityMainlandSlot="),Slot) || !Slot.StartsWith(TEXT("NeonCity_Test_Mainland914")) || !S->ConfigureSlot(Slot,false) || !S->DoesSaveExist() || !S->LoadGame()){Test->AddError(TEXT("Actual fresh-process mainland load failed"));return true;}Requested=true;return false;}
  if(S->IsOperationInProgress())return false;if(S->GetLastResult()!=ENeonCitySaveResult::Success){Test->AddError(S->GetLastDetail());return true;}
  int32 Found=0;for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->GetVehicleProfile()->GetFName()==TEXT("DA_HeroCarrera914"))
  {++Found;Test->TestTrue(TEXT("Restored native coupe art, fixed roof, condition and moved coastal position"),I->GetVehicleProfile()->HeroPresentation.IsValid() && !I->bRetractable && I->GetDamageState()->GetConditionFraction()>.99 && I->GetActorLocation().X<-32500 && I->GetActorLocation().Z>0 && I->GetActorLocation().Z<150);}
  Test->TestEqual(TEXT("Exactly one saved coupe identity"),Found,1);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetNumberField(TEXT("restored_coupes"),Found);Receipt(TEXT("ColdRestore"),J);return true;
 }
private:FAutomationTestBase* Test;bool Requested=false;double Start=0;
};

// A short diagnostic, separate from the successful gameplay/capture gates.
// Engine frame IDs expose any automation sampling gaps; native thread counters
// and the accompanying CPU trace distinguish world work from waiting.
class FPerformanceProbe:public IAutomationLatentCommand
{
public:explicit FPerformanceProbe(FAutomationTestBase* T):Test(T){FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerfProbeSeconds="),Duration);Duration=FMath::Clamp(Duration,32.f,300.f);}
 bool Update()override
 {
  UWorld* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<5)return false;
  const double Now=FPlatformTime::Seconds();
  if(!Camera)
  {
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;
   const FVector Eye(-14100,800,260),Aim(-14100,2900,160);Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(Camera);
   Start=Previous=Now;TRACE_BOOKMARK(TEXT("Mainland915CpuProbeBegin"));return false;
  }
  Metrics.Sample(W,Now-Start<4?TEXT("Warmup"):TEXT("Probe"));
  Rows+=FString::Printf(TEXT("%.6f,%llu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n"),Now-Start,GFrameCounter,(Now-Previous)*1000,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GGameThreadWaitTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadWaitTime),FPlatformTime::ToMilliseconds(GRHIThreadTime));Previous=Now;
  if(Now-Start<Duration)return false;
  TRACE_BOOKMARK(TEXT("Mainland915CpuProbeEnd"));Test->TestTrue(TEXT("Diagnostic metrics written when requested"),Metrics.Flush());if(OldView.IsValid())PC->SetViewTarget(OldView.Get());Camera->Destroy();
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString File=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/Mainland914/Validation")/Label/TEXT("ThreadProbe.csv");
  Test->TestTrue(TEXT("Native engine/thread diagnostic written"),FFileHelper::SaveStringToFile(TEXT("seconds,engine_frame,interval_ms,game_ms,game_wait_ms,render_ms,render_wait_ms,rhi_ms\n")+Rows,*File));
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetNumberField(TEXT("seconds"),Duration);J->SetStringField(TEXT("scope"),TEXT("Motor-atelier CPU diagnosis; not a gameplay regression rerun"));Receipt(TEXT("PerformanceProbe"),J);return true;
 }
private:FAutomationTestBase* Test;FNeonCityLandFrameMetrics Metrics{TEXT("Mainland915-Probe")};ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> OldView;double Start=0,Previous=0;float Duration=32;FString Rows;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPresentation914,"NeonCity.Phase205c.Mainland914.PresentationContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPresentation914::RunTest(const FString&)
{
 const auto* Car=LoadObject<UVehicleData>(nullptr,Mainland914::ProfilePath);auto* P=Car?Car->HeroPresentation.LoadSynchronous():nullptr;
 TestTrue(TEXT("New fixed coupe has complete resolvable fitted data"),P && !P->bRequiresRetractableRoof && P->ResolveRequiredAssets());if(!P)return false;
 auto* Old=LoadObject<UIslandSupercarPresentationData>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/Assembly393/DA_720SSpiderPresentation"));TestTrue(TEXT("Existing720S retains strict full roof default"),Old && Old->bRequiresRetractableRoof && Old->IsComplete());
 auto* Missing=DuplicateObject<UIslandSupercarPresentationData>(P,GetTransientPackage());Missing->bRequiresRetractableRoof=true;TestFalse(TEXT("Retractable profile cannot silently omit mechanism"),Missing->IsComplete());
 auto* WheelMissing=DuplicateObject<UIslandSupercarPresentationData>(P,GetTransientPackage());WheelMissing->Wheels[0].Mesh.Reset();TestFalse(TEXT("Fixed coupe still requires all four real wheels"),WheelMissing->IsComplete());
 auto* Roof=NewObject<UNacreRoofComponent>();Roof->Configure(true,P);TestFalse(TEXT("Mismatched retractable actor flag cannot enable absent coupe roof mechanism"),Roof->IsEnabled());
 Roof->RestoreState(.5f,true);TestFalse(TEXT("Fixed roof restore cannot cycle an absent mechanism"),Roof->IsCycling());
 if(Old){auto* SpiderRoof=NewObject<UNacreRoofComponent>();SpiderRoof->Configure(true,Old);SpiderRoof->RestoreState(.5f,true);TestTrue(TEXT("Existing enabled720S roof still restores intermediate motion"),SpiderRoof->IsEnabled() && SpiderRoof->IsCycling() && FMath::IsNearlyEqual(SpiderRoof->GetProgress(),.5f));}
 TestTrue(TEXT("Fixed coupe explicit native persistence identity"),Car->GetFName()==TEXT("DA_HeroCarrera914") && Car->IsValidProfile());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPhysical914,"NeonCity.Phase205c.Mainland914.Standalone.PhysicalGraph",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPhysical914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FSurvey(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandDrive914,"NeonCity.Phase205c.Mainland914.Standalone.ContinuousDrive",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandDrive914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FDrive(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandParking914,"NeonCity.Phase205c.Mainland914.Standalone.AuthoredParking",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandParking914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FDrive(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandHarbour914,"NeonCity.Phase205c.Mainland914.Standalone.HarbourInteraction",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandHarbour914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FHarbour(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandViews914,"NeonCity.Phase205c.Mainland914.Standalone.DaySunsetNight",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandViews914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FViews(this,false));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPerformance914,"NeonCity.Phase205c.Mainland914.Standalone.FixedPerformance",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPerformance914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FViews(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandAddresses915,"NeonCity.Phase205c.Mainland914.Standalone.AddressVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandAddresses915::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FViews(this,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCold914,"NeonCity.Phase205c.Mainland914.Standalone.ColdRestore",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCold914::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FCold(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandProbe915,"NeonCity.Phase205c.Mainland914.Standalone.PerformanceProbe",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandProbe915::RunTest(const FString&){if(!Mainland914::Allowed(this))return false;ADD_LATENT_AUTOMATION_COMMAND(Mainland914::FPerformanceProbe(this));return true;}
#endif
