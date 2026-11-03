#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Water/CoastalWaveData.h"
#include "Water/CoastalOceanSurface.h"
#include "Water/TidewaterWaterSample.h"
#include "NeonCityOceanFFT.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/NeonCityWorldManager.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Components/BoxComponent.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Editor.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionClockTest,"NeonCity.Immersion.DaylightClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionClockTest::RunTest(const FString&)
{
 auto Advance=[](float H,float DT){return ANeonCityDayNightCycle::AdvanceClock(H,DT,3600,.75f);};
 TestNearlyEqual(TEXT("45 real minutes of daylight"),Advance(6,2700),18.f,.001f);
 TestNearlyEqual(TEXT("15 real minutes of night including midnight"),Advance(18,900),6.f,.001f);
 TestNearlyEqual(TEXT("Whole cycle does not drift"),Advance(10,3600),10.f,.001f);
 TestNearlyEqual(TEXT("Large time step spans several cycles"),Advance(10,10800),10.f,.001f);
 const float Before=Advance(6-.01f,.1f)-(6-.01f),After=Advance(6+.01f,.1f)-(6+.01f);
 TestTrue(TEXT("No dawn rate jump"),FMath::Abs(Before-After)<.0001f);
 TestEqual(TEXT("Invalid time delta leaves clock alone"),Advance(10,std::numeric_limits<float>::infinity()),10.f);
 TestNearlyEqual(TEXT("Uniform 50 percent mode remains available"),ANeonCityDayNightCycle::AdvanceClock(6,1800,3600,.5f),18.f,.001f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionSpectrumTest,"NeonCity.Immersion.WaveField",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionSpectrumTest::RunTest(const FString&)
{
 auto* Field=LoadObject<UCoastalWaveData>(nullptr,TEXT("/Game/NeonCity/Art/Immersion/Ocean/DA_CoastalWaves"));
 if(!TestNotNull(TEXT("Authored native Tidewater profiles"),Field) || !TestTrue(TEXT("Bounded finite field contract"),Field->IsValidField()))return false;
 TestEqual(TEXT("No runtime traces or collision dependency in baked field"),Field->DepthCm.Num(),513*513);
 TestEqual(TEXT("Immutable source profiles retain packed shader layout"),Field->LightProfile.Num(),224);
 TestTrue(TEXT("Calm differs from wind-driven original spectrum"),Field->CalmProfile!=Field->LightProfile);
 TestTrue(TEXT("Moderate uses distinct original spectrum"),Field->ModerateProfile!=Field->LightProfile);
 auto* Corrupt=DuplicateObject<UCoastalWaveData>(Field,GetTransientPackage());
 const float Invalid=std::numeric_limits<float>::infinity();
 FMemory::Memcpy(Corrupt->LightProfile.GetData()+STRUCT_OFFSET(FNeonCityOceanParameters,SysA),&Invalid,sizeof(Invalid));
 TestFalse(TEXT("Malformed packed spectrum is rejected before reaching GPU"),Corrupt->IsValidField());
 AddInfo(TEXT("Exact profile and GPU parity are independently covered by Tidewater ocean source-oracle tests."));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionReadbackTest,"NeonCity.Immersion.ReadbackContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionReadbackTest::RunTest(const FString&)
{
 FTidewaterWaterSample S;
 // A fixed slope traversed horizontally is not vertically rising water.
 const FVector3f Normal=FVector3f(-.2f,1,0).GetSafeNormal();
 for(int32 I=0;I<100;++I)
 {
  S.Advance(.02);S.Accept(FVector4f(I*.004f,Normal.X,Normal.Z,-500),FVector2D(I*.02,0),I*.02);
 }
 TestTrue(TEXT("Horizontal slope motion does not kick the hull"),FMath::Abs(S.Vertical)<.0001);
 S=FTidewaterWaterSample();double MaxError=0,MaxJump=0;
 for(int32 I=0;I<900;++I)
 {
  const double T=I/60.;S.Advance(1./60.);
  // Delayed results at irregular 1-3 frame intervals, matching the source bridge.
  if(I%3==0 || I%7==0)
  {
   const double Before=S.Effective;const bool Had=S.Valid;
   S.Accept(FVector4f(.6*FMath::Sin((T-.05)*1.4),0,0,-500),FVector2D::ZeroVector,T-.05);
   if(Had)MaxJump=FMath::Max(MaxJump,FMath::Abs(S.Effective-Before));
  }
  if(I>60)MaxError=FMath::Max(MaxError,FMath::Abs(S.Effective-.6*FMath::Sin(T*1.4)));
 }
 TestTrue(TEXT("New readback is height-continuous"),MaxJump<1e-7);
 TestTrue(TEXT("Delayed wave retains bounded amplitude/phase error"),MaxError<.2);
 const double Before=S.Effective;
 TestFalse(TEXT("Out-of-order readback rejected"),S.Accept(FVector4f(99,0,0,0),FVector2D::ZeroVector,S.Time-1));
 TestEqual(TEXT("Rejected readback preserves good water"),S.Effective,Before);
 TestFalse(TEXT("Non-finite readback rejected"),S.Accept(FVector4f(std::numeric_limits<float>::infinity(),0,0,0),FVector2D::ZeroVector,S.Time+1));
 S.Advance(5);const double Held=S.Effective;S.Advance(5);
 TestTrue(TEXT("GPU stall cannot extrapolate indefinitely"),FMath::Abs(S.Effective-Held)<.0001);
 FTidewaterWaterSample Relocated;
 Relocated.Accept(FVector4f(0,0,0,0),FVector2D::ZeroVector,1.);
 Relocated.Accept(FVector4f(.2f,0,0,0),FVector2D(.1,0),1.02);
 TestTrue(TEXT("Continuous wave has measured vertical velocity"),Relocated.Vertical>0);
 Relocated.Accept(FVector4f(1,0,0,0),FVector2D(10,0),1.04);
 TestEqual(TEXT("Nearby relocation resets wave velocity"),Relocated.Vertical,0.);
 TestEqual(TEXT("Relocation does not carry old height-offset spring"),Relocated.Effective,1.);
 AddInfo(FString::Printf(TEXT("15s synthetic delayed wave: max height jump %.8f m, max tracking error %.4f m"),MaxJump,MaxError));
 return true;
}
namespace
{
class FImmersionBuoyancyCheck:public IAutomationLatentCommand
{
public:
 explicit FImmersionBuoyancyCheck(FAutomationTestBase* T,bool Deep=false):Test(T),DeepWater(Deep),Evidence(Deep?TEXT("ImmersionDeepOcean"):TEXT("ImmersionOcean")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Ocean hull response timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<12)return false;
  if(!Ocean.IsValid())
  {
   for(TActorIterator<ACoastalOceanSurface> It(W);It;++It)Ocean=*It;
   if(!Test->TestTrue(TEXT("Normal world activates real ocean"),Ocean.IsValid() && Ocean->IsReady()))return true;
   Test->TestTrue(TEXT("Native FFT async query reached game world"),Ocean->GetQuerySequence()>0);
   Test->TestTrue(TEXT("Bounded original CDLOD selection"),Ocean->GetSurfaceNodeCount()>0 && Ocean->GetSurfaceNodeCount()<=1500);
   int32 HiddenPlanes=0;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
    if(auto* M=It->GetStaticMeshComponent()->GetMaterial(0);M && M->GetPathName().Contains(TEXT("M_SB205_CoastalWater")) && It->IsHidden())++HiddenPlanes;
   Test->TestEqual(TEXT("Old flat water replaced only after validated surface ready"),HiddenPlanes,1);
   for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It)
   {if(It->VehicleID==UMarineWorldSubsystem::SpeedboatID())Boats[0]=*It;if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boats[1]=*It;}
   FixtureOrigin=FVector(0,-25000,-130);
   if(!DeepWater)
   {
    // A real shallow coastal location, selected from the authored collision
    // bathymetry. No fake depth or alteration of the source wave amplitude.
    const auto* F=Ocean->GetField();bool Found=false;double Best=1e30;
    for(int32 Y=0;Y<F->GridSize;++Y)for(int32 X=0;X<F->GridSize;++X)
    {
     const FVector2D P=F->GridMinimum+(F->GridMaximum-F->GridMinimum)*FVector2D(double(X),double(Y))/(F->GridSize-1);
     if(F->GetDepth(P)<350 || F->GetDepth(P)>650)continue;
     bool Safe=true;
     for(int32 B=0;B<2;++B)for(int32 DX:{-1,1})for(int32 DY:{-1,1})
     {const float Depth=F->GetDepth(P+FVector2D(B*1800+DX*500,DY*500));Safe &= Depth>300 && Depth<1000;}
     const double Distance=(P-FVector2D(NeonCityCoastalLayout::IslandOrigin)).SizeSquared();
     if(Safe && Distance<Best){FixtureOrigin=FVector(P,F->SeaLevel);Best=Distance;Found=true;}
    }
    if(!Test->TestTrue(TEXT("Authored coastal bathymetry has a safe shallow-water fixture"),Found))return true;
   }
   Test->AddInfo(FString::Printf(TEXT("Actual %s fixture %s, seabed depth %.2f m"),DeepWater?TEXT("deep ocean"):TEXT("coastal"),*FixtureOrigin.ToString(),Ocean->GetField()->GetDepth(FVector2D(FixtureOrigin))*.01));
   for(int32 I=0;I<2;++I)
   {
    if(!Test->TestNotNull(TEXT("Existing stable-ID vessel"),Boats[I].Get()))return true;
    auto* B=Boats[I].Get();B->GetMarineMovement()->StopEngine();B->FindComponentByClass<UPhysicalImpactComponent>()->Suppress();
    B->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);B->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    B->SetActorLocationAndRotation(FixtureOrigin+FVector(I*1800,0,0),FRotator(0,25+I*55,0),false,nullptr,ETeleportType::TeleportPhysics);
    Health[I]=B->GetHealthComponent()->GetCurrentHealth();
   }
   // Explicit isolated measurement placement, not the normal no-teleport route.
   auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
   if(!Test->TestNotNull(TEXT("Normal player exists before measurement boarding"),Player))return true;
   Player->SetActorLocation(FixtureOrigin+FVector(-180,0,70),false,nullptr,ETeleportType::TeleportPhysics);
   if(!Test->TestTrue(TEXT("Existing enter-vehicle path"),Boats[0]->EnterVehicle_Implementation(Player)))return true;
   Boats[0]->GetMarineMovement()->StopEngine();
   PC->SetControlRotation(FRotator(-12,25,0));Ocean->SetSeaState(ECoastalSeaState::Light);
   Phase=W->GetTimeSeconds();return false;
  }
  const float Elapsed=W->GetTimeSeconds()-Phase;
  Evidence.Record(W,PC,Elapsed<12?0:1);
  if(Elapsed<12)return false;
  for(int32 I=0;I<2;++I)
  {
   const auto* B=Boats[I].Get();if(!B){Test->AddError(TEXT("Wave test lost vessel"));return true;}
   const FVector State(B->GetActorLocation().Z,B->GetActorRotation().Pitch,B->GetActorRotation().Roll);
   Minimum[I]=Minimum[I].ComponentMin(State);Maximum[I]=Maximum[I].ComponentMax(State);
   float H;FVector N,V;Ocean->Sample(B->GetActorLocation(),H,N,V);
   WaterMinimum[I]=FMath::Min(WaterMinimum[I],H);WaterMaximum[I]=FMath::Max(WaterMaximum[I],H);
  }
  if(!Captured && Elapsed>20){FNeonCityVisualEvidence::CaptureHUD(W,DeepWater?TEXT("Immersion-Breezy-DeepOcean.png"):TEXT("Immersion-Breezy-Coast.png"));Captured=true;}
  if(Elapsed<36)return false;
  for(int32 I=0;I<2;++I)
  {
   const FVector Range=Maximum[I]-Minimum[I];
   Test->AddInfo(FString::Printf(TEXT("%s sampled sea heave %.2f cm"),*Boats[I]->GetClass()->GetName(),WaterMaximum[I]-WaterMinimum[I]));
   if(DeepWater)
   {
    // Source Breezy includes long offshore swell. Preserve its real amplitude;
    // test the hull response against water motion, not the retired analytic sea.
    const float SeaRange=WaterMaximum[I]-WaterMinimum[I];
    Test->TestTrue(TEXT("Deep-water hull follows source swell without excess heave"),SeaRange>10 && Range.X>SeaRange*.5 && Range.X<SeaRange*1.35+50);
   }
   else Test->TestTrue(TEXT("Coastal hull heaves but remains within 1.8 m"),Range.X>5 && Range.X<180);
   Test->TestTrue(TEXT("Hull pitches with engine off"),Range.Y>.15 && Range.Y<(DeepWater?40:25));
   Test->TestTrue(TEXT("Hull rolls with engine off"),Range.Z>.15 && Range.Z<(DeepWater?40:25));
   Test->TestEqual(TEXT("Ordinary waves do not inflict collision damage"),Boats[I]->GetHealthComponent()->GetCurrentHealth(),Health[I]);
   Test->AddInfo(FString::Printf(TEXT("%s: 24 s source Breezy passive heave %.2f cm, pitch %.2f deg, roll %.2f deg"),*Boats[I]->GetClass()->GetName(),Range.X,Range.Y,Range.Z));
  }
  auto* Manager=W->GetSubsystem<UNeonCityWorldManager>();
  Ocean->SetSeaState(ECoastalSeaState::Moderate);const auto Snapshot=Manager->BuildPersistenceSnapshot();
  Ocean->SetSeaState(ECoastalSeaState::Calm);Manager->ApplyPersistenceSnapshot(Snapshot);
  Test->TestEqual(TEXT("World snapshot restores sea state"),Ocean->GetSeaState(),ECoastalSeaState::Moderate);
  auto Invalid=Snapshot;Invalid.CoastalSeaState=255;Manager->ApplyPersistenceSnapshot(Invalid);
  Test->TestEqual(TEXT("Invalid optional sea state safely falls back"),Ocean->GetSeaState(),ECoastalSeaState::Light);
  Test->AddInfo(TEXT("AUTOMATED fixture: normal authored world, two existing boats temporarily placed offshore. Not manual PIE; full shore-drive-fish-return route remains separate."));
  return true;
 }
private:
 FAutomationTestBase* Test;bool DeepWater=false;FVector FixtureOrigin;double Start=0;float Phase=0;bool Captured=false;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;TWeakObjectPtr<ANeonCityMarineVehicleBase> Boats[2];float Health[2]={};
 FVector Minimum[2]={FVector(1e9),FVector(1e9)},Maximum[2]={FVector(-1e9),FVector(-1e9)};
 float WaterMinimum[2]={1e9,1e9},WaterMaximum[2]={-1e9,-1e9};
 FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionBuoyancyTest,"NeonCity.Immersion.OceanBuoyancyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionBuoyancyTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FImmersionBuoyancyCheck>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionDeepBuoyancyTest,"NeonCity.Immersion.DeepOceanBuoyancyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionDeepBuoyancyTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FImmersionBuoyancyCheck>(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
