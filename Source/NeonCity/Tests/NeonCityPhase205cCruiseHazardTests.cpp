#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Adventure/IslandGarageDoor.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "EventManager.h"
#include "EventsData.h"
#include "Chaos/ExternalCollisionData.h"
namespace UpperCruiseContact
{
class FLocal:public IAutomationLatentCommand
{
public:
 explicit FLocal(FAutomationTestBase* T,bool Entrance=false):Test(T),bEntrance(Entrance){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>75)return End(TEXT("Local cruise contact timed out"));
  if(Stage==0)
  {
   if(!P || W->GetTimeSeconds()<4)return false;auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(50000,-120000,29000))))return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bRoadster && !It->bRetractable)Car=*It;if(!Car)return End(TEXT("Real Kestrel required"));
   FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return End(TEXT("Exact native alignment required"));
   const auto& Points=Data->GetArrayField(TEXT("points"));auto Point=[&](int I){auto A=Points[I]->AsArray();return FVector(NeonCityCoastalLayout::IslandOrigin.X+A[0]->AsNumber()*100,NeonCityCoastalLayout::IslandOrigin.Y-A[1]->AsNumber()*100,A[2]->AsNumber()*100-124);};
   const int32 Station=bEntrance?843:804;const FVector Direction=Point(Station+1)-Point(Station-1);
   const FVector Heading=Direction.GetSafeNormal2D();const FVector Here=Point(Station)+FVector(-Heading.Y,Heading.X,0)*(bEntrance?0:150);
   Car->SetActorTransform(FTransform(Direction.Rotation(),Here+FVector(0,0,55)),false,nullptr,ETeleportType::TeleportPhysics);Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   P->SetActorLocation(Here-Heading.Rotation().RotateVector(FVector(0,260,0))+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();FNeonCityNavigationDestination Goal;Goal.OwnerID=TEXT("Test.UpperCruiseContact");Goal.DestinationID=TEXT("Island.Mansion.Garage");Goal.Label=FText::FromString(TEXT("Aurelia garage"));Goal.Location=NeonCityCoastalLayout::FromLegacyIsland(FVector(47600,-119450,30486));Goal.UseRoadRoute=true;Nav->SetDestination(Goal);return Next(1);
  }
  const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==1 && Age>1.2)
  {PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,25)-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Normal local F entry prompt inaccessible"));Key(EKeys::F,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Native F entry failed"));Initial=Car->GetActorLocation();Marker=Initial;LastProgress=W->GetTimeSeconds();Key(EKeys::C,true);return Next(3);}
  if(Stage==3 && Age>.2){Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(Car->GetRoadCruise()->GetReason());const auto& Route=Car->GetRoadCruise()->GetRoute();if(Route.IsEmpty())return End(TEXT("Native navigation produced no road route"));RouteEnd=Route.Last();if(bEntrance && FVector::Dist(RouteEnd,NeonCityCoastalLayout::FromLegacyIsland(FVector(47600,-119450,30486)))>20)return End(TEXT("Garage route still ends on a neighbouring approach instead of the authored parking aisle"));return Next(4);}
  if(Stage==4)
  {
   if(FVector::Dist2D(Car->GetActorLocation(),Marker)>20){Marker=Car->GetActorLocation();LastProgress=W->GetTimeSeconds();}
   if(W->GetTimeSeconds()-LastProgress>8)return End(FString::Printf(TEXT("Local actual cruise stopped at%s: %s"),*Car->GetActorLocation().ToString(),*Car->GetRoadCruise()->GetReason()));
   // The shared controller stops a car centre 280cm before a terminal point to
   // keep its nose inside supported road. Accept that real parking envelope,
   // while separately requiring the route itself to reach the exact bay.
   const FVector BayDelta=Car->GetActorLocation()-NeonCityCoastalLayout::FromLegacyIsland(FVector(47600,-119450,30486));
   const bool InBay=FMath::Abs(BayDelta.X)<350 && FMath::Abs(BayDelta.Y)<120 && FMath::Abs(BayDelta.Z)<80 && FMath::Abs(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,0))<12;
   if(bEntrance?InBay:FVector::Dist2D(Car->GetActorLocation(),Initial)>4700){Key(EKeys::S,true);Key(EKeys::SpaceBar,true);return Next(5);}
   if(Age>38)return End(TEXT("Local crest cruise did not traverse 47m"));
  }
  if(Stage==5 && Age>1 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1){if(Car->GetDamageState()->GetConditionFraction()<.97)return End(TEXT("Actual local drive damaged the chassis"));FNeonCityVisualEvidence::CaptureHUD(W,bEntrance?TEXT("Cruise-Garage-Entrance-Native.png"):TEXT("Cruise-Upper-Crest-Native.png"));return End(TEXT(""));}
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  for(auto K:{EKeys::F,EKeys::C,EKeys::S,EKeys::SpaceBar})Key(K,false);if(!Failure.IsEmpty())Test->AddError(Failure);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("position"),Car?Car->GetActorLocation().ToString():TEXT("missing"));J->SetNumberField(TEXT("condition"),Car?Car->GetDamageState()->GetConditionFraction():0);J->SetStringField(TEXT("scope"),TEXT("Smallest current crest reproduction: one setup at source804, real F/C/S/brake and actual shared navigation goal to physical garage. No post-T0 vehicle pose write or disabled obstruction checks. Only 47m local cruise/contact acceptance; full route remains separate."));
  if(bEntrance){J->SetStringField(TEXT("scope"),TEXT("Smallest garage-entry reproduction: one source843 setup; real F/C/S and the actual shared garage destination. No post-T0 placement or disabled contacts. Route must terminate within20cm of authored bay; real car must fit its supported parking envelope (longitudinal350cm, lateral120cm, height80cm, heading12deg), accounting for native280cm nose clearance, with at least97% condition. Not a full route pass."));J->SetStringField(TEXT("route_endpoint"),RouteEnd.ToString());}
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bEntrance?TEXT("GarageEntrance.json"):TEXT("UpperCruiseContact.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;FVector Initial,Marker,RouteEnd;int Stage=0;double Start=0;float At=0,LastProgress=0;bool bEntrance=false;FNeonCityVisualEvidence Evidence{TEXT("UpperCruiseContact")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpperCruiseContactTest,"NeonCity.Phase205c.Cruise.UpperHazardPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUpperCruiseContactTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(UpperCruiseContact::FLocal(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarageEntranceTest,"NeonCity.Phase205c.Cruise.GarageEntrancePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGarageEntranceTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(UpperCruiseContact::FLocal(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
namespace CurrentTheOneCruise
{
class FMinimum final:public IAutomationLatentCommand
{
public:
 FMinimum(FAutomationTestBase* In,bool InRevision,bool InLow=false):Test(In),Revision(InRevision),LowObstacle(InLow),Aster(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseAster"))){}
 ~FMinimum(){UnregisterContacts();}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>100)return End(TEXT("Current garage cruise finite timeout"));
  if(!PC || !W->HasBegunPlay() || W->GetTimeSeconds()<4)return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   // Match the failed continuous route's actual saved fleet/property state,
   // while keeping the car/player620 setup explicitly local and pre-T0.
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseCatch")))
   {
    auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
    if(!CatchRequested)
    {
     if(!Save || !Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->DoesSaveExist() || !Save->LoadGame())return End(TEXT("Genuine continuous-route catch checkpoint unavailable"));
     CatchRequested=true;return false;
    }
    if(Save->IsOperationInProgress())return false;
    if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Genuine catch checkpoint restore failed: ")+Save->GetLastDetail());
   }
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return false;
   USplineComponent* Main=nullptr;
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Island.Mountain.Main"))Main=I->GetRoadSpline();
   for(TActorIterator<AIslandGarageDoor> I(W);I;++I)Garage=*I;
   const FVector Village=Main?Main->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::World):FVector::ZeroVector;
   for(TActorIterator<AIslandSupercar> I(W);I;++I)
    if(I->bRoadster!=Aster && !I->bRetractable && (!Car || FVector::DistSquared2D(I->GetActorLocation(),Village)<FVector::DistSquared2D(Car->GetActorLocation(),Village)))Car=*I;
   Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();
   if(!Car || !Garage || !Main || !Nav || Main->GetNumberOfSplinePoints()!=639)return End(TEXT("Selected native car/garage/639 road/navigation absent"));
   Goal=Garage->GetActorTransform().TransformPosition(FVector(0,750,2));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseApproach")))SetupStation=595;
   const FVector A=Main->GetLocationAtSplinePoint(SetupStation,ESplineCoordinateSpace::World),B=Main->GetLocationAtSplinePoint(SetupStation+1,ESplineCoordinateSpace::World);const FRotator Heading=(B-A).Rotation();
   const FVector Here=A+Heading.RotateVector(FVector(0,150,55));Car->SetActorTransform(FTransform(Heading,Here),false,nullptr,ETeleportType::TeleportPhysics);Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   P->SetActorLocation(Here-Heading.RotateVector(FVector(0,260,0))+FVector(0,0,75),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   Destination.OwnerID=TEXT("Test.CurrentTheOneCruise");Destination.DestinationID=TEXT("Island.Mansion.Garage");Destination.Label=FText::FromString(TEXT("The One garage"));Destination.Location=Goal;Destination.UseRoadRoute=true;if(!Nav->SetDestination(Destination))return End(TEXT("Native destination refused"));
   Scene=W->GetPhysicsScene();if(!Scene || !Scene->GetSolver() || !Scene->GetSolver()->GetEventManager())return End(TEXT("Native chassis contact observer unavailable"));
   Scene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData,FMinimum>(Chaos::EEventType::Collision,this,&FMinimum::Contacts);Registered=true;return Next(1);
  }
  if(Stage==1 && Age>1.2f)
  {
   PC->SetControlRotation((Car->GetActorLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Actual F entry prompt inaccessible"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.2f){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Native F entry failed"));Key(EKeys::C,true);return Next(3);}
  if(Stage==3 && Age>.2f)
  {
   Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Actual C cruise refused: ")+Car->GetRoadCruise()->GetReason());const auto& R=Car->GetRoadCruise()->GetRoute();if(R.IsEmpty() || !R.Last().Equals(Goal,2))return End(TEXT("Route must reach actual garage aisle"));RouteEnd=R.Last();NativeRoute=R;Marker=Car->GetActorLocation();LastProgress=W->GetTimeSeconds();
   if(LowObstacle)
   {
    float Distance=0;FVector Position,Direction;bool Found=false;
    for(int32 I=1;I<R.Num();++I){const float Length=FVector::Distance(R[I-1],R[I]);if(Distance+Length>=800){Position=FMath::Lerp(R[I-1],R[I],(800-Distance)/Length);Direction=(R[I]-R[I-1]).GetSafeNormal2D();Found=true;break;}Distance+=Length;}
    if(!Found)return End(TEXT("Current route too short for bounded low-obstacle fixture"));
    FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentLowObstacle),false,Car);
    if(!W->LineTraceSingleByChannel(Floor,Position+FVector(0,0,100),Position-FVector(0,0,100),ECC_Visibility,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Low obstacle requires real road support"));
    ObstaclePosition=Floor.ImpactPoint;ObstacleDirection=Direction;Obstacle=W->SpawnActor<AStaticMeshActor>();Obstacle->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Obstacle->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Obstacle->SetActorLocationAndRotation(ObstaclePosition+FVector(0,0,17.5),Direction.Rotation());Obstacle->SetActorScale3D(FVector(.6,3.4,.35));Obstacle->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
   }
   return Next(4);
  }
  if(Stage==4)
  {
   if(SideContacts)return End(TEXT("Native garage chassis side contact: ")+FirstContact);
   auto* Cruise=Car->GetRoadCruise();const FVector Here=Car->GetActorLocation();
   if((Here-Marker).Size2D()>20){Marker=Here;LastProgress=W->GetTimeSeconds();}
   if(Car->GetDamageState()->GetConditionFraction()<.97f)return End(TEXT("Current cruise damaged the original car"));
   if(LowObstacle)
   {
    const float Gap=FVector::DotProduct(ObstaclePosition-Here,ObstacleDirection);
    if(Gap<215)return End(TEXT("Cruise failed to stop before low35cm road obstruction"));
    if(Age>3 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.2f && Cruise->GetReason().Contains(TEXT("Obstacle")))return Next(20);
    if(Age>15)return End(TEXT("Low road obstruction did not produce a stable automatic stop"));return false;
   }
   if(Revision && Age>3 && Car->GetDrivingComponent()->GetSpeedKmh()>3)
   {Destination.Location+=FVector(200,0,0);Nav->SetDestination(Destination);return Next(10);}
   if(!Cruise->IsActive())return End(TEXT("Cruise cancelled before arrival: ")+Cruise->GetReason());
   const FVector Local=Garage->GetActorTransform().InverseTransformPosition(Here);
   if(!Revision && Local.Y>340 && Local.Y<850 && FMath::Abs(Local.X)<120 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.2f)
   {Parked=Local;Key(EKeys::F,true);return Next(5);}
   if(W->GetTimeSeconds()-LastProgress>9)return End(FString::Printf(TEXT("Actual garage cruise stalled at%s: %s"),*Here.ToString(),*Cruise->GetReason()));
   if(Age>55)return End(TEXT("Measured garage connector not completed within55s"));return false;
  }
  if(Stage==5 && Age>.2f){Key(EKeys::F,false);if(PC->GetPawn()!=P)return End(TEXT("Normal F exit failed"));return Next(6);}
  if(Stage==6 && Age>1)
  {if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Native garage exit lost dry floor"));if(Car->GetRoadCruise()->IsActive())return End(TEXT("Cruise remained active after exit"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-CurrentGarage-Cruise-ParkExit.png"));return End(TEXT(""));}
  if(Stage==10 && Age>.5)
  {
   if(Car->GetRoadCruise()->IsActive())return End(TEXT("Changed same-ID destination kept stale cruise active"));
   if(Car->GetChaosMovement()->GetBrakeInput()<.5f)return End(TEXT("Destination revision did not retain safe braking after cancel"));return Next(11);
  }
  if(Stage==11 && Age>4)
  {if(FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())>1.2f)return End(TEXT("Native revision stop failed to settle"));return End(TEXT(""));}
  if(Stage==20 && Age>1)
  {
   if(FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())>1.2f || Car->GetChaosMovement()->GetBrakeInput()<.5f)return End(TEXT("Low obstruction stop was not held"));
   Obstacle->Destroy();Obstacle=nullptr;Key(EKeys::A,true);return Next(21);
  }
  if(Stage==21 && Age>.2f)
  {Key(EKeys::A,false);if(Car->GetRoadCruise()->IsActive())return End(TEXT("Normal A steering failed to take over cruise"));Key(EKeys::C,true);return Next(22);}
  if(Stage==22 && Age>.2f)
  {Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Explicit C failed to resume the current valid lane"));Key(EKeys::LeftShift,true);Key(EKeys::S,true);return Next(23);}
  if(Stage==23 && Age>.2f)
  {Key(EKeys::S,false);Key(EKeys::LeftShift,false);if(Car->GetRoadCruise()->IsActive() || !Car->GetRoadCruise()->GetReason().Contains(TEXT("reverse")))return End(TEXT("Normal Shift+S failed manual reverse takeover"));return End(TEXT(""));}
  return false;
 }
