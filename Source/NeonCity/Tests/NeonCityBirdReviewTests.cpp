#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandBird.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
namespace
{
class FBirdCandidateReview:public IAutomationLatentCommand
{
public:
 explicit FBirdCandidateReview(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150){Test->AddError(TEXT("Bird candidate PIE timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-173000,200))))return false;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(10);}
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=35;PC->SetViewTarget(Camera);
   for(int32 I=0;I<2;++I)
   {
    const FString Name=I?TEXT("Shrike"):TEXT("Gull");const FString Path=TEXT("/Game/NeonCity/Art/Polish/ReviewedBirds/")+Name+TEXT("/SK_")+Name;
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*Path);auto* Anim=LoadObject<UAnimSequence>(nullptr,*(Path+TEXT("_Anim")));
    if(!Test->TestNotNull(TEXT("Candidate skeletal asset"),Mesh) || !Test->TestNotNull(TEXT("Real wing animation"),Anim))return true;
    Test->TestTrue(TEXT("Wing fold target imported"),Mesh->GetMorphTargets().Num()>0);
    for(const auto& Slot:Mesh->GetMaterials())if(!Test->TestNotNull(TEXT("Saved plumage material is assigned"),Slot.MaterialInterface.Get()))return true;
    FTransform T(FRotator(0,0,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173000+I*1200,180)));
    auto* B=W->SpawnActorDeferred<AIslandBird>(AIslandBird::StaticClass(),T);B->Bird->SetSkeletalMesh(Mesh);B->Flight=Anim;B->FootOffset=I?6:29;B->RestSeconds=8;B->OrbitRadius=800;B->FlightHeight=500;B->FlightSpeed=700;B->FinishSpawning(T);Birds.Add(B);
   }
   At=W->GetTimeSeconds();Stage=1;
  }
  if(Birds.IsEmpty())return true;
  auto* B=Birds[Stage==2?1:0];FVector Offset(0,520,180);
  Camera->GetCameraComponent()->FieldOfView=Stage==2?9:22;
  if(Stage==6){Offset=FVector(0,2000+(W->GetTimeSeconds()-At)*1500,800);Camera->GetCameraComponent()->FieldOfView=60;}
  Camera->SetActorLocation(B->GetActorLocation()+Offset);Camera->SetActorRotation((-Offset).Rotation());
  if(Stage==1 && W->GetTimeSeconds()-At>2)
  {if(!Test->TestTrue(TEXT("Perch is supported and bird visible"),B->Bird->IsVisible()))return true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Gull-Perch.png"));Stage=2;At=W->GetTimeSeconds();}
  else if(Stage==2 && W->GetTimeSeconds()-At>2)
  {FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Shrike-Perch.png"));Stage=3;At=W->GetTimeSeconds();}
  else if(Stage==3 && Birds[0]->GetBirdState()==EIslandBirdState::Cruise)
  {FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Gull-Flight.png"));Stage=4;At=W->GetTimeSeconds();}
  else if(Stage==4 && Birds[0]->GetCompletedFlights()>0 && Birds[1]->GetCompletedFlights()>0)
  {Test->TestTrue(TEXT("Both species complete takeoff, orbit, approach and landing in PIE"),true);Stage=5;At=W->GetTimeSeconds();for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(21);
   if(auto* P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn())){P->SetActorLocation(Birds[0]->GetPerch()+FVector(0,500,100),false,nullptr,ETeleportType::TeleportPhysics);PC->SetViewTarget(P);PC->SetControlRotation(FRotator(-15,-90,0));P->FindComponentByClass<UNeonCityFlashlightComponent>()->SetEquipped(true);}}
  else if(Stage==4 && W->GetTimeSeconds()-At>40)
  {for(auto* Bird:Birds)Test->AddError(FString::Printf(TEXT("Bird failed landing cycle: state %d position %s perch %s count %d"),int(Bird->GetBirdState()),*Bird->GetActorLocation().ToString(),*Bird->GetPerch().ToString(),Bird->GetCompletedFlights()));return true;}
  else if(Stage==5 && W->GetTimeSeconds()-At>20)
  {FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Night.png"));Stage=7;At=W->GetTimeSeconds();PC->SetViewTarget(Camera);}
  else if(Stage==7 && W->GetTimeSeconds()-At>5)
  {FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Night-Bird.png"));Stage=8;At=W->GetTimeSeconds();for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(10);}
  else if(Stage==8 && W->GetTimeSeconds()-At>20)
  {Stage=6;At=W->GetTimeSeconds();}
  else if(Stage==6)
  {
   const float E=W->GetTimeSeconds()-At;
   if(E>3 && !Mid){Mid=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Distance65m.png"));}
   if(E>9){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BirdReview-Distance155m.png"));return true;}
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0;int32 Stage=0;bool Mid=false;ACameraActor* Camera=nullptr;TArray<AIslandBird*> Birds;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBirdCandidatePIE,"NeonCity.Polish.BirdCandidatePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBirdCandidatePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(FBirdCandidateReview(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
