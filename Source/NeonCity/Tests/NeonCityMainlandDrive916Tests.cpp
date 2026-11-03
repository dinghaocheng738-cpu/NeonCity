#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityValidationPSO916.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Property/NeonCityGarageMarker.h"
#include "Property/PropertySubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandEstate916
{
void Key(APlayerController*,FKey,bool);
bool Focus(APlayerController*,ANeonCityPlayerCharacter*,AActor*);
class FDrive:public IAutomationLatentCommand
{
public:explicit FDrive(FAutomationTestBase* T,bool InExitOnly=false,bool InCornerOnly=false,bool InGardenOnly=false,bool InParkingOnly=false):Test(T),ExitOnly(InExitOnly),CornerOnly(InCornerOnly || InGardenOnly),GardenOnly(InGardenOnly),ParkingOnly(InParkingOnly){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>360)return End(TEXT("Continuous mainland drive timeout"));
  const float Age=W->GetTimeSeconds()-At;
  // Sample before every stage-specific early return, including registration
  // approach/prompt frames. Their whole walking interval must not appear as
  // one false long frame when sampling resumes at the mansion doorway.
  if(Stage>=3)Metrics.Sample(W,Stage==3?TEXT("MainlandShowroomExit"):Stage<=6?TEXT("MainlandContinuousCruise"):Stage<=8?TEXT("MainlandEstateParking"):TEXT("MainlandGarageToMansionWalking"));
  if(Stage==0)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal on-foot player absent"));
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(FVector::Dist2D(I->GetActorLocation(),FVector(-13100,2800,0))<400)Car=*I;
   if(!Car)return End(TEXT("Native parked mainland Aster absent; no car fixture is substituted"));
   if(CornerOnly)
   {
    // Isolated initial pose for the measured junction/Garden obstruction.
    // This does not establish continuous-route acceptance.
    Car->SetActorTransform(FTransform(FRotator(0,GardenOnly?95.7f:90.f,0),GardenOnly?FVector(-33795.384,14723.597,43):FVector(-23680,12000,43)),false,nullptr,ETeleportType::TeleportPhysics);
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   }
   if(ParkingOnly)
   {
    // Isolated road-end fixture, followed by actual F entry and native driving.
    // Never counts as a repeated showroom-to-estate continuous journey.
    Car->SetActorTransform(FTransform(FRotator(0,-16.7f,0),FVector(-93381.088,40656.864,1143.021)),false,nullptr,ETeleportType::TeleportPhysics);
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   }
   FVector Stand;if(!Phase205cVehicleApproach(W,P,Car,P->GetActorLocation(),Stand))return End(TEXT("Showroom has no supported clear normal car approach"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   InitialHealth=Car->GetHealthComponent()->GetCurrentHealth();Last=Car->GetActorLocation();
   Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return End(TEXT("Production navigation absent"));
   OriginalDestination=Nav->GetDestination();
   if(!ExitOnly && !CornerOnly)
   {
    Property=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
    for(TActorIterator<ANeonCityGarageMarker> I(W);I;++I)if(I->PropertyID==TEXT("Property.Mainland.RivieraEstate916") && I->SlotIndex==1)Garage=*I;
    if(!Property || !Garage)return End(TEXT("Native estate garage registration missing"));
    PropertySnapshot=Property->BuildPersistenceSnapshot();PropertyCaptured=true;
   }
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Mainland916.Road.07")){auto* S=I->GetRoadSpline();Destination=S->GetLocationAtDistanceAlongSpline(S->GetSplineLength(),ESplineCoordinateSpace::World);}
   if(Destination.IsNearlyZero())return End(TEXT("Native estate road endpoint missing"));
   ManualGoals={FVector(-13600,2400,0),FVector(-14000,1500,0),FVector(-14000,300,0),FVector(-15100,180,0)};
   return Next(1);
  }
  if(Stage==1 && Age>1.5){if(!Focus(PC,P,Car))return End(TEXT("Normal F showroom entry prompt occluded"));Key(PC,EKeys::F,true);return Next(2);}
  if(Stage==2 && Age>.3){Key(PC,EKeys::F,false);if(PC->GetPawn()!=Car || Car->GetVehicleInteraction()->GetDriver()!=P)return End(TEXT("Normal F did not board the native showroom car"));return Next(3);}
  if(Stage>=3 && Stage<=8)
  {
   const FVector Here=Car->GetActorLocation();const float Distance=FVector::Dist2D(Here,Last);Travel+=Distance;if(Stage==5)CruiseTravel+=Distance;Last=Here;
   if(PC->GetPawn()!=Car || Here.ContainsNaN() || !Car->GetMesh()->IsSimulatingPhysics() || Car->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5f)return End(FString::Printf(TEXT("Native drive failure: stage%d leg%d at%s rotation%s health%.2f/%.2f"),Stage,Point,*Here.ToString(),*Car->GetActorRotation().ToString(),Car->GetHealthComponent()->GetCurrentHealth(),InitialHealth));
  }
  if(Stage==3 || Stage==7)
  {
   if(!((CornerOnly || ParkingOnly) && Stage==3) && !ManualDrive(Age))return false;
   if(Failed)return End(TEXT("Actual manual driveway/showroom circulation blocked"));
   if(Stage==3)
   {
    FNeonCityNavigationDestination D;D.OwnerID=TEXT("Validation.Mainland916");D.DestinationID=TEXT("EstateDrive");D.Label=FText::FromString(TEXT("Riviera estate"));D.Location=Destination;D.UseRoadRoute=true;
    if(!Nav->SetDestination(D))return End(TEXT("Production navigation rejected the estate destination"));Key(PC,EKeys::C,true);return Next(4);
   }
   return Next(8);
  }
  if(Stage==4 && Age>.3)
  {
   Key(PC,EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Normal C failed: ")+Car->GetRoadCruise()->GetReason());
#if WITH_EDITOR
   if(GardenOnly && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Mainland916-GardenFurnitureApproach.jpg"));
#endif
   CruiseTravel=0;return Next(5);
  }
  if(Stage==5)
  {
   auto* Cruise=Car->GetRoadCruise();PeakSpeed=FMath::Max(PeakSpeed,Car->GetDrivingComponent()->GetSpeedKmh());MaxLaneError=FMath::Max(MaxLaneError,Cruise->GetLaneErrorCm());
   if(Cruise->GetReason().Contains(TEXT("Lane lost")) || Cruise->GetLaneErrorCm()>330)return End(TEXT("Cruise left the physical authored lane"));
   if((ExitOnly || CornerOnly) && !Cruise->IsActive())return End(TEXT("Focused cruise cancelled before completion: ")+Cruise->GetReason());
   if(GardenOnly && Car->GetActorLocation().Y>17000 && CruiseTravel>8000){Test->AddInfo(FString::Printf(TEXT("Isolated corrected Garden furniture passed %.1fm maxlane%.1fcm"),CruiseTravel/100,MaxLaneError));return End(TEXT(""));}
   if(CornerOnly && !GardenOnly && Car->GetActorLocation().X<-24700 && Car->GetActorLocation().Y>13400 && CruiseTravel>2500){Test->AddInfo(FString::Printf(TEXT("Isolated cross-street corner passed %.1fm maxlane%.1fcm"),CruiseTravel/100,MaxLaneError));return End(TEXT(""));}
   if(ExitOnly && CruiseTravel>15000 && PeakSpeed>30){Test->AddInfo(TEXT("Native showroom exit and first150m of destination cruise passed"));return End(TEXT(""));}
   const float Remaining=FVector::Dist2D(Car->GetActorLocation(),Destination);
   if(!Cruise->IsActive() && Remaining>500)return End(TEXT("Cruise stopped before estate: ")+Cruise->GetReason());
   if(Remaining<450 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.5f)
   {
    Cruise->Cancel(TEXT("Native road destination reached"));
    PrepareParking();return Next(7);
   }
   if(Age>((GardenOnly || ParkingOnly)?45.f:(ExitOnly || CornerOnly)?25.f:260.f))return End(FString::Printf(TEXT("Cruise did not arrive: remaining%.1fm at%s reason%s"),Remaining/100,*Car->GetActorLocation().ToString(),*Cruise->GetReason()));return false;
  }
  if(Stage==8 && Age>.8){Key(PC,EKeys::F,true);return Next(9);}
  if(Stage==9 && Age>.4)
  {Key(PC,EKeys::F,false);if(PC->GetPawn()!=P || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Native garage F exit failed to find supported walking"));return Next(11);}
  if(Stage==11)
  {
   const FVector Goal=Garage->GetActorLocation()+FVector(0,-110,P->GetSimpleCollisionHalfHeight()+3);
   if(!WalkToGoal(Goal,Age,12,TEXT("Parked car to registration post"),70))return false;
   if(!WalkFailure.IsEmpty())return End(WalkFailure);
   if(!Focus(PC,P,Garage))return End(TEXT("Actual parked-car garage F prompt occluded"));Key(PC,EKeys::F,true);return Next(12);
  }
  if(Stage==12 && Age>.3)
  {
   Key(PC,EKeys::F,false);FNeonCityPropertyRecord Record;
   if(!Property->GetProperty(Garage->PropertyID,Record) || !Record.GarageSlots.IsValidIndex(Garage->SlotIndex) || Record.GarageSlots[Garage->SlotIndex].VehicleID!=Car->VehicleID)return End(TEXT("Normal garage F failed to register the actual owned driven car"));
   Test->AddInfo(ParkingOnly?TEXT("Isolated road-end drive/park/F exit/post F records the same vehicle identity"):TEXT("Actual continuous drive/park/F exit/post F records the same vehicle identity"));return Next(10);
  }
  if(Stage==10)
  {
   const FVector Goal(-92000,37700,1068);
   if(!WalkToGoal(Goal,Age,45,TEXT("Parked garage to mansion doorway")))return false;
   if(!WalkFailure.IsEmpty())return End(WalkFailure);
   if(!ParkingOnly){Test->TestTrue(TEXT("Actual driven route exceeds one kilometre"),Travel>100000);Test->TestTrue(TEXT("Cruise reaches at least35km/h where its actual road allows"),PeakSpeed>35);}Test->AddInfo(FString::Printf(TEXT("Native %s/parking/F exit/mansion walk: %.1fm, peak%.1fkm/h, max lane%.1fcm"),ParkingOnly?TEXT("isolated road-end fixture"):TEXT("showroom/car/cruise/estate"),Travel/100,PeakSpeed,MaxLaneError));return End(TEXT(""));
  }
  return false;
 }
private:
 bool WalkToGoal(const FVector& Goal,float Age,float Timeout,const TCHAR* Context,float Spacing=110)
 {
  if(Walk.IsEmpty())
  {
   FPhase205cWalkSurvey Survey;
   const double PlanStarted=FPlatformTime::Seconds();
   const bool Found=Phase205cSupportedWalkPath(W,P,Goal,Walk,Spacing,25,&Survey,10000);
   Test->AddInfo(FString::Printf(TEXT("Validation-only physical walk planner: %s %.3fms, %d probed nodes, %d points"),Context,(FPlatformTime::Seconds()-PlanStarted)*1000,Survey.Probed,Walk.Num()));
   if(!Found){WalkFailure=FString(Context)+TEXT(": ")+Survey.Failure;return true;}
  }
  const FVector D=Walk[WalkPoint]-P->GetActorLocation();
  if(D.Size2D()<18 && FMath::Abs(D.Z)<45){if(++WalkPoint==Walk.Num()){Walk.Reset();WalkPoint=0;return true;}return false;}
  if(Age>Timeout){WalkFailure=FString(Context)+TEXT(": actual walking blocked");return true;}
  P->AddMovementInput(D.GetSafeNormal2D(),FMath::Clamp(float(D.Size2D()/100),.08f,1.f));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));return false;
 }
 void PrepareParking()
 {
  // Centre the first8m entrance before crossing its jambs. The old diagonal
  // aimed at the right-hand stall while still outside and clipped the column.
  ManualGoals={FVector(-92000,39100,0),FVector(-91000,39000,0),FVector(-90150,38800,0),FVector(-90150,37700,0)};Point=0;
 }
 bool ManualDrive(float Age)
 {
  auto* Driving=Car->GetDrivingComponent();const FVector D=ManualGoals[Point]-Car->GetActorLocation();const bool Final=Point==ManualGoals.Num()-1;const float Speed=Driving->GetSpeedKmh();
  if(D.Size2D()<(Final?90:240))
  {
   if(!Final){++Point;At=W->GetTimeSeconds();return false;}
   Driving->SetDriveInput(0);Driving->SetSteerInput(0);Driving->SetBrakeInput(1);return FMath::Abs(Speed)<1;
  }
  if(Age>35){Test->AddError(FString::Printf(TEXT("Native manual leg%d stalled at%s toward%s speed%.1f"),Point,*Car->GetActorLocation().ToString(),*ManualGoals[Point].ToString(),Speed));Failed=true;return true;}
  const float Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,D.Rotation().Yaw);const float TargetSpeed=Final?FMath::Clamp(float(D.Size2D()*.012),3.f,9.f):10.f;
  Driving->SetSteerInput(FMath::Clamp(Error/45.f,-1.f,1.f));Driving->SetDriveInput(FMath::Clamp((TargetSpeed-Speed)*.08f,0.f,.45f));Driving->SetBrakeInput(Speed>TargetSpeed+1?.25f:0);return false;
 }
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error)
 {
  Key(PC,EKeys::F,false);Key(PC,EKeys::C,false);if(!Error.IsEmpty())Test->AddError(Error);
  if(Car){Car->GetRoadCruise()->Cancel(TEXT("Isolated route test ended"));Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);}
  if(Nav){Nav->ClearDestination(TEXT("Validation.Mainland916"));if(!OriginalDestination.DestinationID.IsNone())Nav->SetDestination(OriginalDestination);}
  if(PropertyCaptured)Property->ApplyPersistenceSnapshot(PropertySnapshot);
  Test->TestTrue(TEXT("Continuous route metrics retained"),Metrics.Flush());Test->TestTrue(TEXT("Requested PSO recording saved before forced automation exit"),NeonCitySaveValidationPSO916());return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;UNeonCityNavigationComponent* Nav=nullptr;
 FNeonCityNavigationDestination OriginalDestination;FVector Destination,Last;TArray<FVector> ManualGoals,Walk;FString WalkFailure;TArray<FNeonCityPropertySaveData> PropertySnapshot;UPropertySubsystem* Property=nullptr;ANeonCityGarageMarker* Garage=nullptr;int32 Stage=0,Point=0,WalkPoint=0;float At=0,InitialHealth=0,Travel=0,CruiseTravel=0,PeakSpeed=0,MaxLaneError=0;double Started=0;bool Failed=false,ExitOnly=false,CornerOnly=false,GardenOnly=false,ParkingOnly=false,PropertyCaptured=false;FNeonCityLandFrameMetrics Metrics{TEXT("Mainland916-ContinuousDrive")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandDrive916,"NeonCity.Phase205c.Mainland916.Standalone.ContinuousDrive",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandDrive916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FDrive(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCruiseExit916,"NeonCity.Phase205c.Mainland916.Standalone.CruiseExitRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCruiseExit916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FDrive(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCruiseCorner916,"NeonCity.Phase205c.Mainland916.Standalone.CruiseCornerRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCruiseCorner916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FDrive(this,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCruiseGarden916,"NeonCity.Phase205c.Mainland916.Standalone.CruiseGardenRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCruiseGarden916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FDrive(this,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandParking916,"NeonCity.Phase205c.Mainland916.Standalone.EstateParkingRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandParking916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FDrive(this,false,false,false,true));return true;}
#endif
