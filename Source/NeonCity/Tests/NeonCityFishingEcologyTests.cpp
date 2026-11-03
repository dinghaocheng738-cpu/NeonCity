#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/CoastalWildlife.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Editor.h"
namespace
{
class FActualFishHookCheck : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
public:
 explicit FActualFishHookCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;if(!W || W->GetTimeSeconds()<3)return false;
  auto* E=W->GetSubsystem<UMarineEcologySubsystem>();auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  auto Population=[&](){int32 N=0;for(TActorIterator<AMarineSchool> S(W);S;++S)N+=S->LivingCount();
   for(TActorIterator<ACoastalWildlife> F(W);F;++F)if(!F->bBird)++N;return N;};
  const int32 Before=Population(),Owned=Inventory->PouchCount();
  FRandomStream Random(205106);FNeonCityCaughtFish Fish;FVector Bait,Position;float Delay;FString Habitat;
  Test->TestFalse(TEXT("Empty ocean cannot create a table reward"),E->ReserveFishingEncounter(NeonCityCoastalLayout::FromLegacyIsland(FVector(80000,-210000,-130)),12,false,Random,Fish,Bait,Delay,Habitat));
  bool Reserved=false;
  for(TActorIterator<AMarineSchool> S(W);S && !Reserved;++S)
  {
   if(S->Behaviour==EMarineBehaviour::Benthic || S->LivingCount()==0)continue;
   FVector Bobber=S->CreaturePosition(0);Bobber.Z=-130;
   Reserved=E->ReserveFishingEncounter(Bobber,12,false,Random,Fish,Bait,Delay,Habitat);
  }
  if(!Test->TestTrue(TEXT("A real populated habitat yields an actual candidate"),Reserved))return true;
  Test->TestTrue(TEXT("Encounter points to an existing marine individual"),E->FishingEncounterPosition(Fish.FishID,Position));
  Test->TestTrue(TEXT("Wait includes ecology and approach time"),FMath::IsFinite(Delay) && Delay>=2);
  Test->TestEqual(TEXT("Casting does not grant an inventory reward"),Inventory->PouchCount(),Owned);
  Test->TestFalse(TEXT("Unlanded reservation cannot be kept"),E->KeepFishingEncounter(Fish.FishID,Inventory,Fish));
  bool Locked=false;
  for(TActorIterator<AMarineSchool> S(W);S;++S)for(int32 I=0;I<S->Starts.Num();++I)
   if(S->IsAlive(I) && S->CreaturePosition(I).Equals(Position,.01))
   {Locked=true;Test->TestFalse(TEXT("Diving collector cannot duplicate a line-reserved individual"),S->TryCapture(I,Inventory));}
  for(TActorIterator<ACoastalWildlife> F(W);F;++F)if(F->GetActorLocation().Equals(Position,.01))
  {Locked=true;Test->TestFalse(TEXT("Actor-based fish is also reserved against collection"),F->TryCapture(Inventory));}
  Test->TestTrue(TEXT("The selected animal is present in the actual population"),Locked);
  E->ReleaseFishingEncounter(Fish.FishID);
  Test->TestEqual(TEXT("Releasing a reservation preserves population"),Population(),Before);
  Test->TestFalse(TEXT("Released ticket no longer controls an animal"),E->MoveFishingEncounter(Fish.FishID,Bait,true,false));
  FVector Bobber=Position;Bobber.Z=-130;
  if(!Test->TestTrue(TEXT("Released habitat remains fishable"),E->ReserveFishingEncounter(Bobber,12,false,Random,Fish,Bait,Delay,Habitat)))return true;
  Test->TestTrue(TEXT("Real animal can enter the landed presentation"),E->MoveFishingEncounter(Fish.FishID,Position,true,true));
  Fish.CaughtAt=FDateTime::UtcNow();
  Test->TestTrue(TEXT("Keeping transfers the exact landed individual"),E->KeepFishingEncounter(Fish.FishID,Inventory,Fish));
  Test->TestEqual(TEXT("Exactly one living individual removed"),Population(),Before-1);
  Test->TestEqual(TEXT("Exactly one owned fish added"),Inventory->PouchCount(),Owned+1);
  Test->TestFalse(TEXT("A consumed ticket cannot award a second fish"),E->KeepFishingEncounter(Fish.FishID,Inventory,Fish));
  Test->AddInfo(FString::Printf(TEXT("Actual catch %s, source %s, wait %.2f, %s"),*Fish.SpeciesID.ToString(),*Fish.FishingPointID.ToString(),Delay,*Habitat));
  return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActualFishingEcology,"NeonCity.OceanExperience.FishingEcologyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActualFishingEcology::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FActualFishHookCheck>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
