#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityPhase205cVehicleTurnSurvey.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandLandDoor.h"
#include "Adventure/IslandGarageDoor.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"

namespace NeonCityChangedRoadDriving
{
enum class EScope : uint8 {ManualUp,ManualDown,CruiseUp,CruiseDown,TerminalEntry,LowerSeam,TerminalSeam,ParkedExit,TheOneArrival,TheOneRock};
const TCHAR* ScopeName(EScope S)
{switch(S){case EScope::ManualUp:return TEXT("ManualUp");case EScope::ManualDown:return TEXT("ManualDown");case EScope::CruiseUp:return TEXT("CruiseUp");case EScope::CruiseDown:return TEXT("CruiseDown");case EScope::LowerSeam:return TEXT("LowerSeam");case EScope::TerminalSeam:return TEXT("TerminalSeam");case EScope::ParkedExit:return TEXT("ParkedExit");case EScope::TheOneArrival:return TEXT("TheOneArrival");case EScope::TheOneRock:return TEXT("TheOneRock409");default:return TEXT("TerminalEntry");}}

/** Each case declares one road-supported car/player fixture. Subsequently only
 * actual mapped keys/control rotation are used. Each original car gets its own
 * PIE world, so parked predecessors are never moved or hidden to clear a bay. */
class FDrive : public IAutomationLatentCommand
{
public:
 FDrive(FAutomationTestBase* T,EScope S,int32 VehicleIndex,int32 First=400,int32 Last=435,int32 InArrivalFirst=350):Test(T),Scope(S),CarIndex(VehicleIndex),FocusFirst(First),FocusLast(Last),ArrivalFirst(InArrivalFirst),Evidence(TEXT("ChangedRoadDriving")){}
 virtual ~FDrive(){Unhook();}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>240)return Finish(false,TEXT("Changed-region case exceeded its 240-second bound"));
  if(!W || !PC || W->GetTimeSeconds()<3)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  if(Stage==0)
  {
   // A prior failed car stops later cases; completed cases are never rerun here.
   if(Test->HasAnyErrors())return true;
   auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady((Scope==EScope::TheOneArrival || Scope==EScope::TheOneRock)?NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254)):NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return false;
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("ChangedRoadDriving")/ScopeName(Scope)/FString::FromInt(CarIndex);
   IFileManager::Get().MakeDirectory(*Directory,true);
   CSV=TEXT("seconds,stage,vehicle_guid,source_station,x,y,z,yaw,pitch,roll,speed_kmh,lane_error,wheel_contacts,condition,cruise,steering,requested_steering,throttle,brake,key_W,key_A,key_D\n");
   if(!BuildPath())return Finish(false,TEXT("Current source road/connected native forecourt is missing or inconsistent"));
   TArray<AIslandSupercar*> Cars;for(TActorIterator<AIslandSupercar> It(W);It;++It)Cars.Add(*It);
   if(Cars.Num()!=3)return Finish(false,TEXT("Exactly three original island cars are required"));
   Cars.Sort([](const AIslandSupercar& A,const AIslandSupercar& B){return A.VehicleID.ToString()<B.VehicleID.ToString();});Car=Cars[CarIndex];
   if(Scope==EScope::TheOneArrival || Scope==EScope::TheOneRock)
   {
    Car=nullptr;for(auto* Candidate:Cars)if(Candidate->VehicleData && Candidate->VehicleData->GetName().Contains(TEXT("Kestrel"))){Car=Candidate;break;}
    if(!Car)return Finish(false,TEXT("The actual unchanged Kestrel is required for the new ascent minimum"));
   }
   if(!Car->GetMesh() || !Car->GetChaosMovement() || !Car->GetVehicleInteraction() || !Car->GetDamageState() || !Car->GetRoadCruise())return Finish(false,TEXT("Original car lacks its native driving components"));
   const FRotator Heading=(Path[1]-Path[0]).Rotation();FHitResult Floor;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(ChangedRoadInitialSupport),false,P);Q.AddIgnoredActor(Car);
   if(!W->LineTraceSingleByChannel(Floor,Path[0]+FVector(0,0,220),Path[0]-FVector(0,0,250),ECC_Vehicle,Q) ||
      !Floor.IsValidBlockingHit() || Floor.ImpactNormal.Z<.7 || FMath::Abs(Floor.ImpactPoint.Z-Path[0].Z)>100)
    return Finish(false,TEXT("Declared initial car fixture has no matching real native road support"));
   // The sole initial car fixture. No transform/velocity correction follows.
   Car->SetActorTransform(FTransform(Heading,FVector(Path[0].X,Path[0].Y,Floor.ImpactPoint.Z+55)),false,nullptr,ETeleportType::TeleportPhysics);
   CarPlacements=1;
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   FrameDriver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FDrive::Frame);
   LastProgress=Now;return Next(1);
  }
  if(Stage==1 && Age>1.2)
  {
   if(Car->GetVelocity().Size()>35)return Age>5?Finish(false,TEXT("Initial native road fixture did not settle")):false;
   // Select the sole declared supported player start against the settled
   // chassis, so suspension settling cannot invalidate the initial F trace.
   FVector Stand;if(!Phase205cVehicleApproach(W,P,Car,Car->GetActorLocation()-Car->GetActorRightVector()*260,Stand))return Finish(false,TEXT("Declared initial player fixture has no full-capsule supported native car approach"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();PlayerPlacements=1;
   return Next(12);
  }
  if(Stage==12 && Age>.35)
  {
   if(!P->GetCharacterMovement()->IsMovingOnGround())return Age>3?Finish(false,TEXT("Declared supported player start did not settle")):false;
   if(!MeasureAxles())return Finish(false,TEXT("Original four-wheel rig/axle/steering measurement invalid"));
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return Finish(false,TEXT("Normal initial F eye trace did not focus the original car"));
   Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.2)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Car || Car->GetVehicleInteraction()->GetDriver()!=P)return Finish(false,TEXT("Native F did not board the original car"));
   NativeBoarded=true;InitialCondition=Car->GetDamageState()->GetConditionFraction();LastManualTime=LastProgress=Now;LastCar=Car->GetActorLocation();
   if(Scope==EScope::ParkedExit){Key(EKeys::SpaceBar,true);return Next(5);}
   if(IsCruise())
   {
    auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return Finish(false,TEXT("Actual controller navigation missing"));
    FNeonCityNavigationDestination Goal;Goal.OwnerID=TEXT("Test.ChangedRoadBank");Goal.DestinationID=TEXT("Test.ChangedRoadBank.End");Goal.Label=FText::FromString(TEXT("Changed road segment end"));Goal.Location=Path.Last();Goal.UseRoadRoute=true;Nav->SetDestination(Goal);
    Key(EKeys::C,true);return Next(3);
   }
   return Next(4);
  }
  if(Stage==3 && Age>.2){Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return Finish(false,TEXT("Native C failed to engage on the real changed-region lane: ")+Car->GetRoadCruise()->GetReason());return Next(4);}
  if(Stage==4 || Stage==5)
  {
   Evidence.Record(W,PC,CarIndex*100+Stage);
   if(!SupportValid || AirborneSeconds>.75f){SamplePhysics();FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Actual-SupportFailure.png"));return Finish(false,FString::Printf(TEXT("Physical road support failed at source%d lane%.1f pose%s"),Stations[Node],LaneError,*Car->GetActorTransform().ToHumanReadableString()));}
   if(Car->GetDamageState()->GetConditionFraction()<InitialCondition-.03f)return Finish(false,TEXT("Actual scoped driving materially damaged the original car"));
   Sample();
   if(Stage==4)
   {
    if(IsCruise() && !Car->GetRoadCruise()->IsActive())return Finish(false,TEXT("Native cruise exited before completing the changed bank: ")+Car->GetRoadCruise()->GetReason());
    if(Now-LastProgress>12)
    {
     Phase205cNativeVehicleTurnSurvey(W,Car,Car->GetMesh(),false,0,FailureSurvey);
     FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("ChangedRoad-%s-Car%d-Actual-Stall.png"),ScopeName(Scope),CarIndex));
     return Finish(false,FString::Printf(TEXT("Mapped driving made no progress at source%d (%s); native shape forecast: %s"),Stations[Node],*Car->GetRoadCruise()->GetReason(),*FailureSurvey));
    }
    if(Distance.Last()-Along<250){Release();Key(EKeys::S,IsCruise());Key(EKeys::SpaceBar,true);return Next(5);}return false;
   }
   if(Age>1 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
   {
    if(FVector::Dist2D(Car->GetActorLocation(),Path.Last())>450)return Finish(false,TEXT("Native brakes did not retain the supported scoped endpoint/bay"));
    if(Scope==EScope::TheOneRock)
    {
     for(int32 I=FocusFirst;I<=FocusLast-5;++I)if(!Visited.Contains(I))return Finish(false,FString::Printf(TEXT("Local real road-rock trajectory missed station%d"),I));
     FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Rock409-GenuineClearance.png"));return Finish(true,TEXT(""));
    }
    if(Scope==EScope::TheOneArrival)
    {
     for(int32 I=ArrivalFirst;I<=638;++I)if(!Visited.Contains(I))return Finish(false,FString::Printf(TEXT("Real changed-ascent trajectory missed native station%d"),I));
     Release();Key(EKeys::F,true);return Next(6);
    }
    if(!IsTerminal())
    {
     const int32 FirstRequired=Scope==EScope::LowerSeam?698:704,LastRequired=Scope==EScope::LowerSeam?704:716;
     for(int32 I=FirstRequired;I<=LastRequired;++I)if(!Visited.Contains(I))return Finish(false,FString::Printf(TEXT("Real trajectory did not observe changed source station%d"),I));
     FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("ChangedBank-%s-%s.png"),ScopeName(Scope),*Car->VehicleID.ToString()));return Finish(true,TEXT(""));
    }
    FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("CurrentCourt-%s-Parked.png"),*Car->VehicleID.ToString()));Release();Key(EKeys::F,true);return Next(6);
   }
   if(Age>7)return Finish(false,TEXT("Native endpoint parking did not settle within seven seconds"));return false;
  }
  if(Stage==6 && Age>.2)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=P || P->IsHidden() || !P->GetActorEnableCollision())return Finish(false,TEXT("Normal parked F exit failed"));NativeExited=true;
   return Next(7);
  }
  if(Stage==7 && Age>.5)
  {
   if(!P->GetCharacterMovement()->IsMovingOnGround())return Age>4?Finish(false,TEXT("Native car exit did not establish normal supported walking")):false;
   if(Scope==EScope::TheOneArrival)
   {FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Arrival-GenuineParkExit.png"));return Finish(true,TEXT(""));}
   double Best=FMath::Square(3000.);for(TActorIterator<AIslandLandDoor> It(W);It;++It)
   {const double D=FVector::DistSquared(It->GetActorLocation(),Path.Last());if(It->GetActorLocation().Z>30000 && D<Best){Best=D;Door=*It;}}
   if(!Door)return Finish(false,TEXT("Current authored physical mansion entrance missing"));
   const FVector Outside=Door->GetActorTransform().TransformPosition(FVector(65,-185,P->GetSimpleCollisionHalfHeight()+3));
   // The external arrival stair is an authored multi-level connection. Plan
   // the lower-floor approach separately: a single XY grid interpolated at
   // the upper door height cannot see its lower terrain-supported first tread.
   AActor* Entry=nullptr;for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.PublicEntry292")))Entry=*It;
   if(Entry)
   {
    FString Text;TSharedPtr<FJsonObject> Data;
    const FString EntryFolder=Entry->ActorHasTag(TEXT("NeonCity.EntryMetadata.MansionEntry302"))?TEXT("MansionEntry302"):TEXT("MansionEntry292");
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade")/EntryFolder/TEXT("generated.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return Finish(false,TEXT("Authored entry metadata unavailable"));
    const TArray<TSharedPtr<FJsonValue>>* Bottom=nullptr;const TArray<TSharedPtr<FJsonValue>>* Landing=nullptr;
    if(!Data->TryGetArrayField(TEXT("bottom_local_m"),Bottom) || !Data->TryGetArrayField(TEXT("landing_local_m"),Landing) || Bottom->Num()!=3 || Landing->Num()!=3)return Finish(false,TEXT("Authored entry waypoints invalid"));
    auto Local=[&](const TArray<TSharedPtr<FJsonValue>>& V){return FVector(V[0]->AsNumber()*100,-V[1]->AsNumber()*100,V[2]->AsNumber()*100);};
    StairTop=Entry->GetActorTransform().TransformPosition(FVector(Local(*Bottom).X+Data->GetNumberField(TEXT("step_count"))*Data->GetNumberField(TEXT("tread_depth_m"))*100+20,Local(*Bottom).Y,Local(*Landing).Z))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
    const FVector Approach=Entry->GetActorTransform().TransformPosition(Local(*Bottom)-FVector(80,0,0))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
    if(!PlanWalk(Approach))return Finish(false,TEXT("Native court-to-arrival-stair lower-floor route blocked"));return Next(13);
   }
   if(!PlanWalk(Outside))return Finish(false,TEXT("Native court-to-door floor/capsule walking route blocked"));return Next(8);
  }
  if(Stage==13 || Stage==14)
  {
   // Synchronous continuous GPU readback produced 400ms game ticks and made
   // the 25cm walking targets oscillate. Keep actual walking telemetry and
   // landmark stills; do not disturb native input/physics to obtain a movie.
   RecordWalk();
   if(Now-WalkProgress>15)return Finish(false,TEXT("Actual mapped W arrival-stair walking stalled"));
   if(WalkNode<WalkPath.Num())return false;
   Release();if(P->GetVelocity().Size2D()>60)return false;
   if(Stage==13){FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("CurrentCourt-%s-ArrivalStairBottom.png"),*Car->VehicleID.ToString()));WalkPath={StairTop};WalkNode=0;WalkProgress=Now;return Next(14);}
   if(!P->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(P->GetActorLocation().Z-StairTop.Z)>30)return Finish(false,TEXT("Actual walking did not climb to the supported public landing"));
   NativeArrivalStair=true;
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("CurrentCourt-%s-PublicLanding.png"),*Car->VehicleID.ToString()));
   if(!PlanWalk(Door->GetActorTransform().TransformPosition(FVector(65,-185,P->GetSimpleCollisionHalfHeight()+3))))return Finish(false,TEXT("Native arrival landing-to-door route blocked"));return Next(8);
  }
  if(Stage==8 || Stage==11)
  {
   RecordWalk();
   if(Now-WalkProgress>15)return Finish(false,TEXT("Actual mapped W walking stalled at the current entrance"));
   if(WalkNode<WalkPath.Num())return false;
   Release();if(P->GetVelocity().Size2D()>60)return false;
   if(Stage==11)
   {
    const FVector Local=Door->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
    if(Local.Y<80 || !P->GetCharacterMovement()->IsMovingOnGround())return Finish(false,TEXT("Normal walking did not cross the physical mansion doorway"));
    DoorCrossed=true;FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("CurrentCourt-%s-Interior.png"),*Car->VehicleID.ToString()));return Finish(true,TEXT(""));
   }
   PC->SetControlRotation((Door->GetActorTransform().TransformPosition(FVector(65,0,110))-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Door)return Finish(false,TEXT("Normal entrance eye trace/prompt did not focus the real door"));DoorFocused=true;
   if(Door->IsOpen())return Next(10);Key(EKeys::F,true);return Next(9);
  }
  if(Stage==9 && Age>.2){Key(EKeys::F,false);return Next(10);}
  if(Stage==10)
  {
   if(!Door->IsOpen())return Age>5?Finish(false,TEXT("Normal F did not open the physical mansion entrance")):false;DoorOpened=true;
   if(!PlanWalk(Door->GetActorTransform().TransformPosition(FVector(65,170,P->GetSimpleCollisionHalfHeight()+3))))return Finish(false,TEXT("Opened physical doorway has no full-capsule walking connection"));return Next(11);
  }
  return false;
 }
