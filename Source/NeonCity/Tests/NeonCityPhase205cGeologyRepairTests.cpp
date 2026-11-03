#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMeshActor.h"
#include "Landscape.h"
#include "Adventure/IslandActivityJob.h"
namespace GeologyRepair
{
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T,bool CornersOnly=false,bool Presentation=false):Test(T),View(Presentation?0:CornersOnly?1:0),bCornersOnly(CornersOnly),bPresentation(Presentation){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;
  PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>160)return Finish(TEXT("Scoped geological review timed out"));
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<4)return false;
  if(!Camera)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(45000,-120000,30400))))return false;
   Original=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;
   PC->SetViewTarget(Camera);
   if(bPresentation?!CheckPresentation():bCornersOnly?!CheckCourtSupport():!CheckLaneSupport())return Finish(TEXT("Changed presentation support/floor or geological collision failed; retained diagnostics identify the object"));
  }
  if(bPresentation)return CapturePresentation();
  if(!ViewSet)
  {
   const double Hours[]={12,12,12,17.5,22};
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Hours[View]);}
   const FVector Eye[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(45410,-119560,30600)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44200,-121380,30300)),NeonCityCoastalLayout::FromLegacyIsland(FVector(42400,-126000,35500))};
   const FVector Aim[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(44470,-120410,30600)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46300,-120050,30550)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46400,-121000,29100))};
   const int32 Index=View<3?View:0;Camera->SetActorLocationAndRotation(Eye[Index],(Aim[Index]-Eye[Index]).Rotation());
   At=W->GetTimeSeconds();ViewSet=true;return false;
  }
  if(W->GetTimeSeconds()-At<(View==4?20:6))return false;
  const TCHAR* Names[]={TEXT("Geology-Exit-Day.png"),TEXT("Geology-Upper-Day.png"),TEXT("Geology-Aerial-Day.png"),TEXT("Geology-Exit-Sunset.png"),TEXT("Geology-Exit-Night.png")};
  if(!FNeonCityVisualEvidence::CaptureHUD(W,Names[View]))return Finish(TEXT("Normal PIE comparison capture failed"));
  ViewSet=false;if(++View==(bCornersOnly?3:5))return Finish(TEXT(""));return false;
 }
