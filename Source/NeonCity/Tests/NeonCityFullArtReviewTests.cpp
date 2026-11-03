#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Slate/SceneViewport.h"
#include "EngineUtils.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "AI/Police/PoliceVehicle.h"
#include "World/NeonCityDayNightCycle.h"
#include "ImageUtils.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

namespace NeonCityFullArtReview
{
struct FSubject { FString Name; TWeakObjectPtr<AActor> Actor; FVector Target,Offset; };
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds(); if(!Start) Start=Now;
  if(Now-Start>600) { Test->AddError(TEXT("Art review timed out"));return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<3) return false;
  auto* Client=W->GetGameViewport(); if(!Client || !Client->Viewport) return false;
  if(Subjects.IsEmpty())
  {
   FString Run; FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run); Dir=FPaths::ProjectSavedDir()/TEXT("FullArt")/FPaths::MakeValidFileName(Run.IsEmpty()?TEXT("Review"):Run);
   IFileManager::Get().MakeDirectory(*Dir,true);
   static_cast<FSceneViewport*>(Client->Viewport)->SetFixedViewportSize(1600,900);
   for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality")})
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(3,ECVF_SetByConsole);
   PC->ConsoleCommand(TEXT("r.ScreenPercentage 100")); PC->ConsoleCommand(TEXT("r.VSync 0")); PC->ConsoleCommand(TEXT("t.MaxFPS 0")); PC->ConsoleCommand(TEXT("stat unit"));
   Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->FieldOfView=55; PC->SetViewTarget(Camera);
   GEngine->bEnableOnScreenDebugMessagesDisplay=false;
   FString Audit;
   for(TActorIterator<AActor> It(W);It;++It) Audit+=It->GetClass()->GetName()+TEXT(" ")+It->GetName()+TEXT(" ")+It->GetActorLocation().ToString()+TEXT("\n");
   FFileHelper::SaveStringToFile(Audit,*(Dir/TEXT("runtime-actors.txt")));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityFullArtAuditOnly"))) return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It) { Clock=*It; Clock->bCycleEnabled=false;Clock->SetTimeOfDay(10); }
   TSet<uint8> Types;
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)
   {
    const uint8 Kind=uint8(It->GetVehicleProfile()->VehicleType); if(Types.Contains(Kind)) continue; Types.Add(Kind);
    const TCHAR* Names[]={TEXT("Sedan"),TEXT("Sports"),TEXT("SUV"),TEXT("Pickup")};
    Subjects.Add({Names[Kind],*It,FVector(0,0,50),FVector(Kind==3?-510:510,-510,255)});
   }
   for(TActorIterator<APoliceVehicle> It(W);It;++It) { Subjects.Add({TEXT("Police"),*It,FVector(0,0,10),FVector(540,-520,245)});break; }
   if(!Subjects.ContainsByPredicate([](const FSubject& S){return S.Name==TEXT("Police");}))
   {
    auto* Patrol=W->SpawnActor<APoliceVehicle>(FVector(-2500,7000,481),FRotator::ZeroRotator);
    if(Patrol) Subjects.Add({TEXT("Police-StaticReviewFixture"),Patrol,FVector(0,0,10),FVector(540,-520,245)});
   }
   for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It) { Subjects.Add({TEXT("Speedboat"),*It,FVector(0,0,25),FVector(-550,-500,340)});break; }
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It) { Subjects.Add({TEXT("Helicopter"),*It,FVector(-70,0,55),FVector(150,-900,400)});break; }
   for(TActorIterator<ANeonCityFixedWingAircraft> It(W);It;++It) { Subjects.Add({TEXT("FixedWing"),*It,FVector(0,0,25),FVector(960,-1040,650)});break; }
   // Asset close-ups run in the live game world, clearly labelled as static displays.
   int32 Gallery=0;
   for(const TCHAR* Name:{TEXT("Yacht"),TEXT("Pistol"),TEXT("SMG"),TEXT("Rifle"),TEXT("Shotgun")})
   {
    const bool Yacht=Gallery==0; const TCHAR* Category=Yacht?TEXT("Marine"):TEXT("Weapons");
    auto* A=W->SpawnActor<AStaticMeshActor>(Yacht?FVector(-6000,-3500,-100):FVector(-3000+Gallery*200,-1800,150),FRotator::ZeroRotator);
    auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/FullArt/%s/SM_NC205B_%s.SM_NC205B_%s"),Category,Name,Name)));
    Subjects.Add({FString(Name)+TEXT("-AssetDisplay"),A,Yacht?FVector(0,0,100):FVector(20,0,5),Yacht?FVector(1200,-1600,750):FVector(60,-105,45)});++Gallery;
   }
   Subjects.Add({TEXT("Beach"),nullptr,FVector(-1400,-2300,-100),FVector(2100,-2700,1700)});
   Subjects.Add({TEXT("Buildings"),nullptr,FVector(-4400,2300,650),FVector(100,-3100,-430)});
   CSV=TEXT("subject,lighting,wall_ms,gpu_ms,game_thread_ms,render_thread_ms,width,height\n"); Phase=W->GetTimeSeconds();
   Test->TestTrue(TEXT("All road vehicle classes exist in normal map"),Types.Num()==4);
  }
  const auto& S=Subjects[Subject]; const FVector Target=S.Actor.IsValid()?S.Actor->GetActorTransform().TransformPosition(S.Target):S.Target;
  Camera->SetActorLocation(Target+S.Offset); Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());
  const float Elapsed=W->GetTimeSeconds()-Phase;
  const float Duration=Subject==0?(Light==2?22.f:12.f):6.f;
  if(Elapsed>Duration-3 && Elapsed<Duration && Last>0)
  {
   const auto* Unit=Client->GetStatUnitData();
   CSV+=FString::Printf(TEXT("%s,%d,%.4f,%.4f,%.4f,%.4f,1600,900\n"),*S.Name,Light,(Now-Last)*1000,FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),Unit?Unit->GameThreadTime:-1,Unit?Unit->RenderThreadTime:-1);
  }
  Last=Now;
  if(Elapsed<Duration) return false;
  Client->Viewport->Draw(false); TArray<FColor> Pixels;
  if(Client->Viewport->ReadPixels(Pixels))
  {
   for(auto& P:Pixels) P.A=255; const auto Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
   FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
   FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("%02d-%s-%s.png"),Subject,*S.Name,Light==0?TEXT("Day"):Light==1?TEXT("Sunset"):TEXT("Night"))));
  }
  ++Subject; Phase=W->GetTimeSeconds(); Last=0;
  if(Subject>=Subjects.Num())
  {
   Subject=0; ++Light;
   if(Light<3) { if(Clock) Clock->SetTimeOfDay(Light==1?17.3f:21.f); return false; }
   FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("performance.csv")));
   Test->AddInfo(TEXT("Real PIE art captures (static displays are labelled): ")+Dir);return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test; TArray<FSubject> Subjects; ACameraActor* Camera=nullptr; ANeonCityDayNightCycle* Clock=nullptr;
 FString Dir,CSV; double Start=0,Last=0;float Phase=0;int32 Subject=0,Light=0;
};
class FStart:public IAutomationLatentCommand
{
 bool Update() override
 {
  auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();Settings->NewWindowWidth=1600;Settings->NewWindowHeight=900;
  FRequestPlaySessionParams Params;Params.EditorPlaySettings=Settings;Params.bAllowOnlineSubsystem=false;GEditor->RequestPlaySession(Params);return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtReview,"NeonCity.FullArt.ReviewPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtReview::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityFullArtReview"))) { AddInfo(TEXT("Visual capture opt-in: -NeonCityFullArtReview"));return true; }
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFullArtReview::FStart>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityFullArtReview::FReview>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
