#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NeonCityLandLocalAerial
{
class FLocalAerial : public IAutomationLatentCommand
{
public:
 explicit FLocalAerial(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;if(!W || !PC || W->GetTimeSeconds()<3)return false;
  if(FPlatformTime::Seconds()-Started>470)return Finish(W,false,FString::Printf(TEXT("Local helicopter check timed out stage%d waypoint%d"),Stage,Point));
  if(PausedFrames>0)
  {
   // Keep at least one whole game frame paused after synchronous screenshot
   // readback. The readback wall-time cannot become the next physics delta.
   if(--PausedFrames==0){UGameplayStatics::SetGamePaused(W,WasPaused);++Point;At=W->GetTimeSeconds();Stage=6;}
   return false;
  }
  const float Now=W->GetTimeSeconds();const float DT=FMath::Clamp(Now-Previous,0.f,.1f);Previous=Now;
  Metrics.Sample(W,Stage<=2?TEXT("warmup-Helicopter-native-entry"):Stage==3?TEXT("Helicopter-takeoff"):Stage==4?TEXT("Helicopter-local-transit"):Stage==5?TEXT("HelicopterAerial-live-hover"):TEXT("visual-only-Helicopter-local-comparison"));
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It){Heli=*It;break;}
   if(!Heli || !Heli->GetProfile() || !Heli->AircraftID.IsValid())return Finish(W,false,TEXT("Original authored helicopter required; no replacement spawning"));
   if(!Heli->GetHull()->IsSimulatingPhysics() || !Heli->GetAircraftMovement()->IsGrounded())return Finish(W,false,TEXT("Original helicopter must begin physically grounded on its authored pad"));
   InitialHeliTransform=Heli->GetActorTransform();InitialHealth=Heli->GetHealthComponent()->GetCurrentHealth();
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label.IsEmpty()?FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")):Label)/TEXT("MansionLocalAerial");
   IFileManager::Get().MakeDirectory(*Directory,true);
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,waypoint,x,y,z,yaw,pitch,roll,vx,vy,vz,altitude_cm,engine_power,input_x,input_y,input_z,input_yaw,power_input,flight_state\n"),*(Directory/TEXT("trajectory.csv")));
   // Player-only initial fixture beside the actual pad aircraft. The aircraft
   // keeps the authored transform, contacts, health and physical velocity.
   const FVector Side=Heli->GetActorLocation()+Heli->GetActorRightVector()*240;
   FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(LandLocalHeliEntry),false,Player);Q.AddIgnoredActor(Heli);
   if(!W->LineTraceSingleByChannel(Floor,Side+FVector(0,0,450),Side-FVector(0,0,500),ECC_Pawn,Q)
    || Floor.ImpactNormal.Z<Player->GetCharacterMovement()->GetWalkableFloorZ())return Finish(W,false,TEXT("No native walkable entry support beside the actual authored helicopter"));
   Player->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();++PlayerSetupRelocations;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   Test->AddInfo(TEXT("LOCAL AERIAL ONLY: one initial PLAYER setup beside the authored helicopter; no aircraft relocation. Existing accepted 6km circuit is not rerun or replaced."));
   At=Now;Stage=1;return false;
  }
  if(Stage==1)
  {
   if(Now-At<.8f)return false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return Finish(W,false,TEXT("Helicopter entry fixture did not retain normal walking support"));
   PC->SetControlRotation((Heli->GetActorLocation()+FVector(0,0,25)-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Heli)return Finish(W,false,TEXT("Player eye trace could not focus the actual helicopter hull"));
   Key(PC,IE_Pressed);FPressed=true;At=Now;Stage=2;return false;
  }
  if(Stage==2)
  {
   if(Now-At<.25f)return false;Key(PC,IE_Released);FPressed=false;
   if(PC->GetPawn()!=Heli || Heli->GetAircraftInteraction()->GetDriver()!=Player || !Heli->GetAircraftMovement()->IsEngineRunning())return Finish(W,false,TEXT("Normal F interaction did not possess and start the authored helicopter"));
   TakeoffOrigin=Heli->GetActorLocation();Last=TakeoffOrigin;FlightT0=Now;At=Now;Stage=3;return false;
  }
  const FVector Here=Heli->GetActorLocation();Distance+=FVector::Dist(Here,Last);Last=Here;
  if(PC->GetPawn()!=Heli || !Heli->GetHull()->IsSimulatingPhysics() || !Heli->GetAircraftMovement()->IsEngineRunning())return Finish(W,false,TEXT("Actual local flight lost pilot possession or physical engine state"));
  if(Heli->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5f)return Finish(W,false,TEXT("Local native flight contacted terrain or damaged the authored aircraft"));
  if(Stage==3)
  {
   Submit(PC,FVector(0,0,1),0,.8f);
   PC->SetControlRotation(FRotator(-15,Heli->GetActorRotation().Yaw,0));
   if(Here.Z-TakeoffOrigin.Z>4500)
   {
    // Short outbound approach from the existing pad, then only the changed
    // upper-road / mansion area. These are not perimeter circuit waypoints.
    Transit={FVector(15000,-43000,24000),NeonCityCoastalLayout::FromLegacyIsland(FVector(45000,-135000,35000)),NeonCityCoastalLayout::FromLegacyIsland(FVector(50500,-123200,33700))};
    Comparisons={NeonCityCoastalLayout::FromLegacyIsland(FVector(50100,-121700,33000)),NeonCityCoastalLayout::FromLegacyIsland(FVector(51300,-123400,33900)),NeonCityCoastalLayout::FromLegacyIsland(FVector(53000,-126000,35500))};
    Point=0;At=Now;Stage=4;Test->AddInfo(TEXT("Native rotor/power takeoff cleared authored pad; all later travel remains SubmitFlightInput / SubmitPowerInput."));
   }
   else if(Now-At>22)return Finish(W,false,TEXT("Normal helicopter power / rise input did not clear the city roofs"));
  }
  else if(Stage==4)
  {
   FlyTo(PC,Transit[Point],DT,2200);
   PC->SetControlRotation((MansionAim-Here).Rotation());
   if(FVector::Dist2D(Here,Transit[Point])<350 && FMath::Abs(Here.Z-Transit[Point].Z)<160)
   {
    if(++Point==Transit.Num()){HoverTarget=Transit.Last();At=Now;HoverValidSeconds=0;LastValidSample=Now;Stage=5;Point=0;Test->AddInfo(TEXT("Actual helicopter reached current mansion / terrace / upper-road area without aircraft pose writes."));}
    else At=Now;
   }
   else if(Now-At>135)return Finish(W,false,FString::Printf(TEXT("Native local transit failed waypoint%d at %s"),Point,*Here.ToString()));
  }
  else if(Stage==5)
  {
   FlyTo(PC,HoverTarget,DT,500);PC->SetControlRotation((MansionAim-Here).Rotation());
   const float Since=FMath::Clamp(Now-LastValidSample,0.f,.25f);LastValidSample=Now;
   if(FVector::Dist2D(Here,HoverTarget)<350 && FMath::Abs(Here.Z-HoverTarget.Z)<160 && !Heli->GetAircraftMovement()->IsGrounded()
    && Heli->GetAircraftMovement()->GetAltitude()>1000)HoverValidSeconds+=Since;
   if(HoverValidSeconds>=65)
   {
    Test->AddInfo(TEXT("65 seconds measured actual occupied helicopter hovering at the current island mansion; no camera-only substitute and no continuous image readback."));
    Metrics.Flush();Stage=6;Point=0;At=Now;StableAt=0;
   }
   else if(Now-At>95)return Finish(W,false,TEXT("Could not sustain 65 seconds of real native mansion-area airborne hover"));
  }
  else if(Stage==6)
  {
   if(Point>=Comparisons.Num())
   {
    bool Valid=Test->TestTrue(TEXT("Actual airborne hover performance sample covers at least60 seconds"),HoverValidSeconds>=60);
    Valid=Test->TestEqual(TEXT("Exactly one explicit player entry setup"),PlayerSetupRelocations,1)&&Valid;
    Valid=Test->TestEqual(TEXT("No aircraft pose relocations"),AircraftRelocations,0)&&Valid;
    Valid=Test->TestTrue(TEXT("Actual pad-to-local-mansion flight exceeds1km"),Distance>100000)&&Valid;
    return Finish(W,Valid,Valid?TEXT(""):TEXT("Local aerial quantitative gate failed"));
   }
   FlyTo(PC,Comparisons[Point],DT,600);PC->SetControlRotation((MansionAim-Here).Rotation());
   const bool Stable=FVector::Dist2D(Here,Comparisons[Point])<250 && FMath::Abs(Here.Z-Comparisons[Point].Z)<100 && Heli->GetVelocity().Size2D()<140;
   if(!Stable)StableAt=0;else if(StableAt==0)StableAt=Now;
   if(StableAt!=0 && Now-StableAt>5)
   {
    const FString Name=FString::Printf(TEXT("LandFinal-LocalHelicopter-%s.png"),Point==0?TEXT("Near"):Point==1?TEXT("Middle"):TEXT("Far"));
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) && !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")))
    {
     WasPaused=UGameplayStatics::IsGamePaused(W);PausedFrames=2;UGameplayStatics::SetGamePaused(W,true);
     if(!FNeonCityVisualEvidence::CaptureHUD(W,Name))return Finish(W,false,TEXT("Optional requested local helicopter screenshot failed: ")+Name);
     Captures.Add(Name);StableAt=0;
    }
    else{++Point;At=Now;StableAt=0;}
   }
   else if(Now-At>50)return Finish(W,false,FString::Printf(TEXT("Local comparison hover could not settle view%d"),Point));
  }
  Sample(W);return false;
 }