private:
 bool CheckPresentation()
 {
  AStaticMeshActor* Support=nullptr;FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(ResidencePresentationFloor),true);
  TArray<AStaticMeshActor*> Props;
  for(TActorIterator<AActor> It(W);It;++It)
  {
   bool Retain=false;
   if(auto* A=Cast<AStaticMeshActor>(*It);A && A->GetStaticMeshComponent()->GetStaticMesh())
   {
    const FString Mesh=A->GetStaticMeshComponent()->GetStaticMesh()->GetName();
    if(Mesh==TEXT("SM_RidgeForecourtSupport_01"))Support=A;
    Retain=Mesh==TEXT("SM_RidgeForecourt") || Mesh.StartsWith(TEXT("SM_RidgeHouseStone")) || Mesh.StartsWith(TEXT("SM_RidgeHouseOak")) || Mesh.StartsWith(TEXT("SM_RidgeHouseConcrete"));
    if(A->ActorHasTag(TEXT("NeonCity.IslandLandPass1")))for(int32 Index:{141,142,153,157,163,172,174,176,177,178})if(A->GetName()==FString::Printf(TEXT("StaticMeshActor_%d"),Index))Props.Add(A);
   }
   if(!Retain)FloorQuery.AddIgnoredActor(*It);
  }
  if(!Support || Props.Num()!=10){Test->AddError(FString::Printf(TEXT("Native closed subbase=%s, repaired island furniture/light count=%d expected10; city actors with coincident local names are excluded"),*GetNameSafe(Support),Props.Num()));return false;}
  for(auto* A:Props)
  {
   FVector Origin,Extent;A->GetActorBounds(false,Origin,Extent);const FVector Bottom=Origin-FVector(0,0,Extent.Z);FHitResult Hit;
   if(!W->LineTraceSingleByChannel(Hit,Bottom+FVector(0,0,8),Bottom-FVector(0,0,18),ECC_Visibility,FloorQuery) || FMath::Abs(Hit.ImpactPoint.Z-Bottom.Z)>7)
   {Test->AddError(FString::Printf(TEXT("Repaired object%s has no actual nearby finished floor; bottom%.2f hit%s at%.2f"),*A->GetName(),Bottom.Z,*GetNameSafe(Hit.GetActor()),Hit.ImpactPoint.Z));return false;}++SupportChecks;
  }
  FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(ForecourtClosedSubbase),true);
  for(TActorIterator<AActor> It(W);It;++It)if(*It!=Support)SupportQuery.AddIgnoredActor(*It);
  for(double X:{45050.,45450.,45850.,46250.})for(double Y:{-120450.-80000.,-119650.-80000.,-118850.-80000.,-118050.-80000.})
  {
   FHitResult Paving,Base;
   if(!W->LineTraceSingleByChannel(Paving,FVector(X,Y,30900),FVector(X,Y,29400),ECC_Visibility,FloorQuery) || !W->LineTraceSingleByChannel(Base,Paving.ImpactPoint+FVector(0,0,8),Paving.ImpactPoint-FVector(0,0,70),ECC_Pawn,SupportQuery) || Base.GetActor()!=Support || FMath::Abs((Paving.ImpactPoint.Z-Base.ImpactPoint.Z)-3)>1)
   {Test->AddError(FString::Printf(TEXT("Closed subbase support at%.0f,%.0f failed: floor%s z%.2f base%s z%.2f"),X,Y,*GetNameSafe(Paving.GetActor()),Paving.ImpactPoint.Z,*GetNameSafe(Base.GetActor()),Base.ImpactPoint.Z));return false;}++SupportChecks;
  }
  for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Photo.RidgeHouse"))
  {
   const FVector Point=It->GetActorTransform().TransformPosition(It->PickupLocal);FHitResult Hit;
   if(!W->LineTraceSingleByChannel(Hit,Point+FVector(0,0,20),Point-FVector(0,0,35),ECC_Visibility,FloorQuery) || FMath::Abs(Point.Z-Hit.ImpactPoint.Z-2)>1){Test->AddError(TEXT("Ridge photo marker does not meet the actual graded paving"));return false;}++SupportChecks;
  }
  return true;
 }
 bool CapturePresentation()
 {
  if(!ViewSet)
  {
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(View==4?22:12);}
   const FVector Eye[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(44200,-121380,30300)),NeonCityCoastalLayout::FromLegacyIsland(FVector(42400,-126000,35500)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48000,-119730,31030+155)),NeonCityCoastalLayout::FromLegacyIsland(FVector(47180,-120870,31030+155)),NeonCityCoastalLayout::FromLegacyIsland(FVector(44200,-121380,30300))};
   const FVector Aim[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(46300,-120050,30550)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46400,-121000,29100)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48400,-119390,31100)),NeonCityCoastalLayout::FromLegacyIsland(FVector(47000,-120410,31100)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46300,-120050,30550))};
   Camera->SetActorLocationAndRotation(Eye[View],(Aim[View]-Eye[View]).Rotation());At=W->GetTimeSeconds();ViewSet=true;return false;
  }
  if(W->GetTimeSeconds()-At<(View==4?18:5))return false;
  const TCHAR* Names[]={TEXT("Presentation-Court-Day.png"),TEXT("Presentation-Aerial-Day.png"),TEXT("Presentation-Living-Day.png"),TEXT("Presentation-Seating-Day.png"),TEXT("Presentation-Court-Night.png")};
  if(!FNeonCityVisualEvidence::CaptureHUD(W,Names[View]))return Finish(TEXT("Actual changed presentation image failed"));
  ViewSet=false;if(++View==5)return Finish(TEXT(""));return false;
 }
 bool CheckCourtSupport()
 {
  FString Text;TSharedPtr<FJsonObject> Data;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return false;
  const auto& P=Data->GetArrayField(TEXT("points"));FCollisionQueryParams Query(SCENE_QUERY_STAT(SummitCourtGeologyRepair),true);
  // Isolate rock/landscape penetration through the visible paving. Authored
  // lift, columns, lights and other intentional structures are separate tests.
  for(TActorIterator<AActor> It(W);It;++It)
  {
   bool Retain=It->IsA<ALandscape>();
   if(auto* Mesh=Cast<AStaticMeshActor>(*It);Mesh && Mesh->GetStaticMeshComponent()->GetStaticMesh())
   {const FString Name=Mesh->GetStaticMeshComponent()->GetStaticMesh()->GetName();Retain=Name==TEXT("SM_RidgeForecourt") || Name.Contains(TEXT("Geology")) || Name.StartsWith(TEXT("SM_LandRoadCut"));}
   if(!Retain)Query.AddIgnoredActor(*It);
  }
  for(int32 IX=0;IX<17;++IX)for(int32 IY=0;IY<32;++IY)
  {
   const double X=-50+IX,Y=-523.5+IY;double Z=0;
   for(int32 I=P.Num()-14;I<P.Num()-1;++I)
   {const auto A=P[I]->AsArray(),B=P[I+1]->AsArray();if(X<=B[0]->AsNumber() || I==P.Num()-2){Z=FMath::Lerp(A[2]->AsNumber(),B[2]->AsNumber(),FMath::Clamp((X-A[0]->AsNumber())/(B[0]->AsNumber()-A[0]->AsNumber()),0.,1.));break;}}
   const FVector Floor(NeonCityCoastalLayout::IslandOrigin.X+X*100,NeonCityCoastalLayout::IslandOrigin.Y-Y*100,-130+(Z-.015)*100);FHitResult Hit;
   if(!W->LineTraceSingleByChannel(Hit,Floor+FVector(0,0,300),Floor-FVector(0,0,800),ECC_Visibility,Query) || FMath::Abs(Hit.ImpactPoint.Z-Floor.Z)>12)
   {Test->AddError(FString::Printf(TEXT("Whole court surface (%d,%d), hit %s, difference %.2fcm"),IX,IY,*GetNameSafe(Hit.GetActor()),Hit.ImpactPoint.Z-Floor.Z));return false;}
   ++SupportChecks;
  }
  return true;
 }
 bool CheckLaneSupport()
 {
  FString Text;TSharedPtr<FJsonObject> Data;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return false;
  const auto& Points=Data->GetArrayField(TEXT("points"));FCollisionQueryParams Query(SCENE_QUERY_STAT(SummitGeologyRepair),true);
  for(int32 I=715;I<Points.Num();++I)
  {
   auto Point=[&](int32 N){const auto V=Points[N]->AsArray();return FVector(NeonCityCoastalLayout::IslandOrigin.X+100*V[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*V[1]->AsNumber(),-130+100*V[2]->AsNumber()+6);};
   const FVector Bed=Point(I),Direction=(Point(FMath::Min(I+1,Points.Num()-1))-Point(FMath::Max(I-1,0))).GetSafeNormal2D();
   for(float Offset:{-150.f,0.f,150.f})
   {
    const FVector LanePoint=Bed+FVector(-Direction.Y,Direction.X,0)*Offset;FHitResult Hit;
    const bool Found=W->LineTraceSingleByChannel(Hit,LanePoint+FVector(0,0,160),LanePoint-FVector(0,0,500),ECC_Visibility,Query);
    if(!Found || FMath::Abs(Hit.ImpactPoint.Z-LanePoint.Z)>28)
    {Test->AddError(FString::Printf(TEXT("Lane support station %d offset %.0f: hit %s height error %.2f"),I,Offset,*GetNameSafe(Hit.GetActor()),Found?Hit.ImpactPoint.Z-LanePoint.Z:-9999));return false;}
    ++SupportChecks;
   }
  }
  return true;
 }
 bool Finish(const FString& Failure)
 {
  if(PC && Original.IsValid())PC->SetViewTarget(Original.Get());if(Camera)Camera->Destroy();
  if(!Failure.IsEmpty())Test->AddError(Failure);
  Test->AddInfo(FString::Printf(TEXT("Scoped repair: %d actual %s collision traces; native1600x900 comparisons. Zero player/car placement, no repeated route/transactions. Terrain/road/routes unchanged. Screenshot generation is evidence; visible quality requires separate image review."),SupportChecks,bPresentation?TEXT("changed furniture floors, closed court subbase and photo marker"):bCornersOnly?TEXT("court paving versus rock/landscape"):TEXT("lane")));
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> Original;
 double Start=0;float At=0;int32 View=0,SupportChecks=0;bool ViewSet=false,bCornersOnly=false,bPresentation=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeologyRepairTest,"NeonCity.Phase205c.GeologyTransitionRepairPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGeologyRepairTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GeologyRepair::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeologyCourtRepairTest,"NeonCity.Phase205c.ForecourtSurfaceCornersPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGeologyCourtRepairTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GeologyRepair::FReview(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForecourtPresentationTest,"NeonCity.Phase205c.ForecourtPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForecourtPresentationTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GeologyRepair::FReview(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
namespace DiningPresentation
{
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>150)return Finish(TEXT("Dining/light changed-view timeout"));
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<4)return false;
  if(!Camera)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48840,-119210,30900))))return false;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(DiningFloorAndPartition),true);TArray<AStaticMeshActor*> Dining;
   for(TActorIterator<AActor> It(W);It;++It)
   {
    bool Structural=false;
    if(auto* A=Cast<AStaticMeshActor>(*It);A && A->GetStaticMeshComponent()->GetStaticMesh())
    {
     const FString N=A->GetStaticMeshComponent()->GetStaticMesh()->GetName();Structural=N.StartsWith(TEXT("SM_RidgeHouseStone")) || N.StartsWith(TEXT("SM_RidgeHouseOak"));
     if(A->ActorHasTag(TEXT("NeonCity.IslandLandPass1")))for(int I:{146,147,148,149,150})if(A->GetName()==FString::Printf(TEXT("StaticMeshActor_%d"),I))Dining.Add(A);
    }
    if(!Structural)Query.AddIgnoredActor(*It);
   }
   if(Dining.Num()!=5)return Finish(TEXT("Actual complete dining set not found"));
   FHitResult Wall;if(!W->LineTraceSingleByChannel(Wall,NeonCityCoastalLayout::FromLegacyIsland(FVector(48400,-119210,31100)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48530,-119210,31100)),ECC_Visibility,Query))return Finish(TEXT("Actual retained room partition missing"));
   for(auto* A:Dining)
   {
    FVector Center,Extent;A->GetActorBounds(false,Center,Extent);const FVector Feet=Center-FVector(0,0,Extent.Z);FHitResult Floor;
    if(Center.X-Extent.X<Wall.ImpactPoint.X+60)return Finish(FString::Printf(TEXT("Dining object %s still intersects/blocks its partition approach"),*A->GetName()));
    if(!W->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,8),Feet-FVector(0,0,18),ECC_Visibility,Query) || FMath::Abs(Floor.ImpactPoint.Z-Feet.Z)>7)return Finish(FString::Printf(TEXT("Dining object %s feet lost actual public floor"),*A->GetName()));
   }
   Original=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;PC->SetViewTarget(Camera);
  }
  if(!ViewSet)
  {
   const double Hours[]={12,17.5,22};for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Hours[View/2]);}
   const FVector Eye=View%2==0?NeonCityCoastalLayout::FromLegacyIsland(FVector(49100,-119600,31065)):NeonCityCoastalLayout::FromLegacyIsland(FVector(48000,-119730,31065));
   const FVector Aim=View%2==0?NeonCityCoastalLayout::FromLegacyIsland(FVector(48840,-119210,30990)):NeonCityCoastalLayout::FromLegacyIsland(FVector(48400,-119390,31000));
   Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());At=W->GetTimeSeconds();ViewSet=true;return false;
  }
  if(W->GetTimeSeconds()-At<(View==4?18:6))return false;
  const TCHAR* Hours[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
  if(!FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Dining-Light-%s-%s.png"),Hours[View/2],View%2==0?TEXT("Dining"):TEXT("Living"))))return Finish(TEXT("Actual native dining/light capture failed"));
  ViewSet=false;if(++View==6)return Finish(TEXT(""));return false;
 }
 bool Finish(const FString& Error)
 {
  if(PC && Original.IsValid())PC->SetViewTarget(Original.Get());if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);
  Test->AddInfo(TEXT("Changed dining/light only: real retained partition, five furnishing floor/clearance checks, six normal PIE eye-height Day/Sunset/Night images. No player/vehicle placement, no route or transaction replay. Image creation does not constitute subjective acceptance."));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> Original;
 double Start=0;float At=0;int View=0;bool ViewSet=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiningPresentationTest,"NeonCity.Phase205c.DiningLightingPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDiningPresentationTest::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(DiningPresentation::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
