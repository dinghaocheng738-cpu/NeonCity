#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Vehicles/VehicleData.h"
#include "Components/StaticMeshComponent.h"
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
namespace ParkingDrive798
{
class FFlow:public IAutomationLatentCommand
{
public:explicit FFlow(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>240)return End(TEXT("Native parking drive timed out"));if(!PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  if(Car && Stage>=4 && (SideContacts || Car->GetDamageState()->GetConditionFraction()<InitialCondition-.005f || FMath::Abs(Car->GetActorRotation().Roll)>25))return End(TEXT("Actual parking drive/braking/exit contact or unstable support: ")+FirstContact);
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-153500,700))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return End(TEXT("Native player absent"));
   for(TActorIterator<AIslandSupercar> I(W);I;++I){const FVector P=I->GetActorLocation();if(P.X>54000 && P.X<63000 && P.Y>NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-155000,0)).Y && P.Y<NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-151000,0)).Y)Cars.Add(*I);}
   Cars.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});if(Cars.Num()!=3)return End(TEXT("Three actual authored village cars required; no car fixture is created"));
   if(!FParse::Value(FCommandLine::Get(),TEXT("NeonCityParkingBayIndex="),Bay) || !Cars.IsValidIndex(Bay))return End(TEXT("Select one independent bay0..2; prior test cars must not become obstacles for another bay"));
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Island.Mountain.Main"))Main=I->GetRoadSpline();
   Scene=W->GetPhysicsScene();if(!Main || !Scene || !Scene->GetSolver() || !Scene->GetSolver()->GetEventManager())return End(TEXT("Native main road/Chaos contact observer absent"));
   Scene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData,FFlow>(Chaos::EEventType::Collision,this,&FFlow::Contacts);Registered=true;return Next(1);
  }
  if(Stage==1)
  {
   Car=Cars[Bay];BaySpline=nullptr;for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID.ToString()==TEXT("Island.Village.Parking.")+Car->GetName())BaySpline=I->GetRoadSpline();if(!BaySpline)return End(TEXT("Authored bay exit spline absent"));FVector Stand;if(!Phase205cVehicleApproach(W,Player,Car,Player->GetActorLocation(),Stand))return End(TEXT("No physical door approach for ")+Car->GetName());
   // Independent local bay fixtures relocate only the unoccupied player.
   // Authored cars are never moved, reset, rotated or given test velocities.
   Player->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   StartPosition=LastPosition=Car->GetActorLocation();Distance=0;InitialCondition=Car->GetDamageState()->GetConditionFraction();SideContacts=0;FirstContact.Reset();LastProgress=Now;Steer=0;MergingMain=false;
   FVector Rest[4];const auto* Rig=Car->GetMesh()->GetSkeletalMeshAsset();auto* Movement=Car->GetChaosMovement();if(!Rig || Movement->WheelSetups.Num()!=4)return End(TEXT("Native four-wheel geometry absent"));const auto& Skeleton=Rig->GetRefSkeleton();
   for(int32 I=0;I<4;++I){const auto& Setup=Movement->WheelSetups[I];int32 Bone=Skeleton.FindBoneIndex(Setup.BoneName);if(Bone==INDEX_NONE || !Setup.WheelClass)return End(TEXT("Native wheel geometry invalid"));FTransform Pose=Skeleton.GetRefBonePose()[Bone];while((Bone=Skeleton.GetParentIndex(Bone))!=INDEX_NONE)Pose*=Skeleton.GetRefBonePose()[Bone];Rest[I]=Pose.GetLocation()+Setup.AdditionalOffset+Setup.WheelClass.GetDefaultObject()->Offset;}
   RearAxle=(Rest[2]+Rest[3])*.5;Wheelbase=FVector::Dist2D((Rest[0]+Rest[1])*.5,RearAxle);
   auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return End(TEXT("Native navigation component absent"));
   const float Key=Main->FindInputKeyClosestToWorldLocation(StartPosition),Along=Main->GetDistanceAlongSplineAtSplineInputKey(Key);
   FNeonCityNavigationDestination Goal;Goal.OwnerID=TEXT("Test.Parking798");Goal.DestinationID=TEXT("Island.Main.ParkingExit798");Goal.Label=FText::FromString(TEXT("Village road"));Goal.UseRoadRoute=true;Goal.Location=Main->GetLocationAtDistanceAlongSpline(Along+6000,ESplineCoordinateSpace::World)+Main->GetRightVectorAtDistanceAlongSpline(Along+6000,ESplineCoordinateSpace::World)*150;
   if(!Nav->SetDestination(Goal))return End(TEXT("Native parking-to-main destination refused"));return Next(2);
  }
  if(Stage==2 && Age>1)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Normal supported parking prompt absent: ")+Car->GetName());
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Parking798-%d.png"),Bay));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3 && Age>.25f){Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Normal F entry failed"));return Next(9);}
  if(Stage==9)
  {
   const FVector Here=Car->GetActorLocation();Distance+=FVector::Dist2D(Here,LastPosition);LastPosition=Here;const float K=Main->FindInputKeyClosestToWorldLocation(Here),Along=Main->GetDistanceAlongSplineAtSplineInputKey(K);const FVector On=Main->GetLocationAtSplineInputKey(K,ESplineCoordinateSpace::World),Direction=Main->GetDirectionAtSplineInputKey(K,ESplineCoordinateSpace::World);
   if(SideContacts || Car->GetDamageState()->GetConditionFraction()<InitialCondition-.005f)return End(TEXT("Manual parking pull-out contacted geometry: ")+FirstContact);
   if(FVector::Dist2D(Here,On)<200 && FVector::DotProduct(Car->GetActorForwardVector(),Direction)>.8){Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Key(EKeys::C,true);return Next(4);}
   if(Age>30)return End(FString::Printf(TEXT("Normal manual parking merge failed: %s location%s yaw%.2f"),*Car->GetName(),*Here.ToString(),Car->GetActorRotation().Yaw));
   auto* Movement=Car->GetChaosMovement();const float Speed=Movement->GetForwardSpeed();const float BayKey=BaySpline->FindInputKeyClosestToWorldLocation(Here),BayAlong=BaySpline->GetDistanceAlongSplineAtSplineInputKey(BayKey);if(FVector::Dist2D(Here,On)<500)MergingMain=true;const FVector Target=!MergingMain?BaySpline->GetLocationAtDistanceAlongSpline(FMath::Min(BaySpline->GetSplineLength(),BayAlong+240),ESplineCoordinateSpace::World):Main->GetLocationAtDistanceAlongSpline(Along+450,ESplineCoordinateSpace::World)+Main->GetRightVectorAtDistanceAlongSpline(Along+450,ESplineCoordinateSpace::World)*150;const FVector Local=Car->GetActorTransform().InverseTransformVectorNoScale(Target-Car->GetMesh()->GetComponentTransform().TransformPosition(RearAxle));const float Angle=FMath::RadiansToDegrees(FMath::Atan2(2.f*Wheelbase*Local.Y,FMath::Max(1.,Local.SizeSquared2D())));const float Curve=Movement->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(FMath::Abs(Speed)*.0223694f,1.f);const float AxleRatio=Movement->SteeringSetup.SteeringType==ESteeringType::AngleRatio?.5f*(1+Movement->SteeringSetup.AngleRatio):1;
   if(Now-LastSample>.5f){LastSample=Now;int32 Grounded=0;for(int32 I=0;I<Movement->Wheels.Num();++I)Grounded+=Movement->GetWheelState(I).bInContact?1:0;Telemetry+=FString::Printf(TEXT("%s,%.3f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.3f,%.3f,%d\n"),*Car->GetName(),Now,Stage,Here.X,Here.Y,Here.Z,Car->GetActorRotation().Yaw,Speed,FVector::Dist2D(Here,On),Movement->GetCurrentGear(),Movement->GetThrottleInput(),Movement->GetBrakeInput(),Grounded);}
   Steer=FMath::FInterpConstantTo(Steer,FMath::Clamp(Angle/FMath::Max(2.f,Movement->Wheels[0]->MaxSteerAngle*Curve*AxleRatio),-1.f,1.f),FMath::Clamp(W->GetDeltaSeconds(),.005f,.1f),2.6f);const float Error=260-Speed,Brake=Error<-25?FMath::Clamp((-Error-25)/180,0.f,.85f):0;auto* Drive=Car->GetDrivingComponent();Drive->SetDriveInput(Brake>0?0:FMath::Clamp(Error/230,0.f,.6f));Drive->SetSteerInput(Steer);Drive->SetBrakeInput(Brake);return false;
  }
  if(Stage==4 && Age>.25f){Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Native parking cruise refused: ")+Car->GetRoadCruise()->GetReason());return Next(5);}
  if(Stage==5)
  {
   const FVector Here=Car->GetActorLocation();const float Step=FVector::Dist2D(Here,LastPosition);Distance+=Step;LastPosition=Here;if(Step>.5)LastProgress=Now;
   const float K=Main->FindInputKeyClosestToWorldLocation(Here);MainGap=FVector::Dist2D(Here,Main->GetLocationAtSplineInputKey(K,ESplineCoordinateSpace::World));
   if(SideContacts || Car->GetDamageState()->GetConditionFraction()<InitialCondition-.005f || FMath::Abs(Car->GetActorRotation().Roll)>25)return End(TEXT("Native parking drive made contact or lost stable support: ")+FirstContact);
   if(Distance>1800 && MainGap<220){Car->GetRoadCruise()->Cancel(TEXT("Local bay test stopping on main"));Car->GetDrivingComponent()->SetBrakeInput(1);return Next(6);}
   if(Now-LastProgress>12 || Age>65)return End(FString::Printf(TEXT("Parking pull-out stalled: %s distance%.1f mainGap%.1f reason%s"),*Car->GetName(),Distance,MainGap,*Car->GetRoadCruise()->GetReason()));
  }
  if(Stage==6 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<.8f){Key(EKeys::F,true);return Next(7);}
  if(Stage==7 && Age>.3f)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Player || Car->GetVehicleInteraction()->GetDriver())return End(TEXT("Normal F exit failed after actual parking pull-out"));return Next(8);
  }
  if(Stage==8 && Age>.5f)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Parking exit player has no supported floor"));
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("car"),Car->GetName());J->SetNumberField(TEXT("actual_travel_cm"),Distance);J->SetNumberField(TEXT("main_gap_cm"),MainGap);J->SetNumberField(TEXT("initial_condition"),InitialCondition);J->SetNumberField(TEXT("final_condition"),Car->GetDamageState()->GetConditionFraction());J->SetNumberField(TEXT("native_side_contacts"),SideContacts);J->SetBoolField(TEXT("supported_native_exit"),true);Results.Add(MakeShared<FJsonValueObject>(J));
   return End(TEXT(""));
  }
  return false;
 }
