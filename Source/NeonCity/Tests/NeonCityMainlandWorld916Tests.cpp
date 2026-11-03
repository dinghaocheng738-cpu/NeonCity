#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityValidationPSO916.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/PedestrianSpawnPoint.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Tests/NeonCityRuntimeCook916.inl"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UObjectClusters.h"
#include "UObject/UObjectArray.h"
#include "RenderTimer.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProperties.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandWorld916
{
class FGraph : public IAutomationLatentCommand
{
public:explicit FGraph(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<6)return false;
  int32 Roads=0,Probes=0,Failures=0,ClearanceProbes=0;
  auto* PC=W->GetFirstPlayerController();auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr;
  if(!Nav){Test->AddError(TEXT("Actual navigation component absent"));return true;}Nav->RefreshRoadGuide();
  FCollisionQueryParams Query(SCENE_QUERY_STAT(Mainland916Support),false);
  for(TActorIterator<APawn> I(W);I;++I)Query.AddIgnoredActor(*I);
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
  {
   if(!I->RoadID.ToString().StartsWith(TEXT("Mainland916.Road.")))continue;
   ++Roads;auto* S=I->GetRoadSpline();
   const FVector Goal=S->GetLocationAtDistanceAlongSpline(S->GetSplineLength()*.5f,ESplineCoordinateSpace::World);
   TArray<FVector> Drive;float Limit=0;bool Loop=false;
   Test->TestTrue(*FString::Printf(TEXT("Old mainland connects to %s through real production navigation"),*I->RoadID.ToString()),Nav->GetRoadGuide().FindDriveRoute(FVector(-9000,0,89),FVector(-1,0,0),&Goal,Drive,Limit,Loop) && Drive.Num()>2 && FVector::Dist2D(Drive.Last(),Goal)<410);
   for(float D=0;D<=S->GetSplineLength();D+=400)
   {
    const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
    const FVector N=S->GetRightVectorAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
    for(float Lane:{-250.f,0.f,250.f})
    {
     const FVector Q=P+N*Lane;FHitResult H;
     const bool Hit=W->LineTraceSingleByChannel(H,Q+FVector(0,0,250),Q-FVector(0,0,250),ECC_WorldStatic,Query);
     ++Probes;
     if(!Hit || H.ImpactNormal.Z<.94 || FMath::Abs(H.ImpactPoint.Z-(P.Z-81))>8)
     {
      if(++Failures<=12)Test->AddError(FString::Printf(TEXT("Road support %s at %.1fm lane%.0f: %s zerr%.2f hit%s"),*I->RoadID.ToString(),D/100,Lane,*Q.ToString(),Hit?H.ImpactPoint.Z-(P.Z-81):999.f,*GetNameSafe(H.GetActor())));
     }
    }
    // Chassis-sized continuous structural clearance on both authored lanes.
    // Static support is checked separately; ambient cars/people are ignored.
    if(D+400<=S->GetSplineLength())for(float Lane:{-200.f,200.f})
    {
     const FVector Next=S->GetLocationAtDistanceAlongSpline(D+400,ESplineCoordinateSpace::World);
     const FVector NextN=S->GetRightVectorAtDistanceAlongSpline(D+400,ESplineCoordinateSpace::World);
     FHitResult Block;const FVector A=P+N*Lane-FVector(0,0,1),B=Next+NextN*Lane-FVector(0,0,1);
     ++ClearanceProbes;
     if(W->SweepSingleByChannel(Block,A,B,(B-A).GetSafeNormal2D().Rotation().Quaternion(),ECC_WorldStatic,FCollisionShape::MakeBox(FVector(220,110,50)),Query))
     {if(++Failures<=12)Test->AddError(FString::Printf(TEXT("Structural drive clearance %s %.1fm lane %.0f blocked by %s at %s"),*I->RoadID.ToString(),D/100,Lane,*GetNameSafe(Block.GetActor()),*Block.ImpactPoint.ToString()));}
    }
   }
  }
  Test->TestEqual(TEXT("All ten new roads exist in the actual map"),Roads,10);
  Test->TestTrue(TEXT("Thousands of physical road-floor probes, no unsupported lane"),Probes>3000 && Failures==0);
  Test->AddInfo(FString::Printf(TEXT("Road probes %d, continuous chassis sweeps %d, failures %d"),Probes,ClearanceProbes,Failures));return true;
 }
private:FAutomationTestBase* Test;
};

