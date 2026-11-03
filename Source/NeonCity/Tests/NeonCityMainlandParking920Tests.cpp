#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Adventure/IslandSupercar.h"
#include "Combat/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace MainlandParking920
{
// No BeginPlay, map load, player input, ticking, save reads or save writes. The
// owned registry and collision queries run in an isolated native physics world.
struct FFixture
{
 TStrongObjectPtr<UGameInstance> GI;
 UWorld* World=nullptr;
 FFixture():GI(NewObject<UGameInstance>(GEngine))
 {
  GI->InitializeStandalone(FName(*FString::Printf(TEXT("C920Parking_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
  World=GI->GetWorld();
 }
 ~FFixture()
 {
  // Shutdown still needs its live world context for subsystem teardown. It
  // clears GI's context pointer; only then remove our context and physics world.
  GI->Shutdown();
  if(World){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
 }
};

AActor* StaticFloor(UWorld& World,UStaticMesh* Cube,FVector Location,FVector Size)
{
 auto* Actor=World.SpawnActor<AActor>();
 if(!Actor)return nullptr;
 auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
 Actor->SetRootComponent(Mesh);Actor->AddInstanceComponent(Mesh);
 Mesh->SetStaticMesh(Cube);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
 Mesh->SetCollisionObjectType(ECC_WorldStatic);
 Mesh->SetWorldScale3D(Size/100.);Mesh->SetWorldLocation(Location);Mesh->RegisterComponent();
 return Actor;
}

AIslandSupercar* Car(UWorld& World,const FTransform& Pose,UVehicleData* Profile)
{
 auto* Vehicle=World.SpawnActorDeferred<AIslandSupercar>(AIslandSupercar::StaticClass(),Pose,nullptr,nullptr,
  ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Vehicle)return nullptr;
 Vehicle->VehicleID=FGuid::NewGuid();Vehicle->VehicleData=Profile;Vehicle->bRetractable=true;
 Vehicle->FinishSpawning(Pose);
 Vehicle->GetMesh()->SetAllBodiesSimulatePhysics(true);
 Vehicle->GetHealthComponent()->ApplyPersistenceHealth(.46f*Profile->Durability,Profile->Durability);
 return Vehicle;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandParkingRetention920,
 "NeonCity.Phase205c.MainlandC920.GarageSuspensionRetention",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandParkingRetention920::RunTest(const FString&)
{
 using namespace MainlandParking920;
 FFixture F;
 if(!TestNotNull(TEXT("Isolated native physics world"),F.World))return false;
 auto* Properties=F.GI->GetSubsystem<UPropertySubsystem>();
 auto* Vehicles=F.GI->GetSubsystem<UVehicleOwnershipSubsystem>();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto* Profile=LoadObject<UVehicleData>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Nacre/DA_HeroNacre.DA_HeroNacre"));
 if(!TestNotNull(TEXT("Production property registry"),Properties) || !TestNotNull(TEXT("Production ownership registry"),Vehicles)
  || !TestNotNull(TEXT("Real collision support"),Cube) || !TestNotNull(TEXT("Existing Nacre profile"),Profile))return false;
 const FVector Bay(3799.5,-830.,55.);
 const FTransform RestPose(FRotator(0,90,0),Bay+FVector(0,0,27.63));
 auto* Floor=StaticFloor(*F.World,Cube,Bay-FVector(0,0,10),FVector(700,800,20));
 auto* Vehicle=Car(*F.World,RestPose,Profile);
 if(!TestNotNull(TEXT("Physical garage floor"),Floor) || !TestNotNull(TEXT("Real simulated original Nacre"),Vehicle))return false;
 TestTrue(TEXT("Actual chassis root simulates physics"),Vehicle->GetRootComponent()->IsSimulatingPhysics());
 if(!TestTrue(TEXT("Register actual live car"),Vehicles->RegisterLiveVehicle(Vehicle))
  || !TestTrue(TEXT("Owned car enters existing production collection"),Vehicles->GrantVehicle(Vehicle)))return false;

 auto* Data=NewObject<UNeonCityPropertyData>(F.GI.Get());
 Data->PropertyID=TEXT("Test.C920.SuspensionGarage");Data->DisplayName=FText::FromString(TEXT("Suspension test garage"));
 Data->GarageSlotCount=1;Data->bInitiallyOwned=true;
 auto* Owner=F.World->SpawnActor<AActor>();
 if(!TestNotNull(TEXT("Physical garage owner"),Owner))return false;
 auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Owner->AddInstanceComponent(Root);Root->RegisterComponent();
 auto* Component=NewObject<UPropertyComponent>(Owner);Owner->AddInstanceComponent(Component);
 Component->Data=Data;Component->GarageSlotsLocal={Bay};Component->RegisterComponent();
 if(!TestTrue(TEXT("Production property registers actual bay"),Properties->RegisterLiveProperty(Component)))return false;

 FNeonCityPropertySaveData Saved;Saved.PropertyID=Data->PropertyID;Saved.Owned=true;
 FNeonCityGarageSaveData Slot;Slot.SlotIndex=0;Slot.StoredLocation=Bay;Slot.VehicleID=Vehicle->VehicleID;
 Saved.GarageSlots={Slot};
 auto Linked=[&]()
 {
  FNeonCityPropertyRecord Record;
  return Properties->GetProperty(Data->PropertyID,Record) && Record.GarageSlots.Num()==1
   && Record.GarageSlots[0].VehicleID==Vehicle->VehicleID;
 };
 auto Reset=[&](FVector Velocity,FVector Offset=FVector::ZeroVector)
 {
  Vehicle->SetActorTransform(FTransform(RestPose.GetRotation(),RestPose.GetLocation()+Offset),false,nullptr,ETeleportType::TeleportPhysics);
  Vehicle->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
  Vehicle->GetMesh()->SetAllPhysicsLinearVelocity(Velocity);
  Properties->ApplyPersistenceSnapshot({Saved});
 };

 // Reproduces the actual reader's 129.3 cm/s rebound without depending on
 // whether the half-second property tick happens to coincide with that frame.
 Reset(FVector(0,0,129.3));
 const FVector ActualVelocity=Vehicle->GetVelocity();
 if(!TestTrue(TEXT("Measured physical vertical speed reproduces the old 3D rejection"),
  ActualVelocity.Size()>=50 && ActualVelocity.Size2D()<.01 && FMath::IsNearlyEqual(ActualVelocity.Z,129.3,.1)))return false;
 TestFalse(TEXT("New storage still requires full 3D stationary speed"),Properties->StoreVehicle(Data->PropertyID,0,Vehicle));
 // StoreVehicle reconciles first: the already owned link must survive that
 // call even though its strict new-registration gate rejects the rebound.
 TestTrue(TEXT("Strict registration attempt preserves the prior supported link"),Linked());
 Properties->ReconcileGarage(Data->PropertyID);
 TestTrue(TEXT("Actual ReconcileGarage retains supported vertical suspension motion"),Linked());
 TestTrue(TEXT("Linked garage finder resolves the exact car during vertical settling"),
  Properties->FindParkedVehicle(Data->PropertyID,0)==Vehicle);
 Properties->ReconcileGarage(Data->PropertyID);
 TestTrue(TEXT("Repeated retention is idempotent"),Linked());
 Reset(FVector(0,0,-129.3));Properties->ReconcileGarage(Data->PropertyID);
 TestTrue(TEXT("Supported downward suspension travel also retains the original link"),Linked());

 Reset(FVector(50.1,0,129.3));Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Horizontal departure still releases the slot immediately"),Linked());
 Reset(FVector(0,0,129.3),FVector(251,0,0));Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Leaving the 250 cm bay radius still releases the slot"),Linked());
 Reset(FVector(0,0,-129.3),FVector(0,0,100));Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Airborne car above actual suspension reach cannot retain a link"),Linked());
 Floor->SetActorEnableCollision(false);
 Reset(FVector(0,0,129.3));Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Missing static support releases vertical motion"),Linked());
 Floor->SetActorEnableCollision(true);
 Reset(FVector(0,0,129.3));Vehicle->GetHealthComponent()->ApplyPersistenceHealth(0,Profile->Durability);
 Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Destroyed cars do not retain the slot while rebounding"),Linked());
 Vehicle->GetHealthComponent()->ApplyPersistenceHealth(.46f*Profile->Durability,Profile->Durability);
 Reset(FVector(0,0,129.3));
 auto* PC=F.World->SpawnActor<APlayerController>();
 if(!TestNotNull(TEXT("Native controller for occupancy guard"),PC))return false;
 PC->Possess(Vehicle);Properties->ReconcileGarage(Data->PropertyID);
 TestFalse(TEXT("Entering the registered car still releases its slot"),Linked());PC->UnPossess();

 // An overlapping unowned car must never inherit the original association.
 auto* Unowned=Car(*F.World,RestPose,Profile);
 if(!TestNotNull(TEXT("Independent unowned car"),Unowned)
  || !TestTrue(TEXT("Unowned live actor registers without granting ownership"),Vehicles->RegisterLiveVehicle(Unowned)))return false;
 auto UnownedSave=Saved;UnownedSave.GarageSlots[0].VehicleID=Unowned->VehicleID;
 Properties->ApplyPersistenceSnapshot({UnownedSave});Properties->ReconcileGarage(Data->PropertyID);
 FNeonCityPropertyRecord Record;Properties->GetProperty(Data->PropertyID,Record);
 TestFalse(TEXT("Unowned actor cannot retain a saved garage assignment"),Record.GarageSlots[0].VehicleID.IsValid());

 Reset(FVector::ZeroVector);
 TestTrue(TEXT("Fully settled car still meets strict registration"),Properties->StoreVehicle(Data->PropertyID,0,Vehicle));
 TestTrue(TEXT("Registration and retention preserve original ownership"),Vehicles->IsOwned(Vehicle->VehicleID));
 const FGuid OriginalID=Vehicle->VehicleID;
 Vehicles->UnregisterLiveVehicle(Vehicle);Vehicle->Destroy();
 Properties->ApplyPersistenceSnapshot({Saved});Properties->ReconcileGarage(Data->PropertyID);
 Properties->GetProperty(Data->PropertyID,Record);
 TestTrue(TEXT("Missing owned actors retain their existing value link"),Record.GarageSlots[0].VehicleID==OriginalID);
 return !HasAnyErrors();
}
#endif
