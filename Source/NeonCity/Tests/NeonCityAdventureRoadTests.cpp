#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleInteraction.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "Adventure/IslandAdventureLighting.h"
#include "Components/PostProcessComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
namespace
{
bool ReadAdventureRoute(TArray<FVector>& Points)
{
 FString Text;TSharedPtr<FJsonObject> Data;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/road-alignment.json")))
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return false;
 for(auto Row:Data->GetArrayField(TEXT("points")))
 {
  const auto P=Row->AsArray();if(P.Num()!=3)return false;
  Points.Add(FVector(NeonCityCoastalLayout::IslandOrigin.X+100*P[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*P[1]->AsNumber(),-130+100*P[2]->AsNumber()));
 }
 return Points.Num()>500;
}
class FAdventureRoadAudit:public IAutomationLatentCommand
{
public:
 explicit FAdventureRoadAudit(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>170){Test->AddError(TEXT("Road world audit timeout"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;
  if(Points.IsEmpty())
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(46550,-120050,31000))))return false;
   if(!Test->TestTrue(TEXT("Surveyed route exists"),ReadAdventureRoute(Points)))return true;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(AdventureRoadAudit),true,PC->GetPawn());
   Q.bReturnFaceIndex=true;
   int32 Gaps=0,Obstacles=0;float WorstHeight=0;FString FirstFailure;
   for(int32 I=0;I<Points.Num();++I)
   {
    FVector Tangent=(Points[FMath::Min(I+1,Points.Num()-1)]-Points[FMath::Max(I-1,0)]).GetSafeNormal2D();FVector N(-Tangent.Y,Tangent.X,0);
    for(float Lane:{-160.f,0.f,160.f})
    {
     FVector P=Points[I]+N*Lane;FHitResult Hit;
     const bool Found=W->LineTraceSingleByChannel(Hit,P+FVector(0,0,400),P-FVector(0,0,150),ECC_WorldStatic,Q);
     const float Error=Found?FMath::Abs(Hit.ImpactPoint.Z-P.Z):1000;WorstHeight=FMath::Max(WorstHeight,Error);
     if(I==Points.Num()/2 && Lane==0 && Hit.GetComponent())
     {
      int32 Section=INDEX_NONE;const auto* Mat=Hit.GetComponent()->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex,Section);
      Test->AddInfo(FString::Printf(TEXT("Actual carriageway material %s section %d face %d"),*GetNameSafe(Mat),Section,Hit.FaceIndex));
      Test->TestTrue(TEXT("Driving surface uses asphalt instead of structure concrete"),Mat && Mat->GetName()==TEXT("M_RoadAsphalt"));
     }
     if(!Found || Error>18){++Gaps;if(Gaps<=16)Test->AddInfo(FString::Printf(TEXT("Road support failure sample %d lane %.0f hit %s height error %.1f cm"),I,Lane,*GetNameSafe(Hit.GetActor()),Error));if(FirstFailure.IsEmpty())FirstFailure=FString::Printf(TEXT("sample %d lane %.0f at %s hit %s error %.1f"),I,Lane,*P.ToString(),*GetNameSafe(Hit.GetActor()),Error);}
    }
    if(I+1<Points.Num())
    {
     FHitResult Hit;
     if(W->SweepSingleByChannel(Hit,Points[I]+FVector(0,0,125),Points[I+1]+FVector(0,0,125),Tangent.Rotation().Quaternion(),ECC_WorldStatic,FCollisionShape::MakeBox(FVector(180,88,55)),Q))
     {++Obstacles;if(Obstacles<=16)Test->AddInfo(FString::Printf(TEXT("Road car clearance failure sample %d hit %s at %s"),I,*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString()));if(FirstFailure.IsEmpty())FirstFailure=FString::Printf(TEXT("car clearance sample %d hit %s"),I,*GetNameSafe(Hit.GetActor()));}
    }
   }
   Test->TestEqual(TEXT("All three lane traces retain continuous physical road support"),Gaps,0);
   Test->TestEqual(TEXT("Ordinary vehicle envelope clears route and invisible collision"),Obstacles,0);
   Test->AddInfo(FString::Printf(TEXT("%d route samples; worst road height error %.2f cm; %s"),Points.Num(),WorstHeight,*FirstFailure));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;PC->SetViewTarget(Camera);At=W->GetTimeSeconds();Place(W);return false;
  }
  if(W->GetTimeSeconds()-At<18)return false;
  if(auto* Local=PC->GetLocalPlayer())
  {
   auto* Client=W->GetGameViewport();FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport,W->Scene,Client->EngineShowFlags).SetRealtimeUpdate(true));
   FVector Location;FRotator Rotation;
   if(auto* View=Local->CalcSceneView(&Family,Location,Rotation,Client->Viewport))
   {
    const auto& S=View->FinalPostProcessSettings;
    Test->AddInfo(FString::Printf(TEXT("Mountain view %d: exposure multiplier %.6f luminance %.3f minEV %.2f maxEV %.2f bias %.2f"),Stage,View->GetLastEyeAdaptationExposure(),View->GetLastAverageSceneLuminance(),S.AutoExposureMinBrightness,S.AutoExposureMaxBrightness,S.AutoExposureBias));
   }
  }
  FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Adventure-Road-%02d.png"),Stage));
  if(++Stage==7){PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));PC->ConsoleCommand(TEXT("ShowFlag.VisualizeBuffer 0"));PC->ConsoleCommand(TEXT("viewmode lit"));return true;}
  Place(W);At=W->GetTimeSeconds();return false;
 }
 void Place(UWorld* W)
 {
  const int32 Index=Stage<3?TArray<int32>{20,Points.Num()/2,Points.Num()-6}[Stage]:Points.Num()-6;
  const auto P=Points[Index];const auto Ahead=Points[FMath::Min(Index+8,Points.Num()-1)];
  const FVector Eye=P+(Stage==2?FVector(1400,1200,1400):FVector(-650,650,380));
  Camera->SetActorLocation(Eye);Camera->SetActorRotation(((P+Ahead)*.5-Eye).Rotation());
  const float Hour=Stage==3?17.5f:Stage==4?22.f:12.f;
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Hour);}
  auto* PC=W->GetFirstPlayerController();
  if(Stage==0)
  {
   for(TActorIterator<AIslandAdventureLighting> It(W);It;++It)
   {
    auto* Post=It->FindComponentByClass<UPostProcessComponent>();float Dist=-1;
    const bool Contains=Post->EncompassesPoint(Eye,0,&Dist);
    Test->AddInfo(FString::Printf(TEXT("Mountain exposure actor %s bounds %s parent %s contains %d distance %.0f enabled %d priority %.0f weight %.1f minEV %.2f"),*It->GetActorLocation().ToString(),*It->FindComponentByClass<UBoxComponent>()->GetComponentLocation().ToString(),*GetNameSafe(Post->GetAttachParent()),Contains,Dist,Post->bEnabled,Post->Priority,Post->BlendWeight,Post->Settings.AutoExposureMinBrightness));
   }
  }
  if(Stage==5){PC->ConsoleCommand(TEXT("ShowFlag.VisualizeBuffer 1"));PC->ConsoleCommand(TEXT("r.BufferVisualizationTarget BaseColor"));}
  if(Stage==6){PC->ConsoleCommand(TEXT("ShowFlag.VisualizeBuffer 0"));PC->ConsoleCommand(TEXT("viewmode lit"));PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));}
 }