private:
 bool IsDown()const{return Scope==EScope::ManualDown || Scope==EScope::CruiseDown || Scope==EScope::LowerSeam;}
 bool IsTerminal()const{return Scope==EScope::TerminalEntry || Scope==EScope::TerminalSeam || Scope==EScope::ParkedExit;}
 bool IsCruise()const{return Scope==EScope::CruiseUp || Scope==EScope::CruiseDown;}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down)
 {bool& Previous=Keys.FindOrAdd(K);if(Previous!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Previous=Down;}}
 void Release(){for(auto& K:Keys)if(K.Value && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));K.Value=false;}}
 void Unhook(){if(FrameDriver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(FrameDriver);FrameDriver.Reset();}}
 bool Finish(bool Passed,const FString& Reason)
 {
  Unhook();Release();if(!Passed)Test->AddError(FString::Printf(TEXT("%s car%d: %s"),ScopeName(Scope),CarIndex,*Reason));
  if(!Directory.IsEmpty())
  {
   FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   FFileHelper::SaveStringToFile(PhysicsCSV,*(Directory/TEXT("native-wheel-contact.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   FFileHelper::SaveStringToFile(OverlapCSV,*(Directory/TEXT("native-root-overlaps.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   FFileHelper::SaveStringToFile(WalkCSV,*(Directory/TEXT("walking.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Reason);J->SetStringField(TEXT("mode"),ScopeName(Scope));
   J->SetStringField(TEXT("scope"),Scope==EScope::LowerSeam?TEXT("Minimum original Kestrel collision reproduction only:704 down to696. One initial road-supported car and settled capsule-supported player start. Actual mapped F/W/A/D/brake; no full bank or three-car claim."):Scope==EScope::TerminalSeam?TEXT("Minimum original Kestrel terminal reproduction only:840..847, current court parking/F exit/W door approach/F opening/W entry. No whole832..847 or three-car claim."):IsTerminal()?TEXT("Only current source832..847 and native court parking/F exit/W door approach/F opening/W entry. One initial supported car/player fixture. Cached lower road and full Dock route are not performed."):TEXT("Only current changed bank source696..724, declared direction and manual or C cruise mappings. One initial supported car/player fixture. No full Dock or whole mountain continuity claim."));
   J->SetStringField(TEXT("read_only_failed_shape_forecast"),FailureSurvey);
   if(Scope==EScope::ParkedExit)J->SetStringField(TEXT("scope"),TEXT("Fix-specific actual garage-ceiling F exit and W door entry only, three original cars in independent PIE worlds. One supported initial parked car/player fixture. No road route replay, no full continuous-driving claim, no post-T0 pose/velocity writes."));
   if(Scope==EScope::TheOneArrival)J->SetStringField(TEXT("scope"),FString::Printf(TEXT("Fix-specific actual current stations%d..638 into the physical new garage, braking and normal F/on-foot supported exit. Original Kestrel; one declared initial supported car/player fixture, mapped driving thereafter. No Dock/Village or complete route acceptance claim."),ArrivalFirst));
   if(Scope==EScope::TheOneRock)J->SetStringField(TEXT("scope"),FString::Printf(TEXT("Fix-specific original Kestrel stations%d..%d; mapped input and actual wheel contact/physics telemetry only. One declared supported initial fixture; no whole ascent or Dock/Village acceptance claim."),FocusFirst,FocusLast));
   if(Car)
   {
    if(!Passed && P)
    {
     FTransform ExitAt;J->SetBoolField(TEXT("read_only_native_dry_exit_available"),Car->GetVehicleInteraction()->GetPersistenceExitTransform(ExitAt));
     const FBox LocalBox=Car->CalculateComponentsBoundingBoxInLocalSpace(false);
     J->SetStringField(TEXT("read_only_exit_local_bounds"),LocalBox.ToString());
     const FVector Center=Car->GetActorTransform().TransformPosition(LocalBox.GetCenter());
     const FRotator Heading(0,Car->GetActorRotation().Yaw,0);const FVector Side=FRotationMatrix(Heading).GetUnitAxis(EAxis::Y),Back=-Heading.Vector();
     FCollisionQueryParams ExitQ(SCENE_QUERY_STAT(ChangedRoadExitDiagnosis),false,Car);ExitQ.AddIgnoredActor(P);
     const float Radius=P->GetSimpleCollisionRadius(),Half=P->GetSimpleCollisionHalfHeight();const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
     TArray<TSharedPtr<FJsonValue>> Probes;
     for(const FVector Direction:{Side,-Side,Back,(Back+Side).GetSafeNormal(),(Back-Side).GetSafeNormal()})
     {
      const float Extent=FVector::DotProduct(Car->GetActorTransform().InverseTransformVectorNoScale(Direction).GetAbs(),LocalBox.GetExtent());
      const FVector Candidate=Center+Direction*(Extent+Radius+55);FHitResult Floor;
      const bool HasFloor=W->LineTraceSingleByChannel(Floor,Candidate+FVector(0,0,300),Candidate-FVector(0,0,600),ECC_Visibility,ExitQ);
      auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("candidate"),Candidate.ToString());Row->SetBoolField(TEXT("floor_hit"),HasFloor);
      Row->SetStringField(TEXT("floor_component"),GetPathNameSafe(Floor.GetComponent()));Row->SetStringField(TEXT("floor_point"),Floor.ImpactPoint.ToString());
      Row->SetNumberField(TEXT("floor_normal_z"),Floor.ImpactNormal.Z);Row->SetNumberField(TEXT("floor_vs_body_height"),Floor.ImpactPoint.Z-Center.Z);
      if(HasFloor){const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);FVector OriginAtHeight=Car->GetActorLocation();OriginAtHeight.Z=Destination.Z;
       Row->SetBoolField(TEXT("full_capsule_overlap"),W->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Capsule,ExitQ));FHitResult Route;
       Row->SetBoolField(TEXT("full_capsule_route_blocked"),W->SweepSingleByChannel(Route,OriginAtHeight,Destination,FQuat::Identity,ECC_Pawn,Capsule,ExitQ));Row->SetStringField(TEXT("route_blocker"),GetPathNameSafe(Route.GetComponent()));}
      Probes.Add(MakeShared<FJsonValueObject>(Row));
     }
     J->SetArrayField(TEXT("read_only_native_exit_candidates"),Probes);
    }
    auto* M=Car->GetChaosMovement();J->SetNumberField(TEXT("actual_current_gear"),M->GetCurrentGear());J->SetNumberField(TEXT("actual_target_gear"),M->GetTargetGear());J->SetNumberField(TEXT("actual_engine_rpm"),M->GetEngineRotationSpeed());
    TArray<TSharedPtr<FJsonValue>> WheelRows;for(int32 I=0;I<M->Wheels.Num();++I){const auto& S=M->GetWheelState(I);auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("wheel"),I);Row->SetBoolField(TEXT("contact"),S.bInContact);Row->SetNumberField(TEXT("drive_torque"),S.DriveTorque);Row->SetNumberField(TEXT("brake_torque"),S.BrakeTorque);Row->SetNumberField(TEXT("slip"),S.SlipMagnitude);Row->SetNumberField(TEXT("spring_force"),S.SpringForce);Row->SetStringField(TEXT("contact_position"),S.ContactPoint.ToString());WheelRows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("actual_wheel_states"),WheelRows);
    if(!Passed)
    {
     // Observe actual rigid-body shapes before any floor/forecast rejection.
     // This never changes a body, geometry, collision, force or steering.
     TArray<TSharedPtr<FJsonValue>> Bodies;FCollisionQueryParams Q(SCENE_QUERY_STAT(ChangedRoadActualBodySnapshot),false,Car);
     for(const auto& Setup:Car->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
     {
      if(!Setup)continue;const auto* Body=Car->GetMesh()->GetBodyInstance(Setup->BoneName);if(!Body)continue;
      const FTransform Actual=Body->GetUnrealWorldTransform();auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("bone"),Setup->BoneName.ToString());B->SetNumberField(TEXT("collision_mode"),int32(Body->GetCollisionEnabled()));B->SetNumberField(TEXT("physics_type"),int32(Setup->PhysicsType));B->SetStringField(TEXT("actual_rigid_transform"),Actual.ToHumanReadableString());
      TArray<TSharedPtr<FJsonValue>> Hits;
      auto Observe=[&](FTransform Local,FCollisionShape Shape,const TCHAR* Kind)
      {
       const FTransform T=Local*Actual;TArray<FOverlapResult> Found;W->OverlapMultiByChannel(Found,T.GetLocation(),T.GetRotation(),ECC_Vehicle,Shape,Q);
       for(const auto& Hit:Found)if(Hit.bBlockingHit){auto H=MakeShared<FJsonObject>();H->SetStringField(TEXT("shape"),Kind);H->SetStringField(TEXT("component"),GetPathNameSafe(Hit.GetComponent()));H->SetStringField(TEXT("shape_transform"),T.ToHumanReadableString());Hits.Add(MakeShared<FJsonValueObject>(H));}
      };
      for(const auto& Box:Setup->AggGeom.BoxElems)Observe(FTransform(Box.Rotation,Box.Center),FCollisionShape::MakeBox(FVector(Box.X,Box.Y,Box.Z)*Actual.GetScale3D().GetAbs()*.5),TEXT("box"));
      for(const auto& S:Setup->AggGeom.SphereElems)Observe(FTransform(FQuat::Identity,S.Center),FCollisionShape::MakeSphere(S.Radius*Actual.GetScale3D().GetAbs().GetMax()),TEXT("sphere"));
      B->SetArrayField(TEXT("actual_blocking_overlaps"),Hits);Bodies.Add(MakeShared<FJsonValueObject>(B));
     }
     J->SetArrayField(TEXT("actual_native_body_overlap_records"),Bodies);
    }
   }
   J->SetNumberField(TEXT("initial_car_placements"),CarPlacements);J->SetNumberField(TEXT("initial_player_placements"),PlayerPlacements);J->SetNumberField(TEXT("post_t0_car_pose_or_velocity_writes"),0);J->SetNumberField(TEXT("post_t0_harness_player_pose_writes"),0);
   J->SetBoolField(TEXT("native_F_boarded"),NativeBoarded);J->SetBoolField(TEXT("native_F_exited"),NativeExited);J->SetBoolField(TEXT("native_door_prompt"),DoorFocused);J->SetBoolField(TEXT("physical_door_open"),DoorOpened);J->SetBoolField(TEXT("mapped_W_crossed_door"),DoorCrossed);
   J->SetBoolField(TEXT("mapped_W_climbed_external_arrival_stair"),NativeArrivalStair);
   J->SetStringField(TEXT("vehicle_guid"),Car?Car->VehicleID.ToString():TEXT(""));J->SetNumberField(TEXT("actual_travel_cm"),Travel);J->SetNumberField(TEXT("source_first"),Stations.IsEmpty()?-1:Stations[0]);J->SetNumberField(TEXT("source_last"),Stations.IsEmpty()?-1:Stations.Last());
   TArray<TSharedPtr<FJsonValue>> Seen;for(int32 I=Scope==EScope::TheOneRock?FocusFirst:Scope==EScope::TheOneArrival?ArrivalFirst:Scope==EScope::LowerSeam?696:704;I<=(Scope==EScope::TheOneRock?FocusLast:Scope==EScope::TheOneArrival?638:Scope==EScope::LowerSeam?704:716);++I)if(Visited.Contains(I))Seen.Add(MakeShared<FJsonValueNumber>(I));J->SetArrayField(TEXT("changed_stations_observed"),Seen);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));
  }
  return true;
 }
 bool BuildPath()
 {
  if(Scope==EScope::TheOneArrival || Scope==EScope::TheOneRock)
  {
   FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/TheOne472Arrival/receipt.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return false;
   const auto& Points=Data->GetArrayField(TEXT("points"));if(Points.Num()!=639)return false;
   for(const auto& Row:Points){const auto V=Row->AsArray();if(V.Num()!=3)return false;Source.Add(FVector(NeonCityCoastalLayout::IslandOrigin.X+100*V[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*V[1]->AsNumber(),-130+100*V[2]->AsNumber()+6));}
   for(int32 I=Scope==EScope::TheOneRock?FocusFirst:ArrivalFirst;I<(Scope==EScope::TheOneRock?FocusLast+1:Source.Num());++I)
   {
    const FVector Tangent=(Source[FMath::Min(I+1,638)]-Source[I-1]).GetSafeNormal2D();const float Offset=150*FMath::Clamp((FVector::Dist2D(Source[I],Source.Last())-300)/1500.f,0.f,1.f);
    Path.Add(Source[I]+FVector(-Tangent.Y,Tangent.X,0)*Offset);Stations.Add(I);
   }
   if(Scope==EScope::TheOneArrival)
   {
    AIslandGarageDoor* Garage=nullptr;for(TActorIterator<AIslandGarageDoor> It(W);It;++It){Garage=*It;break;}if(!Garage || FVector::Dist2D(Garage->GetActorLocation(),Source.Last())>10)return false;
    for(int32 I=1;I<=5;++I){Path.Add(Garage->GetActorTransform().TransformPosition(FVector(0,I*150,2)));Stations.Add(-1);}
   }
   Distance.Add(0);for(int32 I=1;I<Path.Num();++I)Distance.Add(Distance.Last()+FVector::Dist(Path[I-1],Path[I]));return true;
  }
  if(Scope==EScope::ParkedExit)
  {const FVector Parked=NeonCityCoastalLayout::FromLegacyIsland(FVector(47366.34,-119490.47,30486)),Direction=FRotator(0,-4,0).Vector();Path={Parked,Parked+Direction*50,Parked+Direction*100};Stations={-1,-1,-1};Distance={0,50,100};return true;}
  FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return false;
  const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;if(!Data->TryGetArrayField(TEXT("points"),Points) || Points->Num()!=848)return false;
  for(const auto& Row:*Points){const auto V=Row->AsArray();if(V.Num()!=3)return false;const FVector P0(NeonCityCoastalLayout::IslandOrigin.X+100*V[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*V[1]->AsNumber(),-130+100*V[2]->AsNumber()+6);if(P0.ContainsNaN())return false;Source.Add(P0);}
  const int32 Start=Scope==EScope::TerminalSeam?840:IsTerminal()?832:Scope==EScope::LowerSeam?704:IsDown()?724:696,End=IsTerminal()?847:IsDown()?696:724,Sign=IsDown()?-1:1;
  for(int32 I=Start;;I+=Sign)
  {
   const FVector T=(Source[FMath::Min(I+1,847)]-Source[FMath::Max(I-1,0)]).GetSafeNormal2D();
   const float Offset=IsTerminal()?150*FMath::Clamp((FVector::Dist2D(Source[I],Source.Last())-300)/1500.f,0.f,1.f):150;
   Path.Add(Source[I]+FVector(-T.Y,T.X,0)*Offset*Sign);Stations.Add(I);if(I==End)break;
  }
  if(IsTerminal())
  {
   USplineComponent* Court=nullptr;for(TActorIterator<ANeonCityTrafficRoute> It(W);It;++It)if(It->RoadID==TEXT("Island.Mansion.Forecourt"))Court=It->GetRoadSpline();
   if(!Court || Court->GetNumberOfSplinePoints()<3 || FVector::Dist(Path.Last(),Court->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::World))>30)return false;
   int32 Park=INDEX_NONE;double Best=FMath::Square(300.);for(int32 I=0;I<Court->GetNumberOfSplinePoints();++I){const double D=FVector::DistSquared(Court->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World),NeonCityCoastalLayout::FromLegacyIsland(FVector(47600,-119450,30486)));if(D<Best){Best=D;Park=I;}}
   if(Park<1)return false;
   for(int32 I=0;I<=Park;++I){const FVector AtCourt=Court->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World);if(!Path.Last().Equals(AtCourt,10)){Path.Add(AtCourt);Stations.Add(-1);}}
  }
  Distance.Add(0);for(int32 I=1;I<Path.Num();++I)Distance.Add(Distance.Last()+FVector::Distance(Path[I-1],Path[I]));return Path.Num()>2;
 }
 bool MeasureAxles()
 {
  auto* M=Car->GetChaosMovement();const auto* Rig=Car->GetMesh()->GetSkeletalMeshAsset();if(!Rig || M->Wheels.Num()!=4 || M->WheelSetups.Num()!=4)return false;
  const auto& Skeleton=Rig->GetRefSkeleton();FVector Rest[4];for(int32 I=0;I<4;++I)
  {const auto& Setup=M->WheelSetups[I];int32 Bone=Skeleton.FindBoneIndex(Setup.BoneName);if(!M->Wheels[I] || !Setup.WheelClass || Bone==INDEX_NONE)return false;FTransform Pose=Skeleton.GetRefBonePose()[Bone];while((Bone=Skeleton.GetParentIndex(Bone))!=INDEX_NONE)Pose*=Skeleton.GetRefBonePose()[Bone];Rest[I]=Pose.GetLocation()+Setup.AdditionalOffset+Setup.WheelClass.GetDefaultObject()->Offset;}
  RearAxleLocal=(Rest[2]+Rest[3])*.5;Wheelbase=FVector::Dist2D((Rest[0]+Rest[1])*.5,RearAxleLocal);return Wheelbase>180 && Wheelbase<400;
 }
 FVector Point(float D)const
 {int32 I=FMath::Clamp(Node,0,Path.Num()-2);while(I>0 && Distance[I]>D)--I;while(I<Path.Num()-2 && Distance[I+1]<D)++I;return FMath::Lerp(Path[I],Path[I+1],FMath::Clamp((D-Distance[I])/FMath::Max(Distance[I+1]-Distance[I],1.f),0.f,1.f));}
 void Progress(float DT)
 {
  const FVector Here=Car->GetActorLocation();float Best=BIG_NUMBER,D=Along;int32 Chosen=Node;FVector OnPath=Point(Along);
  for(int32 I=FMath::Max(0,Node-3);I<Path.Num()-1 && Distance[I]<=Along+1200;++I){const FVector AB=Path[I+1]-Path[I];const float A=FMath::Clamp(float(FVector::DotProduct(Here-Path[I],AB)/FMath::Max(AB.SizeSquared(),1.)),0.f,1.f);const FVector Q=Path[I]+AB*A;const float Error=FVector::DistSquared(Here,Q);if(Error<Best){Best=Error;Chosen=I;D=Distance[I]+AB.Size()*A;OnPath=Q;}}
  LaneError=FVector::Dist2D(Here,OnPath);SupportValid=LaneError<310 && Here.Z-OnPath.Z>-70 && Here.Z-OnPath.Z<180 && FMath::Abs(Car->GetActorRotation().Roll)<28;
  if(D>ProgressMarker+15){LastProgress=W->GetTimeSeconds();ProgressMarker=D;}Node=Chosen;Along=FMath::Max(Along-30,D);if(Stations.IsValidIndex(Node))Visited.Add(Stations[Node]);
  Travel+=FVector::Dist(Here,LastCar);LastCar=Here;auto* M=Car->GetChaosMovement();Contacts=0;for(int32 I=0;I<M->Wheels.Num();++I)Contacts+=M->GetWheelState(I).bInContact?1:0;AirborneSeconds=Contacts<2?AirborneSeconds+DT:0;
 }
 void Manual(float DesiredKmh)
 {
  // Same axle-measured mapped-key pulse driver as cached TripleSummit; no
  // steering/throttle component setter, transform or tire force injection.
  auto* M=Car->GetChaosMovement();const float Speed=FMath::Max(0.f,M->GetForwardSpeed());const FVector Rear=Car->GetMesh()->GetComponentTransform().TransformPosition(RearAxleLocal);
  const FVector V=Car->GetActorTransform().InverseTransformVectorNoScale(Point(Along+FMath::Clamp(420.f+Speed*.4f,450.f,850.f))-Rear);
  const float Scale=FMath::Max(.1f,M->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(Speed*.0223694f));const float Ratio=M->SteeringSetup.SteeringType==ESteeringType::AngleRatio?.5f*(1.f+M->SteeringSetup.AngleRatio):1.f;
  const float Lock=M->Wheels[0]->MaxSteerAngle*Scale*Ratio;if(Lock<=0){SupportValid=false;return;}
  const float Cross=Car->GetActorTransform().InverseTransformVectorNoScale(Point(Along)-Car->GetActorLocation()).Y;const float Correction=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Cross*.75f,Speed+300.f)),-8.f,8.f);
  RequestedSteering=FMath::Clamp((FMath::RadiansToDegrees(FMath::Atan2(2*Wheelbase*V.Y,FMath::Max(V.X*V.X+V.Y*V.Y,1.)))+Correction)/Lock,-1.f,1.f);
  const float DT=FMath::Clamp(W->GetTimeSeconds()-LastManualTime,0.f,.1f);LastManualTime=W->GetTimeSeconds();SteeringIntegral=FMath::Clamp(SteeringIntegral+(RequestedSteering-M->GetSteeringInput())*DT,-.35f,.35f);const float Command=FMath::Clamp(RequestedSteering+2*SteeringIntegral,-1.f,1.f);
  Key(EKeys::D,Command>M->GetSteeringInput()+.02f);Key(EKeys::A,Command<M->GetSteeringInput()-.02f);Key(EKeys::W,Speed*.036f<DesiredKmh-.6f);Key(EKeys::S,false);Key(EKeys::SpaceBar,Speed*.036f>DesiredKmh+1 || DesiredKmh<.2f);
 }
 bool PlanWalk(FVector Goal)
 {
  Release();WalkNode=0;WalkProgress=W->GetTimeSeconds();
  // Survey below the garage ceiling at the real player's reachable height.
  // A +350cm ray starts above the next storey and plans on its slab instead.
  // The public entry is one storey above the garage. Resolve the retained
  // 30cm treads rather than stepping over several at the old 100cm grid.
  if(!Phase205cSupportedWalkPath(W,P,Goal,WalkPath,P->GetSimpleCollisionHalfHeight()+25.f,25.f))return false;
  // Remove only mathematically collinear level-floor intermediate targets.
  // Their identical swept corridor was already validated cell by cell. Keeping
  // every25cm target needlessly made normal keyboard walking turn back after a
  // slow rendered frame; no corner, stair, floor or clearance is skipped here.
  FVector Previous=P->GetActorLocation();TArray<FVector> Reduced;
  for(int32 I=0;I<WalkPath.Num();++I)
  {
   if(I+1<WalkPath.Num() && FMath::Abs(Previous.Z-WalkPath[I].Z)<1 && FMath::Abs(WalkPath[I].Z-WalkPath[I+1].Z)<1)
   {const FVector A=(WalkPath[I]-Previous).GetSafeNormal2D(),B=(WalkPath[I+1]-WalkPath[I]).GetSafeNormal2D();if(FVector::DotProduct(A,B)>.99999)continue;}
   Reduced.Add(WalkPath[I]);Previous=WalkPath[I];
  }
  WalkPath=MoveTemp(Reduced);return !WalkPath.IsEmpty();
 }
 void Frame(UWorld* World,ELevelTick,float DT)
 {
  if(World!=W || !PC)return;
  if(Car && PC->GetPawn()==Car && (Stage==4 || Stage==5))
  {Progress(DT);if(Scope==EScope::TheOneRock)SamplePhysics();if(Stage==4 && !IsCruise()){const float Remaining=Distance.Last()-Along;Manual(FMath::Min(IsTerminal()?12.f:18.f,FMath::Sqrt(500.f*FMath::Max(0.f,Remaining-200.f))*.036f));}return;}
  if(P && PC->GetPawn()==P && (Stage==8 || Stage==11 || Stage==13 || Stage==14) && WalkPath.IsValidIndex(WalkNode))
  {const FVector Delta=WalkPath[WalkNode]-P->GetActorLocation();if(Delta.Size2D()<18){++WalkNode;WalkProgress=W->GetTimeSeconds();if(!WalkPath.IsValidIndex(WalkNode)){Key(EKeys::W,false);return;}}PC->SetControlRotation(FRotator(0,(WalkPath[WalkNode]-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);}
 }
 void RecordWalk()
 {
  if(W->GetTimeSeconds()-LastWalkSample<.1f)return;LastWalkSample=W->GetTimeSeconds();const FVector AtPlayer=P->GetActorLocation();
  WalkCSV+=FString::Printf(TEXT("%.4f,%d,%.3f,%.3f,%.3f,%d,%d\n"),LastWalkSample,Stage,AtPlayer.X,AtPlayer.Y,AtPlayer.Z,P->GetCharacterMovement()->IsMovingOnGround()?1:0,Keys.FindRef(EKeys::W));
 }
 void Sample()
 {
  const float Now=W->GetTimeSeconds();if(Now-LastSample<.1f)return;LastSample=Now;const FVector V=Car->GetActorLocation();const FRotator R=Car->GetActorRotation();auto* M=Car->GetChaosMovement();
  CSV+=FString::Printf(TEXT("%.4f,%d,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%d\n"),Now,Stage,*Car->VehicleID.ToString(),Stations[Node],V.X,V.Y,V.Z,R.Yaw,R.Pitch,R.Roll,Car->GetDrivingComponent()->GetSpeedKmh(),LaneError,Contacts,Car->GetDamageState()->GetConditionFraction(),Car->GetRoadCruise()->IsActive()?1:0,M->GetSteeringInput(),RequestedSteering,M->GetThrottleInput(),M->GetBrakeInput(),Keys.FindRef(EKeys::W),Keys.FindRef(EKeys::A),Keys.FindRef(EKeys::D));
 }
 void SamplePhysics()
 {
  if(!Car || !W || !Stations.IsValidIndex(Node))return;
  auto* M=Car->GetChaosMovement();FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneActualWheelContact),false,Car);Q.AddIgnoredActor(P);const FVector Velocity=Car->GetVelocity();
  const auto* Body=Car->GetMesh()->GetBodyInstance(TEXT("Root"));const auto* Asset=Car->GetMesh()->GetPhysicsAsset();
  if(Body && Asset)for(const auto& Setup:Asset->SkeletalBodySetups)if(Setup && Setup->BoneName==TEXT("Root"))
  {
   const FTransform Actual=Body->GetUnrealWorldTransform();
   for(const auto& Box:Setup->AggGeom.BoxElems)
   {
    const FTransform T=FTransform(Box.Rotation,Box.Center)*Actual;TArray<FOverlapResult> Hits;
    W->OverlapMultiByChannel(Hits,T.GetLocation(),T.GetRotation(),ECC_Vehicle,FCollisionShape::MakeBox(FVector(Box.X,Box.Y,Box.Z)*Actual.GetScale3D().GetAbs()*.5),Q);
    for(const auto& Hit:Hits)if(Hit.bBlockingHit)OverlapCSV+=FString::Printf(TEXT("%.5f,%d,%.3f,%.3f,%.3f,%s\n"),W->GetTimeSeconds(),Stations[Node],T.GetLocation().X,T.GetLocation().Y,T.GetLocation().Z,*GetPathNameSafe(Hit.GetComponent()));
   }
  }
  for(int32 I=0;I<M->Wheels.Num();++I)
  {
   const auto& S=M->GetWheelState(I);FHitResult Hit;const FVector C=S.ContactPoint;W->LineTraceSingleByChannel(Hit,C+FVector(0,0,60),C-FVector(0,0,160),ECC_Vehicle,Q);
   PhysicsCSV+=FString::Printf(TEXT("%.5f,%.6f,%.6f,%d,%.3f,%.3f,%.3f,%d,%d,%.4f,%.4f,%.3f,%.3f,%.3f,%s,%.6f,%.3f\n"),W->GetTimeSeconds(),W->GetDeltaSeconds(),float(FApp::GetDeltaTime()),Stations[Node],Velocity.X,Velocity.Y,Velocity.Z,I,S.bInContact?1:0,S.SpringForce,S.SlipMagnitude,C.X,C.Y,C.Z,*GetPathNameSafe(Hit.GetComponent()),Hit.ImpactNormal.Z,Hit.ImpactPoint.Z);
  }
 }
 FAutomationTestBase* Test;EScope Scope;int32 CarIndex=0,FocusFirst=400,FocusLast=435,ArrivalFirst=350,Stage=0,Node=0,WalkNode=0,Contacts=0,CarPlacements=0,PlayerPlacements=0;double Started=0;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;AIslandLandDoor* Door=nullptr;
 TArray<FVector> Source,Path,WalkPath;TArray<int32> Stations;TArray<float> Distance;TSet<int32> Visited;TMap<FKey,bool> Keys;FDelegateHandle FrameDriver;FNeonCityVisualEvidence Evidence;FString Directory,CSV,FailureSurvey,WalkCSV=TEXT("seconds,stage,x,y,z,grounded,key_W\n");FVector RearAxleLocal=FVector::ZeroVector,LastCar=FVector::ZeroVector,StairTop=FVector::ZeroVector;
 FString PhysicsCSV=TEXT("seconds,world_dt,app_dt,source_station,vx,vy,vz,wheel,contact,spring,slip,contact_x,contact_y,contact_z,native_trace_component,normal_z,native_surface_z\n");
 FString OverlapCSV=TEXT("seconds,source_station,body_x,body_y,body_z,blocking_component\n");
 float At=0,Along=0,ProgressMarker=0,LaneError=0,LastProgress=0,LastSample=0,InitialCondition=0,Wheelbase=280,SteeringIntegral=0,LastManualTime=0,RequestedSteering=0,AirborneSeconds=0,Travel=0,WalkProgress=0,LastWalkSample=0;bool SupportValid=true,NativeBoarded=false,NativeExited=false,DoorFocused=false,DoorOpened=false,DoorCrossed=false,NativeArrivalStair=false;
};
bool Enqueue(FAutomationTestBase* Test,EScope Scope)
{
 const int32 Cases=Scope==EScope::LowerSeam || Scope==EScope::TerminalSeam || Scope==EScope::TheOneArrival || Scope==EScope::TheOneRock?1:3;
 for(int32 I=0;I<Cases;++I)
 {ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FDrive(Test,Scope,I));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());}
 return true;
}
}
#define NEONCITY_CHANGED_DRIVE_TEST(Class,Name,Mode) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Class::RunTest(const FString&){return NeonCityChangedRoadDriving::Enqueue(this,NeonCityChangedRoadDriving::EScope::Mode);}
NEONCITY_CHANGED_DRIVE_TEST(FChangedBankManualUp,"NeonCity.Phase205c.Cruise.ChangedBank704716.ManualUp",ManualUp)
NEONCITY_CHANGED_DRIVE_TEST(FChangedBankManualDown,"NeonCity.Phase205c.Cruise.ChangedBank704716.ManualDown",ManualDown)
NEONCITY_CHANGED_DRIVE_TEST(FChangedBankCruiseUp,"NeonCity.Phase205c.Cruise.ChangedBank704716.CruiseUp",CruiseUp)
NEONCITY_CHANGED_DRIVE_TEST(FChangedBankCruiseDown,"NeonCity.Phase205c.Cruise.ChangedBank704716.CruiseDown",CruiseDown)
NEONCITY_CHANGED_DRIVE_TEST(FChangedTerminalEntry,"NeonCity.Phase205c.Cruise.CurrentTerminalThreeCar.Entry",TerminalEntry)
NEONCITY_CHANGED_DRIVE_TEST(FLowerSeamRepro272,"NeonCity.Phase205c.Cruise.Repro272.LowerSeam",LowerSeam)
NEONCITY_CHANGED_DRIVE_TEST(FTerminalSeamRepro267,"NeonCity.Phase205c.Cruise.Repro267.TerminalSeam",TerminalSeam)
NEONCITY_CHANGED_DRIVE_TEST(FParkedExitRepro277,"NeonCity.Phase205c.Cruise.Repro277.ParkedExit",ParkedExit)
NEONCITY_CHANGED_DRIVE_TEST(FTheOneAscent529,"NeonCity.Phase205cUpgrade.TheOneAscentMinimumPIE",TheOneArrival)
NEONCITY_CHANGED_DRIVE_TEST(FTheOneRock534,"NeonCity.Phase205cUpgrade.TheOneRock409MinimumPIE",TheOneRock)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneBend536,"NeonCity.Phase205cUpgrade.TheOneBend429MinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneBend536::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCityChangedRoadDriving::FDrive(this,NeonCityChangedRoadDriving::EScope::TheOneRock,0,424,450));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneGarageArrival550,"NeonCity.Phase205cUpgrade.TheOneGarageArrivalMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneGarageArrival550::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCityChangedRoadDriving::FDrive(this,NeonCityChangedRoadDriving::EScope::TheOneArrival,0,400,435,620));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#undef NEONCITY_CHANGED_DRIVE_TEST

