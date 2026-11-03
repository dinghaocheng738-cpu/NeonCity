#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Slate/SceneViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "World/NeonCityDayNightCycle.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "UnrealClient.h"

namespace NeonCityLicensedArt
{
class FStart final : public IAutomationLatentCommand
{
 bool Update() override
 {
  auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
  Settings->NewWindowWidth=1600; Settings->NewWindowHeight=900;
  FRequestPlaySessionParams Params; Params.EditorPlaySettings=Settings; Params.bAllowOnlineSubsystem=false;
  GEditor->RequestPlaySession(Params); return true;
 }
};
class FReview final : public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* InTest) : Test(InTest) {}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds(); if(Start==0) Start=Now;
  if(Now-Start>150) { Test->AddError(TEXT("Licensed environment PIE timed out")); return true; }
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Client=World?World->GetGameViewport():nullptr;
  if(!PC || !PC->GetPawn() || !Client || !Client->Viewport || World->GetTimeSeconds()<3) return false;
  if(!bChecked)
  {
   int32 Roads=0,Beaches=0,Hydrants=0;
   for(TActorIterator<AStaticMeshActor> It(World);It;++It)
   {
    auto* C=It->GetStaticMeshComponent(); const UStaticMesh* Mesh=C->GetStaticMesh(); if(!Mesh) continue;
    const FString Name=Mesh->GetName(); const auto* Material=C->GetMaterial(0);
    if(Name==TEXT("SM_SB205_Road12m") || Name==TEXT("SM_NC205B_RoadRepair"))
    {
     ++Roads; Test->TestTrue(TEXT("Road uses asphalt, including SouthBeach-named source path"),Material && Material->GetName()==TEXT("M_NC205C_Asphalt"));
     Test->TestEqual(TEXT("Road skin retains no duplicate collision"),C->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    }
    if(Name==TEXT("SM_NC205C_BeachWest") || Name==TEXT("SM_NC205C_BeachEast"))
    {
     ++Beaches; Test->TestTrue(TEXT("Shore uses fine sand material"),Material && Material->GetName()==TEXT("M_NC205C_Sand"));
     Test->TestEqual(TEXT("Walkable shoreline collision retained"),C->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
    }
    if(Name==TEXT("SM_NC205C_Hydrant"))
    {
     ++Hydrants; Test->TestTrue(TEXT("Hydrant uses reviewed material"),Material && Material->GetName()==TEXT("M_NC205C_Hydrant"));
     Test->TestEqual(TEXT("Presentation prop has no duplicate physics"),C->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    }
   }
   Test->TestEqual(TEXT("All 12 road sections and three repairs are bound"),Roads,15);
   Test->TestEqual(TEXT("Both shore sections retained"),Beaches,2);
   Test->TestEqual(TEXT("Five existing hydrants replaced in place"),Hydrants,5);
   bChecked=true;
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityLicensedArtCapture"))) return true;
   FString Run; FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   Dir=FPaths::ProjectSavedDir()/TEXT("VerticalSlice")/FPaths::MakeValidFileName(Run.IsEmpty()?TEXT("LicensedEnvironment"):Run);
   IFileManager::Get().MakeDirectory(*Dir,true);
   static_cast<FSceneViewport*>(Client->Viewport)->SetFixedViewportSize(1600,900);
   for(const TCHAR* Quality:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality")})
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(Quality)) C->Set(3,ECVF_SetByConsole);
   PC->ConsoleCommand(TEXT("r.ScreenPercentage 100")); PC->ConsoleCommand(TEXT("r.VSync 0")); PC->ConsoleCommand(TEXT("t.MaxFPS 0"));
   Camera=World->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->FieldOfView=65; PC->SetViewTarget(Camera);
   GEngine->bEnableOnScreenDebugMessagesDisplay=false;
   for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It) { Clock=*It; Clock->bCycleEnabled=false; }
   CSV=TEXT("view,lighting,game_ms,render_ms,frame_ms\n"); Phase=World->GetTimeSeconds();
  }
  const FVector Targets[]={FVector(-1700,0,12),FVector(-1550,-2400,-40),FVector(-2670,740,89)};
  const FVector Offsets[]={FVector(-250,-290,170),FVector(310,180,170),FVector(130,-160,85)};
  const TCHAR* Names[]={TEXT("Road"),TEXT("Sand"),TEXT("Hydrant")};
  const float Times[]={10.f,17.3f,21.f};
  if(Clock) Clock->SetTimeOfDay(Times[Light]);
  Camera->SetActorLocation(Targets[View]+Offsets[View]); Camera->SetActorRotation((-Offsets[View]).Rotation());
  const float Elapsed=World->GetTimeSeconds()-Phase;
  if(Elapsed>3)
  {
   if(const auto* Unit=Client->GetStatUnitData()) CSV+=FString::Printf(TEXT("%s,%d,%.3f,%.3f,%.3f\n"),Names[View],Light,Unit->GameThreadTime,Unit->RenderThreadTime,Unit->FrameTime);
  }
  if(Elapsed<6) return false;
  Client->Viewport->Draw(false); TArray<FColor> Pixels;
  if(!Client->Viewport->ReadPixels(Pixels)) { Test->AddError(TEXT("PIE pixel capture failed"));return true; }
  for(auto& P:Pixels) P.A=255;
  const auto Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
  FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
  Test->TestTrue(TEXT("PIE image written"),FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("%s-%d.png"),Names[View],Light))));
  Phase=World->GetTimeSeconds(); ++View;
  if(View==3) { View=0; ++Light; }
  if(Light==3) { FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("performance.csv")));return true; }
  return false;
 }
private:
 FAutomationTestBase* Test; bool bChecked=false; double Start=0; float Phase=0; int32 View=0,Light=0;
 ACameraActor* Camera=nullptr; ANeonCityDayNightCycle* Clock=nullptr; FString Dir,CSV;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityLicensedEnvironment,"NeonCity.VerticalSlice.LicensedEnvironmentPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityLicensedEnvironment::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLicensedArt::FStart>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLicensedArt::FReview>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