// Fixed rendered comparisons and natural-GC diagnosis are separate from the
// subsequent real walking/driving/flight route tests. No input mapping changes.
enum class EViewScope { World,Frontage,Estate,EstateSuites,Coast };
class FViews : public IAutomationLatentCommand
{
public:explicit FViews(FAutomationTestBase* T,bool Pictures,EViewScope InScope=EViewScope::World):Test(T),Stills(Pictures),Scope(InScope)
 {FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerfProbeSeconds="),Duration);Duration=FMath::Clamp(Duration,65.f,300.f);if(Scope==EViewScope::Coast)Limit=5;else if(Scope==EViewScope::Frontage)Limit=9;else if(Scope==EViewScope::EstateSuites)Limit=12;else if(Scope==EViewScope::Estate){Limit=31;FParse::Value(FCommandLine::Get(),TEXT("NeonCityEstateViewLimit="),Limit);Limit=FMath::Clamp(Limit,1,31);}else{FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerfViewLimit="),Limit);Limit=FMath::Clamp(Limit,1,6);}}
 bool Update()override
 {
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>Limit*(Duration+40)+90){Test->AddError(TEXT("Bounded view probe timeout"));return Finish(W,PC);}
  if(!Camera)
  {
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;break;}
   if(!Clock){Test->AddError(TEXT("Actual lighting clock missing"));return Finish(W,PC);}
   OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);PC->SetViewTarget(Camera);
   if(Scope==EViewScope::Frontage)
   {
    int32 Actors=0,Missing=0;
    for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Mainland916.StreetFrontage15")))
    {
     ++Actors;auto* C=I->GetStaticMeshComponent();UStaticMesh* M=C->GetStaticMesh();
     if(!M || !M->GetBodySetup() || !C->IsCollisionEnabled() || C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || C->GetCollisionResponseToChannel(ECC_Camera)!=ECR_Block){++Missing;continue;}
     for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot)if(!C->GetMaterial(Slot))++Missing;
    }
    Test->TestEqual(TEXT("All202 installed frontage addresses retain opaque and glass actors"),Actors,404);
    Test->TestEqual(TEXT("Installed frontage retains material, pawn and camera collision"),Missing,0);
   }
   Test->AddInfo(FString::Printf(TEXT("Resident objects %d; allocated GC clusters %d"),GUObjectArray.GetObjectArrayNum(),GUObjectClusters.GetNumAllocatedClusters()));Place(W);return false;
  }
  const double Age=FPlatformTime::Seconds()-At;
  if(!Stills)Metrics.Sample(W,Age<10?TEXT("Warmup"):*FString::Printf(TEXT("Mainland916View%d"),View));
  if(Age<(Stills?8:Duration))return false;
#if WITH_EDITOR
  if(Stills)Test->TestTrue(TEXT("Actual rendered mainland comparison"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("Mainland916-%s%02d.jpg"),Scope==EViewScope::Coast?TEXT("Coast-"):Scope==EViewScope::Frontage?TEXT("Frontage-"):Scope==EViewScope::Estate?TEXT("Estate-"):Scope==EViewScope::EstateSuites?TEXT("Suite-"):TEXT(""),View)));
#endif
  if(++View>=Limit)return Finish(W,PC);Place(W);return false;
 }
