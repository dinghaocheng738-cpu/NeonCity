#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/PointLight.h"
#include "EngineUtils.h"
#include "Editor.h"
#include <limits>

namespace
{
class FWorldLightingChecks : public IAutomationLatentCommand
{
public:
 explicit FWorldLightingChecks(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Lighting scheduler fixture timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<2)return false;
  auto* Manager=W->GetSubsystem<UWorldLightingSubsystem>();
  if(!Test->TestNotNull(TEXT("One world lighting scheduler"),Manager))return true;
  ANeonCityDayNightCycle* Clock=nullptr;for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)Clock=*It;
  if(!Test->TestNotNull(TEXT("World clock drives scheduler"),Clock))return true;
  Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(22);
  const int32 Original=Manager->GetRegisteredLightCount();
  Test->TestTrue(TEXT("Original street lights and boat registered"),Original>=10);
  FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
  TArray<APointLight*> Fixtures;
  FNeonCityLocalLightPolicy Policy;Policy.Intensity=350;Policy.CullDistance=10000;Policy.ShadowDistance=2000;Policy.Priority=100;
  for(int32 I=0;I<30;++I)
  {
   auto* A=W->SpawnActor<APointLight>(Eye+FVector(40+I*10,0,30),FRotator::ZeroRotator);Fixtures.Add(A);
   auto* L=CastChecked<UPointLightComponent>(A->GetLightComponent());L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);
   Test->TestTrue(TEXT("Late registration accepted after night already began"),Manager->RegisterLight(L,Policy));
   Test->TestEqual(TEXT("Late light receives existing night value"),L->Intensity,350.f);
  }
  auto* Near=CastChecked<UPointLightComponent>(Fixtures[0]->GetLightComponent());
  const int32 Registered=Manager->GetRegisteredLightCount();Manager->RegisterLight(Near,Policy);
  Test->TestEqual(TEXT("Idempotent registration"),Manager->GetRegisteredLightCount(),Registered);
  Manager->RefreshLighting();
  Test->TestEqual(TEXT("Dynamic budget enforced across all owners"),Manager->GetActiveLightCount(),UWorldLightingSubsystem::DynamicLightBudget);
  Test->TestEqual(TEXT("Only bounded near shadow lights"),Manager->GetShadowLightCount(),UWorldLightingSubsystem::ShadowLightBudget);
  // Release the artificial crowd before checking distance tiers for one lamp.
  for(int32 I=1;I<Fixtures.Num();++I)Manager->SetLightEnabled(CastChecked<UPointLightComponent>(Fixtures[I]->GetLightComponent()),false);
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Village/M_NCTW_Village_hard"));
  auto* Material=Base?UMaterialInstanceDynamic::Create(Base,Fixtures[0]):nullptr;
  if(Test->TestNotNull(TEXT("Real source village night-emission material"),Material))
  {
   Manager->RegisterNightMaterial(Material);
   Test->TestTrue(TEXT("Late window material lit"),Material->K2_GetScalarParameterValue(TEXT("Night"))>.99f);
  }
  Fixtures[0]->SetActorLocation(Eye+FVector(4000,0,0));Manager->RefreshLighting();
  Test->TestTrue(TEXT("Middle distance lighting retains illumination without shadows"),Near->IsVisible() && !Near->CastShadows);
  Fixtures[0]->SetActorLocation(Eye+FVector(12000,0,0));Manager->RefreshLighting();
  Test->TestFalse(TEXT("Far local light is culled"),Near->IsVisible());
  if(Material)Test->TestTrue(TEXT("Far material remains emissive independently of dynamic budget"),Material->K2_GetScalarParameterValue(TEXT("Night"))>.99f);
  Manager->SetLightEnabled(Near,false);Fixtures[0]->SetActorLocation(Eye);Manager->RefreshLighting();
  Test->TestTrue(TEXT("Disabled or broken fixture cannot relight when returning near"),!Near->IsVisible() && Near->Intensity==0);
  Manager->SetLightEnabled(Near,true);Manager->RefreshLighting();Test->TestTrue(TEXT("Manual enable restores nearby lamp"),Near->IsVisible());
  Clock->SetTimeOfDay(12);
  Test->TestTrue(TEXT("Automatic dawn disables night light"),Near->Intensity==0 && !Near->IsVisible());
  if(Material)Test->TestEqual(TEXT("Automatic dawn disables windows"),Material->K2_GetScalarParameterValue(TEXT("Night")),0.f);
  Manager->SetTimeOfDay(std::numeric_limits<float>::infinity());Test->TestEqual(TEXT("Invalid clock value rejected"),Manager->GetNightAmount(),0.f);
  auto Bad=Policy;Bad.CullDistance=-1;Test->TestFalse(TEXT("Invalid policy rejected without replacing valid fixture"),Manager->RegisterLight(Near,Bad));
  for(auto* A:Fixtures){Manager->UnregisterLight(CastChecked<UPointLightComponent>(A->GetLightComponent()));A->Destroy();}
  Manager->RefreshLighting();Test->TestEqual(TEXT("Fixture teardown restores original registrations"),Manager->GetRegisteredLightCount(),Original);
  Clock->SetTimeOfDay(10);return true;
 }
private:
 FAutomationTestBase* Test;double Start=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldLightingBudgetTest,"NeonCity.Lighting.WorldBudgetPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldLightingBudgetTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWorldLightingChecks>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
