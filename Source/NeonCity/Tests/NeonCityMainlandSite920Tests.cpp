#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/NeonCityMainlandSite.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/MarineTestArea.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Aircraft/AircraftTestArea.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Water/CoastalOceanSurface.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandSitePolicy920,
 "NeonCity.Phase205c.MainlandC920.TransportFixturePolicy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandSitePolicy920::RunTest(const FString&)
{
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 TestNull(TEXT("A legacy world does not opt in to mainland replacement"),ANeonCityMainlandSite::Find(World));
 auto* Marine=World->SpawnActor<AMarineTestArea>();
 auto* Aircraft=World->SpawnActor<AAircraftTestArea>();
 if(!TestNotNull(TEXT("Legacy marine fixture"),Marine) || !TestNotNull(TEXT("Legacy aircraft fixture"),Aircraft)) return false;
 TestTrue(TEXT("Legacy marine collision remains available without a marker"),Marine->GetActorEnableCollision());
 UPrimitiveComponent* Barrier=nullptr;
 TInlineComponentArray<UPrimitiveComponent*> AircraftComponents(Aircraft);
 for(auto* Component:AircraftComponents) if(Component->GetFName()==TEXT("ImpactBarrier")) Barrier=Component;
 if(!TestNotNull(TEXT("Test reaches the actual legacy aircraft air-wall component"),Barrier)) return false;
 TestTrue(TEXT("Legacy isolated aircraft test retains its impact obstacle"),Barrier->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);

 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Authored mainland site"),Site)) return false;
 Site->SetActorLocationAndRotation(FVector(1000,2000,300),FRotator(0,90,0));
 Site->SetActorScale3D(FVector(3));
 Site->SpeedboatBerth=FTransform(FRotator(0,-90,0),FVector(200,-400,-100),FVector(2));
 Site->HelicopterLanding=FTransform(FRotator(0,90,0),FVector(-600,300,50));
 TestEqual(TEXT("Persistent authored site is found"),ANeonCityMainlandSite::Find(World),Site);
 TestTrue(TEXT("Berth uses site rotation and position, never visual scale"),
  Site->GetSpeedboatBerth().Equals(FTransform(FRotator::ZeroRotator,FVector(1400,2200,200)),.01f));
 TestTrue(TEXT("Helicopter landing independently follows the same rigid site anchor"),
  Site->GetHelicopterLanding().Equals(FTransform(FRotator(0,180,0),FVector(700,1400,350)),.01f));

 Site->bReplaceLegacyTransportFixtures=false;
 Site->ApplyLegacyTransportFixturePolicy(*Aircraft);
 TestTrue(TEXT("Explicit legacy policy leaves the fixture obstacle intact"),Barrier->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
 Site->bReplaceLegacyTransportFixtures=true;
 for(AActor* Helper:{static_cast<AActor*>(Marine),static_cast<AActor*>(Aircraft)})
 {
  Site->ApplyLegacyTransportFixturePolicy(*Helper);
  TestFalse(TEXT("Replacement helper cannot block players"),Helper->GetActorEnableCollision());
  TInlineComponentArray<UPrimitiveComponent*> Components(Helper);
  for(auto* Component:Components)
  {
   TestEqual(*FString::Printf(TEXT("Retired fixture has no collision: %s"),*Component->GetName()),
    Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   TestFalse(*FString::Printf(TEXT("Retired fixture is invisible: %s"),*Component->GetName()),Component->IsVisible());
  }
 }
 Marine->Destroy();Aircraft->Destroy();Site->Destroy();
 return !HasAnyErrors();
}
#endif

namespace MainlandSite920
{
/** Checks normal runtime bootstrap, not a synthetic replacement vehicle. */
class FBootstrap : public IAutomationLatentCommand
{
public:
 explicit FBootstrap(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override
 {
  auto* World=NeonCityValidationWorld();
  if(!World || World->GetTimeSeconds()<3)
  {
   if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("Normal mainland game world did not become ready"));return true;}
   return false;
  }
  const auto* Site=ANeonCityMainlandSite::Find(World);
  if(!Test->TestNotNull(TEXT("The replacement map authors its persistent transport site"),Site))return true;
  Test->TestTrue(TEXT("Replacement site retires the obsolete fixture presentation"),Site->bReplaceLegacyTransportFixtures);
  int32 MarineHelpers=0,AircraftHelpers=0,Speedboats=0,Helicopters=0,Oceans=0;
  for(TActorIterator<AMarineTestArea> It(World);It;++It){++MarineHelpers;CheckHelper(*It);}
  for(TActorIterator<AAircraftTestArea> It(World);It;++It){++AircraftHelpers;CheckHelper(*It);}
  for(TActorIterator<ANeonCityMarineVehicleBase> It(World);It;++It) if(It->VehicleID==UMarineWorldSubsystem::SpeedboatID())++Speedboats;
  for(TActorIterator<ANeonCityAircraftVehicleBase> It(World);It;++It) if(It->AircraftID==UAircraftWorldSubsystem::HelicopterID())++Helicopters;
  for(TActorIterator<ACoastalOceanSurface> It(World);It;++It)++Oceans;
  Test->TestEqual(TEXT("Existing marine bootstrap remains unique"),MarineHelpers,1);
  Test->TestEqual(TEXT("Existing aircraft bootstrap remains unique"),AircraftHelpers,1);
  Test->TestEqual(TEXT("Canonical speedboat identity remains unique"),Speedboats,1);
  Test->TestEqual(TEXT("Canonical helicopter identity remains unique"),Helicopters,1);
  Test->TestTrue(TEXT("Normal ocean provider survives fixture retirement"),Oceans>0);

  const auto* Marine=World->GetSubsystem<UMarineWorldSubsystem>();
  const auto* Aircraft=World->GetSubsystem<UAircraftWorldSubsystem>();
  if(!Test->TestNotNull(TEXT("Existing marine registry"),Marine) || !Test->TestNotNull(TEXT("Existing aircraft registry"),Aircraft))return true;
  // Design defaults record the actual FinishSpawning pose before buoyancy or
  // landing settling, so this assertion does not depend on sea state or frame rate.
  const auto Boats=Marine->BuildPersistenceSnapshot(true);
  const auto* Speedboat=Boats.FindByPredicate([](const auto& Boat){return Boat.VehicleID==UMarineWorldSubsystem::SpeedboatID();});
  Test->TestTrue(TEXT("Canonical speedboat spawns at the authored berth"),Speedboat && Speedboat->Transform.Equals(Site->GetSpeedboatBerth(),.1f));
  if(const auto* Coast=ANeonCityCoastalRegion::Find(World))
  {
   const auto* FishingBoat=Boats.FindByPredicate([](const auto& Boat){return Boat.VehicleID==UMarineWorldSubsystem::HarborlineID();});
   Test->TestTrue(TEXT("Island fishing berth is unchanged by mainland replacement"),FishingBoat && FishingBoat->Transform.Equals(Coast->GetFishingBerth(),.1f));
  }
  const auto AircraftDefaults=Aircraft->BuildPersistenceSnapshot(true);
  const auto* Helicopter=AircraftDefaults.FindByPredicate([](const auto& Entry){return Entry.AircraftID==UAircraftWorldSubsystem::HelicopterID();});
  Test->TestTrue(TEXT("Canonical helicopter spawns at the authored landing"),Helicopter && Helicopter->Transform.Equals(Site->GetHelicopterLanding(),.1f));
  float WaterHeight=0;FVector Flow;
  Test->TestTrue(TEXT("Authored speedboat berth has the existing live ocean query"),Marine->SampleWater(Site->GetSpeedboatBerth().GetLocation(),WaterHeight,Flow));
  return true;
 }
private:
 void CheckHelper(AActor* Helper)
 {
  Test->TestFalse(*FString::Printf(TEXT("Retired runtime helper cannot create an air wall: %s"),*Helper->GetName()),Helper->GetActorEnableCollision());
  TInlineComponentArray<UPrimitiveComponent*> Components(Helper);
  for(const auto* Component:Components)
  {
   Test->TestEqual(*FString::Printf(TEXT("Runtime fixture collision retired: %s/%s"),*Helper->GetName(),*Component->GetName()),
    Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   Test->TestFalse(*FString::Printf(TEXT("Runtime fixture presentation retired: %s/%s"),*Helper->GetName(),*Component->GetName()),Component->IsVisible());
  }
 }
 FAutomationTestBase* Test;
 double Started;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandTransportBootstrap920,
 "NeonCity.Phase205c.MainlandC920.Standalone.TransportBootstrap",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandTransportBootstrap920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Use the isolated validation session, not the user's save slot"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandSite920::FBootstrap(this));
 return true;
}
#endif
