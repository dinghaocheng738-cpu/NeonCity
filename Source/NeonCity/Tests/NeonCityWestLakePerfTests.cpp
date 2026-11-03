#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/AutomationEditorCommon.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "RHIStats.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "World/NeonCityDayNightCycle.h"

namespace NeonCityWestLakePerf
{
struct FView{const TCHAR* Name;FVector Eye,Aim;};
// UE metres. The places people spend time at the lake; the city avenue last (A/B and diag run there).
const FView Views[]={
 {TEXT("Hubin"),FVector(-35,2,1.8),FVector(300,-120,8)},
 {TEXT("LakeToLeifeng"),FVector(380,-430,2.2),FVector(458,-926,30)},
 {TEXT("SuCauseway"),FVector(965,-420,1.8),FVector(972,-300,2.4)},
 {TEXT("BaiCauseway"),FVector(380,540,1.8),FVector(560,527,3)},
 {TEXT("Residence"),FVector(46.7,435.6,1.8),FVector(123,371,4)},
 {TEXT("CityAvenue"),FVector(-150,334,1.8),FVector(-420,326,12)}};

/** Frame cost at the West Lake viewpoints in normal PIE: for each view the camera settles (streaming,
 * Lumen), then 4 s of frames are sampled - frame / game / render / GPU ms (mean and 95th percentile),
 * draw calls, primitives and texture memory. Writes Saved/WestLake/PerfProbe.json and one still per
 * view (-NeonCityArtStills). Measures; it does not assert a budget. */
class FProbe:public IAutomationLatentCommand
{
public:
 explicit FProbe(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>600){Test->AddError(TEXT("Perf probe timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Camera.IsValid())
  {
   // -NeonCityPerfOnly=Name: measure (and run A/B / diag at) that view alone
   FString Only;if(FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerfOnly="),Only))
    for(int32 I=0;I<UE_ARRAY_COUNT(Views);++I)if(Only==Views[I].Name){Index=I;Last_=I;}
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(70);PC->SetViewTarget(Camera.Get());Place(W);
  }
  const double Now=FPlatformTime::Seconds(),Phase=Now-ViewStarted;
  if(Phase<10){Last=Now;return false;}                                  // settle: streaming, Lumen, shadows
  if(Phase<14)
  {
   Frame.Add((Now-Last)*1000);Last=Now;
   Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Render.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
   GPU.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));Draws.Add(GNumDrawCallsRHI[0]);Prims.Add(GNumPrimitivesDrawnRHI[0]);
   return false;
  }
  // -NeonCityPerfAB: at the last view, after a long warm-up, hide one instanced group at a time (base in
  // between each, same run, adjacent seconds) and log the GPU median - attribution free of thermal drift.
  if(Index==Last_ && FParse::Param(FCommandLine::Get(),TEXT("NeonCityPerfAB")))
  {
   static const TCHAR* Groups[]={TEXT("tag:WL.City"),TEXT("tag:WL.Residence"),TEXT("tag:WL.Scenic"),TEXT("tag:WL.Landmarks"),TEXT("forest"),TEXT("grass"),TEXT("willow"),TEXT("peach|camphor|trees |osmanthus"),TEXT("path|kerbs"),
    TEXT("revetment"),TEXT("lotus|reeds|azalea|boxwood|shrubs"),TEXT("lamps|benches|street lights"),TEXT("city lowj|city arcade"),TEXT("city mid|city tower"),
    TEXT("crosswalks|marks|centre line"),TEXT("<non-WL instances>"),TEXT("<WL static meshes>")};
   const int32 Steps=UE_ARRAY_COUNT(Groups)*2+1;
   if(ABIndex==0 && ABAt==0 && Phase<45)return false;                    // long warm-up at this view
   if(ABIndex<Steps)
   {
    const bool bHide=ABIndex%2==1;const int32 G=ABIndex/2;
    if(ABAt==0){SetGroupHidden(W,bHide?Groups[G]:nullptr);ABAt=Now;ABGPU.Reset();return false;}
    if(Now-ABAt<2.5)return false;
    if(Now-ABAt<6.5){ABGPU.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));return false;}
    ABGPU.Sort();
    UE_LOG(LogTemp,Display,TEXT("WL perf A/B %-40s gpu median %.2f ms (%d frames, %d actors hidden)"),bHide?Groups[G]:TEXT("base"),ABGPU.Num()?ABGPU[ABGPU.Num()/2]:0.,ABGPU.Num(),Hidden.Num());
    ++ABIndex;ABAt=0;return false;
   }
   SetGroupHidden(W,nullptr);
  }
  // -NeonCityPerfDiag: at the most expensive view also dump the GPU pass profile and GPU memory by resource.
  if(Index==Last_ && FParse::Param(FCommandLine::Get(),TEXT("NeonCityPerfDiag")) && DiagStage<2)
  {
   if(DiagStage==0){GEngine->Exec(W,TEXT("ProfileGPU"));GEngine->Exec(W,TEXT("stat game"));GEngine->Exec(W,TEXT("stat tickgroups"));DiagStage=1;DiagAt=Now;return false;}
   if(Now-DiagAt<3)return false;
   GEngine->Exec(W,TEXT("stat dumpframe -ms=0.4 -root=GameThread"));GEngine->Exec(W,TEXT("DumpTicks"));GEngine->Exec(W,TEXT("rhi.DumpResourceMemory 60"));GEngine->Exec(W,TEXT("rhi.DumpResourceMemory summary"));GEngine->Exec(W,TEXT("ListTextures"));DiagStage=2;
  }
  FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("WL-Perf-%s.jpg"),Views[Index].Name));
  FTextureMemoryStats Mem;RHIGetTextureMemoryStats(Mem);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("view"),Views[Index].Name);
  auto Put=[&](const TCHAR* K,TArray<double> V)
  {
   V.Sort();double S=0;for(double X:V)S+=X;
   Row->SetNumberField(FString(K)+TEXT("_mean"),V.Num()?S/V.Num():0);Row->SetNumberField(FString(K)+TEXT("_p95"),V.Num()?V[FMath::Min(V.Num()-1,int32(V.Num()*.95))]:0);
  };
  Put(TEXT("frame_ms"),Frame);Put(TEXT("game_ms"),Game);Put(TEXT("render_ms"),Render);Put(TEXT("gpu_ms"),GPU);Put(TEXT("draws"),Draws);Put(TEXT("prims"),Prims);
  Row->SetNumberField(TEXT("frames"),Frame.Num());
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Row->SetNumberField(TEXT("time_of_day"),It->GetTimeOfDay());break;}
  Row->SetNumberField(TEXT("tex_streaming_mb"),double(Mem.StreamingMemorySize)/1048576.);Row->SetNumberField(TEXT("tex_nonstreaming_mb"),double(Mem.NonStreamingMemorySize)/1048576.);
  Row->SetNumberField(TEXT("tex_pool_mb"),double(Mem.TexturePoolSize)/1048576.);Row->SetNumberField(TEXT("gpu_dedicated_mb"),double(Mem.DedicatedVideoMemory)/1048576.);
  Row->SetNumberField(TEXT("process_mb"),double(FPlatformMemory::GetStats().UsedPhysical)/1048576.);
  Rows.Add(MakeShared<FJsonValueObject>(Row));
  UE_LOG(LogTemp,Display,TEXT("WL perf %s: frame %.1f ms (p95 %.1f) gpu %.1f render %.1f game %.1f draws %.0f tex %.0f+%.0f MB, %.2f h"),Views[Index].Name,
   Row->GetNumberField(TEXT("frame_ms_mean")),Row->GetNumberField(TEXT("frame_ms_p95")),Row->GetNumberField(TEXT("gpu_ms_mean")),Row->GetNumberField(TEXT("render_ms_mean")),
   Row->GetNumberField(TEXT("game_ms_mean")),Row->GetNumberField(TEXT("draws_mean")),Row->GetNumberField(TEXT("tex_streaming_mb")),Row->GetNumberField(TEXT("tex_nonstreaming_mb")),
   Row->HasField(TEXT("time_of_day"))?Row->GetNumberField(TEXT("time_of_day")):-1.);
  Frame.Reset();Game.Reset();Render.Reset();GPU.Reset();Draws.Reset();Prims.Reset();
  if(Index<Last_){++Index;Place(W);return false;}
  auto Report=MakeShared<FJsonObject>();Report->SetArrayField(TEXT("views"),Rows);FString Json;
  FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("WestLake/PerfProbe.json")));
  if(Camera.IsValid())Camera->Destroy();
  return true;
 }
