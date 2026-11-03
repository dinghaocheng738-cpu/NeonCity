#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "EventManager.h"
#include "EventsData.h"
#include "Chaos/ExternalCollisionData.h"
#include "Misc/OutputDevice.h"
#include "World/NeonCityDayNightCycle.h"
namespace CruiseSpeed784
{
class FFlow:public IAutomationLatentCommand,public FOutputDevice
{
public:
 explicit FFlow(FAutomationTestBase* In,bool Approach=false):Test(In),bApproach(Approach){}
 ~FFlow(){if(bLogAttached && GLog)GLog->RemoveOutputDevice(this);}
 bool CanBeUsedOnAnyThread()const override{return true;}
 void Serialize(const TCHAR* Message,ELogVerbosity::Type,const FName&)override
 {if(FCString::Strstr(Message,TEXT("Non-Nanite Marking Job Queue overflow")))bShadowOverflow.Store(true);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>100)return End(TEXT("Fast cruise minimum timed out"));if(!W || !PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(bApproach?NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254)):NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-156000,1000))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return End(TEXT("Native player absent"));
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(!I->bRoadster && !I->bRetractable)Car=*I;
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Island.Mountain.Main"))Road=I->GetRoadSpline();
   if(!Car || !Road)return End(TEXT("Native Aster/main road absent"));
   FTransform Pose=bApproach?Road->GetTransformAtSplineInputKey(Road->FindInputKeyClosestToWorldLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(41000,-115900,26200))),ESplineCoordinateSpace::World,false):Road->GetTransformAtSplinePoint(110,ESplineCoordinateSpace::World,false);Pose.AddToTranslation(Pose.GetRotation().GetRightVector()*150+FVector(0,0,65));
   ApproachStart=Pose.GetLocation();
   if(bApproach)for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){I->SetCycleEnabled(false);I->SetTimeOfDay(12);}
   Car->SetActorTransform(Pose,false,nullptr,ETeleportType::TeleportPhysics);Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   Player->SetActorLocation(Pose.GetLocation()-Pose.GetRotation().GetRightVector()*260+FVector(0,0,75),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();InitialCondition=Car->GetDamageState()->GetConditionFraction();
   Scene=W->GetPhysicsScene();if(!Scene || !Scene->GetSolver() || !Scene->GetSolver()->GetEventManager())return End(TEXT("Native Chaos contact observer unavailable"));
   Scene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData,FFlow>(Chaos::EEventType::Collision,this,&FFlow::Contacts);Registered=true;return Next(1);
  }
  if(Stage==1 && Age>1.2f){PC->SetControlRotation((Car->GetActorLocation()-Player->GetPawnViewLocation()).Rotation());if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Native car prompt unavailable"));Key(EKeys::F,true);return Next(2);}
  if(Stage==2 && Age>.25f){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Native F car entry failed"));Key(EKeys::C,true);return Next(3);}
  if(Stage==3 && Age>.25f)
  {
   Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Actual C failed: ")+Car->GetRoadCruise()->GetReason());Requested=Car->GetRoadCruise()->GetRequestedSpeedKmh();
   if(bApproach){if(!GLog)return End(TEXT("Native overflow log observer absent"));GLog->AddOutputDevice(this);bLogAttached=true;for(const TCHAR* Name:{TEXT("r.Shadow.Virtual.NonNanite.NumPageAreaDiagSlots"),TEXT("r.Shadow.Virtual.NonNanite.LargeInstancePageAreaThreshold")})if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)){OldShadowPolicy.Add(Name,C->GetInt());C->Set(FCString::Strstr(Name,TEXT("NumPage"))?-1:128,ECVF_SetByConsole);}}
   if(!bApproach && Requested<55)return End(FString::Printf(TEXT("Cruise useful default still too slow: %.2fkm/h (required55+)"),Requested));return Next(4);
  }
  if(Stage>=4)
  {
   if(SideContacts>0)return End(TEXT("Actual Chaos chassis side contact: ")+FirstContact);
   const float Speed=Car->GetDrivingComponent()->GetSpeedKmh();Peak=FMath::Max(Peak,Speed);MaxLane=FMath::Max(MaxLane,Car->GetRoadCruise()->GetLaneErrorCm());
   if(Car->GetDamageState()->GetConditionFraction()<InitialCondition-.005f)return End(TEXT("Fast cruise made contact/damaged the native chassis"));
   if(bApproach)
   {
    const float Distance=FVector::Dist2D(ApproachStart,Car->GetActorLocation());
    if(Distance>2500 || Age>35)
    {if(Distance<1500)return End(TEXT("Local approach did not traverse the measured shadow-warning vicinity"));return End(bShadowOverflow.Load()?TEXT("Local native approach reproduced non-Nanite marking queue overflow"):TEXT(""));}
    return false;
   }
   if(Stage==4 && Speed>=55)
   {
    const auto& Route=Car->GetRoadCruise()->GetRoute();int32 Closest=0;double Best=TNumericLimits<double>::Max();
    for(int32 I=0;I<Route.Num();++I){const double D=FVector::DistSquared2D(Route[I],Car->GetActorLocation());if(D<Best){Best=D;Closest=I;}}
    double Distance=0;bool Found=false;
    for(int32 I=Closest+1;I<Route.Num();++I){const double Length=FVector::Distance(Route[I-1],Route[I]);if(Distance+Length>=9000){ObstaclePoint=FMath::Lerp(Route[I-1],Route[I],(9000-Distance)/Length);ObstacleForward=(Route[I]-Route[I-1]).GetSafeNormal2D();Found=true;break;}Distance+=Length;}
    if(!Found)return End(TEXT("Actual route lacks90m obstruction-test preview"));
    Obstacle=W->SpawnActor<AStaticMeshActor>();auto* C=Obstacle->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetCollisionProfileName(TEXT("BlockAll"));Obstacle->SetActorScale3D(FVector(.6,2.8,.35));Obstacle->SetActorLocationAndRotation(ObstaclePoint+FVector(0,0,17.5),ObstacleForward.Rotation());return Next(5);
   }
   if(Stage==4 && Age>20)return End(TEXT("Native cruise did not reach55km/h on measured broad road110.."));
   if(Stage==5)
   {
    Gap=FVector::DotProduct(ObstaclePoint-Car->GetActorLocation(),ObstacleForward);if(Gap<240)return End(TEXT("Fast cruise crossed safe front clearance of35cm obstacle"));
    if(Speed<1.2f && Car->GetRoadCruise()->GetReason().Contains(TEXT("Obstacle"))){Obstacle->Destroy();Obstacle=nullptr;return Next(6);}
    if(Age>20)return End(TEXT("Native fast cruise did not stop at visible low obstacle"));
   }
   if(Stage==6 && Age>3 && Speed>4){Key(EKeys::A,true);return Next(7);}
   if(Stage==7 && Age>.25f){Key(EKeys::A,false);if(Car->GetRoadCruise()->IsActive())return End(TEXT("Manual A takeover did not release cruise"));return End(TEXT(""));}
  }
  return false;
 }
