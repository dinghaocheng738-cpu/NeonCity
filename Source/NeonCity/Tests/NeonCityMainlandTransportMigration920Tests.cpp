#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "World/NeonCityMainlandSite.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace MainlandTransportMigration920
{
AActor* StaticBlock(UWorld& World,UStaticMesh* Cube,FVector Location,FVector Size)
{
 auto* Actor=World.SpawnActor<AActor>();
 if(!Actor)return nullptr;
 auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
 Actor->SetRootComponent(Mesh);Actor->AddInstanceComponent(Mesh);
 Mesh->SetStaticMesh(Cube);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
 Mesh->SetCollisionObjectType(ECC_WorldStatic);
 Mesh->SetWorldLocation(Location);Mesh->SetWorldScale3D(Size/100);Mesh->RegisterComponent();
 return Actor;
}

FNeonCityMarineSaveData LegacyBoat()
{
 FNeonCityMarineSaveData Record;
 Record.VehicleID=UMarineWorldSubsystem::SpeedboatID();
 Record.DataAssetID=GetDefault<UMarineVehicleData>()->VehicleType;
 Record.Transform=FTransform(FRotator(0,-90,0),FVector(4860,-3350,-134.5));
 Record.Owned=false;
 Record.Condition=.37f;
 Record.AuthoredBerthVersion=1;
 return Record;
}
FNeonCityAircraftSaveData LegacyHelicopter()
{
 FNeonCityAircraftSaveData Record;
 Record.AircraftID=UAircraftWorldSubsystem::HelicopterID();
 Record.DataAssetID=GetDefault<UAircraftVehicleData>()->AircraftType;
 Record.Transform=FTransform(FRotator(0,-90,0),FVector(-3000,2450,137));
 Record.Owned=false;
 Record.Condition=.29f;
 return Record;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandUntouchedBoatMigration920,
 "NeonCity.Phase205c.MainlandC920.UntouchedSpeedboatBerthMigration",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandUntouchedBoatMigration920::RunTest(const FString&)
{
 using namespace MainlandTransportMigration920;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Authored mainland marker"),Site) || !TestNotNull(TEXT("Real static collision mesh"),Cube))return false;
 const auto Original=LegacyBoat();
 auto Record=Original;
 TestFalse(TEXT("Legacy worlds without marker retain their original berth"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(nullptr,Record));
 TestFalse(TEXT("A marker alone cannot rewrite saves"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 TestTrue(TEXT("Disabled recovery leaves the full pose intact"),Record.Transform.Equals(Original.Transform));
 Site->bRecoverUnsupportedMainlandVehicles=true;
 Site->SpeedboatBerth=FTransform(FRotator(0,-90,0),FVector(4670,-4700,-134.5));
 TestFalse(TEXT("A clear original/custom berth is preserved without an actual obstruction"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 auto* Ramp=StaticBlock(*World,Cube,FVector(5000,-3350,30),FVector(320,1000,20));
 if(!TestNotNull(TEXT("Actual replacement boarding ramp above the old buoyancy hull"),Ramp))return false;
 TestTrue(TEXT("A trapped default boat moves to the authored berth"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 TestTrue(TEXT("The authored pose is used exactly"),Record.Transform.Equals(Site->GetSpeedboatBerth()));
 TestEqual(TEXT("Boat GUID is preserved"),Record.VehicleID,Original.VehicleID);
 TestEqual(TEXT("Boat profile is preserved"),Record.DataAssetID,Original.DataAssetID);
 TestEqual(TEXT("Boat ownership is preserved"),Record.Owned,Original.Owned);
 TestEqual(TEXT("Boat damage condition is preserved"),Record.Condition,Original.Condition);
 TestEqual(TEXT("Boat schema version is preserved"),Record.AuthoredBerthVersion,Original.AuthoredBerthVersion);
 TestFalse(TEXT("Repeated normalization does not relocate an authored berth"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 auto CheckRetained=[&](const TCHAR* Label,FNeonCityMarineSaveData Candidate)
 {
  const FTransform Before=Candidate.Transform;
  TestFalse(Label,UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Candidate));
  TestTrue(TEXT("Rejected boat migration leaves its pose intact"),Candidate.Transform.Equals(Before));
 };
 Record=Original;Record.Transform=FTransform(FRotator(1.3,-88.7,.6),FVector(4878,-3371,-127.8));
 TestTrue(TEXT("Realistic bob, settling, yaw and fender drift under the ramp are recovered"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 TestEqual(TEXT("A settled boat's damage is retained"),Record.Condition,Original.Condition);
 Record=Original;Record.Transform.AddToTranslation(FVector(.1,0,0));
 TestTrue(TEXT("Sub-centimetre physics drift does not evade obstruction recovery"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 Record=Original;Record.Transform.SetScale3D(FVector(2));CheckRetained(TEXT("Malformed scale stays under structural validation"),Record);
 Record=Original;Record.VehicleID=UMarineWorldSubsystem::HarborlineID();CheckRetained(TEXT("The canonical island fishing boat is protected"),Record);
 Record=Original;Record.VehicleID=FGuid::NewGuid();CheckRetained(TEXT("Unknown boat GUIDs are protected"),Record);
 Record=Original;Record.DataAssetID=TEXT("OtherBoatProfile");CheckRetained(TEXT("Other boat profiles are protected"),Record);
 Record=Original;Record.Transform.SetLocation(FVector(148000,-280000,-134.5));CheckRetained(TEXT("Island and offshore saved positions are protected"),Record);
 Record=Original;Record.Condition=-.1f;CheckRetained(TEXT("Structural condition validation precedes migration"),Record);
 Record=Original;Record.AuthoredBerthVersion=2;CheckRetained(TEXT("Unknown future berth versions are not migrated"),Record);
 Record=Original;Record.AuthoredBerthVersion=0;
 TestTrue(TEXT("An older trapped record is eligible"),UMarineWorldSubsystem::MigrateUntouchedMainlandSpeedboatBerth(Site,Record));
 TestEqual(TEXT("The migration does not independently change schema version"),Record.AuthoredBerthVersion,0);
 auto* BlockedBerth=StaticBlock(*World,Cube,FVector(4670,-4700,-30),FVector(100,100,200));
 if(!TestNotNull(TEXT("Obstructed authored destination"),BlockedBerth))return false;
 Record=Original;CheckRetained(TEXT("Migration must not put a boat into an obstructed new berth"),Record);
 BlockedBerth->Destroy();Ramp->Destroy();
 Record=Original;Record.Transform.AddToTranslation(FVector(23,-16,5));
 CheckRetained(TEXT("A nearby clear custom boat pose stays exact"),Record);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandUntouchedHelicopterMigration920,
 "NeonCity.Phase205c.MainlandC920.UntouchedHelicopterLandingMigration",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandUntouchedHelicopterMigration920::RunTest(const FString&)
{
 using namespace MainlandTransportMigration920;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Authored mainland marker"),Site) || !TestNotNull(TEXT("Real static collision mesh"),Cube))return false;
 const auto Original=LegacyHelicopter();
 auto Record=Original;
 TestFalse(TEXT("Legacy worlds without marker retain their helicopter landing"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(nullptr,Record));
 TestFalse(TEXT("A marker alone cannot relocate a saved helicopter"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 Site->bRecoverUnsupportedMainlandVehicles=true;
 Site->HelicopterLanding=FTransform(FRotator(0,-90,0),FVector(-13500,7000,137));
 auto* RoadFloor=StaticBlock(*World,Cube,FVector(-3000,2450,25),FVector(1000,1300,30));
 auto* Landing=StaticBlock(*World,Cube,FVector(-13500,7000,40),FVector(1000,1300,30));
 if(!TestNotNull(TEXT("Real replaced-pad road support"),RoadFloor) || !TestNotNull(TEXT("Real new stand support"),Landing))return false;
 TestFalse(TEXT("Static support alone cannot identify a retired pad replacement"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 auto* Road=World->SpawnActor<ANeonCityTrafficRoute>();
 if(!TestNotNull(TEXT("Actual authored road route"),Road))return false;
 Road->RoadID=TEXT("C920.QuayRear");Road->Tags.Add(TEXT("NeonCity.MainlandC920"));
 auto* Spline=Road->GetRoadSpline();Spline->ClearSplinePoints(false);
 Spline->SetClosedLoop(false,false);
 Spline->AddSplinePoint(FVector(-16000,2500,40),ESplineCoordinateSpace::World,false);
 Spline->AddSplinePoint(FVector(16000,2500,40),ESplineCoordinateSpace::World,true);
 TestTrue(TEXT("A grounded old-pad helicopter clears the actual replacement road"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 TestTrue(TEXT("The authored landing is used exactly"),Record.Transform.Equals(Site->GetHelicopterLanding()));
 TestEqual(TEXT("Aircraft GUID is preserved"),Record.AircraftID,Original.AircraftID);
 TestEqual(TEXT("Aircraft profile is preserved"),Record.DataAssetID,Original.DataAssetID);
 TestEqual(TEXT("Aircraft ownership is preserved"),Record.Owned,Original.Owned);
 TestEqual(TEXT("Aircraft damage condition is preserved"),Record.Condition,Original.Condition);
 TestFalse(TEXT("Repeated normalization is idempotent"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 auto CheckRetained=[&](const TCHAR* Label,FNeonCityAircraftSaveData Candidate)
 {
  const FTransform Before=Candidate.Transform;
  TestFalse(Label,UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Candidate));
  TestTrue(TEXT("Rejected aircraft migration leaves its pose intact"),Candidate.Transform.Equals(Before));
 };
 Record=Original;Record.Transform=FTransform(FRotator(.3,-89.1,.2),FVector(-2984,2468,116));
 TestTrue(TEXT("A realistically settled and slightly drifted helicopter clears the replacement road"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 TestEqual(TEXT("A settled helicopter's condition is retained"),Record.Condition,Original.Condition);
 Record=Original;Record.Transform.AddToTranslation(FVector(0,.1,0));
 TestTrue(TEXT("Sub-centimetre settling does not evade retired-road detection"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 Record=Original;Record.Transform.SetLocation(FVector(-3000,1940,137));CheckRetained(TEXT("A clear off-road custom choice on the old pad stays exact"),Record);
 Record=Original;Record.AircraftID=FGuid::NewGuid();CheckRetained(TEXT("Other aircraft identities are protected"),Record);
 Record=Original;Record.DataAssetID=TEXT("OtherAircraftProfile");CheckRetained(TEXT("Other aircraft profiles are protected"),Record);
 Record=Original;Record.Transform.SetLocation(FVector(148000,-280000,30000));CheckRetained(TEXT("Island and airborne saved choices are protected"),Record);
 Record=Original;Record.Transform.SetLocation(FVector(-3000,2450,500));CheckRetained(TEXT("An airborne pose over the retired pad is protected"),Record);
 Record=Original;Record.Condition=1.1f;CheckRetained(TEXT("Structural condition validation precedes migration"),Record);
 Record=Original;Record.Transform.SetScale3D(FVector(2));CheckRetained(TEXT("Malformed poses cannot be rescued by default migration"),Record);
 auto* BlockedLanding=StaticBlock(*World,Cube,FVector(-13500,7000,140),FVector(100,300,200));
 if(!TestNotNull(TEXT("Obstruction inside the new stand"),BlockedLanding))return false;
 Record=Original;CheckRetained(TEXT("An obstructed authored stand is not used"),Record);
 BlockedLanding->Destroy();Road->Destroy();
 Record=Original;Record.Transform.AddToTranslation(FVector(17,6,0));
 CheckRetained(TEXT("A clear custom location without the replacement road remains exact"),Record);
 auto* Wall=StaticBlock(*World,Cube,FVector(-3000,2450,140),FVector(100,300,200));
 if(!TestNotNull(TEXT("Real static wall trapping a drifted old-pad helicopter"),Wall))return false;
 TestTrue(TEXT("Actual obstruction within the retired pad independently triggers safe migration"),UAircraftWorldSubsystem::MigrateUntouchedMainlandHelicopterLanding(Site,Record));
 return true;
}
#endif
