#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandAquarium.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace AquariumPresentation
{
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>100)return End(TEXT("Aquarium access/presentation timed out"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(47900,-118400,30900))))return false;
   for(TActorIterator<AIslandLandAquarium> It(W);It;++It)Tank=*It;Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Fish=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
   if(!Tank || !Save->ConfigureSlot(TEXT("NeonCity_Test_Aquarium01"),false) || !Save->DoesSaveExist())return End(TEXT("Retained genuine living-fish checkpoint required"));Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>1)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());
   for(const auto& R:Fish->GetFish())if(R.bAlive && R.Storage==ENeonCityFishStorage::Aquarium && R.ContainerID==Tank->ContainerID){ID=R.FishID;break;}if(!ID.IsValid())return End(TEXT("Actual saved tank fish missing"));
   Goal=Tank->GetActorTransform().TransformPosition(FVector(0,-160,0));Key(EKeys::W,true);return Next(2);
  }
  if(Stage==2)
  {
   const FVector Delta=Goal-P->GetActorLocation();PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));
   if(Delta.Size2D()<25){Key(EKeys::W,false);return Next(3);}if(Age>7)return End(TEXT("Real saved player cannot walk to moved tank through clear public floor"));
  }
  if(Stage==3 && Age>.3)
  {
   PC->SetControlRotation((Tank->InteractionBounds->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Tank)return End(TEXT("Moved tank native F prompt inaccessible"));Key(EKeys::F,true);return Next(4);
  }
  if(Stage==4 && Age>.2)
  {
   Key(EKeys::F,false);auto* Interaction=PC->FindComponentByClass<UFishingComponent>();FTransform Pose;
   if(!Interaction || Interaction->GetActiveStation()!=Tank || !Tank->GetDisplayedFishTransform(ID,Pose))return End(TEXT("Changed placement lost real menu or saved living visual"));Key(EKeys::F,true);return Next(5);
  }
  if(Stage==5 && Age>.2){Key(EKeys::F,false);Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);return Next(6);}
  if(Stage==6)
  {
   if(!ViewSet)
   {
    const double Hours[]={12,17.5,22};for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Hours[Time]);}
    const FVector Eye=Tank->GetActorTransform().TransformPosition(View==0?FVector(-240,-280,175):FVector(-400,-470,195));
    const FVector Aim=Tank->GetActorTransform().TransformPosition(FVector(0,0,135));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());At=W->GetTimeSeconds();ViewSet=true;return false;
   }
   if(Age<(Time==2 && View==0?20:5))return false;
   const TCHAR* Names[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
   Test->TestTrue(TEXT("Changed native tank view captured"),FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Aquarium-Repair-%s-%s.png"),Names[Time],View==0?TEXT("Near"):TEXT("Room"))));
   ViewSet=false;if(++View==2){View=0;if(++Time==3)return End(TEXT(""));}return false;
  }
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure){for(auto K:{EKeys::W,EKeys::F,EKeys::F7})Key(K,false);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(!Failure.IsEmpty())Test->AddError(Failure);Test->AddInfo(TEXT("Changed placement/light only: genuine F7 living-fish checkpoint, normal2m W walk/F menu access, six fixed-camera Day/Sunset/Night renders. No catch/transfer/ownership fixture, no save overwrite, no repeated aquarium transfer acceptance. Technical/obvious visual evidence only; user subjective review separate."));return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandAquarium* Tank=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Fish=nullptr;ACameraActor* Camera=nullptr;FGuid ID;FVector Goal;int Stage=0,View=0,Time=0;float At=0;double Start=0;bool ViewSet=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumPresentationTest,"NeonCity.Phase205c.Aquarium.PlacementPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAquariumPresentationTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(AquariumPresentation::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
