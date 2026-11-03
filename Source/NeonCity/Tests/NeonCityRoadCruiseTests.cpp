#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Navigation/NeonCityRoadGuide.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Adventure/IslandSupercar.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCruiseGraphTest,"NeonCity.Phase205c.Cruise.ThreeDimensionalRoadGraph",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCruiseGraphTest::RunTest(const FString&)
{
 FNeonCityRoadGuide G;FNeonCityGuideRoad R;R.Bidirectional=true;R.RoadID=TEXT("Main");R.Points={FVector(0,0,0),FVector(1000,0,0),FVector(2000,0,0)};G.AddRoad(R);
 R.RoadID=TEXT("Branch");R.Points={FVector(2000,0,0),FVector(2000,1000,150),FVector(2000,2000,300)};G.AddRoad(R);
 R.RoadID=TEXT("Upper");R.Points={FVector(0,30,650),FVector(1000,30,650),FVector(2000,30,650)};G.AddRoad(R);
 TArray<FVector> P;float Limit;bool Loop;FVector Goal(2000,1800,270);
 TestTrue(TEXT("Connected deterministic junction reaches actual ascending branch"),G.FindDriveRoute(FVector(100,0,20),FVector::ForwardVector,&Goal,P,Limit,Loop));
 TestTrue(TEXT("No direct chord across junction"),P.ContainsByPredicate([](const FVector& V){return V.Equals(FVector(2000,0,0),1);}));
 TestTrue(TEXT("Lower road stays separate from the overlapping upper hairpin"),P.ContainsByPredicate([](const FVector& V){return V.X<1500 && V.Z<100;}));
 TestTrue(TEXT("Bidirectional authored mountain road can descend"),G.FindDriveRoute(FVector(1800,0,20),-FVector::ForwardVector,nullptr,P,Limit,Loop));
 TestTrue(TEXT("Descending route ends at genuine road endpoint"),P.Last().Equals(FVector::ZeroVector,1));
 TestTrue(TEXT("Elevated lane can be selected only at its real elevation"),G.FindDriveRoute(FVector(100,30,665),FVector::ForwardVector,nullptr,P,Limit,Loop));
 for(const FVector& V:P)TestTrue(TEXT("Elevation-aware projection cannot jump to lower lane"),V.Z>600);
 Goal=FVector(2000,1800,1000);TestFalse(TEXT("Goal on a disconnected level is refused"),G.FindDriveRoute(FVector(100,0,20),FVector::ForwardVector,&Goal,P,Limit,Loop));
 Goal=FVector(10000,0,0);TestFalse(TEXT("Off-road destination cannot be replaced by a straight drive"),G.FindDriveRoute(FVector(100,0,20),FVector::ForwardVector,&Goal,P,Limit,Loop));
 TestFalse(TEXT("Wrong heading cannot enable cruise onto an opposing one-way lane"),[&](){FNeonCityRoadGuide One;R.Bidirectional=false;R.Points={FVector(0,0,0),FVector(2000,0,0)};One.AddRoad(R);return One.FindDriveRoute(FVector(1000,0,10),-FVector::ForwardVector,nullptr,P,Limit,Loop);}());
 FNeonCityRoadGuide Closed;R.RoadID=TEXT("Forecourt");R.Closed=true;R.Bidirectional=false;
 R.Points={FVector(0,0,0),FVector(1500,0,0),FVector(1500,1500,0),FVector(0,1500,0)};Closed.AddRoad(R);
 TestTrue(TEXT("Closed forecourt safely appends its first point and produces a drive route"),Closed.FindDriveRoute(FVector(300,0,20),FVector::ForwardVector,nullptr,P,Limit,Loop));
 TestTrue(TEXT("Closed forecourt returns to its projected start without array aliasing"),Loop && P.Num()>4 && P[0].Equals(P.Last(),1));
 FNeonCityRoadGuide Garage;R.Closed=false;R.Bidirectional=false;R.RoadID=TEXT("Approach");
 R.Points={FVector(0,0,0),FVector(2000,0,0)};Garage.AddRoad(R);
 R.RoadID=TEXT("Garage");R.Points={FVector(2000,0,0),FVector(2000,100,0),FVector(1500,100,0)};Garage.AddRoad(R);
 Goal=FVector(1500,100,0);
 TestTrue(TEXT("An exact reachable garage destination beats the shorter nearby approach projection"),Garage.FindDriveRoute(FVector(100,0,20),FVector::ForwardVector,&Goal,P,Limit,Loop));
 TestTrue(TEXT("Garage route reaches its actual authored destination"),!P.IsEmpty() && P.Last().Equals(Goal,1));
 TestTrue(TEXT("Garage route follows its connected entrance without cutting across the road"),P.ContainsByPredicate([](const FVector& V){return V.Equals(FVector(2000,0,0),1);}));
 // A straight physical road must remain drivable when its navigation lane
 // changes from a right-hand mountain lane to a centred private driveway.
 // The old route appended a150cm perpendicular step at their shared port.
 FNeonCityRoadGuide LaneJoin;FNeonCityGuideRoad Mountain;Mountain.RoadID=TEXT("Mountain");Mountain.LaneOffsetCm=150;Mountain.SpeedLimitKmh=70;
 for(int32 X=0;X<=3000;X+=200)Mountain.Points.Add(FVector(X,0,0));LaneJoin.AddRoad(Mountain);
 FNeonCityGuideRoad Driveway;Driveway.RoadID=TEXT("PrivateGarage");Driveway.NativeMeasured=true;Driveway.SpeedLimitKmh=15;
 for(int32 X=3000;X<=5000;X+=200)Driveway.Points.Add(FVector(X,0,0));LaneJoin.AddRoad(Driveway);
 Goal=FVector(5000,0,0);TArray<float> Limits;
 TestTrue(TEXT("Offset mountain lane reaches centred garage"),LaneJoin.FindDriveRoute(FVector(100,150,20),FVector::ForwardVector,&Goal,P,Limit,Loop,&Limits));
 TestEqual(TEXT("Lane transition preserves one speed policy per point"),Limits.Num(),P.Num());
 TestTrue(TEXT("Lane transition keeps the exact garage destination"),!P.IsEmpty() && P.Last().Equals(Goal,1));
 if(Limits.Num()==P.Num())for(int32 I=0;I<P.Num();++I)
  TestEqual(TEXT("Road join preserves each authored speed limit, using the lower limit at the shared port"),Limits[I],P[I].X<2999?70.f:15.f);
 for(int32 I=1;I<P.Num();++I)
 {
  TestTrue(TEXT("Straight-road join has no perpendicular lane step"),(P[I]-P[I-1]).GetSafeNormal2D().X>.95);
  TestTrue(TEXT("Blend remains inside original supported lane corridor"),P[I].Y>=-.01 && P[I].Y<=150.01 && FMath::Abs(P[I].Z)<.01);
 }
 FNeonCityRoadGuide OffsetCycle;LaneJoin.Reset();
 Mountain.Points.Reset();for(int32 X=0;X<=3000;X+=200)Mountain.Points.Add(FVector(X,0,0));OffsetCycle.AddRoad(Mountain);
 Driveway.RoadID=TEXT("ReturnRoad");Driveway.Points={FVector(3000,0,0),FVector(3000,3000,0),FVector(0,3000,0),FVector(0,0,0)};OffsetCycle.AddRoad(Driveway);
 TestTrue(TEXT("Different-offset road cycle remains available"),OffsetCycle.FindDriveRoute(FVector(100,150,20),FVector::ForwardVector,nullptr,P,Limit,Loop));
 TestTrue(TEXT("Different-offset cycle returns to the same tapered lane point"),Loop && P.Num()>4 && P[0].Equals(P.Last(),1));
 // Equal numeric offsets still point in different world directions at a
 // perpendicular junction. Their common port must not form a reverse chord.
 FNeonCityRoadGuide Corner;FNeonCityGuideRoad North;North.RoadID=TEXT("North");North.LaneOffsetCm=180;North.NativeMeasured=true;North.Bidirectional=true;
 for(int32 Y=0;Y<=3000;Y+=200)North.Points.Add(FVector(0,Y,0));Corner.AddRoad(North);
 FNeonCityGuideRoad West=North;West.RoadID=TEXT("West");West.Points.Reset();
 for(int32 X=0;X>=-3000;X-=200)West.Points.Add(FVector(X,3000,0));Corner.AddRoad(West);
 Goal=FVector(-3000,2820,0);
 TestTrue(TEXT("Equal-offset perpendicular roads produce a connected drive route"),Corner.FindDriveRoute(FVector(-180,100,20),FVector::RightVector,&Goal,P,Limit,Loop));
 const int32 Port=P.IndexOfByPredicate([](const FVector& V){return V.Equals(FVector(0,3000,0),1);});
 TestTrue(TEXT("Perpendicular join includes the shared supported port"),Port!=INDEX_NONE);
 for(int32 I=1;I<P.Num();++I)
 {
  const FVector A=P[I-1],B=P[I];
  if(Port!=INDEX_NONE && I<=Port)
   TestTrue(TEXT("Approach to equal-offset corner never reverses its heading"),B.Y>A.Y);
  if(Port!=INDEX_NONE && I>Port)
   TestTrue(TEXT("Exit from equal-offset corner never reverses into the incoming lane"),B.X<A.X);
  TestTrue(TEXT("Perpendicular blend stays on the original road surface"),FMath::Abs(B.Z)<.01);
 }
 return true;
}