private:
 void Contacts(const Chaos::FCollisionEventData& Data)
 {
  if(!Registered || !Car || !((Stage>=4 && Stage<=7) || Stage==9))return;
  for(const auto& C:Data.CollisionData.AllCollisionsArray)
  {
   auto* A=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy1);auto* B=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy2);if(A!=Car->GetMesh() && B!=Car->GetMesh())continue;auto* Other=A==Car->GetMesh()?B:A;if(!Other)continue;
   const auto* M=Cast<UStaticMeshComponent>(Other);const FString Name=M && M->GetStaticMesh()?M->GetStaticMesh()->GetName():FString();bool Guard=Name.Contains(TEXT("Guard"));
   if(Name.Contains(TEXT("AdventureRoad"))){const FVector P(C.Location);const float K=Main->FindInputKeyClosestToWorldLocation(P);const FVector On=Main->GetLocationAtSplineInputKey(K,ESplineCoordinateSpace::World);const float L=FMath::Abs(FVector::DotProduct(P-On,Main->GetRightVectorAtSplineInputKey(K,ESplineCoordinateSpace::World)));Guard|=L>365 && L<470 && P.Z-On.Z>8 && P.Z-On.Z<130;}
   if(Guard || FMath::Abs(FVector(C.Normal).Z)<.7){++SideContacts;if(FirstContact.IsEmpty())FirstContact=Other->GetPathName()+TEXT(" at ")+FVector(C.Location).ToString();}
  }
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(FString Error)
 {
  const bool Observed=Registered;if(Registered && Scene && Scene->GetSolver() && Scene->GetSolver()->GetEventManager())Scene->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this);Registered=false;
  for(FKey K:{EKeys::F,EKeys::C})Key(K,false);if(Car){if(Car->GetRoadCruise()->IsActive())Car->GetRoadCruise()->Cancel(TEXT("Parking validation ended"));Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);if(PC && PC->GetPawn()==Car)Car->GetVehicleInteraction()->Exit();}
  if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("bay_index"),Bay);J->SetNumberField(TEXT("stage"),Stage);J->SetBoolField(TEXT("native_contact_observer_registered"),Observed);J->SetArrayField(TEXT("bays"),Results);J->SetNumberField(TEXT("failed_bay_travel_cm"),Distance);J->SetNumberField(TEXT("failed_bay_main_gap_cm"),MainGap);J->SetStringField(TEXT("scope"),TEXT("One selected independent local player approach fixture, all authored cars unmodified; actual F entry, manual alignment, C navigation/cruise pull-out to Main, braking and F exit. No car relocation, physics velocity writes, collision changes or persistence. Does not substitute for continuous Dock route."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("parking-drive798.json")));FFileHelper::SaveStringToFile(Telemetry,*(Dir/TEXT("parking-drive798.csv")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;USplineComponent* Main=nullptr;USplineComponent* BaySpline=nullptr;FPhysScene_Chaos* Scene=nullptr;TArray<AIslandSupercar*> Cars;TArray<TSharedPtr<FJsonValue>> Results;
 bool Registered=false,MergingMain=false;int32 Stage=0,Bay=0,SideContacts=0;double Started=0;float At=0,Distance=0,MainGap=0,InitialCondition=0,LastProgress=0,LastSample=0,Steer=0,Wheelbase=0;FVector StartPosition,LastPosition,RearAxle;FString FirstContact;FString Telemetry=TEXT("car,time,stage,x,y,z,yaw,speed_cm_s,main_gap,gear,throttle,brake,grounded\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParkingBayNative798,"NeonCity.Phase205c.Estate.Standalone.ZParkingBayDrive798",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FParkingBayNative798::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Parking validation must preserve player saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(ParkingDrive798::FFlow(this));return true;}
#endif