private:
 void UnregisterContacts()
 {
  if(Registered && Scene && Scene->GetSolver() && Scene->GetSolver()->GetEventManager())Scene->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this);
  Registered=false;Scene=nullptr;
 }
 void Contacts(const Chaos::FCollisionEventData& Data)
 {
  if(!Registered || !Scene || !Car || Stage<4)return;
  for(const auto& C:Data.CollisionData.AllCollisionsArray)
  {
   auto* A=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy1);auto* B=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy2);
   if(A!=Car->GetMesh() && B!=Car->GetMesh())continue;auto* Other=A==Car->GetMesh()?B:A;if(!Other)continue;
   if(FMath::Abs(FVector(C.Normal).Z)<.7){++SideContacts;if(FirstContact.IsEmpty())FirstContact=Other->GetPathName()+TEXT(" at ")+FVector(C.Location).ToString();}
  }
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  UnregisterContacts();
  FString RunLabel;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),RunLabel);
  const FString RouteDir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(RunLabel);IFileManager::Get().MakeDirectory(*RouteDir,true);
  FString RouteCSV=TEXT("station,x,y,z\n");for(int32 I=0;I<NativeRoute.Num();++I)RouteCSV+=FString::Printf(TEXT("%d,%.6f,%.6f,%.6f\n"),I,NativeRoute[I].X,NativeRoute[I].Y,NativeRoute[I].Z);
  FFileHelper::SaveStringToFile(RouteCSV,*(RouteDir/TEXT("CurrentGarageNativeRoute.csv")));
  for(FKey K:{EKeys::F,EKeys::C,EKeys::A,EKeys::S,EKeys::LeftShift})Key(K,false);if(Obstacle){Obstacle->Destroy();Obstacle=nullptr;}if(!Failure.IsEmpty())Test->AddError(Failure);
  if(LowObstacle)
  {
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("obstacle_floor"),ObstaclePosition.ToString());J->SetStringField(TEXT("position"),Car?Car->GetActorLocation().ToString():TEXT("absent"));J->SetNumberField(TEXT("car_pose_writes_after_setup"),0);J->SetStringField(TEXT("scope"),TEXT("Current639 road, one initial620 car/player fixture and35cm blocking road obstacle. Normal F/C, real Chaos braking/held stop then A and Shift+S takeover. No gameplay pose/velocity writes after setup."));FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("CurrentCruiseLowObstacle.json")));return true;
  }
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetBoolField(TEXT("revision_only"),Revision);J->SetStringField(TEXT("input_checkpoint"),CatchRequested?TEXT("NeonCity_Test_LandFinal_BoatCatch"):TEXT("NONE"));J->SetStringField(TEXT("car"),Aster?TEXT("Aster"):TEXT("Kestrel"));J->SetStringField(TEXT("vehicle_guid"),Car?Car->VehicleID.ToString():TEXT("absent"));J->SetStringField(TEXT("vehicle_actor"),Car?Car->GetPathName():TEXT("absent"));J->SetNumberField(TEXT("setup_station"),SetupStation);J->SetNumberField(TEXT("native_chassis_side_contacts"),SideContacts);J->SetStringField(TEXT("first_side_contact"),FirstContact);J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("position"),Car?Car->GetActorLocation().ToString():TEXT("absent"));J->SetStringField(TEXT("goal"),Goal.ToString());J->SetStringField(TEXT("route_endpoint"),RouteEnd.ToString());J->SetStringField(TEXT("parked_door_local_cm"),Parked.ToString());J->SetNumberField(TEXT("car_pose_writes_after_setup"),0);J->SetStringField(TEXT("scope"),TEXT("One declared initial supported fixture at setup_station; normal F/C input and real Chaos thereafter. Exact native garage destination or same-ID revision safety stop only; full Dock route remains separate."));FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(Revision?TEXT("CurrentDriveRevision.json"):TEXT("CurrentGarageCruise.json"))));return true;
 }
 FAutomationTestBase* Test;bool Revision,LowObstacle,Aster;FPhysScene_Chaos* Scene=nullptr;bool Registered=false,CatchRequested=false;int32 SideContacts=0,SetupStation=620;TArray<FVector> NativeRoute;FString FirstContact;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;AIslandGarageDoor* Garage=nullptr;UNeonCityNavigationComponent* Nav=nullptr;AStaticMeshActor* Obstacle=nullptr;FNeonCityNavigationDestination Destination;FVector Goal,RouteEnd,Marker,Parked,ObstaclePosition,ObstacleDirection;double Started=0;int32 Stage=0;float At=0,LastProgress=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentGarageCruise,"NeonCity.Phase205c.Cruise.CurrentGarageMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentGarageCruise::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentTheOneCruise::FMinimum(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentDriveRevision,"NeonCity.Phase205c.Cruise.CurrentRevisionMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentDriveRevision::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentTheOneCruise::FMinimum(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentLowCruise,"NeonCity.Phase205c.Cruise.CurrentLowObstacleMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentLowCruise::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentTheOneCruise::FMinimum(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
