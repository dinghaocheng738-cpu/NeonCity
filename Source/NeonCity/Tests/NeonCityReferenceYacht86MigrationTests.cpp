#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "World/NeonCityMainlandSite.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceYacht86Migration,
 "NeonCity.Phase205c.MainlandC923.Reference86.RetiredYachtMigration",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceYacht86Migration::RunTest(const FString&)
{
 UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Site=W->SpawnActor<ANeonCityMainlandSite>();
 auto* Boat=W->SpawnActor<ANeonCityMotorYacht>();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Site"),Site)||!TestNotNull(TEXT("Yacht"),Boat)||!TestNotNull(TEXT("Static geometry"),Cube))return false;
 Site->bRecoverUnsupportedMainlandVehicles=true;
 Boat->VehicleID=FGuid(0x3A8B376B,0x91695E5C,0xB164FD5E,0xEA9E8457);
 Boat->Tags.Add(TEXT("NeonCity.MainlandC923.Reference86.RelocatedYacht"));
 const FTransform Destination(FRotator(0,-90,0),FVector(-9500,-3796.9651413,-130));
 FNeonCityMarineSaveData Original;Original.VehicleID=Boat->VehicleID;Original.DataAssetID=Boat->GetProfile()->VehicleType;
 Original.Transform=FTransform(FRotator(0,-90,0),FVector(-4000,-3420,-130));Original.AuthoredBerthVersion=1;Original.Owned=false;Original.Condition=.37;
 auto R=Original;
 TestFalse(TEXT("Clear player berth remains exact"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R));
 auto* Obstruction=W->SpawnActor<AActor>();auto* Mesh=NewObject<UStaticMeshComponent>(Obstruction);Obstruction->SetRootComponent(Mesh);Obstruction->AddInstanceComponent(Mesh);
 Mesh->SetStaticMesh(Cube);Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->SetCollisionObjectType(ECC_WorldStatic);Mesh->SetWorldLocation(FVector(-4000,-3420,-30));Mesh->SetWorldScale3D(FVector(6,6,4));Mesh->RegisterComponent();
 TestTrue(TEXT("Only actual retired berth obstruction triggers migration"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R));
 TestTrue(TEXT("Exact authored berth"),R.Transform.Equals(Destination));
 TestEqual(TEXT("GUID retained"),R.VehicleID,Original.VehicleID);TestEqual(TEXT("Profile retained"),R.DataAssetID,Original.DataAssetID);
 TestEqual(TEXT("Ownership retained"),R.Owned,Original.Owned);TestEqual(TEXT("Damage retained"),R.Condition,Original.Condition);
 TestFalse(TEXT("Migration is idempotent"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R));
 auto Retained=[&](const TCHAR* Label,FNeonCityMarineSaveData C){auto Before=C.Transform;TestFalse(Label,UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,C));TestTrue(TEXT("Rejected full pose unchanged"),C.Transform.Equals(Before));};
 R=Original;R.Transform.SetLocation(FVector(-6000,-6000,-130));Retained(TEXT("Player sea pose protected"),R);
 R=Original;R.VehicleID=FGuid::NewGuid();Retained(TEXT("Other yacht protected"),R);
 R=Original;R.AuthoredBerthVersion=2;Retained(TEXT("Future versions protected"),R);
 R=Original;R.DataAssetID=TEXT("Other");Retained(TEXT("Other profiles protected"),R);
 R=Original;R.Transform.SetRotation(FRotator(0,0,0).Quaternion());Retained(TEXT("Different mooring orientation protected"),R);
 R=Original;R.Transform.SetRotation(FRotator(45,-90,0).Quaternion());Retained(TEXT("Capsized/custom attitude protected"),R);
 Site->bRecoverUnsupportedMainlandVehicles=false;R=Original;Retained(TEXT("Disabled site recovery protected"),R);Site->bRecoverUnsupportedMainlandVehicles=true;
 auto* Water=W->SpawnActor<ANeonCityWaterVolume>(Destination.GetLocation(),FRotator::ZeroRotator);
 if(!TestNotNull(TEXT("Real native overlap-only water envelope"),Water))return false;
 // Editor worlds do not initialize the runtime brush in PostInitializeComponents.
 // Supply the equivalent registered overlap envelope for this isolated test.
 auto* WaterEnvelope=NewObject<UBoxComponent>(Water);Water->AddInstanceComponent(WaterEnvelope);
 WaterEnvelope->SetBoxExtent(FVector(1000));WaterEnvelope->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
 WaterEnvelope->SetCollisionObjectType(ECC_WorldDynamic);WaterEnvelope->SetWorldLocation(Destination.GetLocation());WaterEnvelope->RegisterComponent();
 R=Original;
 TestTrue(TEXT("Nonblocking water at destination does not prevent retired berth recovery"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R));
 auto* DestinationBlock=W->SpawnActor<AActor>();auto* Block=NewObject<UStaticMeshComponent>(DestinationBlock);DestinationBlock->SetRootComponent(Block);DestinationBlock->AddInstanceComponent(Block);
 Block->SetStaticMesh(Cube);Block->SetCollisionProfileName(TEXT("BlockAll"));Block->SetCollisionObjectType(ECC_WorldStatic);Block->SetWorldLocation(Destination.GetLocation());Block->RegisterComponent();
 R=Original;Retained(TEXT("Obstructed destination protected"),R);DestinationBlock->Destroy();
 auto* OtherBoat=W->SpawnActor<ANeonCityMotorYacht>();OtherBoat->VehicleID=FGuid::NewGuid();OtherBoat->SetActorLocation(Destination.GetLocation());
 R=Original;Retained(TEXT("Live boat occupying destination protected"),R);OtherBoat->SetActorLocation(FVector(10000,10000,-130));
 auto Other=Original;Other.VehicleID=OtherBoat->VehicleID;Other.Transform=Destination;TArray<FNeonCityMarineSaveData> Incoming={Original,Other};R=Original;
 TestFalse(TEXT("Saved boat at destination protected before poses apply"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R,&Incoming));
 TestTrue(TEXT("Occupied saved destination leaves original pose exact"),R.Transform.Equals(Original.Transform));
 Other.Transform.SetLocation(FVector(12000,12000,-130));Incoming={Original,Other};R=Original;
 TestTrue(TEXT("Distinct clear saved boats do not prevent safe recovery"),UMarineWorldSubsystem::MigrateRetiredReferenceYachtBerth(Boat,Destination,R,&Incoming));
 Boat->Tags.Reset();R=Original;Retained(TEXT("Old maps without authored opt-in protected"),R);
 return true;
}
#endif
