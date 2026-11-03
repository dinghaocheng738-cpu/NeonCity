#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Adventure/DivingComponent.h"
#include "Water/UnderwaterViewComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"

namespace {
class FOceanOpticalSurvey : public IAutomationLatentCommand
{
public:
 explicit FOceanOpticalSurvey(FAutomationTestBase* InTest):Test(InTest),Evidence(TEXT("OpticalSurvey")){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>300){Test->AddError(TEXT("Optical survey timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,0))))return false;
  Evidence.Record(W,PC,Stage);
  if(!bEquipped)
  {
   if(!Test->TestTrue(TEXT("Survey uses ordinary equipped player camera"),Dive && Dive->Equip()))return true;
   Dive->SetFirstPerson(true);P->SetActorRotation(FRotator::ZeroRotator);P->GetCharacterMovement()->DisableMovement();bEquipped=true;
  }
  const FVector2D Points[]={NeonCityCoastalLayout::FromLegacyIsland(FVector2D(55400,-173500)),{55000,-176000},{55000,-178000},{141450,-170450},{142550,-211950},{142550,-211950},{55000,-178000},{55000,-178000},{55400,-173500},{142550,-211950},{142550,-211950},{142550,-211950},{142550,-211950}};
  const TCHAR* Names[]={TEXT("Shore-Day"),TEXT("Shallow-Day"),TEXT("Reef-Day"),TEXT("Deep24-Day"),TEXT("Deep37-Day"),TEXT("Deep37-Up"),TEXT("Reef-Night"),TEXT("Reef-Night-Torch"),TEXT("Shore-Dusk"),TEXT("Deep37-Day-Torch"),TEXT("Deep37-Day-Down-NoTorch"),TEXT("Deep37-Near-Torch"),TEXT("Deep37-Near-NoTorch")};
  if(Stage>=UE_ARRAY_COUNT(Points))
  {Test->AddInfo(TEXT("Diagnostic camera survey only, not a manual playthrough or visual acceptance."));return true;}
  if(!bPositioned)
  {
   const FVector2D XY=Points[Stage];if(!Region->EnsureDestinationReady(FVector(XY.X,XY.Y,0)))return false;
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(OceanOpticsFloor),false,P);
   if(!W->LineTraceSingleByChannel(Hit,FVector(XY.X,XY.Y,-250),FVector(XY.X,XY.Y,-22000),ECC_WorldStatic,Q))
   {Test->AddError(FString::Printf(TEXT("No real sea floor at survey point %s"),Names[Stage]));return true;}
   const float Sea=-130;
   const bool Shore=Stage==0 || Stage==8;
   const float Z=Shore?Sea+420:Stage==1?FMath::Max(Sea-170.f,float(Hit.ImpactPoint.Z)+165.f)
    :Stage==3?FMath::Max(Sea-2400.f,float(Hit.ImpactPoint.Z)+230.f)
    :Stage==4 || Stage==5 || Stage>=9?FMath::Max(Sea-3700.f,float(Hit.ImpactPoint.Z)+230.f):float(Hit.ImpactPoint.Z)+165.f;
   if(Stage==3 || Stage==4)Test->TestTrue(TEXT("Deep survey is at least 23m below actual surface"),Sea-Z>2300);
   P->SetActorLocation(FVector(XY.X-5,XY.Y,Z-64),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation(FRotator(Stage>=11?-65:Shore || Stage>=9?-22:Stage==5?38:-5,Shore?-82:60,0));
   if(auto* Torch=P->FindComponentByClass<UNeonCityFlashlightComponent>())
   {
    const bool On=Stage==7 || Stage==9 || Stage==11;Test->TestTrue(TEXT("Survey light state is actually applied"),Torch->SetEquipped(On));
    if(On)Test->TestTrue(TEXT("Suit lamp preserves both tool hands"),Torch->IsDiveMounted());
   }
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Stage==6 || Stage==7?22:Stage==8?17.3f:11);}
   Test->AddInfo(FString::Printf(TEXT("%s: real seabed Z=%.1f, diagnostic eye Z=%.1f, depth=%.2fm"),Names[Stage],Hit.ImpactPoint.Z,Z,(Sea-Z)*.01f));
   At=W->GetTimeSeconds();bPositioned=true;return false;
  }
  // Time jumps invalidate sky capture, Lumen history and eye adaptation. Let
  // those ordinary systems settle; do not force a flattering exposure.
  if(W->GetTimeSeconds()-At<(Stage>=6?18:8))return false;
  if(auto* Lamp=P->FindComponentByClass<UNeonCityFlashlightComponent>();Lamp && Lamp->IsEquipped() && Lamp->GetBeam())
  {
   const auto* Beam=Lamp->GetBeam();FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(OpticalLampReceiver),false,P);
   const bool Found=W->LineTraceSingleByChannel(Hit,Beam->GetComponentLocation(),Beam->GetComponentLocation()+Beam->GetForwardVector()*2200,ECC_WorldStatic,Q);
   Test->AddInfo(FString::Printf(TEXT("Lamp receiver %s: found=%d distance=%.2fm position=%s direction=%s eye=%s intensity=%.1f"),Names[Stage],Found,Found?Hit.Distance*.01f:-1,*Beam->GetComponentLocation().ToString(),*Beam->GetForwardVector().ToString(),*P->GetFollowCamera()->GetComponentLocation().ToString(),Beam->Intensity));
  }
  Test->TestTrue(TEXT("Native viewport capture succeeded"),FNeonCityVisualEvidence::CaptureHUD(W,FString(TEXT("Optics-"))+Names[Stage]+TEXT(".png")));
  if((Stage==2 || Stage==7) && FParse::Param(FCommandLine::Get(),TEXT("NeonCityOceanGPUProfile")))
  {PC->ConsoleCommand(TEXT("r.ProfileGPU.ShowUI 0"));PC->ConsoleCommand(TEXT("ProfileGPU"));}
  ++Stage;bPositioned=false;return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;double Started=0;float At=0;int32 Stage=0;bool bEquipped=false,bPositioned=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanOpticalSurvey,"NeonCity.OceanExperience.OpticalSurvey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityOceanOpticalSurvey::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FOceanOpticalSurvey(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