// Read-only collision diagnosis at the captured real walking stall. The sole
// initial capsule placement is a diagnostic fixture, never traversal evidence.
class FEntryLipSurvey300 : public IAutomationLatentCommand
{
public:
 explicit FEntryLipSurvey300(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<3)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(46165,-118965,30961))))return false;
  if(!Placed){P->SetActorLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(46165.276,-118965.002,30961.235)),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Placed=W->GetTimeSeconds();return false;}
  if(W->GetTimeSeconds()-Placed<.5)return false;
  const auto* Move=P->GetCharacterMovement();const FVector Here=P->GetActorLocation();const float R=P->GetSimpleCollisionRadius(),H=P->GetSimpleCollisionHalfHeight();
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("Read-only native PIE collision diagnosis at captured299 walking stall; one initial player fixture; NO traversal PASS"));
  J->SetStringField(TEXT("actual_capsule_position"),Here.ToString());J->SetNumberField(TEXT("radius"),R);J->SetNumberField(TEXT("half_height"),H);J->SetNumberField(TEXT("max_step_height"),Move->MaxStepHeight);
  const auto Shape=FCollisionShape::MakeCapsule(R,H);FCollisionQueryParams Q(SCENE_QUERY_STAT(EntryLip300),false,P);TArray<TSharedPtr<FJsonValue>> Rows;
  auto Read=[&](const TCHAR* Name,FVector A,FVector B,bool Line)
  {FHitResult Hit;const bool Block=Line?W->LineTraceSingleByChannel(Hit,A,B,ECC_Pawn,Q):W->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Pawn,Shape,Q);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("probe"),Name);Row->SetStringField(TEXT("from"),A.ToString());Row->SetStringField(TEXT("to"),B.ToString());Row->SetBoolField(TEXT("blocking"),Block);Row->SetStringField(TEXT("component"),GetPathNameSafe(Hit.GetComponent()));Row->SetStringField(TEXT("impact"),Hit.ImpactPoint.ToString());Row->SetStringField(TEXT("normal"),Hit.ImpactNormal.ToString());Row->SetNumberField(TEXT("penetration"),Hit.PenetrationDepth);Row->SetBoolField(TEXT("walkable"),Move->IsWalkable(Hit));Row->SetBoolField(TEXT("can_step_up"),Move->CanStepUp(Hit));Rows.Add(MakeShared<FJsonValueObject>(Row));};
  Read(TEXT("forward100"),Here,Here+FVector(100,0,0),false);
  const FVector Up=Here+FVector(0,0,Move->MaxStepHeight-3);Read(TEXT("step_up"),Here,Up,false);Read(TEXT("raised_forward100"),Up,Up+FVector(100,0,0),false);
  for(int32 D:{0,32,64,96,128,160})Read(*FString::Printf(TEXT("floor_plus%d"),D),Here+FVector(D,0,100),Here+FVector(D,0,-250),true);
  J->SetArrayField(TEXT("native_probes"),Rows);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("EntryLipSurvey300");IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveStringToFile(Text,*(Folder/TEXT("receipt.json")));
  PC->SetControlRotation(FRotator(0,0,0));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("EntryLip300-ActualCollision.png"));Test->AddInfo(TEXT("Native collision diagnostic recorded; this case does not certify entry"));return true;
 }
private: FAutomationTestBase* Test;float Placed=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntryLip300,"NeonCity.Phase205c.EntryLipSurvey300PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEntryLip300::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FEntryLipSurvey300(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