private:
 void Place(UWorld* W)
 {
  if(Scope==EViewScope::Coast)
  {
   const FVector Eyes[]={FVector(-54800,-12800,2000),FVector(-56500,-3100,620),FVector(-108000,-24000,27000),FVector(-54800,-12800,2000),FVector(-54800,-12800,2000)};
   const FVector Aims[]={FVector(-54000,-7000,150),FVector(-57000,-5200,200),FVector(-55000,21000,800),FVector(-54000,-7000,150),FVector(-54000,-7000,150)};
   Camera->SetActorLocationAndRotation(Eyes[View],(Aims[View]-Eyes[View]).Rotation());Clock->SetTimeOfDay(View==3?17.8f:View==4?22:12);At=FPlatformTime::Seconds();return;
  }
  if(Scope==EViewScope::Estate || Scope==EViewScope::EstateSuites)
  {
   // Fixed human-height evidence for every existing room programme, then the
   // same views at night and the actual pool at day/sunset/night. These cameras
   // inspect art; they do not claim a walked route or subjective acceptance.
   const FVector Site(-92000,36000,973);
   const FVector Eyes[]={FVector(2,-22,1.7),FVector(0,11,1.7),FVector(-19,-4.6,1.7),FVector(-18,5.7,1.7),FVector(-18,15.2,1.7),FVector(19,2.8,1.7),FVector(17.7,21,1.7),FVector(-19,-4,5.9),FVector(-19,5.8,5.9),FVector(-19,15.2,5.9),FVector(19,-4,5.9),FVector(19,5.8,5.9),FVector(19,15.2,5.9),FVector(10,20.5,5.9)};
   const FVector Aims[]={FVector(-9,-17,1.4),FVector(0,21,1.6),FVector(-28,.8,1.3),FVector(-25,10,1.4),FVector(-26,19,1.2),FVector(26,-3.7,1.2),FVector(28,13,1.3),FVector(-26,0,5.3),FVector(-26,10,5.3),FVector(-26,19.2,5.3),FVector(26,0,5.3),FVector(26,10,5.3),FVector(26,19.2,5.3),FVector(-4,17,5.6)};
   FVector Eye,Aim;float Hour;
   const int32 RoomView=Scope==EViewScope::EstateSuites?(View<6?View+7:View+15):View;
   if(RoomView<28){Eye=Eyes[RoomView%14];Aim=Aims[RoomView%14];Hour=RoomView<14?12:22;}
   else{Eye=FVector(0,-62,1.8);Aim=FVector(0,-45,1.2);Hour=View==28?12:View==29?17.8f:22;}
   Camera->SetActorLocationAndRotation(Site+Eye*100,(Aim-Eye).Rotation());Clock->SetTimeOfDay(Hour);At=FPlatformTime::Seconds();return;
  }
  if(Scope==EViewScope::Frontage)
  {
   // Fixed pedestrian-height comparisons face the actual installed frontage
   // in each district. They are visual evidence, not player travel acceptance.
   const FVector Addresses[]={FVector(-73668.258,-1430.275,301.132),FVector(-61036.409,28049.161,482.210),FVector(-78200,16100,349.5),FVector(-53903.763,9493.190,111.630),FVector(-12498.725,19574.968,127.904)};
   const float Yaws[]={-7.75755f,-192.86810f,-90.f,-89.97375f,-77.89874f};
   const int32 Address=View<5?View:View==5 || View==7?1:0;
   const FVector P=Addresses[Address],Direction=FRotator(0,Yaws[Address],0).RotateVector(FVector::RightVector);
   const FVector Eye=P+Direction*1800+FVector(0,0,170),Aim=P+FVector(0,0,380);
   Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
   Clock->SetTimeOfDay(View<5?12:View<7?17.8f:22);At=FPlatformTime::Seconds();return;
  }
  const FVector Eyes[]={FVector(-14100,800,260),FVector(-48500,-1300,380),FVector(-92000,34500,1140),FVector(-92000,28600,1800),FVector(-108000,-24000,27000),FVector(-92000,34500,1140)};
  const FVector Aims[]={FVector(-14100,2900,160),FVector(-55000,-800,400),FVector(-92000,35500,1200),FVector(-92000,36500,1600),FVector(-55000,21000,800),FVector(-92000,35500,1200)};
  Camera->SetActorLocationAndRotation(Eyes[View],(Aims[View]-Eyes[View]).Rotation());Clock->SetTimeOfDay(View==5?22:View==3?17.8f:12);At=FPlatformTime::Seconds();
 }
 bool Finish(UWorld* W,APlayerController* PC)
 {
  Test->TestTrue(TEXT("All sampled frames retained"),Metrics.Flush());
  Test->TestTrue(TEXT("Requested PSO recording saved before forced automation exit"),NeonCitySaveValidationPSO916());
  if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();
  if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}return true;
 }
 FAutomationTestBase* Test;bool Stills=false,OldCycle=true;EViewScope Scope;FNeonCityLandFrameMetrics Metrics{TEXT("Mainland916-World")};
 ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;TWeakObjectPtr<AActor> OldView;double Start=0,At=0;float Duration=70,OldHour=12;int32 View=0,Limit=6;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandGraph916,"NeonCity.Phase205c.Mainland916.Standalone.ExpansionGraph",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandGraph916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FGraph(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPerf916,"NeonCity.Phase205c.Mainland916.Standalone.PerformanceProbe",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPerf916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))){AddError(TEXT("Metrics flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,false));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCookedContent916,"NeonCity.Phase205c.Mainland916.Standalone.CookedContentGate",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCookedContent916::RunTest(const FString&)
{
 if(!FPlatformProperties::RequiresCookedData()){AddError(TEXT("Cooked-content gate requires the actual Game binary"));return false;}
 for(const TCHAR* Package:{TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"),TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")})
  TestTrue(*FString::Printf(TEXT("Required playable map is cooked: %s"),Package),FPackageName::DoesPackageExist(Package));
 auto* RuntimeLabel=LoadObject<UPrimaryAssetLabel>(nullptr,TEXT("/Game/NeonCity/RuntimeCook/PAL_RuntimeNameLoads916"));
 if(!TestNotNull(TEXT("Observed name-loaded cook omissions have a runtime asset label"),RuntimeLabel))return false;
 TSet<FString> Expected,Actual;
 for(const TCHAR* Package:MainlandCook916::RequiredPackages)Expected.Add(Package);
 for(const auto& Asset:RuntimeLabel->ExplicitAssets)Actual.Add(Asset.GetLongPackageName());
 TestEqual(TEXT("Cooked label retains every independently recorded required asset"),Actual.Num(),Expected.Num());
 TestEqual(TEXT("Cooked label contains no duplicate entries"),Actual.Num(),RuntimeLabel->ExplicitAssets.Num());
 for(const auto& Package:Expected)
 {
  TestTrue(*FString::Printf(TEXT("Observed omission is in the actual label: %s"),*Package),Actual.Contains(Package));
  TestNotNull(*FString::Printf(TEXT("Observed runtime asset actually loads: %s"),*Package),LoadObject<UObject>(nullptr,*Package));
 }
 // These families are selected at runtime by name. A successful cook alone
 // must not certify a faster session that silently lost art/audio/wake assets.
 for(const TCHAR* Path:{
  TEXT("/Game/NeonCity/Art/VerticalSlice/Transport/SM_NC205C_Sedan"),
  TEXT("/Game/NeonCity/Art/VerticalSlice/Transport/SM_NC205C_Helicopter"),
  TEXT("/Game/NeonCity/Art/VerticalSlice/Weapons/SM_NC205C_Pistol"),
  TEXT("/Game/NeonCity/Art/Immersion/Ocean/T_WakeHull"),
  TEXT("/Game/NeonCity/Art/Immersion/Ocean/T_WakePiles"),
  TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape"),
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillHingedLid"),
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillTongs"),
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillTongJaw"),
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillPlate"),
  TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/M_GrillSteam")})
  TestNotNull(*FString::Printf(TEXT("Runtime-selected asset loads in cooked game: %s"),Path),LoadObject<UObject>(nullptr,Path));
 AddInfo(FString::Printf(TEXT("Actual cooked game: %d resident objects, %d GC clusters; production GC unchanged"),GUObjectArray.GetObjectArrayNum(),GUObjectClusters.GetNumAllocatedClusters()));
 return !HasAnyErrors();
}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCoastViews916,"NeonCity.Phase205c.Mainland916.Standalone.CoastVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCoastViews916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){AddError(TEXT("Stills flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,true,MainlandWorld916::EViewScope::Coast));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandViews916,"NeonCity.Phase205c.Mainland916.Standalone.WorldVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandViews916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){AddError(TEXT("Stills flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandFrontageViews916,"NeonCity.Phase205c.Mainland916.Standalone.FrontageVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandFrontageViews916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){AddError(TEXT("Stills flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,true,MainlandWorld916::EViewScope::Frontage));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateViews916,"NeonCity.Phase205c.Mainland916.Standalone.EstateFinishVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateViews916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){AddError(TEXT("Stills flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,true,MainlandWorld916::EViewScope::Estate));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandSuiteViews916,"NeonCity.Phase205c.Mainland916.Standalone.EstateSuiteVisuals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandSuiteViews916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"))){AddError(TEXT("Stills flag required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandWorld916::FViews(this,true,MainlandWorld916::EViewScope::EstateSuites));return true;}
#endif
#endif
