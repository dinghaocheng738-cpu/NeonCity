#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/PostProcessComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Tests/NeonCityVisualEvidence.h"

// Explicitly requested diagnostic only. Does not save assets or reduce the
// production forest. Separate stages restore the baseline before each change.
class FNeonCityForestOcclusionProbe
{
public:
 ~FNeonCityForestOcclusionProbe() { Restore(); }
 bool Update(UWorld* World, APlayerController* PC, APawn* Player, FAutomationTestBase* Test)
 {
  if(Stage==0)
  {
   Controller=PC;
   const TCHAR* Names[]={TEXT("r.Lumen.HardwareRayTracing"),TEXT("r.Lumen.HardwareRayTracing.LightingMode"),
    TEXT("r.Lumen.HardwareRayTracing.HitLighting.Allowed"),TEXT("r.Lumen.HardwareRayTracing.HitLighting.ForceOpaque"),
    TEXT("r.RayTracing.Enable"),TEXT("r.RayTracing.EnableInGame"),TEXT("r.RayTracing.EnableInEditor"),TEXT("r.RayTracing.EnableOnDemand")};
   for(const auto* Name:Names) if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name))
    Test->AddInfo(FString::Printf(TEXT("Occlusion diagnostic %s=%s"),Name,*V->GetString()));
   At=World->GetTimeSeconds()+8;Stage=1;return false;
  }
  if(World->GetTimeSeconds()<At)return false;
  const TCHAR* Labels[]={TEXT(""),TEXT("Occlusion-Baseline.png"),TEXT("Occlusion-BaselineHDR.png"),
   TEXT("Occlusion-ForcedHitLighting.png"),TEXT("Occlusion-ForcedHitLightingHDR.png"),
   TEXT("Occlusion-NoUnderstoryGI.png"),TEXT("Occlusion-NoCanopyGI.png"),TEXT("Occlusion-Restored.png")};
  FNeonCityVisualEvidence::CaptureHUD(World,Labels[Stage]);
  if(Stage==1)PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
  if(Stage==2)
  {
   PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
   auto* P=NewObject<UPostProcessComponent>(Player);P->bUnbound=true;P->Priority=10000;
   P->Settings.bOverride_LumenRayLightingMode=true;P->Settings.LumenRayLightingMode=ELumenRayLightingModeOverride::HitLighting;
   Player->AddInstanceComponent(P);P->RegisterComponent();Volume=P;
  }
  if(Stage==3)PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
  if(Stage==4 || Stage==5)
  {
   Restore();
   int32 Changed=0;
   for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)
   {
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Parts(*It);
    for(auto* Part:Parts)
    {
     if(!Part->bVisibleInRayTracing || !Part->GetStaticMesh())continue;
     const FString MeshName=Part->GetStaticMesh()->GetName();
     const bool Canopy=MeshName.StartsWith(TEXT("SM_NCTW_Tree")) || MeshName.StartsWith(TEXT("SM_NCTW_Shrub"));
     if(Canopy!=(Stage==5))continue;
     Saved.Add({Part,bool(Part->bVisibleInRayTracing),bool(Part->bAffectDynamicIndirectLighting),bool(Part->bAffectDistanceFieldLighting)});
     Part->SetVisibleInRayTracing(false);Part->SetAffectDynamicIndirectLighting(false);Part->SetAffectDistanceFieldLighting(false);++Changed;
    }
   }
   Test->TestTrue(TEXT("Occlusion diagnostic isolated actual near components"),Changed>0);
   Test->AddInfo(FString::Printf(TEXT("Occlusion stage %d temporarily changed %d GI components; visibility, shadow and collision retained"),Stage,Changed));
  }
  if(Stage==6)Restore();
  if(Stage==7){Restore();Test->AddInfo(TEXT("Forest occlusion diagnostic completed and restored"));return true;}
  ++Stage;At=World->GetTimeSeconds()+12;return false;
 }
