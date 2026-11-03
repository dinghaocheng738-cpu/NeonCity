#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
namespace Coast798
{
class FSurvey:public IAutomationLatentCommand
{
public:explicit FSurvey(FAutomationTestBase* T,bool VisualComparisonOnly=false):Test(T),VisualOnly(VisualComparisonOnly){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>240)return End(TEXT("Native coastal comparison timed out"));if(!PC || W->GetTimeSeconds()<4)return false;
  if(!PolicyReady)
  {
   for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage")})
   {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Required coastal comparison quality variable absent"));const float Required=FCString::Strcmp(Name,TEXT("r.ScreenPercentage"))==0?100:3;if(!FMath::IsNearlyEqual(C->GetFloat(),Required)){OldPolicy.Add(Name,C->GetFloat());C->Set(Required,ECVF_SetByConsole);}}
   PolicyReady=true;
  }
  if(!Ready)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(P(FVector(145,-120,6))))return false;
   if(!VisualOnly)
   {
   auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return End(TEXT("Native navigation component absent"));Nav->RefreshRoadGuide();
   int32 NativeRoads=0,CoastalRoads=0;ANeonCityTrafficRoute* Main=nullptr;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalPhysical798),false,PC->GetPawn());
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
   {
    ++NativeRoads;if(I->RoadID==TEXT("Island.Mountain.Main"))Main=*I;if(!I->RoadID.ToString().StartsWith(TEXT("Island.Coast794.")))continue;++CoastalRoads;
    auto* S=I->GetRoadSpline();if(!S || S->GetNumberOfSplinePoints()<2)return End(TEXT("Native coastal spline absent"));
    for(float D=0;D<S->GetSplineLength();D+=200)
    {
     const FVector OnLane=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World),Right=S->GetRightVectorAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);const bool Private=I->RoadID.ToString().Contains(TEXT("Villa"));
     for(float Offset:{-145.f,0.f,145.f,420.f})
     {
      if(Private && Offset==420)continue;FHitResult H;const FVector Point=OnLane+Right*Offset;const bool Found=W->LineTraceSingleByChannel(H,Point+FVector(0,0,120),Point-FVector(0,0,100),ECC_Pawn,Q);++FloorProbes;
      const float Expected=Offset==420?8:0;const float Error=Found?FMath::Abs(H.ImpactPoint.Z-OnLane.Z-Expected):9999;MaxFloorError=FMath::Max(MaxFloorError,Error);
      if(!Found || Error>18 || H.ImpactNormal.Z<.9)
      {++Failures;FailureRows+=FString::Printf(TEXT("%s along%.1f offset%.1f expected%s floor%s normal%s actor%s\n"),*I->RoadID.ToString(),D,Offset,*Point.ToString(),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString(),*GetNameSafe(H.GetActor()));}
     }
    }
   }
   if(!Main || CoastalRoads!=6 || NativeRoads<17 || Nav->GetRoadGuide().GetRoads().Num()!=NativeRoads)return End(FString::Printf(TEXT("All native roads including six coastal routes must be retained: actors%d guide%d coast%d"),NativeRoads,Nav->GetRoadGuide().GetRoads().Num(),CoastalRoads));
   const auto* S=Main->GetRoadSpline();const FVector Origin=S->GetLocationAtSplinePoint(16,ESplineCoordinateSpace::World)+S->GetRightVectorAtSplinePoint(16,ESplineCoordinateSpace::World)*150;
   for(int32 Villa=0;Villa<4;++Villa)
   {
    const FName ExpectedID(*FString::Printf(TEXT("Island.Coast794.Villa%d"),Villa));const auto* Found=Nav->GetRoadGuide().GetRoads().FindByPredicate([&](const auto& Road){return Road.RoadID==ExpectedID;});if(!Found)return End(TEXT("Required coastal private route missing: ")+ExpectedID.ToString());const auto& R=*Found;
    TArray<FVector> Route;float Limit=0;bool Loop=false;const FVector Goal=R.Points[0];if(!Nav->GetRoadGuide().FindDriveRoute(Origin,S->GetDirectionAtSplinePoint(16,ESplineCoordinateSpace::World),&Goal,Route,Limit,Loop))return End(TEXT("Village main cannot reach private coastal drive: ")+R.RoadID.ToString());++ConnectedVillas;
   }
   if(ConnectedVillas!=4)return End(TEXT("All four named coastal villas must be connected"));
   }
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;break;}if(!Clock)return End(TEXT("Shared native lighting clock absent"));OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);
   Camera=W->SpawnActor<ACameraActor>();OldView=PC->GetViewTarget();PC->SetViewTarget(Camera);Ready=true;Place();return false;
  }
  if(W->GetTimeSeconds()-At<(View%3==2?20:3))return false;
  const bool Paused=W->IsPaused();UGameplayStatics::SetGamePaused(W,true);const bool OK=FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Coast798-%d.png"),View));UGameplayStatics::SetGamePaused(W,Paused);if(!OK)return End(TEXT("Native coastal screenshot failed"));
  if(++View==6)return End(Failures?TEXT("Native coastal lane/promenade floor probes failed"):TEXT(""));Place();return false;
 }