namespace NeonCityCruiseTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
/** Minimum reproducible fixture: one initial placement per car, followed solely
 * by real mapped controls and Chaos. This is not Dock-to-mansion route acceptance. */
class FInputFixture : public IAutomationLatentCommand
{
public:
 explicit FInputFixture(FAutomationTestBase* T,bool OneKestrel=false,bool RemainingCars=false,bool OneNacre=false):Test(T),Evidence(TEXT("AllThreeCruiseMappedInput")),bOneKestrel(OneKestrel),bRemainingCars(RemainingCars),bOneNacre(OneNacre){}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool Finish()
 {
  if(PC)for(FKey K:{EKeys::W,EKeys::S,EKeys::A,EKeys::D,EKeys::C,EKeys::F,EKeys::SpaceBar})Key(PC,K,IE_Released);
  if(Car && Car->GetRoadCruise()->IsActive())Car->GetRoadCruise()->Cancel(TEXT("Fixture ended"));
  if(Obstacle)Obstacle->Destroy();return true;
 }
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>400){Test->AddError(FString::Printf(TEXT("Mapped cruise fixture timed out at car %d stage %d"),Index,Stage));return Finish();}
  if(!PC || !W->HasBegunPlay() || W->GetTimeSeconds()<3)return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)Cars.Add(*It);
   Cars.Sort([](const AIslandSupercar& A,const AIslandSupercar& B){return A.VehicleID.ToString()<B.VehicleID.ToString();});
   if(!Test->TestEqual(TEXT("Default group has the three distinct cars"),Cars.Num(),3))return Finish();
   if(bOneKestrel)Cars.RemoveAll([](const auto* C){return !C->bRoadster || C->bRetractable;});
   if(bRemainingCars)Cars.RemoveAll([](const auto* C){return C->bRoadster && !C->bRetractable;});
   if(bOneNacre)Cars.RemoveAll([](const auto* C){return !C->bRetractable;});
   for(TActorIterator<ANeonCityTrafficRoute> It(W);It;++It)if(It->RoadID==TEXT("Island.Mountain.Main"))Road=It->GetRoadSpline();
   if(!Test->TestNotNull(TEXT("Explicit real mountain road is authored"),Road))return Finish();
   Test->AddInfo(TEXT("One initial test placement per car; all subsequent route motion, braking and steering are native Chaos. No route teleport evidence claimed."));
   return Next(1);
  }
  if(Stage==1)
  {
   Car=Cars[Index];const float Distance=FMath::Min(9000.f,Road->GetSplineLength()*.1f);
   FTransform Pose=Road->GetTransformAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World,false);
   Pose.AddToTranslation(Pose.GetRotation().GetRightVector()*150+FVector(0,0,65));
   Car->SetActorTransform(Pose,false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   Player->SetActorLocation(Pose.GetLocation()-Pose.GetRotation().GetRightVector()*260+FVector(0,0,75),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(2);
  }
  if(Stage==2 && Age>1.2f)
  {
   if(!Test->TestTrue(TEXT("Normal native entry binds vehicle controls"),Car->GetVehicleInteraction()->Enter(Player)))return Finish();
   return Next(3);
  }
  auto* Cruise=Car?Car->GetRoadCruise():nullptr;if(Car)Evidence.Record(W,PC,Index*100+Stage);
  if(Car && Cruise && NowTelemetry+0.1f<W->GetTimeSeconds())
  {
   NowTelemetry=W->GetTimeSeconds();const FVector P=Car->GetActorLocation();auto* Movement=Car->GetChaosMovement();
   const FVector Rear=Movement->Wheels.Num()>=4?(Movement->Wheels[2]->Location+Movement->Wheels[3]->Location)*.5:P;
   UE_LOG(LogTemp,Display,TEXT("CruiseSample car%d stage%d t%.2f p%s yaw%.2f speed%.2f request%.2f safe%.2f lane%.2f steer%.3f chaosSteer%.3f rearLocal%s reason=%s"),Index,Stage,NowTelemetry,*P.ToString(),Car->GetActorRotation().Yaw,Car->GetDrivingComponent()->GetSpeedKmh(),Cruise->GetRequestedSpeedKmh(),Cruise->GetSafeSpeedKmh(),Cruise->GetLaneErrorCm(),Cruise->GetAppliedSteering(),Movement->GetSteeringInput(),*Car->GetActorTransform().InverseTransformPosition(Rear).ToString(),*Cruise->GetReason());
  }
  if(Cruise && Cruise->IsActive() && Cruise->GetReason().Contains(TEXT("Lane lost")))
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Cruise-FirstLaneLoss.png"));Test->AddError(TEXT("Actual cruise lost its lane; ending this minimal fixture immediately"));return Finish();
  }
  if(Stage==3 && Age>.4f){Key(PC,EKeys::C,IE_Pressed);return Next(4);}
  if(Stage==4 && Age>.2f)
  {
   Key(PC,EKeys::C,IE_Released);if(!Test->TestTrue(TEXT("Actual C mapping enables route steering"),Cruise->IsActive()))return Finish();
   Requested=Cruise->GetRequestedSpeedKmh();Heading=Car->GetActorRotation().Yaw;Origin=Car->GetActorLocation();Key(PC,EKeys::W,IE_Pressed);return Next(5);
  }
  if(Stage==5 && Age>1.2f)
  {
   Key(PC,EKeys::W,IE_Released);Test->TestTrue(TEXT("W adjusts target while route steering remains active"),Cruise->IsActive() && Cruise->GetRequestedSpeedKmh()>Requested+3);
   Requested=Cruise->GetRequestedSpeedKmh();return Next(6);
  }
  if(Stage==6 && Age>8)
  {
   Test->TestTrue(TEXT("W release retains cruise speed target"),Cruise->IsActive() && FMath::IsNearlyEqual(Requested,Cruise->GetRequestedSpeedKmh(),.2f));
   Test->TestTrue(TEXT("Vehicle physically follows an actual road bend"),FVector::Dist2D(Origin,Car->GetActorLocation())>1200 && FMath::Abs(FMath::FindDeltaAngleDegrees(Heading,Car->GetActorRotation().Yaw))>3);
   Test->TestTrue(TEXT("Bend stays inside authored driving lane"),Cruise->GetLaneErrorCm()<220);
   if(Test->HasAnyErrors())return Finish();
   Key(PC,EKeys::S,IE_Pressed);return Next(7);
  }
  if(Stage==7 && Cruise->GetRequestedSpeedKmh()<.2f)
  {Key(PC,EKeys::S,IE_Released);return Next(8);}
  if(Stage==8 && Age>3)
  {
   Test->TestTrue(TEXT("S stops and retains route without engaging reverse"),Cruise->IsActive() && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.2f && Car->GetChaosMovement()->GetTargetGear()>=0);
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Cruise-Car%d-Stopped.png"),Index));Key(PC,EKeys::W,IE_Pressed);return Next(9);
  }
  if(Stage==9 && Age>1.8f){Key(PC,EKeys::W,IE_Released);return Next(10);}
  if(Stage==10 && Age>3)
  {
   Test->TestTrue(TEXT("W resumes same route after an S stop"),Cruise->IsActive() && Car->GetDrivingComponent()->GetSpeedKmh()>4);
   // An ordinary visible blocking crate on the chosen lane is a minimal safety fixture.
   const auto& Route=Cruise->GetRoute();int32 Nearest=0;double Best=TNumericLimits<double>::Max();
   for(int32 I=0;I<Route.Num();++I){const double D=FVector::DistSquared(Route[I],Car->GetActorLocation());if(D<Best){Best=D;Nearest=I;}}
   float Distance=0;FVector Target=Route[Nearest];
   for(int32 I=Nearest+1;I<Route.Num();++I)
   {
    const float Length=FVector::Distance(Route[I-1],Route[I]);
    if(Distance+Length>=1500){Target=FMath::Lerp(Route[I-1],Route[I],(1500-Distance)/FMath::Max(Length,1.f));break;}
    Distance+=Length;
   }
   FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Obstacle=W->SpawnActor<AStaticMeshActor>(Target+FVector(0,0,55),Car->GetActorRotation(),Spawn);
   auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
   if(!Test->TestNotNull(TEXT("Visible safety obstacle asset exists"),Cube))return Finish();
   Obstacle->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Obstacle->GetStaticMeshComponent()->SetStaticMesh(Cube);Obstacle->SetActorScale3D(FVector(1,2.8,1.1));
   Obstacle->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));ObstaclePoint=Target;return Next(100);
  }
  if(Stage==100 && Age>2 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.1f)
  {
   Test->TestTrue(TEXT("Obstacle stops real Chaos car without disabling cruise"),Cruise->IsActive() && Cruise->GetReason().Contains(TEXT("Obstacle")));
   Test->TestTrue(TEXT("Obstacle is not crossed or teleported around"),FVector::Dist2D(Car->GetActorLocation(),ObstaclePoint)>320);
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Cruise-Car%d-ObstacleStop.png"),Index));
   Obstacle->Destroy();Obstacle=nullptr;return Next(101);
  }
  if(Stage==101 && Age>3 && Car->GetDrivingComponent()->GetSpeedKmh()>3)
  {
   Test->TestTrue(TEXT("Removing the fixture resumes the retained native route"),Cruise->IsActive());
   Key(PC,EKeys::A,IE_Pressed);return Next(11);
  }
  if(Stage==101 && Age>8)
  {Test->AddError(FString::Printf(TEXT("Retained route could not resume after fixture removal: %s"),*Cruise->GetReason()));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Cruise-BlockedResume.png"));return Finish();}
  if(Stage==11 && Age>.08f)
  {
   Test->TestFalse(TEXT("Actual A immediately cancels controller"),Cruise->IsActive());Key(PC,EKeys::A,IE_Released);Key(PC,EKeys::C,IE_Pressed);return Next(12);
  }
  if(Stage==12 && Age>.2f)
  {
   Key(PC,EKeys::C,IE_Released);if(!Test->TestTrue(TEXT("Explicit C reenables after manual takeover"),Cruise->IsActive()))return Finish();
   Key(PC,EKeys::D,IE_Pressed);return Next(13);
  }
  if(Stage==13 && Age>.08f)
  {
   Test->TestFalse(TEXT("Actual D immediately cancels controller"),Cruise->IsActive());Key(PC,EKeys::D,IE_Released);Key(PC,EKeys::SpaceBar,IE_Pressed);return Next(14);
  }
  if(Stage==14 && Age>2 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
  {
   Key(PC,EKeys::SpaceBar,IE_Released);
   if(!Test->TestTrue(TEXT("Car remains drivable and native exit works after takeover"),Car->GetVehicleInteraction()->Exit()))return Finish();
   Test->TestFalse(TEXT("Leaving driving seat does not reenable cruise"),Cruise->IsActive());
   if(++Index<Cars.Num())
   {
    // This isolated case deliberately ends on the lane. Remove its completed
    // fixture before the next car, which must still detect real parked cars.
    // Otherwise the second car correctly stops behind the first case's car.
    auto* Completed=Car;Car=nullptr;Test->TestTrue(TEXT("Completed prior-car fixture is removed from the shared lane"),Completed->Destroy());
    return Next(1);
   }
   return Finish();
  }
  return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;USplineComponent* Road=nullptr;
 TArray<AIslandSupercar*> Cars;AStaticMeshActor* Obstacle=nullptr;int32 Index=0,Stage=0;float At=0,Requested=0,Heading=0,NowTelemetry=0;FVector Origin,ObstaclePoint;double Start=0;bool bOneKestrel=false,bRemainingCars=false,bOneNacre=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCruiseInputTest,"NeonCity.Phase205c.Cruise.MappedInputFixture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCruiseInputTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityCruiseTests::FInputFixture(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCruiseKestrelTest,"NeonCity.Phase205c.Cruise.KestrelMappedInputFixture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCruiseKestrelTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityCruiseTests::FInputFixture(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCruiseRemainingTest,"NeonCity.Phase205c.Cruise.RemainingCarsMappedInputFixture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCruiseRemainingTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityCruiseTests::FInputFixture(this,false,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCruiseNacreTest,"NeonCity.Phase205c.Cruise.NacreMappedInputFixture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCruiseNacreTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityCruiseTests::FInputFixture(this,false,false,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