private:
 void Restore()
 {
  if(Volume.IsValid())Volume->DestroyComponent();Volume.Reset();
  for(const auto& S:Saved)if(S.Component.IsValid())
  {
   S.Component->SetVisibleInRayTracing(S.Ray);S.Component->SetAffectDynamicIndirectLighting(S.Dynamic);S.Component->SetAffectDistanceFieldLighting(S.Distance);
  }
  Saved.Reset();
  if(Controller.IsValid())Controller->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
 }
 struct FState { TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;bool Ray,Dynamic,Distance; };
 TArray<FState> Saved;
 TWeakObjectPtr<UPostProcessComponent> Volume;
 TWeakObjectPtr<APlayerController> Controller;
 int32 Stage=0;float At=0;
};

// Factorial exposure / ray clamp diagnosis. Earlier probes changed these
// separately; this determines whether dark-view exposure feeds a GI clamp loop.
class FNeonCityForestExposureTransportProbe
{
public:
 ~FNeonCityForestExposureTransportProbe() { Restore(); }
 bool Update(UWorld* World,APlayerController* PC,APawn* Player,FAutomationTestBase* Test)
 {
  if(Stage==0)
  {
   Controller=PC;Screen=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.MaxRayIntensity"));
   Radiosity=IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity.MaxRayIntensity"));
   if(!Test->TestTrue(TEXT("Exposure transport probe has both actual ray limit controls"),Screen && Radiosity))return true;
   OldScreen=Screen->GetFloat();OldRadiosity=Radiosity->GetFloat();
   PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));Stage=1;At=World->GetTimeSeconds()+12;return false;
  }
  if(World->GetTimeSeconds()<At)return false;
  const TCHAR* Names[]={TEXT(""),TEXT("ExposureTransport-Baseline.png"),TEXT("ExposureTransport-Unclamped.png"),
   TEXT("ExposureTransport-Fixed9Unclamped.png"),TEXT("ExposureTransport-Fixed9Default.png"),TEXT("ExposureTransport-Restored.png")};
  FNeonCityVisualEvidence::CaptureHUD(World,Names[Stage]);
  if(Stage==1){Screen->Set(100000.f,ECVF_SetByConsole);Radiosity->Set(100000.f,ECVF_SetByConsole);}
  if(Stage==2)
  {
   auto* P=NewObject<UPostProcessComponent>(Player);P->bUnbound=true;P->Priority=10000;
   P->Settings.bOverride_AutoExposureMinBrightness=true;P->Settings.AutoExposureMinBrightness=9;
   P->Settings.bOverride_AutoExposureMaxBrightness=true;P->Settings.AutoExposureMaxBrightness=9;
   Player->AddInstanceComponent(P);P->RegisterComponent();Volume=P;
  }
  if(Stage==3){Screen->Set(OldScreen,ECVF_SetByConsole);Radiosity->Set(OldRadiosity,ECVF_SetByConsole);}
  if(Stage==4){if(Volume.IsValid())Volume->DestroyComponent();Volume.Reset();}
  if(Stage==5){Restore();Test->AddInfo(TEXT("Forest exposure transport diagnostic completed and restored"));return true;}
  ++Stage;At=World->GetTimeSeconds()+16;return false;
 }
private:
 void Restore()
 {
  if(Stage==0)return;
  if(Screen)Screen->Set(OldScreen,ECVF_SetByConsole);if(Radiosity)Radiosity->Set(OldRadiosity,ECVF_SetByConsole);
  if(Volume.IsValid())Volume->DestroyComponent();Volume.Reset();
  if(Controller.IsValid())Controller->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
 }
 IConsoleVariable* Screen=nullptr;IConsoleVariable* Radiosity=nullptr;
 float OldScreen=0,OldRadiosity=0,At=0;int32 Stage=0;
 TWeakObjectPtr<APlayerController> Controller;TWeakObjectPtr<UPostProcessComponent> Volume;
};
#endif
