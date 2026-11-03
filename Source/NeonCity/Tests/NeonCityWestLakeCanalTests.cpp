#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicles/Marine/NeonCityLamborghini63.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Waterways/NeonCityBasculeBridge.h"
#include "World/Waterways/NeonCityShipLock.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"

namespace NeonCityWestLakeCanal
{
void Key(APlayerController* PC,FKey K){for(EInputEvent E:{IE_Pressed,IE_Released})PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityCanalCapture")))return;
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/Canal");IFileManager::Get().MakeDirectory(*Dir,true);
 FScreenshotRequest::RequestScreenshot(Dir/Name,false,false);
}
float WaterAt(UWorld* W,const FVector& P){float Z=0;FVector C;return W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(P,Z,C)?Z:-1e6f;}

/** Lake to sea through the city: the Lamborghini 63, under an autopilot holding the canal's axis, leaves the lake mouth;
 * each bascule bridge warns, raises both leaves for her and lowers them behind her; she stops in the lock, which
 * closes its upper gate, drains the chamber to sea level and opens the sea gate; she steams out past the jetties. */
class FVoyage:public IAutomationLatentCommand
{
public:
 explicit FVoyage(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(FPlatformTime::Seconds()-Started>1500 || (Stage>0 && W->GetTimeSeconds()-Begun>480))
  {Test->AddError(FString::Printf(TEXT("Canal voyage timed out at stage %d, yacht at %s"),Stage,Yacht.IsValid()?*Yacht->GetActorLocation().ToCompactString():TEXT("-")));return true;}
  if(Stage==0)
  {
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(!It->ActorHasTag(TEXT("WL.Leisure")))Yacht=*It;      // the player's own 63, not the boat club's
   for(TActorIterator<ANeonCityBasculeBridge> It(W);It;++It)Bridges.Add(*It);
   for(TActorIterator<ANeonCityShipLock> It(W);It;++It)Lock=*It;
   if(!Test->TestNotNull(TEXT("swan yacht"),Yacht.Get()) || !Test->TestNotNull(TEXT("ship lock"),Lock.Get()) || !Test->TestEqual(TEXT("bridges over the canal (promenade + three roads)"),Bridges.Num(),4))return true;
   Bridges.Sort([](const TWeakObjectPtr<ANeonCityBasculeBridge>& A,const TWeakObjectPtr<ANeonCityBasculeBridge>& B){return A->GetActorLocation().X>B->GetActorLocation().X;});
   MaxOpen.Init(0.f,Bridges.Num());Closed.Init(false,Bridges.Num());
   AxisY=Bridges[0]->GetActorLocation().Y;
   // into the canal mouth, heading west
   const FVector Start(-1000,AxisY,0);
   Yacht->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);Yacht->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Yacht->SetActorLocationAndRotation(FVector(Start.X,Start.Y,WaterAt(W,Start)),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
   Yacht->GetMarineMovement()->StartEngine();
   Cam=W->SpawnActor<ACameraActor>();PC->SetViewTarget(Cam.Get());
   UpperZ=WaterAt(W,Start);Begun=W->GetTimeSeconds();Home=W;
   return Next(1);
  }
  // a PIE session torn down under the check (a previous test's world, a crash) ends it instead of crashing it
  if(W!=Home.Get() || !Yacht.IsValid() || !Lock.IsValid()){Test->AddError(TEXT("Canal voyage lost its PIE world"));return true;}
  if(W->GetTimeSeconds()-LastLog>20)
  {
   LastLog=W->GetTimeSeconds();
   Test->AddInfo(FString::Printf(TEXT("t %.0f stage %d yacht %s speed %.0f, bridges %d/%d/%d lock %d level %.0f"),LastLog-Begun,Stage,*Yacht->GetActorLocation().ToCompactString(),
    Yacht->GetVelocity().Size2D(),int32(Bridges[0]->GetState()),int32(Bridges[1]->GetState()),int32(Bridges[2]->GetState()),int32(Lock->GetPhase()),Lock->GetLevel()));
  }
  if(auto* C=Cam.Get())
  {
   // a chase view from astern and above for the stills (the unpossessed yacht's own camera rig is not running)
   const FVector Eye=Yacht->GetActorTransform().TransformPosition(FVector(-2600,-1300,900));
   C->SetActorLocationAndRotation(Eye,(Yacht->GetActorTransform().TransformPosition(FVector(800,0,150))-Eye).Rotation());
  }
  const float Dt=W->GetDeltaSeconds();
  const FVector P=Yacht->GetActorLocation();const float Speed=FVector::DotProduct(Yacht->GetVelocity(),Yacht->GetActorForwardVector());
  MaxOff=FMath::Max(MaxOff,FMath::Abs(P.Y-AxisY));MaxDt=FMath::Max(MaxDt,Dt);if(Dt>.25f)++SlowFrames;
  for(int32 I=0;I<Bridges.Num();++I)
  {
   MaxOpen[I]=FMath::Max(MaxOpen[I],Bridges[I]->GetOpenFraction());
   if(MaxOpen[I]>=1.f && P.X<Bridges[I]->GetActorLocation().X-9000 && Bridges[I]->GetState()==ENeonCityBridgeState::Closed)Closed[I]=true;
  }
  // autopilot: hold the axis, speed by what lies ahead
  const float Bow=P.X-Yacht->GetProfile()->HullExtent.X-100;
  float Target=500;                                                                       // ~18 km/h
  for(const auto& B:Bridges)
  {
   const float Ahead=Bow-(B->GetActorLocation().X+1500);
   if(Ahead>0 && Ahead<9000 && B->GetState()!=ENeonCityBridgeState::Open)Target=FMath::Min(Target,FMath::Max(0.f,(Ahead-2500)*.06f));
  }
  const float Gate=Lock->GetActorLocation().X+4300,Centre=Lock->GetActorLocation().X;   // upper gate (lock faces the sea, -X)
  if(Stage==1)
  {
   // toward the lock: stop with her centre on the chamber's
   Target=FMath::Min(Target,FMath::Max(0.f,(P.X-Centre)*.05f));
   if(FMath::Abs(P.X-Centre)<600 && FMath::Abs(Speed)<120)
   {Shot(TEXT("voyage_02_in_lock"));Stage=2;At=W->GetTimeSeconds();}
   if(!bShotBridge && Bridges[0]->GetOpenFraction()>=1.f && Bow<Bridges[0]->GetActorLocation().X+3000){Shot(TEXT("voyage_01_bridge_open"));bShotBridge=true;}
  }
  if(Stage==2)
  {
   Target=0;
   MinLevel=FMath::Min(MinLevel,Lock->GetLevel());
   if(!bShotDrain && Lock->GetPhase()==ENeonCityLockPhase::Draining && W->GetTimeSeconds()-At>12){Shot(TEXT("voyage_03_draining"));bShotDrain=true;}
   if(Lock->IsLowerGateOpen() && Lock->GetPhase()==ENeonCityLockPhase::Ready)
   {
    Test->TestTrue(FString::Printf(TEXT("chamber drained to sea level (%.0f cm)"),Lock->GetLevel()),FMath::Abs(Lock->GetLevel()-(-130.f))<5);
    Test->TestTrue(FString::Printf(TEXT("yacht lowered with the water (%.0f cm)"),UpperZ-P.Z),UpperZ-P.Z>45);
    Stage=3;At=W->GetTimeSeconds();
   }
  }
  if(Stage==3)
  {
   Target=500;
   if(P.X<-60000){SeaMinZ=FMath::Min(SeaMinZ,P.Z);SeaMaxZ=FMath::Max(SeaMaxZ,P.Z);}
   if(P.X<-66000)
   {
    Shot(TEXT("voyage_04_at_sea"));
    for(int32 I=0;I<Bridges.Num();++I)
     Test->TestTrue(FString::Printf(TEXT("bridge at x %.0f m raised for her"),Bridges[I]->GetActorLocation().X/100),MaxOpen[I]>=1.f);
    Test->TestTrue(FString::Printf(TEXT("held the canal axis (max %.1f m off)"),MaxOff/100),MaxOff<450);
    Test->TestTrue(TEXT("first bridge lowered again behind her"),Closed[0]);
    Test->TestTrue(FString::Printf(TEXT("rides the sea's swell outside the lock (heave %.0f cm)"),SeaMaxZ-SeaMinZ),SeaMaxZ-SeaMinZ>8);
    Test->TestTrue(FString::Printf(TEXT("frames stayed light through the voyage (%d over 0.25 s, worst %.3f s)"),SlowFrames,MaxDt),SlowFrames<=3);
    Test->AddInfo(FString::Printf(TEXT("lake to sea in %.0f s"),W->GetTimeSeconds()-Begun));
    return true;
   }
  }
  const float Steer=FMath::Clamp((P.Y-AxisY)/100.f*.25f+FMath::FindDeltaAngleDegrees(Yacht->GetActorRotation().Yaw,180.f)*.12f,-1.f,1.f);
  const float Throttle=FMath::Clamp((Target-Speed)*.01f,-1.f,1.f);
  Yacht->GetMarineMovement()->SetInput(Throttle,Steer,0);
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 FAutomationTestBase* Test;UWorld* W=nullptr;double Started=0,At=0,Begun=0,LastLog=0;int32 Stage=0;
 float AxisY=0,UpperZ=0,MaxOff=0,MinLevel=0,MaxDt=0,SeaMinZ=1e9f,SeaMaxZ=-1e9f;int32 SlowFrames=0;bool bShotBridge=false,bShotDrain=false;
 TArray<float> MaxOpen;TArray<bool> Closed;TWeakObjectPtr<ACameraActor> Cam;TWeakObjectPtr<UWorld> Home;
 TWeakObjectPtr<ANeonCityLamborghini63> Yacht;TWeakObjectPtr<ANeonCityShipLock> Lock;TArray<TWeakObjectPtr<ANeonCityBasculeBridge>> Bridges;
};

/** Running the Lamborghini 63 into a quay must not stall the world: she is driven at the Hubin promenade's revetment
 * (and held against it) while the frame time is watched. A big hull pressed on revetment / slab trimeshes once held
 * every frame at the 0.4 s clamp; the shore meshes have box collision since. */
class FQuay:public IAutomationLatentCommand
{
public:
 explicit FQuay(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(TEXT("Quay contact check timed out"));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;if(!W || W->GetTimeSeconds()<4)return false;
  if(!Yacht.IsValid())
  {
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(!It->ActorHasTag(TEXT("WL.Leisure")))Yacht=*It;      // the player's own 63, not the boat club's
   if(!Test->TestNotNull(TEXT("swan yacht"),Yacht.Get()))return true;
   Yacht->GetMarineMovement()->StartEngine();At=W->GetTimeSeconds();return false;
  }
  const double E=W->GetTimeSeconds()-At;
  // shove her sideways onto the promenade (port side, local -Y) at a few km/h and keep pushing
  Yacht->GetHull()->AddForce(-Yacht->GetActorRightVector()*Yacht->GetHull()->GetMass()*300.f);
  Yacht->GetMarineMovement()->SetInput(.3f,-1.f,0);
  if(E>3){Worst=FMath::Max(Worst,W->GetDeltaSeconds());Sum+=W->GetDeltaSeconds();++Frames;}
  if(E<12)return false;
  const float Avg=Frames?Sum/Frames:1.f;
  Test->AddInfo(FString::Printf(TEXT("against the quay: %d frames, average %.3f s, worst %.3f s"),Frames,Avg,Worst));
  Test->TestTrue(FString::Printf(TEXT("pressed against the quay the world keeps its frame rate (average %.3f s)"),Avg),Avg<.1f);
  return true;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;double Started=0,At=0;float Worst=0,Sum=0;int32 Frames=0;
 TWeakObjectPtr<ANeonCityLamborghini63> Yacht;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeYachtQuayPIE,"NeonCity.WestLake.YachtQuayContactPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeYachtQuayPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeCanal::FQuay(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeCanalVoyagePIE,"NeonCity.WestLake.CanalVoyagePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeCanalVoyagePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeCanal::FVoyage(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
