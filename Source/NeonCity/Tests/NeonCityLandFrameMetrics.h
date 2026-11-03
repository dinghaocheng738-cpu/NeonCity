#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "RenderTimer.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GarbageCollection.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

/** All gameplay frames, with no image/GPU readback or tracing enabled here. GPU cycles are
 * the latest completed frame, and driver VRAM is an explicitly dated 1 Hz
 * process-local sample. Warmup remains in the raw CSV under its own stage.
 * Thread counters are the engine's latest completed unsmoothed unit timings,
 * not synchronous timers or additive components of this wall-clock interval.
 * The trailing351 fields preserve all existing CSV column names/order. */
class FNeonCityLandFrameMetrics
{
public:
 explicit FNeonCityLandFrameMetrics(const FString& InName,bool InAlwaysSample=false):Name(InName),bAlwaysSample(InAlwaysSample) {}
 ~FNeonCityLandFrameMetrics(){Flush();}
 void Sample(UWorld* W,const FString& Stage)
 {
  if((!bAlwaysSample && !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))) || !W)return;
  TRACE_CPUPROFILER_EVENT_SCOPE(LandMetricsSample);
  auto* PC=W->GetFirstPlayerController();auto* Client=W->GetGameViewport();
  if(!PC || !Client || !Client->Viewport)return;
  const double Now=FPlatformTime::Seconds();
  if(File.IsEmpty())
  {
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label)/TEXT("Metrics");IFileManager::Get().MakeDirectory(*Folder,true);
   File=Folder/(Name+TEXT("-allframes.csv"));
   PolicyFile=Folder/(Name+TEXT("-render-policy.csv"));
   FFileHelper::SaveStringToFile(TEXT("wall_seconds,sg.ViewDistanceQuality,sg.AntiAliasingQuality,sg.ShadowQuality,sg.GlobalIlluminationQuality,sg.ReflectionQuality,sg.PostProcessQuality,sg.TextureQuality,sg.EffectsQuality,sg.FoliageQuality,r.ScreenPercentage,r.Nanite,r.Lumen.HardwareRayTracing,r.Water.SingleLayer.Reflection,D3D12.EvictAllResidentResourcesInBackground,D3D12.ResidencyDebugBudgetMB,r.PurgeEditorSceneDuringPIE,gc.AllowIncrementalReachability,gc.AllowIncrementalGather,gc.IncrementalReachabilityTimeLimit,gc.VerifyNoUnreachableObjects,gc.ContinuousIncrementalGC,gc.DelayReachabilityIterations,gc.AllowParallelGC,r.VSync,t.MaxFPS,r.OneFrameThreadLag,r.RHICmdBypass,r.GTSyncType\n"),*PolicyFile);
   FFileHelper::SaveStringToFile(TEXT("wall_seconds,game_seconds,stage,frame_ms,gpu_ms,x,y,z,width,height,vram_used_mib,vram_budget_mib,vram_demoted_mib,vram_valid,memory_age_seconds,editor_throttled,shader_compiling,async_loading,gc,gc_reachability_pending,game_active_ms,render_active_ms,rhi_active_ms,game_critical_ms,render_critical_ms,game_wait_ms,render_wait_ms,present_ms,engine_frame_counter\n"),*File);
   Start=Previous=Now;
  }
  if(GDynamicRHI && (MemoryAt==0 || Now-MemoryAt>=1))
  {
   TRACE_CPUPROFILER_EVENT_SCOPE(LandMetricsMemoryAndPolicy);
   RHIGetMemoryStats(Memory);MemoryAt=Now;
   PolicyRows+=FString::Printf(TEXT("%.6f"),Now-Start);
   for(const TCHAR* N:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage"),TEXT("r.Nanite"),TEXT("r.Lumen.HardwareRayTracing"),TEXT("r.Water.SingleLayer.Reflection"),TEXT("D3D12.EvictAllResidentResourcesInBackground"),TEXT("D3D12.ResidencyDebugBudgetMB"),TEXT("r.PurgeEditorSceneDuringPIE"),TEXT("gc.AllowIncrementalReachability"),TEXT("gc.AllowIncrementalGather"),TEXT("gc.IncrementalReachabilityTimeLimit"),TEXT("gc.VerifyNoUnreachableObjects"),TEXT("gc.ContinuousIncrementalGC"),TEXT("gc.DelayReachabilityIterations"),TEXT("gc.AllowParallelGC"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.OneFrameThreadLag"),TEXT("r.RHICmdBypass"),TEXT("r.GTSyncType")})
   {
    const auto* C=IConsoleManager::Get().FindConsoleVariable(N);
    PolicyRows+=C?FString::Printf(TEXT(",%.4f"),C->GetFloat()):TEXT(",unavailable");
   }
   PolicyRows+=TEXT("\n");
  }
  FVector View;FRotator Rotation;PC->GetPlayerViewPoint(View,Rotation);const FIntPoint Size=Client->Viewport->GetSizeXY();constexpr double MiB=1024.*1024.;
  bool EditorThrottled=false,ShaderCompiling=false;
#if WITH_EDITOR
  EditorThrottled=GEditor && GEditor->ShouldThrottleCPUUsage();
  ShaderCompiling=GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
#endif
  Rows+=FString::Printf(TEXT("%.6f,%.6f,%s,%.4f,%.4f,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f,%d,%.6f,%d,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%llu\n"),
   Now-Start,W->GetTimeSeconds(),*Stage,(Now-Previous)*1000,FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),View.X,View.Y,View.Z,Size.X,Size.Y,Memory.UsedLocal/MiB,Memory.BudgetLocal/MiB,Memory.DemotedLocal/MiB,Memory.IsValid()?1:0,Now-MemoryAt,
   EditorThrottled?1:0,ShaderCompiling?1:0,IsAsyncLoading()?1:0,IsGarbageCollecting()?1:0,IsIncrementalReachabilityAnalysisPending()?1:0,
   FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(GRHIThreadTime),
   FPlatformTime::ToMilliseconds(GGameThreadTimeCriticalPath),FPlatformTime::ToMilliseconds(GRenderThreadTimeCriticalPath),
   FPlatformTime::ToMilliseconds(GGameThreadWaitTime),FPlatformTime::ToMilliseconds(GRenderThreadWaitTime),FPlatformTime::ToMilliseconds(GSwapBufferTime),static_cast<unsigned long long>(GFrameCounter));
  Previous=Now;if(++Buffered>=120)Flush();
 }
 bool Flush()
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(LandMetricsDiskFlush);
  // A concurrent evidence reader can briefly hold the file on Windows. Never
  // discard an unwritten120-frame block; keep it for the next flush and make
  // an unrecovered final write failure visible to the collecting test.
  bool Written=true;
  if(!Rows.IsEmpty())
  {
   if(FFileHelper::SaveStringToFile(Rows,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append|FILEWRITE_AllowRead)){Rows.Reset();Buffered=0;}
   else Written=false;
  }
  if(!PolicyRows.IsEmpty())
  {
   if(FFileHelper::SaveStringToFile(PolicyRows,*PolicyFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append|FILEWRITE_AllowRead))PolicyRows.Reset();
   else Written=false;
  }
  return Written;
 }
private:
 FString Name,File,Rows,PolicyFile,PolicyRows;double Start=0,Previous=0,MemoryAt=0;int32 Buffered=0;FRHIMemoryStats Memory;
 bool bAlwaysSample=false;
};
#endif
