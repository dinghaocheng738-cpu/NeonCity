#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandBird.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
namespace
{
class FBirdHabitatCheck:public IAutomationLatentCommand
{
public:
 explicit FBirdHabitatCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Ready)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-173000,200))))return false;
   for(TActorIterator<AIslandBird> It(W);It;++It)if(It->ActorHasTag(TEXT("PolishBird")))Birds.Add(*It);
   if(!Test->TestEqual(TEXT("16 permanent birds in authored habitats"),Birds.Num(),16))return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(10);}
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=60;PC->SetViewTarget(Camera);
   At=W->GetTimeSeconds();Ready=true;
  }
  bool All=true;int32 Visible=0,Complete=0;
  for(auto* B:Birds)
  {
   Visible+=B->Bird->IsVisible()?1:0;Complete+=B->GetCompletedFlights()>0?1:0;
   All &= B->Bird->IsVisible() && B->GetCompletedFlights()>0;
   if(B->Bird->IsVisible())
   {
    FWaterSurfaceSample Water;
    if(W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(B->GetActorLocation(),Water) && B->GetActorLocation().Z<Water.SurfacePoint.Z)
    {Test->AddError(TEXT("Bird entered water: ")+B->GetActorLabel());return true;}
    if(const FVector* Previous=Positions.Find(B);Previous && FVector::Distance(*Previous,B->GetActorLocation())>250)
    {Test->AddError(TEXT("Discontinuous bird movement: ")+B->GetActorLabel());return true;}
    Positions.Add(B,B->GetActorLocation());
   }
  }
  const float E=W->GetTimeSeconds()-At;
  const int32 Subject=FMath::Clamp(int32(E/6)%Birds.Num(),0,Birds.Num()-1);const FVector Target=Birds[Subject]->GetActorLocation();
  const FVector Offset(550,600,400);Camera->SetActorLocation(Target+Offset);Camera->SetActorRotation((-Offset).Rotation());
  if(E>8 && !Shot){Shot=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdHabitat-Coast.png"));}
  if(All && E>10){Test->AddInfo(FString::Printf(TEXT("%d visible birds / %d complete outdoor flight cycles"),Visible,Complete));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdHabitat-Complete.png"));return true;}
  if(FPlatformTime::Seconds()-Started>140)
  {
   for(auto* B:Birds)if(!B->Bird->IsVisible() || B->GetCompletedFlights()==0)Test->AddError(FString::Printf(TEXT("Habitat rejected %s visible %d state %d at %s perch %s"),*B->GetActorLabel(),B->Bird->IsVisible(),int(B->GetBirdState()),*B->GetActorLocation().ToString(),*B->GetPerch().ToString()));
   return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0;bool Ready=false,Shot=false;ACameraActor* Camera=nullptr;TArray<AIslandBird*> Birds;TMap<AIslandBird*,FVector> Positions;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBirdHabitatPIE,"NeonCity.Polish.BirdHabitatPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBirdHabitatPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FBirdHabitatCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
