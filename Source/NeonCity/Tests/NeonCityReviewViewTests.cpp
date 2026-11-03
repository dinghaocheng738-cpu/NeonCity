#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"

namespace NeonCityReviewViews
{
struct FView{FString Name;FVector Eye,Target;float Fov=70,Hour=-1;};

/** Review stills of any part of the live map in normal PIE lighting. Views come from a JSON file
 * (-NeonCityReviewViews=<path>): [{"name":..,"eye":[x,y,z],"target":[x,y,z],"fov":70,"hour":21}] in UE world metres
 * ("hour" optional: the time of day set before that view).
 * Stills go to Saved/Review/<-NeonCityReviewLabel=>/<name>.png. Each view is held before capture so
 * Lumen and streaming settle. */
class FCapture:public IAutomationLatentCommand
{
public:
 FCapture(FAutomationTestBase* T,TArray<FView> InViews,FString InDir,float InHold):Test(T),Views(MoveTemp(InViews)),Dir(MoveTemp(InDir)),Hold(InHold){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60+Views.Num()*(Hold+2)){Test->AddError(TEXT("Review view capture timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<5)return false;
  if(Index>=Views.Num()){if(Camera)Camera->Destroy();PC->SetViewTarget(PC->GetPawn());return true;}
  const FView& V=Views[Index];
  if(!Camera)Camera=W->SpawnActor<ACameraActor>();
  if(!bPlaced)
  {
   // The player rides along so the streaming and Lumen around the view are resident.
   PC->GetPawn()->SetActorLocation(V.Eye+FVector(0,0,500),false,nullptr,ETeleportType::TeleportPhysics);
   PC->GetPawn()->SetActorHiddenInGame(true);PC->GetPawn()->SetActorEnableCollision(false);
   if(V.Hour>=0)
   {
    // the map clock drives the sun and sky; the lighting subsystem follows it (or is set directly without one)
    bool bClock=false;
    for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(V.Hour);bClock=true;}
    if(!bClock)if(auto* Lighting=W->GetSubsystem<UWorldLightingSubsystem>())Lighting->SetTimeOfDay(V.Hour);
   }
   Camera->SetActorLocationAndRotation(V.Eye,(V.Target-V.Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(V.Fov);
   PC->SetViewTarget(Camera);PC->PlayerCameraManager->SetGameCameraCutThisFrame();bPlaced=true;At=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-At<Hold)return false;
  bool Done=false;
  if(auto* VP=W->GetGameViewport();VP && VP->Viewport)
  {
   VP->Viewport->Draw(false);TArray<FColor> Pixels;
   if(VP->Viewport->ReadPixels(Pixels))
   {
    for(auto& P:Pixels)P.A=255;TArray64<uint8> PNG;const auto Size=VP->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*Dir,true);Done=FFileHelper::SaveArrayToFile(PNG,*(Dir/(V.Name+TEXT(".png"))));
   }
  }
  Test->TestTrue(FString::Printf(TEXT("captured %s"),*V.Name),Done);
  ++Index;bPlaced=false;return false;
 }
private:
 FAutomationTestBase* Test;TArray<FView> Views;FString Dir;float Hold;ACameraActor* Camera=nullptr;double Started=0,At=0;int32 Index=0;bool bPlaced=false;
};

bool LoadViews(const FString& Path,TArray<FView>& Out)
{
 FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))return false;
 TArray<TSharedPtr<FJsonValue>> Items;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Items))return false;
 auto Vec=[](const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FVector& V)
 {
  const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
  if(!O->TryGetArrayField(Key,A) || A->Num()!=3)return false;
  V=FVector((*A)[0]->AsNumber(),(*A)[1]->AsNumber(),(*A)[2]->AsNumber())*100;return true;
 };
 for(const auto& Item:Items)
 {
  const auto O=Item->AsObject();FView V;
  if(!O || !O->TryGetStringField(TEXT("name"),V.Name) || !Vec(O,TEXT("eye"),V.Eye) || !Vec(O,TEXT("target"),V.Target))return false;
  double Fov=70,Hour=-1;O->TryGetNumberField(TEXT("fov"),Fov);O->TryGetNumberField(TEXT("hour"),Hour);V.Fov=Fov;V.Hour=Hour;Out.Add(V);
 }
 return Out.Num()>0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityReviewViews,"NeonCity.Review.Views",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityReviewViews::RunTest(const FString&)
{
 FString Path,Label=TEXT("Review");float Hold=1.5f;
 FParse::Value(FCommandLine::Get(),TEXT("NeonCityReviewViews="),Path);FParse::Value(FCommandLine::Get(),TEXT("NeonCityReviewLabel="),Label);
 FParse::Value(FCommandLine::Get(),TEXT("NeonCityReviewHold="),Hold);
 TArray<NeonCityReviewViews::FView> Views;
 if(!NeonCityReviewViews::LoadViews(Path,Views)){AddError(FString::Printf(TEXT("No review views in '%s'"),*Path));return false;}
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Review")/FPaths::MakeValidFileName(Label);
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityReviewViews::FCapture(this,MoveTemp(Views),Dir,Hold));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
