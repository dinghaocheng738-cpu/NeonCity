#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

namespace NeonCityWestLakeResidenceViews
{
/** Residence_WL plot frame: Blender-frame metres (x along the lake front, -y towards the lake, z above the finished
 * ground floor) to UE world centimetres. */
const FVector2D Origin(62,436);const double Yaw=-130,FloorZ=111;
FVector Local(double X,double Y,double Z)
{
 const double C=FMath::Cos(FMath::DegreesToRadians(Yaw)),S=FMath::Sin(FMath::DegreesToRadians(Yaw)),UY=-Y;
 return FVector((Origin.X+X*C-UY*S)*100,(Origin.Y+X*S+UY*C)*100,FloorZ+Z*100);
}
struct FView{const TCHAR* Name;FVector Eye,Target;float Fov;};

/** Review stills of the player's residence, outside and every room, in normal PIE lighting:
 * Saved/WestLake/ResidenceViews/<-NeonCityResidenceViewLabel=>/. Each view is held a second before capture so
 * Lumen and streaming settle. */
class FCapture:public IAutomationLatentCommand
{
public:
 explicit FCapture(FAutomationTestBase* T):Test(T)
 {
  const double GF=4.0;
  Views={
   {TEXT("01-LakeFront"),Local(10,-30,3),Local(10,6,3.5),70},
   {TEXT("02-RoadSide"),Local(10,34,2.2),Local(10,10,4),70},
   {TEXT("03-GarageCorner"),Local(36,27,3),Local(22,10,3),70},
   {TEXT("04-WestEnd"),Local(-16,-8,3),Local(6,6,4),70},
   {TEXT("05-Aerial"),Local(10,-32,26),Local(10,6,0),60},
   {TEXT("06-Living"),Local(7.8,7.3,1.7),Local(1.5,2.4,.8),85},
   {TEXT("06b-DiningFromLiving"),Local(11.5,1.2,1.6),Local(8.9,3.6,1.6),75},
   {TEXT("07-DiningKitchen"),Local(9,5,1.6),Local(19,4,1.2),85},
   {TEXT("08-Study"),Local(7.4,10.5,1.6),Local(1,10.5,1.2),85},
   {TEXT("08b-Shelf"),Local(3.4,10.4,1.4),Local(2.0,12.4,1.1),70},
   {TEXT("09-Hall"),Local(10.2,12.4,1.7),Local(10,4,1.5),85},
   {TEXT("10-Stair"),Local(19.2,10.7,1.6),Local(13,10.7,3),85},
   {TEXT("11-Master"),Local(8.6,8.2,GF+1.6),Local(2,3,GF+.8),85},
   {TEXT("12-Bath"),Local(12.6,7.4,GF+1.6),Local(10.5,2,GF+.8),85},
   {TEXT("13-Bedroom2"),Local(19.2,8.4,GF+1.6),Local(15,4,GF+.8),85},
   {TEXT("14-Corridor"),Local(1,11,GF+1.6),Local(12,11,GF+1.4),85},
   {TEXT("15-Garage"),Local(20.6,5.2,1.8),Local(25,12,.8),85},
   {TEXT("16-MoonGate"),Local(10.15,31,1.7),Local(10.15,16,1.0),70},
   {TEXT("17-EntranceSteps"),Local(13.5,17.5,1.9),Local(10,13.2,-.4),70},
   {TEXT("18-HomeBoat"),Local(6,-5,2.6),Local(3,-14,-.6),70},
  };
 }
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>300){Test->AddError(TEXT("Residence view capture timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<5)return false;
  if(Index>=Views.Num()){if(Camera)Camera->Destroy();PC->SetViewTarget(PC->GetPawn());return true;}
  const FView& V=Views[Index];
  if(!Camera)Camera=W->SpawnActor<ACameraActor>();
  if(!bPlaced)
  {
   Camera->SetActorLocationAndRotation(V.Eye,(V.Target-V.Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(V.Fov);
   PC->SetViewTarget(Camera);PC->PlayerCameraManager->SetGameCameraCutThisFrame();bPlaced=true;At=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-At<1.2)return false;
  FString Label=TEXT("Review");FParse::Value(FCommandLine::Get(),TEXT("NeonCityResidenceViewLabel="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/ResidenceViews")/FPaths::MakeValidFileName(Label);
  bool Done=false;
  if(auto* VP=W->GetGameViewport();VP && VP->Viewport)
  {
   VP->Viewport->Draw(false);TArray<FColor> Pixels;
   if(VP->Viewport->ReadPixels(Pixels))
   {
    for(auto& P:Pixels)P.A=255;TArray64<uint8> PNG;const auto Size=VP->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*Dir,true);Done=FFileHelper::SaveArrayToFile(PNG,*(Dir/(FString(V.Name)+TEXT(".png"))));
   }
  }
  Test->TestTrue(FString::Printf(TEXT("captured %s"),V.Name),Done);
  ++Index;bPlaced=false;return false;
 }
private:
 FAutomationTestBase* Test;TArray<FView> Views;ACameraActor* Camera=nullptr;double Started=0,At=0;int32 Index=0;bool bPlaced=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeResidenceViews,"NeonCity.WestLake.ResidenceViews",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeResidenceViews::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeResidenceViews::FCapture(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
