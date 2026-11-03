#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FLandPerformanceView { const TCHAR* Name;FVector Eye,Look;float Hour; };
const FLandPerformanceView Views[]={
 {TEXT("Village"),NeonCityCoastalLayout::FromLegacyIsland(FVector(57100,-157800,630)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57200,-158950,500)),12},
 {TEXT("MountainRoad"),NeonCityCoastalLayout::FromLegacyIsland(FVector(53375,-124413,20810)),NeonCityCoastalLayout::FromLegacyIsland(FVector(53074.788,-124113.022,20531.146)),12},
 {TEXT("MansionExterior"),NeonCityCoastalLayout::FromLegacyIsland(FVector(46350,-119200,30820)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30700)),12},
 {TEXT("MansionInterior"),NeonCityCoastalLayout::FromLegacyIsland(FVector(48160,-118470,30645)),NeonCityCoastalLayout::FromLegacyIsland(FVector(49600,-119200,30600)),12},
 {TEXT("Night"),NeonCityCoastalLayout::FromLegacyIsland(FVector(49150,-120100,30660)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48700,-119500,30630)),22},
 {TEXT("HelicopterAerial"),NeonCityCoastalLayout::FromLegacyIsland(FVector(50300,-126000,33300)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48100,-119100,30600)),12}
};
class FLandSixScenePerformance : public IAutomationLatentCommand
{
public:
 explicit FLandSixScenePerformance(FAutomationTestBase* In,int32 InScope=0):Test(In),Scope(InScope),Metrics(InScope==1?TEXT("VillageHitch"):InScope==2?TEXT("PostPlatform"):TEXT("SixScene"))
 {if(Scope==1)Selected={0};else if(Scope==2)Selected={2,3,4,5};else for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)Selected.Add(I);}
 ~FLandSixScenePerformance(){if(IsValid(PC) && IsValid(OriginalView))PC->SetViewTarget(OriginalView);if(IsValid(Camera))Camera->Destroy();}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  if(Now-Started>900){Test->AddError(TEXT("Six-scene performance timeout"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W->HasBegunPlay() || W->GetTimeSeconds()<3)return false;
  auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast)return false;
  if(!Camera)
  {
   if(!Coast->EnsureDestinationReady(Views[Selected[0]].Eye))return false;
   OriginalView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;
   PC->SetViewTarget(Camera);static_cast<FSceneViewport*>(W->GetGameViewport()->Viewport)->SetFixedViewportSize(1600,900);
   // Use the project's accepted visual policy. These are the same Epic/100%
   // settings used by the existing evidence client, never a quality downgrade.
   for(const TCHAR* N:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality")})
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(N))C->Set(3,ECVF_SetByConsole);
   PC->ConsoleCommand(TEXT("r.ScreenPercentage 100"));Place(W);At=Now;
  }
  if(Now-StreamingAt>1){TRACE_CPUPROFILER_EVENT_SCOPE(LandPerformanceEnsureDestinationReady);if(!Coast->EnsureDestinationReady(Views[Selected[Index]].Eye)){Test->AddError(TEXT("Selected final island view failed to stream"));return true;}StreamingAt=Now;}
  const bool Warmup=Now-At<20;
  Metrics.Sample(W,FString(Views[Selected[Index]].Name)+(Warmup?TEXT("_Warmup"):TEXT("_Sample")));
  if(Now-At<80)return false;
  Test->AddInfo(FString::Printf(TEXT("Recorded %s: 20 s warmup + 60 s all-frame performance, 1600x900 Epic/100%%, no image readback"),Views[Selected[Index]].Name));
  if(++Index==Selected.Num()){WriteManifest();return true;}
  Place(W);At=Now;return false;
 }
private:
 void Place(UWorld* W)
 {
  const auto& V=Views[Selected[Index]];Camera->SetActorLocationAndRotation(V.Eye,(V.Look-V.Eye).Rotation());
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(V.Hour);}
 }
 void WriteManifest()
 {
  auto Json=MakeShared<FJsonObject>();Json->SetStringField(TEXT("scope"),TEXT("Six fixed-camera technical performance views in normal PIE; continuous-driving metrics are a separate track; USER HANDLING REVIEW REQUIRED"));
  if(Scope)Json->SetStringField(TEXT("scope"),Scope==1?TEXT("FIX-SPECIFIC: one unchanged Village camera with CPU trace to diagnose the retained 452ms frame; does not repeat six-scene acceptance"):TEXT("FIX-SPECIFIC: four views affected by platform render/collision changes; Village and MountainRoad results remain cached"));
  Json->SetBoolField(TEXT("computer_control"),false);Json->SetBoolField(TEXT("image_readback"),false);Json->SetNumberField(TEXT("width"),1600);Json->SetNumberField(TEXT("height"),900);Json->SetNumberField(TEXT("quality"),3);Json->SetNumberField(TEXT("screen_percentage"),100);
  Json->SetStringField(TEXT("gpu_timing"),TEXT("Latest completed RHI frame; may lag the CPU frame. All spikes retained."));Json->SetStringField(TEXT("vram"),TEXT("Process-local driver UsedLocal/BudgetLocal/DemotedLocal at 1 Hz, with explicit sample age."));
  TArray<TSharedPtr<FJsonValue>> Items;for(int32 SelectedIndex:Selected){const auto& V=Views[SelectedIndex];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),V.Name);J->SetStringField(TEXT("eye"),V.Eye.ToString());J->SetStringField(TEXT("look"),V.Look.ToString());J->SetNumberField(TEXT("hour"),V.Hour);J->SetNumberField(TEXT("warmup_seconds"),20);J->SetNumberField(TEXT("sampling_seconds"),60);Items.Add(MakeShared<FJsonValueObject>(J));}Json->SetArrayField(TEXT("scenes"),Items);
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);
  Test->TestTrue(TEXT("Six-scene method and exact view manifest saved"),FFileHelper::SaveStringToFile(Text,*(Folder/(Scope==1?TEXT("village-hitch-scope.json"):Scope==2?TEXT("post-platform-scope.json"):TEXT("six-scene-scope.json")))));
 }
 FAutomationTestBase* Test;APlayerController* PC=nullptr;AActor* OriginalView=nullptr;ACameraActor* Camera=nullptr;
 double Started=0,At=0,StreamingAt=0;int32 Index=0,Scope=0;TArray<int32> Selected;FNeonCityLandFrameMetrics Metrics;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandSixScenePerformance,"NeonCity.IslandLand.SixScenePerformancePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandSixScenePerformance::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))){AddError(TEXT("Explicit native metrics opt-in required"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FLandSixScenePerformance(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandVillageHitchDiagnostic,"NeonCity.IslandLand.VillageHitchDiagnosticPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandVillageHitchDiagnostic::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FLandSixScenePerformance(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandPostPlatformPerformance,"NeonCity.IslandLand.PostPlatformPerformancePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandPostPlatformPerformance::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FLandSixScenePerformance(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
