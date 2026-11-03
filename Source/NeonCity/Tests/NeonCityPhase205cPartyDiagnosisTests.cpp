#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandPartyControl.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "EngineUtils.h"
namespace PartyLayout
{
class FReadOnly:public IAutomationLatentCommand
{
public:
 explicit FReadOnly(FAutomationTestBase* InTest):Test(InTest){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  if(FPlatformTime::Seconds()-Start>40){Test->AddError(TEXT("Read-only party layout stream timeout"));return true;}
  if(!W || W->GetTimeSeconds()<4)return false;auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(47800,-121000,31000))))return false;
  AIslandPartyControl* Party=nullptr;for(TActorIterator<AIslandPartyControl> I(W);I;++I)Party=*I;if(!Party)return false;
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("PartyLayout");
  if(!Party->WriteReadOnlyDiagnostics(Dir,true))Test->AddError(TEXT("Could not write actual read-only deck collision/support survey"));else Test->AddInfo(TEXT("Read-only party layout evidence written; no activity/movement was initiated and no gameplay PASS is claimed"));return true;
 }
 FAutomationTestBase* Test;double Start=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyLayoutDiagnosis,"NeonCity.Phase205c.Party.Layout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPartyLayoutDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PartyLayout::FReadOnly(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