private:
 FVector P(FVector Source)const{return FVector(NeonCityCoastalLayout::IslandOrigin.X+Source.X*100,NeonCityCoastalLayout::IslandOrigin.Y-Source.Y*100,-130+Source.Z*100);}
 void Place()
 {
  const FVector Eye=View<3?FVector(205,-175,37):FVector(174,-147,8),Aim=View<3?FVector(145,-120,4):FVector(150,-132,6);Camera->SetActorLocationAndRotation(P(Eye),(P(Aim)-P(Eye)).Rotation());Camera->GetCameraComponent()->FieldOfView=68;Clock->SetTimeOfDay(View%3==0?12:View%3==1?17.8f:22);At=W->GetTimeSeconds();
 }
 bool End(FString Error)
 {
  if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}for(const auto& Entry:OldPolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Key))C->Set(Entry.Value,ECVF_SetByConsole);if(!Error.IsEmpty())Test->AddError(Error);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
  J->SetBoolField(TEXT("visual_comparison_only"),VisualOnly);J->SetNumberField(TEXT("native_floor_probes"),FloorProbes);
  J->SetNumberField(TEXT("floor_failures"),Failures);J->SetNumberField(TEXT("maximum_floor_error_cm"),MaxFloorError);
  J->SetNumberField(TEXT("villas_connected_from_actual_main"),ConnectedVillas);J->SetNumberField(TEXT("native_screenshots"),View);
  J->SetStringField(TEXT("scope"),VisualOnly?TEXT("Six native fixed Day/Sunset/Night cameras after bounded crown clearance. Unchanged floor/graph PASS806 is cached, never rerun or inferred here. No traversal or subjective acceptance claim."):TEXT("Actual native island world floor/lane/promenade collision probes, retained graph, four main-to-villa routes and Day/Sunset/Night fixed cameras; no player pose writes, driving and subjective acceptance separate"));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("Coastal798.json")));
  if(!VisualOnly)FFileHelper::SaveStringToFile(FailureRows,*(Dir/TEXT("coastal-floor-failures.txt")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;TWeakObjectPtr<AActor> OldView;bool Ready=false,OldCycle=true,PolicyReady=false,VisualOnly=false;double Started=0;float OldHour=12,At=0,MaxFloorError=0;int32 View=0,FloorProbes=0,Failures=0,ConnectedVillas=0;FString FailureRows;TMap<FString,float> OldPolicy;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalNative798,"NeonCity.Phase205c.Estate.Standalone.CoastalEvidence798",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalNative798::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Native coast diagnostics must preserve user saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(Coast798::FSurvey(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastCrownComparison812,"NeonCity.Phase205c.Estate.Standalone.CoastCrownComparison812",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastCrownComparison812::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Native comparisons must preserve user saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(Coast798::FSurvey(this,true));return true;}
#endif