private:
 void Place(UWorld* W)
 {
  const FView& V=Views[Index];const FVector Eye=V.Eye*100,Aim=V.Aim*100;
  Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
  // Stream and light the place as a player standing there would.
  if(APawn* P=W->GetFirstPlayerController()->GetPawn())P->SetActorLocation(Eye+FVector(0,0,-60),false,nullptr,ETeleportType::TeleportPhysics);
  ViewStarted=FPlatformTime::Seconds();
 }
 void SetGroupHidden(UWorld* W,const TCHAR* Group)
 {
  for(auto& A:Hidden)if(A.IsValid())A->SetActorHiddenInGame(false);
  Hidden.Reset();if(!Group)return;
  TArray<FString> Keys;FString(Group).ParseIntoArray(Keys,TEXT("|"));
  for(TActorIterator<AActor> It(W);It;++It)
  {
   bool bWL=false;for(const FName& T:It->Tags)if(T.ToString().StartsWith(TEXT("WL.")))bWL=true;
   const bool bISM=It->FindComponentByClass<UInstancedStaticMeshComponent>()!=nullptr;
   bool bHit=false;const FString Label=It->GetActorNameOrLabel().ToLower();
   if(Keys[0].StartsWith(TEXT("tag:"))){for(const FName& T:It->Tags)if(T.ToString()==Keys[0].Mid(4))bHit=true;}   // every actor of a step
   else if(Keys[0]==TEXT("<non-WL instances>"))bHit=bISM && !bWL;
   else if(Keys[0]==TEXT("<WL static meshes>"))bHit=bWL && !bISM;
   else if(bWL && bISM)for(const FString& K:Keys)if(Label.Contains(K))bHit=true;
   if(bHit && !It->IsHidden()){It->SetActorHiddenInGame(true);Hidden.Add(*It);}
  }
 }
 TArray<TWeakObjectPtr<AActor>> Hidden;
 FAutomationTestBase* Test;double Started=0,ViewStarted=0,Last=0,DiagAt=0,ABAt=0;int32 Index=0,Last_=UE_ARRAY_COUNT(Views)-1,DiagStage=0,ABIndex=0;TArray<double> ABGPU;
 TWeakObjectPtr<ACameraActor> Camera;
 TArray<double> Frame,Game,Render,GPU,Draws,Prims;
 TArray<TSharedPtr<FJsonValue>> Rows;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakePerfProbe,"NeonCity.WestLake.PerfProbe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakePerfProbe::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakePerf::FProbe(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
