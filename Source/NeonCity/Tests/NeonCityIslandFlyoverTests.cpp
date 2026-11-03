#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Components/BoxComponent.h"
#include "Physics/PhysicalImpactComponent.h"
#include "EngineUtils.h"
namespace
{
class FIslandFlyover:public IAutomationLatentCommand
{
public:
 explicit FIslandFlyover(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>160){Test->AddError(FString::Printf(TEXT("Island helicopter route timeout waypoint %d"),Point));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Heli)
  {
   auto* P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return false;
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-173000,200))))return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It){Heli=*It;break;}if(!Heli)return false;
   P->SetActorLocation(Heli->GetActorLocation()+FVector(0,300,50),false,nullptr,ETeleportType::TeleportPhysics);
   if(!Test->TestTrue(TEXT("Ordinary driver enters helicopter"),Heli->EnterAircraft_Implementation(P)))return true;
   Heli->GetAircraftMovement()->StopEngine();Heli->SetActorLocationAndRotation(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-176000,5000)),FRotator(0,45,0),false,nullptr,ETeleportType::TeleportPhysics);
   Heli->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);Heli->GetHull()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Heli->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);
   if(!Test->TestTrue(TEXT("Engine starts at flight review fixture"),Heli->GetAircraftMovement()->StartEngine()))return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(10);}
   Last=Heli->GetActorLocation();return false;
  }
  const FVector Route[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(64500,-168000,5000)),NeonCityCoastalLayout::FromLegacyIsland(FVector(61000,-153500,6000)),NeonCityCoastalLayout::FromLegacyIsland(FVector(48500,-145000,8500)),NeonCityCoastalLayout::FromLegacyIsland(FVector(43000,-160000,6500)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-175000,5000))};
  const FVector Here=Heli->GetActorLocation();Distance+=FVector::Distance(Here,Last);Last=Here;
  if(!Test->TestTrue(TEXT("Flight remains physically simulated and occupied"),Heli->GetHull()->IsSimulatingPhysics() && PC->GetPawn()==Heli))return true;
  const FVector Error=Route[Point]-Here;const FVector Local=FRotator(0,Heli->GetActorRotation().Yaw,0).UnrotateVector(Error.GetSafeNormal2D());
  const float Yaw=FMath::Clamp(FMath::FindDeltaAngleDegrees(Heli->GetActorRotation().Yaw,Error.Rotation().Yaw)/65.f,-1.f,1.f);
  Heli->SubmitFlightInput_Implementation(PC,FVector(Local.X,Local.Y,FMath::Clamp(Error.Z/800.,-1.,1.)),Yaw);
  PC->SetControlRotation(FRotator(-30,Heli->GetActorRotation().Yaw,0));
  if(W->GetTimeSeconds()-LastShot>2)
  {
   LastShot=W->GetTimeSeconds();
   FNeonCityVisualEvidence::CaptureHUD(W,*FString::Printf(TEXT("IslandFlyover-Sequence-%03d.png"),Sequence++));
  }
  if(Error.Size2D()<1200 && FMath::Abs(Error.Z)<1500)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,*FString::Printf(TEXT("IslandFlyover-%d.png"),Point));++Point;
   if(Point==UE_ARRAY_COUNT(Route)){Heli->SubmitFlightInput_Implementation(PC,FVector::ZeroVector,0);Test->TestTrue(TEXT("Actual island circuit exceeds 500 metres"),Distance>50000);return true;}
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0,Distance=0;float LastShot=0;int32 Point=0,Sequence=0;ANeonCityHelicopterPrototype* Heli=nullptr;FVector Last;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandFlyoverPIE,"NeonCity.Polish.IslandFlyoverPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandFlyoverPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FIslandFlyover(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