private:
 void Contacts(const Chaos::FCollisionEventData& Data)
 {
  if(!Registered || !Scene || !Car || Stage<4)return;
  for(const auto& C:Data.CollisionData.AllCollisionsArray)
  {
   auto* A=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy1);auto* B=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy2);
   if(A!=Car->GetMesh() && B!=Car->GetMesh())continue;auto* Other=A==Car->GetMesh()?B:A;if(!Other)continue;
   ++ChassisContacts;const auto* Mesh=Cast<UStaticMeshComponent>(Other);const FString MeshName=Mesh && Mesh->GetStaticMesh()?Mesh->GetStaticMesh()->GetName():FString();
   bool Guard=MeshName.Contains(TEXT("Guard"));
   if(Road && MeshName.Contains(TEXT("AdventureRoad")))
   {
    const FVector Point(C.Location);const float K=Road->FindInputKeyClosestToWorldLocation(Point);const FVector OnRoad=Road->GetLocationAtSplineInputKey(K,ESplineCoordinateSpace::World);
    const FVector Right=Road->GetRightVectorAtSplineInputKey(K,ESplineCoordinateSpace::World);const float Lateral=FMath::Abs(FVector::DotProduct(Point-OnRoad,Right)),Height=Point.Z-OnRoad.Z;
    Guard|=Lateral>365 && Lateral<470 && Height>8 && Height<130;
   }
   // Floor support is expected. Count even gentle side rubbing; damage and
   // impulse thresholds cannot prove clearance from actual road hardware.
   if(Guard || FMath::Abs(FVector(C.Normal).Z)<.7)
   {
    ++SideContacts;if(FirstContact.IsEmpty())FirstContact=Other->GetPathName()+TEXT(" at ")+FVector(C.Location).ToString();
   }
  }
 }
 bool Next(int32 Value){Stage=Value;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(FString Error)
 {
  if(bLogAttached && GLog)GLog->RemoveOutputDevice(this);bLogAttached=false;
  for(const auto& P:OldShadowPolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*P.Key))C->Set(P.Value,ECVF_SetByConsole);OldShadowPolicy.Reset();
  const bool Observed=Registered;
  if(Registered && Scene && Scene->GetSolver() && Scene->GetSolver()->GetEventManager())Scene->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this);Registered=false;Scene=nullptr;
  for(FKey K:{EKeys::F,EKeys::C,EKeys::A})Key(K,false);if(Obstacle)Obstacle->Destroy();if(Car){if(Car->GetRoadCruise()->IsActive())Car->GetRoadCruise()->Cancel(TEXT("Local fast-cruise test finished"));Car->GetDrivingComponent()->SetBrakeInput(1);}
  if(PC && Car && PC->GetPawn()==Car)Car->GetVehicleInteraction()->Exit();
  if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("requested_kmh"),Requested);J->SetNumberField(TEXT("peak_kmh"),Peak);J->SetNumberField(TEXT("maximum_lane_error_cm"),MaxLane);J->SetNumberField(TEXT("obstacle_front_gap_cm"),Gap);J->SetNumberField(TEXT("initial_condition"),InitialCondition);J->SetNumberField(TEXT("final_condition"),Car?Car->GetDamageState()->GetConditionFraction():0);J->SetBoolField(TEXT("native_contact_observer_registered"),Observed);J->SetNumberField(TEXT("native_chassis_contacts"),ChassisContacts);J->SetNumberField(TEXT("native_chassis_side_contacts"),SideContacts);J->SetStringField(TEXT("first_native_side_contact"),FirstContact);J->SetBoolField(TEXT("vsm_marking_overflow"),bShadowOverflow.Load());J->SetStringField(TEXT("initial_approach_cm"),ApproachStart.ToString());J->SetNumberField(TEXT("approach_distance_cm"),Car?FVector::Dist2D(ApproachStart,Car->GetActorLocation()):0);J->SetStringField(TEXT("scope"),bApproach?TEXT("Local shadow-attribution fixture: one initial Aster/player setup at the actual mountain road nearest measured41000,-115900 approach; normal F/C driving and engine overflow observer. No postT0 pose writes, no player save writes, not whole-route or handling/performance acceptance."):TEXT("One declared initial supported Aster/player road110 fixture; actual F/C driving,55km/h straight,90m low obstacle, brake/recover/A takeover. No pose writes afterT0; full mountain-route final regression separate."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bApproach?TEXT("arrival-shadow899.json"):TEXT("fast-cruise784.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;USplineComponent* Road=nullptr;AStaticMeshActor* Obstacle=nullptr;
 FPhysScene_Chaos* Scene=nullptr;bool Registered=false;int32 ChassisContacts=0,SideContacts=0;FString FirstContact;
 bool bApproach=false,bLogAttached=false;TAtomic<bool> bShadowOverflow{false};TMap<FString,int32> OldShadowPolicy;FVector ApproachStart;
 FVector ObstaclePoint,ObstacleForward;double Started=0;float At=0,Requested=0,Peak=0,MaxLane=0,Gap=0,InitialCondition=0;int32 Stage=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCruiseSpeed784,"NeonCity.Phase205c.Estate.Standalone.CruiseSpeed784",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCruiseSpeed784::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Fast cruise fixture cannot modify player saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(CruiseSpeed784::FFlow(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrivalShadow899,"NeonCity.Phase205c.Estate.Standalone.ArrivalShadow899",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FArrivalShadow899::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation"))){AddError(TEXT("Isolated local native approach only"));return false;}ADD_LATENT_AUTOMATION_COMMAND(CruiseSpeed784::FFlow(this,true));return true;}
#endif