private:
 static void Key(APlayerController* PC,EInputEvent Event){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,Event,FPlatformTime::Cycles64()));}
 void Submit(APlayerController* PC,const FVector& Command,float Yaw,float Power)
 {
  Input=Command;YawInput=Yaw;PowerInput=Power;
  Heli->SubmitFlightInput_Implementation(PC,Command,Yaw);Heli->SubmitPowerInput(PC,Power);
 }
 void FlyTo(APlayerController* PC,const FVector& Destination,float DT,float SpeedLimit)
 {
  const FVector Position=Heli->GetActorLocation(),Velocity=Heli->GetVelocity(),Error=Destination-Position;
  const FVector DesiredVelocity=(FVector(Error.X,Error.Y,0)*.55f).GetClampedToMaxSize(SpeedLimit);
  const FVector Horizontal(Velocity.X,Velocity.Y,0);
  const auto* Profile=Heli->GetProfile();const float Current=Heli->GetAircraftMovement()->GetCurrentPower();
  const float Boost=FMath::Clamp((Current-Profile->HoverPower)/(1-Profile->HoverPower),0.f,1.f);
  const float Acceleration=Profile->Acceleration*100*FMath::Lerp(1.f,Profile->FastFlightAccelerationScale,Boost);
  const FVector WorldCommand=((DesiredVelocity-Horizontal)*.9f+Horizontal*(Profile->AirResistance+.05f))/Acceleration;
  const FRotator Heading(0,Heli->GetActorRotation().Yaw,0);FVector Local=Heading.UnrotateVector(WorldCommand.GetClampedToMaxSize(1));
  // The existing native vertical-speed port includes hover support, gravity,
  // inertia and contacts. The controller requests motion, never lift forces.
  Local.Z=FMath::Clamp(Error.Z/(Profile->VerticalSpeed*100*1.5f),-1.,1.);
  const float YawError=FMath::FindDeltaAngleDegrees(Heli->GetActorRotation().Yaw,(MansionAim-Position).Rotation().Yaw);
  YawInput=FMath::FInterpConstantTo(YawInput,FMath::Clamp(YawError/50.f,-1.f,1.f),DT,1.5f);
  const float Power=Current<.85f && Stage==4?.45f:0;
  Submit(PC,Local,YawInput,Power);
 }
 void Sample(UWorld* W)
 {
  if(!Heli || W->GetTimeSeconds()-LastSample<.1f)return;LastSample=W->GetTimeSeconds();
  const FVector P=Heli->GetActorLocation(),V=Heli->GetVelocity();const FRotator R=Heli->GetActorRotation();
  Rows+=FString::Printf(TEXT("%.6f,%d,%d,%.5f,%.5f,%.5f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d\n"),W->GetTimeSeconds(),Stage,Point,P.X,P.Y,P.Z,R.Yaw,R.Pitch,R.Roll,V.X,V.Y,V.Z,Heli->GetAircraftMovement()->GetAltitude(),Heli->GetAircraftMovement()->GetCurrentPower(),Input.X,Input.Y,Input.Z,YawInput,PowerInput,static_cast<int32>(Heli->GetAircraftMovement()->GetFlightState()));
  if(Rows.Len()>32000)Flush();
 }
 void Flush(){if(Rows.IsEmpty() || Directory.IsEmpty())return;FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);Rows.Reset();}
 bool Finish(UWorld* W,bool Passed,const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(W && PausedFrames>0){UGameplayStatics::SetGamePaused(W,WasPaused);PausedFrames=0;}
  if(PC && FPressed){Key(PC,IE_Released);FPressed=false;}
  if(PC && Heli && PC->GetPawn()==Heli)Submit(PC,FVector::ZeroVector,0,0);
  Flush();Metrics.Flush();if(Directory.IsEmpty())return true;
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Error);
  J->SetStringField(TEXT("scope"),TEXT("Short actual helicopter takeoff, outbound approach and current mansion/terrace/upper-road local hover; NOT full island circuit, NOT a replacement for accepted 6km circuit"));
  J->SetBoolField(TEXT("fullIslandCircuitRerun"),false);J->SetNumberField(TEXT("initialPlayerSetupRelocations"),PlayerSetupRelocations);J->SetNumberField(TEXT("aircraftPoseRelocations"),AircraftRelocations);
  J->SetNumberField(TEXT("actualFlightTravelCm"),Distance);J->SetNumberField(TEXT("actualAirborneHoverSeconds"),HoverValidSeconds);J->SetNumberField(TEXT("nativeFlightStartedGameSeconds"),FlightT0);
  J->SetStringField(TEXT("authoredPadAircraftTransform"),InitialHeliTransform.ToHumanReadableString());J->SetNumberField(TEXT("initialHealth"),InitialHealth);J->SetNumberField(TEXT("finalHealth"),Heli?Heli->GetHealthComponent()->GetCurrentHealth():0);
  J->SetBoolField(TEXT("performanceSampleHasContinuousReadback"),false);J->SetBoolField(TEXT("staticComparisonsTakenAfterPerformanceWindow"),true);J->SetBoolField(TEXT("driverInputsReset"),true);
  TArray<TSharedPtr<FJsonValue>> Images;for(const auto& Capture:Captures)Images.Add(MakeShared<FJsonValueString>(Capture));J->SetArrayField(TEXT("pausedStaticImages"),Images);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));Test->TestTrue(TEXT("Local actual helicopter evidence receipt saved"),FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json"))));return true;
 }
 FAutomationTestBase* Test;ANeonCityHelicopterPrototype* Heli=nullptr;ANeonCityPlayerCharacter* Player=nullptr;
 FNeonCityLandFrameMetrics Metrics{TEXT("MansionLocalAerial")};int32 Stage=0,Point=0,PlayerSetupRelocations=0,AircraftRelocations=0,PausedFrames=0;
 bool FPressed=false,WasPaused=false;double Started=0,Distance=0;float Previous=0,At=0,StableAt=0,FlightT0=0,LastSample=0,LastValidSample=0,HoverValidSeconds=0,InitialHealth=0,YawInput=0,PowerInput=0;
 FTransform InitialHeliTransform;FVector TakeoffOrigin=FVector::ZeroVector,Last=FVector::ZeroVector,HoverTarget=FVector::ZeroVector,Input=FVector::ZeroVector;
 const FVector MansionAim=NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-119600,30640));TArray<FVector> Transit,Comparisons;TArray<FString> Captures;FString Directory,Rows;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandMansionLocalAerial,"NeonCity.IslandLand.MansionLocalAerialPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandMansionLocalAerial::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandLocalAerial::FLocalAerial(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
