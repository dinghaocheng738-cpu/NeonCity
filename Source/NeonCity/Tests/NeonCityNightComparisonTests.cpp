#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Editor.h"

namespace
{
/** Repeatable ordinary player-camera comparisons, separate from continuous route gates. */
class FNightComparison : public IAutomationLatentCommand
{
public:
 FNightComparison(FAutomationTestBase* In,bool Island):Test(In),bIsland(Island) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Night comparison timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<2)return false;
  if(!Placed)
  {
   ANeonCityDayNightCycle* Clock=nullptr;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)Clock=*It;
   if(!Clock && bIsland){Clock=W->SpawnActor<ANeonCityDayNightCycle>();W->SpawnActor<ASkyAtmosphere>();}
   if(!Test->TestNotNull(TEXT("Actual world clock"),Clock))return true;
   Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(22);
   const FVector XY=bIsland?FVector(5500,-3600,0):FVector(-3800,1000,0);
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(NightComparisonFloor),false,P);
   if(!W->LineTraceSingleByChannel(Hit,XY+FVector(0,0,5000),XY-FVector(0,0,10000),ECC_Pawn,Q))
   {Test->AddError(TEXT("Comparison floor missing"));return true;}
   auto* Move=P->GetCharacterMovement();Move->SetMovementMode(MOVE_Falling);
   P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();const FRotator View(-8,bIsland?90:-15,0);
   P->SetActorRotation(FRotator(0,View.Yaw,0));PC->SetControlRotation(View);
   Evidence.Record(W,PC,0);SettledAt=W->GetTimeSeconds()+12;Placed=true;return false;
  }
  if(W->GetTimeSeconds()<SettledAt)return false;
  Test->TestTrue(TEXT("Night player remains supported by real ground"),P->GetCharacterMovement()->IsMovingOnGround());
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  Test->AddInfo(FString::Printf(TEXT("Normal player camera %s %s, 22:00; comparison fixture, not continuous-route acceptance"),*Eye.ToString(),*View.ToString()));
  Test->TestTrue(TEXT("Normal night viewport recorded"),FNeonCityVisualEvidence::CaptureHUD(W,bIsland?TEXT("IslandNightPier.png"):TEXT("SouthBeachNightRoad.png")));
  return true;
 }
private:
 FAutomationTestBase* Test;bool bIsland=false,Placed=false;double Started=0;float SettledAt=0;
 FNeonCityVisualEvidence Evidence{TEXT("NightComparison")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightRoadComparison,"NeonCity.Lighting.NightRoadComparisonPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNightRoadComparison::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FNightComparison>(this,false));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightPierComparison,"NeonCity.Lighting.NightPierComparisonPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNightPierComparison::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FNightComparison>(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
