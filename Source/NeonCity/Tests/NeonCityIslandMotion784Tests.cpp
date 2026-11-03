#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/NeonCityDayNightCycle.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "RenderTimer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace IslandMotion784
{
/** Two disclosed local diagnostic fixtures. After each T0, only ordinary
 * character/flight input is submitted. Neither fixture certifies a journey. */
class FFlow : public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* In,bool InVisual=false):Test(In),FlightOnly(FParse::Param(FCommandLine::Get(),TEXT("NeonCityMotionFlightOnly"))),Visual(InVisual){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>210)return Finish(TEXT("Motion diagnosis finite timeout"));
  if(!W || !PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds();const float DT=FMath::Clamp(Now-Previous,0.f,.1f);Previous=Now;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-156000,1000))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return Finish(TEXT("Normal character absent"));
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Island.Mountain.Main"))Road=I->GetRoadSpline();
   for(TActorIterator<ANeonCityReferenceVegetation> I(W);I;++I)if(I->GrassData)Grass=I->FindComponentByClass<UNeonCityReferenceGrassComponent>();
   for(TActorIterator<ANeonCityHelicopterPrototype> I(W);I;++I){Heli=*I;break;}
   if(!Road || !Grass || !Heli)return Finish(TEXT("Actual road, grass or authored helicopter absent"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);
   IFileManager::Get().MakeDirectory(*Directory,true);
   if(IFileManager::Get().FileExists(*(Directory/TEXT("motion-cpu.csv"))))return Finish(TEXT("Fresh evidence label required"));
   Rows=TEXT("game_seconds,stage,game_ms,game_wait_ms,render_ms,x,y,z,grass0,grass1,grass2\n");
   for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage")})
   {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return Finish(TEXT("Render policy absent"));OldPolicy.Add(Name,C->GetFloat());C->Set(FCString::Strcmp(Name,TEXT("r.ScreenPercentage"))==0?100.f:3.f,ECVF_SetByConsole);}
   if(Visual || FParse::Param(FCommandLine::Get(),TEXT("NeonCityFinalTemporal908")))
    for(const TCHAR* Name:{TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"),TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections")})
    {const auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C || C->GetInt()!=64)return Finish(TEXT("Final motion requires the actual Epic temporal64/64 policy"));}
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;OldHour=I->GetTimeOfDay();OldCycle=I->bCycleEnabled;I->SetCycleEnabled(false);I->SetTimeOfDay(12);break;}
   OriginalPlayer=Player->GetActorTransform();OriginalHeli=Heli->GetActorTransform();
   StartDistance=Road->GetDistanceAlongSplineAtSplinePoint(34);
   const FVector P=Road->GetLocationAtDistanceAlongSpline(StartDistance,ESplineCoordinateSpace::World);FHitResult H;
   if(!Floor(P,H))return Finish(TEXT("Initial walking fixture has no native road support"));
   Player->SetActorLocation(H.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();WalkStart=Last=Player->GetActorLocation();At=Now;Stage=1;return false;
  }
  Sample();
  if(Stage==1)
  {
   if(Now-At<8)return false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return Finish(TEXT("Walking fixture did not retain floor support"));
   At=Now;Stage=2;return false;
  }
  if(Stage==2)
  {
   const float Elapsed=Now-At;const float Travel=FMath::Min(Elapsed*260.f,9000.f);
   if(!FlightOnly)
   {
   const FVector Target=Road->GetLocationAtDistanceAlongSpline(StartDistance+Travel+250,ESplineCoordinateSpace::World);
   const FVector Direction=(Target-Player->GetActorLocation()).GetSafeNormal2D();Player->AddMovementInput(Direction,.55f);
   PC->SetControlRotation(FRotator(-8,Direction.Rotation().Yaw+35*FMath::Sin(Elapsed*.22f),0));
   WalkDistance+=FVector::Dist2D(Last,Player->GetActorLocation());Last=Player->GetActorLocation();
   if(!Player->GetCharacterMovement()->IsMovingOnGround())++AirborneWalkFrames;
   if(Visual && !CaptureMoving(TEXT("Walk"),Elapsed,8,WalkFrames))return Finish(TEXT("Incomplete walking temporal evidence"));
   if(Elapsed<36)return false;
   Player->ConsumeMovementInputVector();if(WalkDistance<5000)return Finish(TEXT("Normal input failed to walk50m on the actual road"));
   }
   // Independent flight fixture: same genuine native aircraft, one declared
   // pre-flight placement on measured road support; not authored-pad acceptance.
   FVector Center;FRotator Yaw;bool FoundPad=false;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(Motion784AirframeClearance),false,Player);Q.AddIgnoredActor(Heli);
   for(int32 Station=37;Station<70 && !FoundPad;++Station)
   {
    const FVector Pad=Road->GetLocationAtSplinePoint(Station,ESplineCoordinateSpace::World);FHitResult H;if(!Floor(Pad,H))continue;
    Yaw=FRotator(0,Road->GetDirectionAtSplinePoint(Station,ESplineCoordinateSpace::World).Rotation().Yaw,0);
    double Highest=H.ImpactPoint.Z;bool Supported=true;
    for(float X:{-200.f,200.f})for(float Y:{-90.f,90.f}){FHitResult Corner;if(!Floor(Pad+Yaw.RotateVector(FVector(X,Y,0)),Corner)){Supported=false;break;}Highest=FMath::Max(Highest,Corner.ImpactPoint.Z);}
    if(!Supported || Highest-H.ImpactPoint.Z>45)continue;
    Center=FVector(Pad.X,Pad.Y,Highest+Heli->GetProfile()->BodyExtent.Z+8);
    FoundPad=!W->OverlapBlockingTestByChannel(Center,Yaw.Quaternion(),ECC_Vehicle,FCollisionShape::MakeBox(Heli->GetProfile()->BodyExtent),Q);
   }
   if(!FoundPad)return Finish(TEXT("No collision-free supported local flight fixture on surveyed road37..69"));
   Heli->SetActorLocationAndRotation(Center,Yaw,false,nullptr,ETeleportType::TeleportPhysics);++AircraftSetups;
   Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);Heli->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   const FVector Side=Center+Heli->GetActorRightVector()*240;FHitResult Entry;
   if(!Floor(Side,Entry))return Finish(TEXT("No native player entry support beside local aircraft"));
   Player->SetActorLocation(Entry.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();At=Now;Stage=3;return false;
  }
  if(Stage==3)
  {
   if(Now-At<1.5f)return false;
   PC->SetControlRotation((Heli->GetActorLocation()+FVector(0,0,25)-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Heli)return Finish(TEXT("Native F focus failed at local aircraft"));
   Key(IE_Pressed);Pressed=true;At=Now;Stage=4;return false;
  }
  if(Stage==4)
  {
   if(Now-At<.25f)return false;Key(IE_Released);Pressed=false;
   if(PC->GetPawn()!=Heli || Heli->GetAircraftInteraction()->GetDriver()!=Player || !Heli->GetAircraftMovement()->IsEngineRunning())return Finish(TEXT("Native F did not start/possess local aircraft"));
   FlightStart=Last=Heli->GetActorLocation();InitialHealth=Heli->GetHealthComponent()->GetCurrentHealth();At=Now;Stage=5;return false;
  }
  if(PC->GetPawn()!=Heli || Heli->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5f)return Finish(TEXT("Native local flight lost pilot or damaged aircraft"));
  FlightDistance+=FVector::Dist(Last,Heli->GetActorLocation());Last=Heli->GetActorLocation();
  if(Stage==5)
  {
   Submit(FVector(0,0,1),0,.8f);PC->SetControlRotation(FRotator(-20,Heli->GetActorRotation().Yaw,0));
   if(Heli->GetActorLocation().Z-FlightStart.Z>10000){FlightOrigin=Heli->GetActorLocation();At=Now;Stage=6;}
   else if(Now-At>35)return Finish(TEXT("Native vertical input did not take off"));
   return false;
  }
  if(Stage==6)
  {
   // Outbound/inbound across the same forest/road region at100m above T0.
   const float Elapsed=Now-At;const FVector Axis(1,0,0);
   const FVector Goal=FlightOrigin+Axis*(Elapsed<20?20000:0);
   FlyTo(Goal,DT);PC->SetControlRotation((FlightStart+Axis*10000-Heli->GetActorLocation()).Rotation());
   if(Visual && !CaptureMoving(TEXT("Flight"),Elapsed,10,FlightFrames))return Finish(TEXT("Incomplete flight temporal evidence"));
   if(Elapsed<42)return false;
   if(FlightDistance<15000 || MaximumGrass==0)return Finish(TEXT("Local motion diagnostic lacks150m actual flight or visible grass"));
   if(Visual && (WalkFrames!=18 || FlightFrames!=18 || CaptureRows.Num()!=36))return Finish(TEXT("Final temporal evidence requires all36 real motion frames"));
   return Finish(TEXT(""));
  }
  return false;
 }
