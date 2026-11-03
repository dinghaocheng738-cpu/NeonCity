#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandLandLight.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
namespace RidgePoolPresentation
{
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+X*100,-118800-Y*100,30480+Z*100));}
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>70){Test->AddError(TEXT("Pool presentation fixture unavailable"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!W || !PC || W->GetTimeSeconds()<4)return false;
  if(!Camera)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(M(0,0,0)))return false;
   auto* P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return false;
   // One stationary dry-deck fixture: entry/swim/exit PASS is cached separately.
   P->SetActorLocation(M(-7,21,4.25)+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();
   FWaterSurfaceSample Sample;
   Test->TestTrue(TEXT("Contained basin labels its optical medium independently of ocean"),W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(M(3,25,3.1),Sample) && Sample.ContainedWater);
   Test->TestTrue(TEXT("Open sea preserves its separate normal medium"),W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(FVector(10000,-10000,-100),Sample) && !Sample.ContainedWater);
   Camera=W->SpawnActor<ACameraActor>();Camera->SetActorLocation(M(-9,24,7));Camera->SetActorRotation((M(2,25,3.9)-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);
  }
  if(!Changed)
  {for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Index==0?12:Index==1?17.5:22);}At=W->GetTimeSeconds();Changed=true;return false;}
  // Normal histogram adaptation descends at roughly one EV per second. A
  // three-second noon-to-night cut captures stale exposure, not usable night.
  // Wait for genuine adaptation; do not fix EV, bias, bloom or camera settings.
  if(W->GetTimeSeconds()-At<(Index==2?20.f:5.f))return false;
  if(Index==2)
  {
   int32 Visible=0;float Lumens=0;
   for(TActorIterator<AIslandLandLight> It(W);It;++It)if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.PoolLighting")))
   {auto* Light=Cast<UPointLightComponent>(It->GetLightComponent());if(Light && Light->IsVisible() && Light->Intensity>1){++Visible;Lumens+=Light->Intensity;}}
   Test->TestTrue(TEXT("Actual local deck and basin lights are active at night"),Visible>=4 && Lumens>3000);
   TArray<FColor> Pixels;auto* View=W->GetGameViewport()->Viewport;
   if(View && View->ReadPixels(Pixels))
   {
    const FIntPoint Size=View->GetSizeXY();double Sum=0;int32 Count=0,Lit=0;
    for(int32 Y=Size.Y/4;Y<Size.Y*7/8;++Y)for(int32 X=Size.X/5;X<Size.X*4/5;++X)
    {const auto& C=Pixels[Y*Size.X+X];const double Value=(C.R+C.G+C.B)/3.;Sum+=Value;++Count;Lit+=Value>12;}
    Test->TestTrue(TEXT("Settled real night image contains illuminated deck/basin rather than isolated light dots"),Count>0 && Sum/Count>4 && double(Lit)/Count>.01);
   }
   else Test->AddError(TEXT("Normal night viewport readback unavailable"));
  }
  Test->TestTrue(TEXT("Native pool presentation captured"),FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Pool-Repair-%s.png"),Index==0?TEXT("Day"):Index==1?TEXT("Sunset"):TEXT("Night"))));
  Changed=false;return ++Index==3;
 }
private:FAutomationTestBase* Test;ACameraActor* Camera=nullptr;int32 Index=0;float At=0;double Start=0;bool Changed=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRidgePoolPresentation,"NeonCity.Phase205c.PoolPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRidgePoolPresentation::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(RidgePoolPresentation::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
