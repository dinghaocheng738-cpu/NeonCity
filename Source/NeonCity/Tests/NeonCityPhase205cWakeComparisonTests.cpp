#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Water/CoastalOceanSurface.h"
#include "Combat/HealthComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace WakeComparison
{
class FCapture:public IAutomationLatentCommand
{
public:
 explicit FCapture(FAutomationTestBase* T,bool StopOnly=false):Test(T),Evidence(TEXT("SternWakeNative")),Metrics(TEXT("SternWakeNative")),bStopOnly(StopOnly){}
 ~FCapture(){RemoveDriver();}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>300)return End(TEXT("Native wake sequence timed out"));
  if(!PC || W->GetTimeSeconds()<4)return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return End(TEXT("Normal player missing"));
   for(TActorIterator<ANeonCityFishingBoat> It(W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boat=*It;
   for(TActorIterator<ACoastalOceanSurface> It(W);It;++It)Ocean=*It;
   if(!Boat || !Ocean || !Ocean->IsReady())return false;
   const FVector Deck=Boat->GetActorTransform().TransformPosition(FVector(-75,0,35));FHitResult Floor;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(WakeDeckFixture),false,Player);
   if(!W->LineTraceSingleByChannel(Floor,Deck+FVector(0,0,18),Deck-FVector(0,0,25),ECC_Pawn,Q) || Floor.GetActor()!=Boat)return End(TEXT("Actual authored helm deck lacks a safe floor"));
   // One initial supported character fixture only. The existing physical
   // boat is never spawned, positioned, rotated, paused or given a velocity.
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   Health=Boat->GetHealthComponent()->GetCurrentHealth();Origin=Boat->GetActorLocation();
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("SternWakeNative");IFileManager::Get().MakeDirectory(*Directory,true);
   CSV=TEXT("game_seconds,wall_seconds,stage,x,y,z,yaw,speed_mps,forward_mps,thrust,target_throttle,steer,roll,pitch,spray_count,contact_bursts,physical_wake\n");
   return Next(1);
  }
  if(Stage==1 && Age>.7f)
  {
   const auto* Base=Cast<UPrimitiveComponent>(Player->GetMovementBaseObject());if(!Base || Base->GetOwner()!=Boat)return End(TEXT("Initial real deck fixture did not settle on the boat"));
   PC->SetControlRotation((Boat->GetActorTransform().TransformPosition(FVector(110,55,120))-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Boat)return End(TEXT("Native F helm interaction unavailable"));
   Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3f)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Boat || Boat->GetMarineMovement()->IsMoored())return End(TEXT("Native F did not take helm and release berth"));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);
   Desired=bStopOnly?8:2;Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FCapture::DriveFrame);return Next(bStopOnly?5:3);
  }
  if(Stage>=3)
  {
   Evidence.Record(W,PC,Stage);
   const auto* Spray=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
   if(!Spray || Spray->GetActiveSprayCount()!=0 || Spray->GetContactBurstCount()!=0)return End(TEXT("Boat bow/side spray regressed"));
   if(Boat->GetHealthComponent()->GetCurrentHealth()<Health-1 || FMath::Abs(Boat->GetActorRotation().Roll)>30)return End(TEXT("Continuous native wake drive suffered unsafe impact/attitude"));
   if(Capturing)
   {
    SetCamera(View);
    if(W->GetTimeSeconds()-ViewAt<1.3f)return false;
    const TCHAR* Views[]={TEXT("Player"),TEXT("SideRear"),TEXT("Top")};
    const FString Name=FString::Printf(TEXT("UE-Wake-%s-%s.png"),*Label(),Views[View]);
    if(!FNeonCityVisualEvidence::CaptureHUD(W,Name))return End(TEXT("Actual native wake image failed"));
    auto J=MakeShared<FJsonObject>();const auto Telemetry=Boat->GetMarineMovement()->GetTelemetry();
    J->SetStringField(TEXT("label"),Label());J->SetStringField(TEXT("view"),Views[View]);J->SetStringField(TEXT("image"),Name);
    J->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());J->SetNumberField(TEXT("actual_speed_mps"),Telemetry.SpeedKmh/3.6);
    J->SetNumberField(TEXT("actual_forward_mps"),Telemetry.ForwardSpeedKmh/3.6);J->SetNumberField(TEXT("yaw"),Boat->GetActorRotation().Yaw);
    J->SetNumberField(TEXT("seconds_since_actual_stop"),StoppedAt>0?W->GetTimeSeconds()-StoppedAt:0);
    J->SetNumberField(TEXT("horizontal_fov_degrees"),View==0?Boat->GetMarineCamera()->FieldOfView:Camera->GetCameraComponent()->FieldOfView);
    J->SetStringField(TEXT("actual_position_cm"),Boat->GetActorLocation().ToString());Captures.Add(MakeShared<FJsonValueObject>(J));
    ViewAt=W->GetTimeSeconds();if(++View<3)return false;
    Capturing=false;View=0;SaveFields();
    if(Stage==3){Desired=2;return Next(4);}if(Stage==4){Desired=8;return Next(5);}
    if(Stage==5){Turn=true;TurnStart=Boat->GetActorRotation().Yaw;return Next(6);}
    if(Stage==6){Turn=false;Desired=8;return Next(7);}if(Stage==7){Desired=0;return Next(8);}if(Stage==8)return Next(9);
    return End(TEXT(""));
   }
   SetCamera(0);
   const float Speed=Boat->GetMarineMovement()->GetTelemetry().ForwardSpeedKmh/3.6f;
   if(Stage==3 && Age>4 && Speed>1 && Speed<3.4f)return BeginCapture();
   if(Stage==4 && Age>1)return BeginCapture();
   if(Stage==5 && Age>7 && Speed>6.2f){if(bStopOnly){Desired=8;return Next(7);}return BeginCapture();}
   if(Stage==6 && Age>8)
   {if(FMath::Abs(FMath::FindDeltaAngleDegrees(TurnStart,Boat->GetActorRotation().Yaw))<15)return End(TEXT("Native A/D produced no real wake turn"));return BeginCapture();}
   if(Stage==7 && Age>(bStopOnly?4.f:.6f)){if(bStopOnly){Desired=0;return Next(8);}return BeginCapture();}
   if(Stage==8)
   {if(FMath::Abs(Speed)<.7f){if(!StoppedAt)StoppedAt=W->GetTimeSeconds();if(W->GetTimeSeconds()-StoppedAt>4)return BeginCapture();}else StoppedAt=0;}
   if(Stage==9 && W->GetTimeSeconds()-StoppedAt>24)return BeginCapture();
   if(Age>45)return End(FString::Printf(TEXT("Native stage %d target speed not reached; actual%.2fm/s"),Stage,Speed));
  }
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down)
 {bool& Previous=Keys.FindOrAdd(K);if(Previous!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Previous=Down;}}
 FString Label()const
 {const TCHAR* Names[]={TEXT("Slow"),TEXT("Accelerate"),TEXT("Fast"),TEXT("TurnHistory"),TEXT("Decelerate"),TEXT("Stop4s"),TEXT("StopDecay24s")};return Names[FMath::Clamp(Stage-3,0,6)];}
 bool BeginCapture(){if(!Ocean->HasPhysicalWakeFor(Boat))return End(TEXT("Actual source-derived physical wake not selected for driven boat"));Capturing=true;View=0;ViewAt=W->GetTimeSeconds();return false;}
 void SetCamera(int V)
 {
  // Player evidence is the normal native chase camera. Side/top comparison
  // uses the source's55-degree vertical lens converted to UE horizontal FOV.
  if(V==0){PC->SetViewTarget(Boat);return;}
  PC->SetViewTarget(Camera);
  Camera->GetCameraComponent()->FieldOfView=FMath::RadiansToDegrees(2*FMath::Atan(FMath::Tan(FMath::DegreesToRadians(55.f)*.5f)*(1600.f/900.f)));
  const FVector F=Boat->GetActorForwardVector().GetSafeNormal2D(),R=FVector(-F.Y,F.X,0),P=Boat->GetActorLocation();
  const FVector Aim=P-F*(V==2?2000:700);FVector Eye=P-F*(V==2?2000:V==1?1500:1700)+R*(V==1?2300:V==2?0:250);
  Eye.Z+=V==2?5500:V==1?800:580;Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
 }
 void DriveFrame(UWorld* World,ELevelTick,float)
 {
  if(World!=W || !Boat || PC->GetPawn()!=Boat)return;
  if(Stage==4)Desired=FMath::Lerp(2.f,8.f,FMath::Clamp((W->GetTimeSeconds()-At)/4,0.f,1.f));
  if(Stage==7)Desired=FMath::Lerp(8.f,0.f,FMath::Clamp((W->GetTimeSeconds()-At)/4,0.f,1.f));
  const auto T=Boat->GetMarineMovement()->GetTelemetry();const float Speed=T.ForwardSpeedKmh/3.6;
  // Space unloads the propeller; this displacement hull retains inertia.
  // Stop like an ordinary helm user: briefly counter-thrust with native S,
  // then neutral. Never change source hull forces or force a zero velocity.
  if(Stage>=8)
  {
   // Close brief native W/S pulses around measured thrust. A full reverse
   // key held until near zero overshot because propeller thrust fades slowly.
   const float Need=FMath::Clamp(-Speed*.18f,-.2f,.2f);
   const bool Forward=Need>T.SignedThrust+.025f,Reverse=Need<T.SignedThrust-.025f;
   Key(EKeys::W,Forward);Key(EKeys::S,Reverse);Key(EKeys::SpaceBar,!Forward && !Reverse);
  }
  else
  {
   Key(EKeys::W,Desired>0 && Speed<Desired+.15f);Key(EKeys::S,false);
   Key(EKeys::SpaceBar,Desired<=0 || Speed>Desired+.7f);
  }
  Key(EKeys::A,false);Key(EKeys::D,Turn);
  const auto* Spray=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();const FVector P=Boat->GetActorLocation();const FRotator Rot=Boat->GetActorRotation();
  CSV+=FString::Printf(TEXT("%.6f,%.6f,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d\n"),W->GetTimeSeconds(),FPlatformTime::Seconds()-Start,Stage,P.X,P.Y,P.Z,Rot.Yaw,T.SpeedKmh/3.6,Speed,T.SignedThrust,T.TargetThrottle,T.Steering,Rot.Roll,Rot.Pitch,Spray?Spray->GetActiveSprayCount():-1,Spray?Spray->GetContactBurstCount():-1,Ocean->HasPhysicalWakeFor(Boat));
  Metrics.Sample(W,Capturing?TEXT("ImageReadbackInterval"):Label());
 }
 void SaveFields()
 {
  for(const TCHAR* Name:{TEXT("WakeDisplay"),TEXT("WakeAeration")})
   if(const auto* Property=FindFProperty<FObjectProperty>(Ocean->GetClass(),Name))
    if(auto* Texture=Cast<UTextureRenderTarget2D>(Property->GetObjectPropertyValue_InContainer(Ocean)))
    {TArray<FFloat16Color> Pixels;if(Texture->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels))
      FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Pixels.GetData()),Pixels.Num()*sizeof(FFloat16Color)),*(Directory/(Label()+TEXT("-")+Name+TEXT("-512x512-rgba16f.bin"))));}
 }
 void RemoveDriver(){if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}}
 bool End(const FString& Failure)
 {
  RemoveDriver();if(PC)for(auto& K:Keys)if(K.Value)Key(K.Key,false);Metrics.Flush();
  if(PC && Boat && PC->GetPawn()==Boat)PC->SetViewTarget(Boat);if(Camera)Camera->Destroy();
  if(!Failure.IsEmpty())Test->AddError(Failure);
  if(!Directory.IsEmpty())
  {
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);
   J->SetStringField(TEXT("scope"),TEXT("Actual authored physical boat, native F helm and W/S/Space/A/D continuous feedback input. One initial supported player fixture; zero boat pose/velocity writes, no freeze, no catch/reward/save fixture. Native player chase camera; source55-degree vertical lens mapped to UE horizontal FOV for SideRear/Top. Native S counter-thrust stops the hull without changing source forces. Snapshots/continuous frames occur during continued simulation, not exact pixel/time equality with source analytic fixture. Six-scene performance is separate; image-readback/motion costs remain in raw metrics and cannot certify final performance."));
   J->SetStringField(TEXT("source_reference"),TEXT("Saved/Phase205cUpgrade/TidewaterSourceGpu01/ReferenceSequence01/receipt.json"));
   J->SetNumberField(TEXT("post_t0_boat_pose_writes"),0);J->SetNumberField(TEXT("initial_player_fixtures"),1);J->SetArrayField(TEXT("captures"),Captures);
   J->SetBoolField(TEXT("stop_only_minimal_reproduction"),bStopOnly);
   J->SetNumberField(TEXT("actual_distance_cm"),Boat?FVector::Dist2D(Origin,Boat->GetActorLocation()):0);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")));
  }
  return true;
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;FNeonCityLandFrameMetrics Metrics;UWorld* W=nullptr;APlayerController* PC=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;ANeonCityFishingBoat* Boat=nullptr;ACoastalOceanSurface* Ocean=nullptr;ACameraActor* Camera=nullptr;
 FDelegateHandle Driver;TMap<FKey,bool> Keys;TArray<TSharedPtr<FJsonValue>> Captures;FString Directory,CSV;FVector Origin;
 double Start=0;float At=0,ViewAt=0,StoppedAt=0,Desired=0,Health=0,TurnStart=0;int Stage=0,View=0;bool Turn=false,Capturing=false,bStopOnly=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeNativeComparisonTest,"NeonCity.Phase205c.SternWakeNativeComparisonPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWakeNativeComparisonTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WakeComparison::FCapture(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeNativeStopTest,"NeonCity.Phase205c.SternWakeStopOnlyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWakeNativeStopTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(WakeComparison::FCapture(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