private:
 bool CaptureMoving(const TCHAR* Name,float Elapsed,float First,int32& Frame)
 {
  const float Local=Elapsed-First;
  // Target6Hz. Allow bounded encoding/readback latency without inventing
  // missing frames; every sample keeps its actual world/camera timestamp.
  if(Local>=6)return Frame==18;
  if(Local<0 || Frame>=18 || Local<Frame/6.f)return true;
  const FString File=FString::Printf(TEXT("MotionTemporal908-%s-%02d.jpg"),Name,Frame);
  if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,File))return false;
  FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("image"),File);Row->SetStringField(TEXT("stage"),Name);Row->SetNumberField(TEXT("stage_seconds"),Elapsed);Row->SetNumberField(TEXT("world_seconds"),W->GetTimeSeconds());Row->SetStringField(TEXT("pawn_cm"),PC->GetPawn()->GetActorLocation().ToString());Row->SetStringField(TEXT("camera_cm"),Eye.ToString());Row->SetStringField(TEXT("camera_rotation"),Rotation.ToString());CaptureRows.Add(MakeShared<FJsonValueObject>(Row));++Frame;return true;
 }
 bool Floor(FVector P,FHitResult& H) const
 {FCollisionQueryParams Q(SCENE_QUERY_STAT(Motion784Floor),false,Player);Q.AddIgnoredActor(Heli);return W->LineTraceSingleByChannel(H,P+FVector(0,0,500),P-FVector(0,0,1500),ECC_Pawn,Q) && H.ImpactNormal.Z>.7;}
 void Key(EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,E,FPlatformTime::Cycles64()));}
 void Submit(FVector Input,float Yaw,float Power){Heli->SubmitFlightInput_Implementation(PC,Input,Yaw);Heli->SubmitPowerInput(PC,Power);}
 void FlyTo(FVector Goal,float DT)
 {
  const FVector Error=Goal-Heli->GetActorLocation(),V=Heli->GetVelocity(),Horizontal(V.X,V.Y,0);const auto* D=Heli->GetProfile();
  const float Power=Heli->GetAircraftMovement()->GetCurrentPower();const float Boost=FMath::Clamp((Power-D->HoverPower)/(1-D->HoverPower),0.f,1.f);
  const float Accel=D->Acceleration*100*FMath::Lerp(1.f,D->FastFlightAccelerationScale,Boost);
  const FVector Desired=(FVector(Error.X,Error.Y,0)*.55f).GetClampedToMaxSize(1800);
  FVector Input=FRotator(0,Heli->GetActorRotation().Yaw,0).UnrotateVector(((Desired-Horizontal)*.9f+Horizontal*(D->AirResistance+.05f))/Accel).GetClampedToMaxSize(1);
  Input.Z=FMath::Clamp(Error.Z/(D->VerticalSpeed*150.f),-1.,1.);
  Submit(Input,0,Power<.85f?.45f:0);
 }
 void Sample()
 {
  const FString Name=Stage<2?TEXT("Walk_Warmup"):Stage==2?TEXT("Walk_Moving"):Stage<5?TEXT("Flight_Fixture"):Stage==5?TEXT("Flight_Takeoff"):TEXT("Flight_Moving");
  Metrics.Sample(W,Name);const FVector P=PC->GetPawn()->GetActorLocation();
  int32 Counts[3]={Grass->GetActiveCellCount(0),Grass->GetActiveCellCount(1),Grass->GetActiveCellCount(2)};MaximumGrass=FMath::Max(MaximumGrass,Counts[0]+Counts[1]+Counts[2]);
  Rows+=FString::Printf(TEXT("%.6f,%s,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%d,%d,%d\n"),W->GetTimeSeconds(),*Name,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GGameThreadWaitTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),P.X,P.Y,P.Z,Counts[0],Counts[1],Counts[2]);
 }
 bool Finish(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);if(Pressed && PC)Key(IE_Released);
  if(PC && Heli && PC->GetPawn()==Heli)Submit(FVector::ZeroVector,0,0);
  if(Player)Player->ConsumeMovementInputVector();Metrics.Flush();
  for(const auto& E:OldPolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*E.Key))C->Set(E.Value,ECVF_SetByConsole);
  if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  if(!Directory.IsEmpty())
  {
   FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("motion-cpu.csv")));
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("diagnostic_flow_passed"),Error.IsEmpty());J->SetBoolField(TEXT("performance_pass"),false);J->SetStringField(TEXT("failure"),Error);
   J->SetStringField(TEXT("scope"),Visual?TEXT("Two disclosed local fixtures, normal character/flight input after each T0.36 captured motion frames for temporal/disocclusion review only; readback invalidates performance acceptance. No journey acceptance."):TEXT("Two independent local walking/flight diagnostic fixtures. No journey acceptance. Native input after each declared T0; no image readback or system power changes. Performance requires separate trace/CSV review."));
   J->SetBoolField(TEXT("image_readback"),Visual);J->SetBoolField(TEXT("visual_collection_passed"),Visual && Error.IsEmpty() && CaptureRows.Num()==36);J->SetArrayField(TEXT("motion_frames"),CaptureRows);
   J->SetNumberField(TEXT("walk_cm"),WalkDistance);J->SetNumberField(TEXT("flight_cm"),FlightDistance);J->SetNumberField(TEXT("aircraft_initial_setups"),AircraftSetups);J->SetNumberField(TEXT("maximum_visible_grass_cells"),MaximumGrass);J->SetNumberField(TEXT("airborne_walk_frames"),AirborneWalkFrames);
   J->SetBoolField(TEXT("flight_only"),FlightOnly);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("motion-scope.json")));
  }
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityHelicopterPrototype* Heli=nullptr;
 USplineComponent* Road=nullptr;UNeonCityReferenceGrassComponent* Grass=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 FNeonCityLandFrameMetrics Metrics{TEXT("IslandMotion784")};TMap<FString,float> OldPolicy;FString Directory,Rows;FTransform OriginalPlayer,OriginalHeli;
 FVector WalkStart,FlightStart,FlightOrigin,Last;double Started=0,WalkDistance=0,FlightDistance=0;float At=0,Previous=0,StartDistance=0,InitialHealth=0,OldHour=0;
 int32 Stage=0,AircraftSetups=0,MaximumGrass=0,AirborneWalkFrames=0,WalkFrames=0,FlightFrames=0;bool Pressed=false,OldCycle=false,FlightOnly=false,Visual=false;
 TArray<TSharedPtr<FJsonValue>> CaptureRows;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandMotion784,"NeonCity.Phase205c.Estate.Standalone.IslandMotion784",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FIslandMotion784::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"))){AddError(TEXT("Diagnostic requires metrics, disabled persistence and no readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(IslandMotion784::FFlow(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandMotionTemporal908,"NeonCity.Phase205c.Estate.Standalone.MotionTemporalVisual908",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FIslandMotionTemporal908::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityMotionFlightOnly"))){AddError(TEXT("Temporal evidence requires native isolated walk/flight, real captures and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(IslandMotion784::FFlow(this,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandMotionPIE846,"NeonCity.Phase205c.Estate.IslandMotionPIE846",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandMotionPIE846::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"))){AddError(TEXT("Moving PIE diagnosis requires metrics, disabled persistence and no readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandMotion784::FFlow(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
