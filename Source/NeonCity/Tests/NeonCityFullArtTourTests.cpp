#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Presentation/FullArtReviewSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
namespace
{
class FArtTourCheck:public IAutomationLatentCommand
{
public:
 explicit FArtTourCheck(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<3) return false;
  if(!Initialized)
  {
   Initialized=true;At=W->GetTimeSeconds();
   if(!Test->TestNotNull(TEXT("Opt-in review subsystem exists"),W->GetSubsystem<UFullArtReviewSubsystem>())) return true;
   Test->TestTrue(TEXT("Review begins at building stop"),FVector::Dist2D(P->GetActorLocation(),FVector(-4300,-800,0))<10);
   auto* Inventory=P->FindComponentByClass<UWeaponInventoryComponent>();
   for(int32 I=1;I<=4;++I) Test->TestTrue(TEXT("Four review weapons available"),Inventory && Inventory->Owns(NeonCityWeapons::Slot(I)));
  }
  if(W->GetTimeSeconds()-At<.3f) return false;
  At=W->GetTimeSeconds();
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::PageDown,Pressed?IE_Released:IE_Pressed,FPlatformTime::Cycles64()));
  if(Pressed) ++Stops;
  Pressed=!Pressed;
  if(Stops<15) return false;
  Test->TestTrue(TEXT("15 PageDown transitions return to first stop"),FVector::Dist2D(P->GetActorLocation(),FVector(-4300,-800,0))<10);
  return true;
 }
private:
 FAutomationTestBase* Test;bool Initialized=false,Pressed=false;int32 Stops=0;float At=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtTourTest,"NeonCity.FullArt.TourPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtTourTest::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityFullArtTour"))) { AddInfo(TEXT("Tour validation requires isolated -NeonCityFullArtTour session"));return true; }
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FArtTourCheck>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
