#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "ChaosVehicleWheel.h"

namespace NeonCityHighSpeedHandling
{
/** The player's three garage cars at motorway speed on an open asphalt test pad (raised clear of the map):
 *  - step steer: from 140 km/h, full keyboard lock for 1.2 s, then straight: the car turns hard but never spins
 *    (body slip stays under 12 deg) or rolls, and settles straight within 2 s of release;
 *  - lane change: at 140 km/h, right 0.7 s, left 0.7 s, straight: no slide, oscillation dies out.
 * Metrics per car to Saved/Automation/HighSpeedHandling.json. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){Metrics=MakeShared<FJsonObject>();}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>420){Test->AddError(FString::Printf(TEXT("Handling check timed out (car %d stage %d)"),CarIndex,Stage));return Finish();}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   Player=Cast<ACharacter>(PC->GetPawn());if(!Player.IsValid())return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)
    if(It->bStartsOwned && FVector::Dist2D(It->GetActorLocation(),Player->GetActorLocation())<6000)Cars.Add(*It);
   Cars.Sort([](const TWeakObjectPtr<AIslandSupercar>& A,const TWeakObjectPtr<AIslandSupercar>& B){return A->GetName()<B->GetName();});
   if(!Test->TestTrue(FString::Printf(TEXT("garage cars found (%d)"),Cars.Num()),Cars.Num()>=3))return Finish();
   FString Values;
   if(FParse::Value(FCommandLine::Get(),TEXT("NeonCityHandlingSweep="),Values))
   {
    TArray<FString> Parts;Values.ParseIntoArray(Parts,TEXT(","));for(const FString& V:Parts)Sweep.Add(FCString::Atof(*V));
    if(Sweep.Num())IConsoleManager::Get().FindConsoleVariable(TEXT("nc.Vehicle.SteerSlipAllowance"))->Set(Sweep[0]);
    for(int32 I=0;I<Cars.Num();++I)if(Cars[I]->bRoadster){Cars.Swap(0,I);break;}
   }
   // a 2 km asphalt-friction pad 400 m above the lake
   Pad=W->SpawnActor<AStaticMeshActor>(PadCentre,FRotator::ZeroRotator);
   Pad->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Pad->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Pad->SetActorScale3D(FVector(2000,2000,1));Pad->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
   return Next(1);
  }
  if(Stage==8 && CarIndex<Cars.Num())
  {
   AIslandSupercar* C=Cars[CarIndex].Get();
   if(C && C->GetVelocity().Size()>100 && W->GetTimeSeconds()-At<20)return false;
   if(C && PC->GetPawn()==C && !C->GetVehicleInteraction()->Exit() && W->GetTimeSeconds()-At<25)return false;
   ++CarIndex;Next(9);
  }
  if(Stage==9){if(W->GetTimeSeconds()-At<2)return false;Stage=1;}
  if(CarIndex>=Cars.Num())return Finish();
  AIslandSupercar* Car=Cars[CarIndex].Get();if(!Car){Test->AddError(TEXT("car lost"));return Finish();}
  auto* Drive=Car->GetDrivingComponent();auto* M=Car->GetChaosMovement();
  const double E=W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   // the player takes the wheel on the pad (driver seated: inputs reach Chaos)
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Car->SetActorLocationAndRotation(PadCentre+FVector(-60000,0,200),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   ACharacter* P=Player.Get();if(!P){Test->AddError(TEXT("player lost"));return Finish();}
   P->SetActorLocation(Car->GetActorLocation()+FVector(0,-250,0),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetVehicleInteraction()->Enter(P);
   return Next(2);
  }
  if(Stage==2)
  {
   if(E<1.5)return false;
   if(PC->GetPawn()!=Car && E<8)
   {
    // still settling from the last car: walk up to this one again and get in
    if(ACharacter* P=Cast<ACharacter>(PC->GetPawn())){P->SetActorLocation(Car->GetActorLocation()+FVector(0,-250,0),false,nullptr,ETeleportType::TeleportPhysics);Car->GetVehicleInteraction()->Enter(P);}
    return false;
   }
   if(!Test->TestTrue(TEXT("seated in the car"),PC->GetPawn()==Car))return Finish();
   Car->GetRoadCruise()->Cancel(TEXT("handling test"));
   Name=Car->GetVehicleProfile()?Car->GetVehicleProfile()->DisplayName.ToString():Car->GetName();
   Drive->SetBrakeInput(1);Drive->SetSteerInput(1);Drive->SetDriveInput(0);
   return Next(20);
  }
  if(Stage==20)
  {
   // diagnostic: full lock at a standstill
   if(E<.8)return false;
   Test->AddInfo(FString::Printf(TEXT("standstill full lock: chaos %.2f wheel0 %.2f wheel1 %.2f desc max %.1f"),M->GetSteeringInput(),M->Wheels[0]->GetSteerAngle(),M->Wheels[1]->GetSteerAngle(),M->Wheels[0]->MaxSteerAngle));
   Drive->SetBrakeInput(0);Drive->SetSteerInput(0);Drive->SetDriveInput(1);
   return Next(3);
  }
  if(Stage==3 || Stage==6)
  {
   // up to speed in a straight line, holding the heading
   Hold(Car,0);
   if(Speed(Car)<140){if(E>25){Test->AddError(FString::Printf(TEXT("%s never reached 140 km/h (%.0f)"),*Name,Speed(Car)));return NextCar();}return false;}
   Reset();Drive->SetDriveInput(.35f);
   return Next(Stage==6 && bRepeatStep?4:Stage+1);
  }
  if(Stage==4 || Stage==7)
  {
   const bool Step=Stage==4;
   float Steer=0;
   if(Step)Steer=E<1.2?1.f:0.f;
   else Steer=E<.7?1.f:E<1.4?-1.f:0.f;
   Drive->SetSteerInput(Steer);Sample(Car);
   if(W->GetTimeSeconds()-LastLog>.2){LastLog=W->GetTimeSeconds();
    Test->AddInfo(FString::Printf(TEXT("t %.1f in %.2f chaos %.3f wheel %.2f yaw %.1f slip %.1f kmh %.0f esc %.2f"),E,Steer,M->GetSteeringInput(),M->Wheels.Num()&&M->Wheels[0]?M->Wheels[0]->GetSteerAngle():0.f,YawRate(Car),
     FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Car->GetVelocity().Rotation().Yaw),Speed(Car),Drive->GetStabilityLevel()));}
   if(E<(Step?3.2:3.8))return false;
   FString K=Name+(Step?TEXT(" step"):TEXT(" lane"));
   if(Sweep.Num())K+=FString::Printf(TEXT(" allow%.1f"),Sweep[FMath::Min(SweepIndex,Sweep.Num()-1)]);
   Metrics->SetNumberField(K+TEXT(" peak_wheel_deg"),PeakSteer);
   Metrics->SetNumberField(K+TEXT(" peak_slip_deg"),PeakSlip);Metrics->SetNumberField(K+TEXT(" peak_yaw_dps"),PeakYaw);
   Metrics->SetNumberField(K+TEXT(" peak_lat_g"),PeakLat);Metrics->SetNumberField(K+TEXT(" peak_roll_deg"),PeakRoll);
   Metrics->SetNumberField(K+TEXT(" end_yaw_dps"),FMath::Abs(YawRate(Car)));Metrics->SetNumberField(K+TEXT(" heading_change_deg"),HeadingChange(Car));
   Metrics->SetNumberField(K+TEXT(" exit_kmh"),Speed(Car));
   Test->AddInfo(FString::Printf(TEXT("%s: slip %.1f deg, yaw %.0f dps, lat %.2f g, roll %.1f deg, end yaw %.1f dps, heading %.0f deg, exit %.0f km/h"),
    *K,PeakSlip,PeakYaw,PeakLat,PeakRoll,FMath::Abs(YawRate(Car)),HeadingChange(Car),Speed(Car)));
   Test->TestTrue(FString::Printf(TEXT("%s: never slides out (peak body slip %.1f deg)"),*K,PeakSlip),PeakSlip<12);
   Test->TestTrue(FString::Printf(TEXT("%s: stays on its wheels (peak roll %.1f deg)"),*K,PeakRoll),PeakRoll<12);
   Test->TestTrue(FString::Printf(TEXT("%s: settles straight (yaw %.1f deg/s at the end)"),*K,FMath::Abs(YawRate(Car))),FMath::Abs(YawRate(Car))<6);
   if(Step)Test->TestTrue(FString::Printf(TEXT("%s: still turns hard (peak %.2f g)"),*K,PeakLat),PeakLat>.55f);
   if(Step && Sweep.Num() && SweepIndex+1<Sweep.Num())
   {
    // tuning sweep: next slip allowance, same car, step steer again
    ++SweepIndex;IConsoleManager::Get().FindConsoleVariable(TEXT("nc.Vehicle.SteerSlipAllowance"))->Set(Sweep[SweepIndex]);bRepeatStep=true;
    Drive->SetSteerInput(0);Drive->SetDriveInput(0);Drive->SetBrakeInput(1);return Next(5);
   }
   if(Sweep.Num()){bRepeatStep=false;return Finish();}
   if(!Step)return NextCar();
   // stop, face the pad again and run the lane change
   Drive->SetSteerInput(0);Drive->SetDriveInput(0);Drive->SetBrakeInput(1);
   return Next(5);
  }
  if(Stage==5)
  {
   if(Speed(Car)>5 && E<15)return false;
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Car->SetActorLocationAndRotation(PadCentre+FVector(-60000,0,150),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Drive->SetBrakeInput(0);Drive->SetDriveInput(1);
   return Next(6);
  }
  return false;
 }
private:
 static float Speed(const AIslandSupercar* C){return C->GetVelocity().Size2D()*.036f;}
 static float YawRate(const AIslandSupercar* C){return C->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z;}
 void Hold(AIslandSupercar* C,float Yaw)
 {
  // a driver keeping it straight while accelerating (gentle corrections only)
  const float Err=FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw,Yaw);
  C->GetDrivingComponent()->SetSteerInput(FMath::Clamp(Err/12.f,-.3f,.3f));
 }
 void Reset(){PeakSlip=PeakYaw=PeakLat=PeakRoll=PeakSteer=0;StartYaw=Cars[CarIndex]->GetActorRotation().Yaw;PrevVel=Cars[CarIndex]->GetVelocity();}
 float HeadingChange(const AIslandSupercar* C) const{return FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw,C->GetActorRotation().Yaw));}
 void Sample(AIslandSupercar* C)
 {
  const FVector V=C->GetVelocity();const float Dt=FMath::Max(1e-3f,W->GetDeltaSeconds());
  if(V.Size2D()>500)PeakSlip=FMath::Max(PeakSlip,FMath::Abs(FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw,V.Rotation().Yaw)));
  PeakYaw=FMath::Max(PeakYaw,FMath::Abs(YawRate(C)));
  if(C->GetChaosMovement()->Wheels.Num() && C->GetChaosMovement()->Wheels[0])PeakSteer=FMath::Max(PeakSteer,FMath::Abs(C->GetChaosMovement()->Wheels[0]->GetSteerAngle()));PeakRoll=FMath::Max(PeakRoll,FMath::Abs(C->GetActorRotation().Roll));
  const FVector A=(V-PrevVel)/Dt;PrevVel=V;
  PeakLat=FMath::Max(PeakLat,float(FMath::Abs(FVector::DotProduct(A,C->GetActorRightVector()))/980.f));
 }
 bool NextCar()
 {
  if(AIslandSupercar* C=Cars[CarIndex].Get())
  {C->GetDrivingComponent()->SetDriveInput(0);C->GetDrivingComponent()->SetSteerInput(0);C->GetDrivingComponent()->SetBrakeInput(1);}
  return Next(8);                                                                     // stop, step out, then the next car
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool Finish()
 {
  FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Metrics.ToSharedRef(),Writer);
  FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/TEXT("Automation/HighSpeedHandling.json")));
  if(Pad.IsValid())Pad->Destroy();
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;double Started=0,At=0;int32 Stage=0,CarIndex=0;
 const FVector PadCentre=FVector(6000,43000,40000);
 TArray<TWeakObjectPtr<AIslandSupercar>> Cars;TWeakObjectPtr<ACharacter> Player;TWeakObjectPtr<AStaticMeshActor> Pad;
 double LastLog=0;TArray<float> Sweep;int32 SweepIndex=0;bool bRepeatStep=false;float PeakSteer=0;
 FString Name;float PeakSlip=0,PeakYaw=0,PeakLat=0,PeakRoll=0,StartYaw=0;FVector PrevVel=FVector::ZeroVector;
 TSharedPtr<FJsonObject> Metrics;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityHighSpeedHandlingPIE,"NeonCity.WestLake.HighSpeedHandlingPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityHighSpeedHandlingPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityHighSpeedHandling::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