private:
 FAutomationTestBase* Test;TArray<FVector> Points;ACameraActor* Camera=nullptr;double Started=0;float At=0;int32 Stage=0;
};
class FAdventureDriveAudit:public IAutomationLatentCommand
{
public:
 explicit FAdventureDriveAudit(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>850){Test->AddError(FString::Printf(TEXT("Continuous mountain drive timed out at sample %d"),Index));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Car)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-154000,800))))return false;
   if(!Test->TestTrue(TEXT("Surveyed route exists"),ReadAdventureRoute(Points)))return true;
   auto* P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)
    if(!It->bRoadster && (!Car || FVector::DistSquared(It->GetActorLocation(),Points[0])<FVector::DistSquared(Car->GetActorLocation(),Points[0])))Car=*It;
   if(!Test->TestNotNull(TEXT("Production Aster in authored lower lay-by"),Car))return true;
   Test->TestEqual(TEXT("Real player vehicle profile on ascent"),Car->GetVehicleProfile()->GetFName(),FName(TEXT("DA_HeroAster")));
   Index=5;
   P->SetActorLocation(Car->GetActorLocation()-Car->GetActorRightVector()*255+FVector(0,0,95),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   if(!Test->TestTrue(TEXT("Ordinary vehicle entry"),Car->GetVehicleInteraction()->Enter(P)))return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   At=LastProgress=W->GetTimeSeconds();Last=Car->GetActorLocation();return false;
  }
  if(W->GetTimeSeconds()-At<2)return false;
  const FVector Here=Car->GetActorLocation();Distance+=FVector::Dist(Here,Last);Last=Here;
  int32 Best=Index;double BestDistance=DBL_MAX;
  for(int32 I=FMath::Max(0,Index-2);I<FMath::Min(Index+14,Points.Num());++I)
  {
   const double D=FVector::DistSquared2D(Here,Points[I]);if(D<BestDistance){BestDistance=D;Best=I;}
  }
  if(Best>Index){Index=Best;LastProgress=W->GetTimeSeconds();}
  const bool LeavingLayby=W->GetTimeSeconds()-At<7 && Index<18;
  if((FMath::Sqrt(BestDistance)>400 && !LeavingLayby) || Here.Z<Points[Index].Z-160 || W->GetTimeSeconds()-LastProgress>35)
  {Test->AddError(FString::Printf(TEXT("Drive failed sample %d: lane error %.0f cm, height %.0f cm, speed %.1f km/h"),Index,FMath::Sqrt(BestDistance),Here.Z-Points[Index].Z,Car->GetDrivingComponent()->GetSpeedKmh()));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Adventure-Drive-Failure.png"));return true;}
  FVector Error=Points[FMath::Min(Index+3,Points.Num()-1)]-Here;
  float Angle=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Error.Rotation().Yaw);
  const float Speed=Car->GetChaosMovement()->GetForwardSpeed();
  const float Desired=FMath::Abs(Angle)>14?480:750;
  Car->GetDrivingComponent()->SetSteerInput(FMath::Clamp(Angle/30.f,-1.f,1.f));
  Car->GetDrivingComponent()->SetDriveInput(FMath::Clamp((Desired-Speed)/240.f,0.f,1.f));
  Car->GetDrivingComponent()->SetBrakeInput(FMath::Clamp((Speed-Desired-40)/300.f,0.f,.65f));
  if(Index/100>LastShot){LastShot=Index/100;FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Adventure-Drive-%02d.png"),LastShot));}
  // Reach the summit itself. Five samples short still sits on the final
  // incline, where a low supercar's chassis is below the summit-height gate.
  if(Index>=Points.Num()-2 && FVector::Dist2D(Here,Points.Last())<350)
  {
   Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);
   Test->TestTrue(TEXT("Continuous Chaos ascent traversed over 2.4 km without teleportation"),Distance>240000);
   Test->TestTrue(TEXT("Reached actual highest summit"),Here.Z>30300);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Adventure-Drive-Summit.png"));return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;TArray<FVector> Points;AIslandSupercar* Car=nullptr;FVector Last;double Started=0,Distance=0;float At=0,LastProgress=0;int32 Index=1,LastShot=-1;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdventureRoadPIE,"NeonCity.Polish.AdventureRoadPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdventureRoadPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FAdventureRoadAudit(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdventureDrivePIE,"NeonCity.Polish.AdventureDrivePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdventureDrivePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FAdventureDriveAudit(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
