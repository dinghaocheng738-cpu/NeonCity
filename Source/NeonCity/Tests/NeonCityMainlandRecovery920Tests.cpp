#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Vehicles/VehicleData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace MainlandRecovery920
{
FNeonCityVehicleSaveData SavedSpider()
{
 FNeonCityVehicleSaveData Data;
 Data.VehicleID=FGuid(0x92000001,0x12345678,0x76543210,0x92000002);
 Data.VehicleType=uint8(ENeonCityVehicleType::SportsCar);Data.DataAssetID=TEXT("DA_HeroNacre");
 Data.Owned=true;Data.Condition=.46f;Data.RoofProgress=.62f;Data.RoofTargetOpen=true;
 Data.Transform=FTransform(FRotator(0,31,0),FVector(10000,10000,7500));
 Data.IslandFleetMigrationVersion=2;
 return Data;
}
bool SameCollectionValues(const FNeonCityVehicleSaveData& A,const FNeonCityVehicleSaveData& B)
{
 return A.VehicleID==B.VehicleID && A.VehicleType==B.VehicleType && A.DataAssetID==B.DataAssetID
  && A.Owned==B.Owned && A.Condition==B.Condition && A.RoofProgress==B.RoofProgress
  && A.RoofTargetOpen==B.RoofTargetOpen && A.StoredPropertyID==B.StoredPropertyID
  && A.GarageSlot==B.GarageSlot && A.IslandFleetMigrationVersion==B.IslandFleetMigrationVersion;
}
AActor* Support(UWorld& World,UStaticMesh* Cube,FVector Location,FVector Size,bool Pawn=false)
{
 AActor* Actor=Pawn?static_cast<AActor*>(World.SpawnActor<APawn>()):World.SpawnActor<AActor>();
 if(!Actor)return nullptr;
 auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
 Actor->SetRootComponent(Mesh);Actor->AddInstanceComponent(Mesh);
 Mesh->SetStaticMesh(Cube);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
 Mesh->SetCollisionObjectType(ECC_WorldStatic);Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Mesh->SetWorldLocation(Location);Mesh->SetWorldScale3D(Size/100);Mesh->RegisterComponent();
 return Actor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandRecoveryBounds920,
 "NeonCity.Phase205c.MainlandC920.VehicleRecoveryBounds",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandRecoveryBounds920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Real collision mesh for support probes"),Cube))return false;
 const auto Original=MainlandRecovery920::SavedSpider();
 auto Data=Original;
 TestEqual(TEXT("Worlds without a mainland marker do not migrate saved vehicles"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Authored opt-in marker"),Site))return false;
 TestEqual(TEXT("New markers leave save recovery disabled until scene adoption opts in"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Site->bRecoverUnsupportedMainlandVehicles=true;

 for(const FVector Position:{NeonCityCoastalLayout::FromLegacyIsland(FVector(60000,-153000,750)),FVector(5000,-4100,-130),FVector(0,160001,100),FVector(0,0,40001)})
 {
  Data=Original;Data.Transform.SetLocation(Position);const auto Before=Data;
  TestEqual(TEXT("Island, offshore and locations outside the audited retired city remain exact"),
   RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
  TestTrue(TEXT("Excluded saved pose and collection values are untouched"),
   Data.Transform.Equals(Before.Transform) && !Data.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Before));
 }
 Site->RetiredMainlandMin=FVector(-1000000);Site->RetiredMainlandMax=FVector(1000000);
 Data=Original;Data.Transform.SetLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(60000,-153000,750)));
 TestEqual(TEXT("An accidentally oversized authoring bound cannot capture island saves"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Site->RetiredMainlandMin=FVector(-160000,-3000,-20000);Site->RetiredMainlandMax=FVector(160000,160000,40000);
 Site->RetiredMainlandMin.X=11000;Data=Original;
 TestEqual(TEXT("Authored bounds may deliberately narrow the retired region"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Site->RetiredMainlandMin.X=-160000;
 Data=Original;Data.Owned=false;
 TestEqual(TEXT("Recovery never adopts or stores an unowned car"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Data=Original;Data.StoredPropertyID=TEXT("Riviera.Estate.Garage");
 TestEqual(TEXT("Property storage remains under the property domain"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Data=Original;Data.GarageSlot=0;
 TestEqual(TEXT("Even an incomplete slot binding is not rewritten as mainland storage"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);

 // This is a real world trace: a pawn's own WorldStatic fixture must not be
 // mistaken for authored support and prevent recovery from the deleted city.
 auto* Pawn=MainlandRecovery920::Support(*World,Cube,FVector(10000,10000,7400),FVector(1000,1000,40),true);
 if(!TestNotNull(TEXT("Pawn collision decoy"),Pawn))return false;
 Data=Original;
 TestEqual(TEXT("Unsupported missing owned car enters the existing owned collection"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::OwnedStorage);
 TestTrue(TEXT("Storage preserves the exact original pose, GUID, profile, damage and roof"),
  Data.LegacyFleetStored && Data.Transform.Equals(Original.Transform) && MainlandRecovery920::SameCollectionValues(Data,Original));
 TestEqual(TEXT("Repeated normalization leaves a stored collection record unchanged"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 Pawn->Destroy();

 auto* Floor=MainlandRecovery920::Support(*World,Cube,FVector(10000,10000,7400),FVector(1000,1000,40));
 if(!TestNotNull(TEXT("Authored static parking support"),Floor))return false;
 Data=Original;
 TestEqual(TEXT("Missing owned cars on a supported player-chosen surface stay in the world"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 TestTrue(TEXT("Supported saved pose is kept exactly"),Data.Transform.Equals(Original.Transform) && !Data.LegacyFleetStored);
 Floor->Destroy();
 Data=Original;Data.Transform.SetRotation(FQuat::Identity);
 auto* Corner=MainlandRecovery920::Support(*World,Cube,FVector(10180,10080,7400),FVector(40,40,40));
 if(!TestNotNull(TEXT("One supported footprint corner"),Corner))return false;
 const FTransform CornerPose=Data.Transform;
 TestEqual(TEXT("One real corner support conservatively preserves the saved choice"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::Unchanged);
 TestTrue(TEXT("Corner support does not move or store the car"),Data.Transform.Equals(CornerPose) && !Data.LegacyFleetStored);
 Corner->Destroy();Site->Destroy();
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandRecoveryParking920,
 "NeonCity.Phase205c.MainlandC920.VehicleRecoveryAuthoredParking",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandRecoveryParking920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Collision fixture"),Cube) || !TestNotNull(TEXT("Authored mainland marker"),Site))return false;
 Site->bRecoverUnsupportedMainlandVehicles=true;
 const auto Original=MainlandRecovery920::SavedSpider();
 auto Default=Original;Default.Transform=FTransform(FRotator(0,-90,0),FVector(1200,800,130));
 auto* Bay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,40),FVector(1000,1000,40));
 if(!TestNotNull(TEXT("Supported new authored bay"),Bay))return false;
 auto Data=Original;
 TestEqual(TEXT("A live car's unsupported saved pose returns to its matching supported authored bay"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::AuthoredParking);
 TestTrue(TEXT("Only the pose changes; ownership, condition, roof and identity remain exact"),
  Data.Transform.Equals(Default.Transform) && !Data.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Original));
 const auto Once=Data;
 TestEqual(TEXT("The now-supported recovered parking is idempotent"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::Unchanged);
 TestTrue(TEXT("Repeated normalization leaves the authored recovery pose exact"),Data.Transform.Equals(Once.Transform));

 auto* ChosenFloor=MainlandRecovery920::Support(*World,Cube,FVector(10000,10000,7400),FVector(1000,1000,40));
 if(!TestNotNull(TEXT("Player-selected supported parking"),ChosenFloor))return false;
 Data=Original;
 TestEqual(TEXT("A supported player choice takes priority over a different authored default"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::Unchanged);
 TestTrue(TEXT("Default parking cannot overwrite supported saved parking"),Data.Transform.Equals(Original.Transform));
 auto* Wall=MainlandRecovery920::Support(*World,Cube,FVector(10000,10000,7520),FVector(40,500,200));
 if(!TestNotNull(TEXT("New wall through the supported legacy chassis"),Wall))return false;
 Data=Original;
 TestEqual(TEXT("Ground support cannot preserve a saved car embedded in a new static wall"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::AuthoredParking);
 TestTrue(TEXT("Obstructed recovery changes only the pose"),Data.Transform.Equals(Default.Transform)
  && !Data.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Original));
 Data=Original;
 TestEqual(TEXT("A supported but obstructed missing car stays usable in owned storage"),
  RecoverUnsupportedMainlandVehicle(World,Data,nullptr),EMainlandVehicleRecovery::OwnedStorage);
 TestTrue(TEXT("Supported obstruction never discards the owned car's exact state"),Data.Transform.Equals(Original.Transform)
  && Data.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Original));
 Wall->Destroy();
 ChosenFloor->Destroy();

 auto* BlockedBay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,140),FVector(40,500,200));
 if(!TestNotNull(TEXT("Static obstruction in a supported authored recovery bay"),BlockedBay))return false;
 Data=Original;
 TestEqual(TEXT("Recovery cannot place an owned car in a supported but obstructed destination"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::OwnedStorage);
 BlockedBay->Destroy();

 Default.VehicleID=FGuid::NewGuid();Data=Original;
 TestEqual(TEXT("Another vehicle's default cannot be used as a recovery bay"),
  RecoverUnsupportedMainlandVehicle(World,Data,&Default),EMainlandVehicleRecovery::OwnedStorage);
 TestTrue(TEXT("A mismatched default still preserves the complete owned collectible"),
  Data.Transform.Equals(Original.Transform) && MainlandRecovery920::SameCollectionValues(Data,Original));
 Bay->Destroy();Site->Destroy();
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandStoredVehicleClasses920,
 "NeonCity.Phase205c.MainlandC920.StoredVehicleRestoreClasses",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandStoredVehicleClasses920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 TestEqual(TEXT("Stored starter sedan retrieves through the normal sedan implementation"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::Sedan),TEXT("DA_StarterSedan")).Get(),ANeonCityStarterSedan::StaticClass());
 TestEqual(TEXT("Stored prototype sports car keeps its original implementation"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::SportsCar),TEXT("DA_SportsCar")).Get(),ANeonCityPrototypeVehicle::StaticClass());
 TestEqual(TEXT("Stored prototype SUV keeps its original implementation"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::SUV),TEXT("DA_SUV")).Get(),ANeonCityPrototypeVehicle::StaticClass());
 TestEqual(TEXT("Stored prototype pickup keeps its original implementation"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::Pickup),TEXT("DA_Pickup")).Get(),ANeonCityPrototypeVehicle::StaticClass());
 for(const FName Profile:{FName(TEXT("DA_HeroAster")),FName(TEXT("DA_HeroKestrel")),FName(TEXT("DA_HeroNacre")),FName(TEXT("DA_HeroCarrera914"))})
  TestEqual(TEXT("Every known hero collectible retains the hero implementation"),
   RestoreVehicleClass(uint8(ENeonCityVehicleType::SportsCar),Profile).Get(),AIslandSupercar::StaticClass());
 TestEqual(TEXT("Stored expedition SUV keeps offroad mechanics"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::SUV),TEXT("DA_ExpeditionSUV")).Get(),AIslandOffroadVehicle::StaticClass());
 TestEqual(TEXT("Stored expedition pickup keeps offroad mechanics"),
  RestoreVehicleClass(uint8(ENeonCityVehicleType::Pickup),TEXT("DA_ExpeditionPickup")).Get(),AIslandOffroadVehicle::StaticClass());
 TestNull(TEXT("Unknown collectible profiles cannot silently become a hero car"),RestoreVehicleClass(0,TEXT("UnknownProfile")).Get());
 TestNull(TEXT("Profile/type mismatch cannot select a default class"),RestoreVehicleClass(0,TEXT("DA_HeroNacre")).Get());
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandFleetProfilePolicy920,
 "NeonCity.Phase205c.MainlandC920.FleetProfileReplacementPolicy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandFleetProfilePolicy920::RunTest(const FString&)
{
 const FGuid MainlandID(0x92000005,0x01234567,0x76543210,0x92000006);
 TArray<FNeonCityMainlandFleetReplacement> Rows;
 FName Replacement=TEXT("StaleValue");
 TestFalse(TEXT("An empty authoring table never upgrades a car"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 TestTrue(TEXT("Failed lookup clears its output"),Replacement.IsNone());
 FNeonCityMainlandFleetReplacement Row;Row.VehicleID=MainlandID;
 Row.PreviousProfile=TEXT("DA_StarterSedan");Row.ReplacementProfile=TEXT("DA_HeroNacre");Rows.Add(Row);
 TestTrue(TEXT("One exact ID/old-profile row resolves its approved replacement"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 TestEqual(TEXT("The requested replacement is returned exactly"),Replacement,FName(TEXT("DA_HeroNacre")));
 TestFalse(TEXT("An unlisted vehicle of the same old model is protected"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,FGuid(1,2,3,4),TEXT("DA_StarterSedan"),Replacement));
 TestFalse(TEXT("A different previous model at the same ID is protected"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_SUV"),Replacement));
 TestFalse(TEXT("Already-replaced profiles are idempotent"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_HeroNacre"),Replacement));
 Rows.Add(Row);
 TestFalse(TEXT("Even identical duplicate IDs cannot request a migration"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 Rows[1].ReplacementProfile=TEXT("DA_HeroAster");
 TestFalse(TEXT("Conflicting duplicate IDs cannot request a migration"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 Rows.SetNum(1);Rows[0].ReplacementProfile=TEXT("DA_HeroCarrera914");
 TestFalse(TEXT("Unselected replacement profiles are refused"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 Rows[0].ReplacementProfile=TEXT("UnknownHero");
 TestFalse(TEXT("Unknown replacement profiles are refused"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
 Rows[0]=Row;Rows[0].PreviousProfile=TEXT("DA_ExpeditionSUV");
 TestFalse(TEXT("An unrelated expedition profile cannot become a mainland prototype migration"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_ExpeditionSUV"),Replacement));
 Rows[0]=Row;Rows[0].VehicleID=FGuid();
 TestFalse(TEXT("An empty authored identity is refused"),
  ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,FGuid(),TEXT("DA_StarterSedan"),Replacement));
 for(const TCHAR* ProtectedID:{TEXT("5FF7803645833C8D593E77A50B006245"),TEXT("7F242340469FEA4D1A7D21A5D5DED892"),TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8")})
 {
  Rows[0]=Row;FGuid ID;FGuid::ParseExact(ProtectedID,EGuidFormats::Digits,ID);Rows[0].VehicleID=ID;
  TestFalse(TEXT("Canonical island car identity is rejected even if copied into the mainland table"),
   ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,ID,TEXT("DA_StarterSedan"),Replacement));
 }
 for(const FName Previous:{FName(TEXT("DA_StarterSedan")),FName(TEXT("DA_SportsCar")),FName(TEXT("DA_SUV")),FName(TEXT("DA_Pickup")),FName(TEXT("DA_HeroCarrera914"))})
 {
  Rows[0]=Row;Rows[0].PreviousProfile=Previous;
  TestTrue(TEXT("Each supported retired mainland profile can be explicitly mapped"),
   ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,Previous,Replacement));
 }
 for(const FName Hero:{FName(TEXT("DA_HeroAster")),FName(TEXT("DA_HeroKestrel")),FName(TEXT("DA_HeroNacre"))})
 {
  Rows[0]=Row;Rows[0].ReplacementProfile=Hero;
  TestTrue(TEXT("Each of the three approved presentations is accepted"),
   ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rows,MainlandID,TEXT("DA_StarterSedan"),Replacement));
  TestEqual(TEXT("Approved target remains exact"),Replacement,Hero);
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandFleetProfileValues920,
 "NeonCity.Phase205c.MainlandC920.FleetProfileReplacementValues",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandFleetProfileValues920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 auto Original=MainlandRecovery920::SavedSpider();Original.VehicleType=uint8(ENeonCityVehicleType::Sedan);
 Original.DataAssetID=TEXT("DA_StarterSedan");Original.StoredPropertyID=TEXT("Riviera.Estate.Garage");Original.GarageSlot=1;
 auto Data=Original;
 TestFalse(TEXT("No authored marker means no profile migration"),MigrateAuthoredMainlandVehicleProfile(nullptr,Data));
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Authored mainland marker"),Site))return false;
 TestFalse(TEXT("A marker with no authored replacement rows changes nothing"),MigrateAuthoredMainlandVehicleProfile(Site,Data));
 TestTrue(TEXT("No opt-in preserves every saved value"),Data.Transform.Equals(Original.Transform)
  && Data.LegacyFleetStored==Original.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Original));
 FNeonCityMainlandFleetReplacement Row;Row.VehicleID=Original.VehicleID;
 Row.PreviousProfile=Original.DataAssetID;Row.ReplacementProfile=TEXT("DA_HeroNacre");Site->FleetProfileReplacements.Add(Row);
 TestTrue(TEXT("An explicit row upgrades the original saved vehicle"),MigrateAuthoredMainlandVehicleProfile(Site,Data));
 auto Expected=Original;Expected.VehicleType=uint8(ENeonCityVehicleType::SportsCar);Expected.DataAssetID=TEXT("DA_HeroNacre");
 TestTrue(TEXT("Only profile/type change: GUID, pose, ownership, condition, roof, slots and versions remain exact"),
  Data.Transform.Equals(Expected.Transform) && Data.LegacyFleetStored==Expected.LegacyFleetStored
  && MainlandRecovery920::SameCollectionValues(Data,Expected));
 TestFalse(TEXT("A second normalization does not migrate the replacement again"),MigrateAuthoredMainlandVehicleProfile(Site,Data));
 TestTrue(TEXT("Repeated normalization preserves every migrated saved value"),Data.Transform.Equals(Expected.Transform)
  && Data.LegacyFleetStored==Expected.LegacyFleetStored && MainlandRecovery920::SameCollectionValues(Data,Expected));
 Site->Destroy();
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandGarageParkingMigration920,
 "NeonCity.Phase205c.MainlandC920.GarageParkingMigration",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandGarageParkingMigration920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!TestNotNull(TEXT("Real collision mesh for the replacement garage"),Cube))return false;
 const FName Starter=TEXT("NeonCity.Property.StarterApartment");
 auto Original=MainlandRecovery920::SavedSpider();Original.StoredPropertyID=Starter;Original.GarageSlot=1;
 FNeonCityPropertySaveData OldProperty;OldProperty.PropertyID=Starter;OldProperty.Owned=true;
 FNeonCityGarageSaveData OldSlot;OldSlot.SlotIndex=1;OldSlot.VehicleID=Original.VehicleID;
 OldSlot.StoredLocation=Original.Transform.GetLocation();OldProperty.GarageSlots.Add(OldSlot);
 // Existing parking accepts this offset in XY and chassis height. Moving a
 // valid garage binding must not require an exact obsolete slot-centre pose.
 Original.Transform.AddToTranslation(FVector(200,0,100));
 auto NewProperty=OldProperty;NewProperty.GarageSlots[0].StoredLocation=FVector(1200,800,130);
 const TArray<FNeonCityPropertySaveData> BeforeProperties={OldProperty};
 const TArray<FNeonCityPropertySaveData> AfterProperties={NewProperty};
 TArray<FNeonCityVehicleSaveData> Vehicles={Original};
 TestEqual(TEXT("Ordinary worlds do not migrate garage positions"),
  MigrateAuthoredMainlandGarageParking(World,BeforeProperties,AfterProperties,Vehicles),0);
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Opt-in mainland marker"),Site))return false;
 TestEqual(TEXT("Scene adoption must explicitly opt into garage recovery"),
  MigrateAuthoredMainlandGarageParking(World,BeforeProperties,AfterProperties,Vehicles),0);
 Site->bRecoverUnsupportedMainlandVehicles=true;
 TestEqual(TEXT("An authored destination without static support cannot move an owned car"),
  MigrateAuthoredMainlandGarageParking(World,BeforeProperties,AfterProperties,Vehicles),0);
 auto* Bay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,40),FVector(1000,1000,40));
 if(!TestNotNull(TEXT("Supported replacement garage floor"),Bay))return false;
 TestEqual(TEXT("A valid owned old-slot binding migrates once to its supported new bay"),
  MigrateAuthoredMainlandGarageParking(World,BeforeProperties,AfterProperties,Vehicles),1);
 const FTransform Expected(FRotator(0,90,0),NewProperty.GarageSlots[0].StoredLocation);
 TestTrue(TEXT("Only transform changes; identity, profile, condition, ownership, roof and garage links remain exact"),
  Vehicles[0].Transform.Equals(Expected) && Vehicles[0].LegacyFleetStored==Original.LegacyFleetStored
  && MainlandRecovery920::SameCollectionValues(Vehicles[0],Original));
 TestTrue(TEXT("The caller's old property snapshot remains immutable"),
  BeforeProperties[0].GarageSlots[0].StoredLocation.Equals(OldSlot.StoredLocation));
 TestEqual(TEXT("Reusing the old snapshot cannot relocate an already recovered car again"),
  MigrateAuthoredMainlandGarageParking(World,BeforeProperties,AfterProperties,Vehicles),0);
 TestEqual(TEXT("Subsequent saves with the new slot position are idempotent"),
  MigrateAuthoredMainlandGarageParking(World,AfterProperties,AfterProperties,Vehicles),0);

 auto EstateOld=BeforeProperties;auto EstateNew=AfterProperties;
 EstateOld[0].PropertyID=EstateNew[0].PropertyID=TEXT("Property.Mainland.RivieraEstate916");
 Vehicles={Original};Vehicles[0].StoredPropertyID=EstateOld[0].PropertyID;
 TestEqual(TEXT("The exact relocated mainland estate also retains its owned garage car"),
  MigrateAuthoredMainlandGarageParking(World,EstateOld,EstateNew,Vehicles),1);
 Bay->Destroy();Site->Destroy();
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandGarageParkingGuards920,
 "NeonCity.Phase205c.MainlandC920.GarageParkingGuards",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandGarageParkingGuards920::RunTest(const FString&)
{
 using namespace NeonCityVehiclePersistence;
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto* Site=World->SpawnActor<ANeonCityMainlandSite>();
 if(!TestNotNull(TEXT("Collision mesh"),Cube) || !TestNotNull(TEXT("Mainland marker"),Site))return false;
 Site->bRecoverUnsupportedMainlandVehicles=true;
 auto Original=MainlandRecovery920::SavedSpider();
 Original.StoredPropertyID=TEXT("NeonCity.Property.StarterApartment");Original.GarageSlot=0;
 FNeonCityPropertySaveData OldProperty;OldProperty.PropertyID=Original.StoredPropertyID;OldProperty.Owned=true;
 FNeonCityGarageSaveData OldSlot;OldSlot.VehicleID=Original.VehicleID;OldSlot.StoredLocation=Original.Transform.GetLocation();
 OldProperty.GarageSlots.Add(OldSlot);
 auto NewProperty=OldProperty;NewProperty.GarageSlots[0].StoredLocation=FVector(1200,800,130);
 auto* Bay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,40),FVector(1000,1000,40));
 if(!TestNotNull(TEXT("Supported authored bay"),Bay))return false;
 const auto Reject=[&](const TCHAR* Label,TArray<FNeonCityPropertySaveData> Before,
  TArray<FNeonCityPropertySaveData> After,FNeonCityVehicleSaveData Saved)
 {
  TArray<FNeonCityVehicleSaveData> Vehicles={Saved};
  TestEqual(Label,MigrateAuthoredMainlandGarageParking(World,Before,After,Vehicles),0);
  TestTrue(TEXT("A rejected garage migration preserves every vehicle value"),
   Vehicles[0].Transform.Equals(Saved.Transform) && Vehicles[0].LegacyFleetStored==Saved.LegacyFleetStored
   && MainlandRecovery920::SameCollectionValues(Vehicles[0],Saved));
 };
 auto Before=OldProperty;auto After=NewProperty;auto Saved=Original;
 Before.PropertyID=After.PropertyID=Saved.StoredPropertyID=TEXT("Property.Island.Mansion");
 Reject(TEXT("Island and all unlisted property identities are excluded"),{Before},{After},Saved);
 Before=OldProperty;Before.Owned=false;
 Reject(TEXT("An unowned original property cannot move a vehicle"),{Before},{NewProperty},Original);
 After=NewProperty;After.Owned=false;
 Reject(TEXT("A property whose ownership was not retained cannot move a vehicle"),{OldProperty},{After},Original);
 Saved=Original;Saved.Owned=false;
 Reject(TEXT("An unowned vehicle cannot be adopted through a garage link"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.StoredPropertyID=TEXT("AnotherProperty");
 Reject(TEXT("The vehicle's original property link must match exactly"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.GarageSlot=1;
 Reject(TEXT("The vehicle's original garage index must match exactly"),{OldProperty},{NewProperty},Saved);
 After=NewProperty;After.GarageSlots[0].VehicleID=FGuid::NewGuid();
 Reject(TEXT("A replacement slot containing a different vehicle is excluded"),{OldProperty},{After},Original);
 Before=OldProperty;Before.GarageSlots[0].VehicleID.Invalidate();
 Reject(TEXT("An invalid original slot identity is excluded"),{Before},{NewProperty},Original);
 Saved=Original;Saved.Transform.AddToTranslation(FVector(251,0,0));
 Reject(TEXT("A player-chosen pose outside the original parking tolerance is preserved"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.Transform.AddToTranslation(FVector(0,0,150));
 Reject(TEXT("A car above or below the old parking surface is preserved"),{OldProperty},{NewProperty},Saved);
 Before=OldProperty;Before.GarageSlots[0].StoredLocation.Y=NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-153000,0)).Y;Saved=Original;Saved.Transform.SetLocation(Before.GarageSlots[0].StoredLocation);
 Reject(TEXT("Old island/offshore slot positions cannot enter the retired mainland policy"),{Before},{NewProperty},Saved);
 Site->RetiredMainlandMin=FVector(-1000000);Site->RetiredMainlandMax=FVector(1000000);
 Reject(TEXT("Oversized authored bounds still cannot capture an island slot"),{Before},{NewProperty},Saved);
 Site->RetiredMainlandMin=FVector(-160000,-3000,-20000);Site->RetiredMainlandMax=FVector(160000,160000,40000);
 Site->RetiredMainlandMin.X=11000;
 Reject(TEXT("Narrow authored bounds exclude deliberately retained mainland garages"),{OldProperty},{NewProperty},Original);
 Site->RetiredMainlandMin.X=-160000;
 After=OldProperty;After.GarageSlots[0].StoredLocation.X+=250;
 Reject(TEXT("Unchanged or nearby existing garage placements need no migration"),{OldProperty},{After},Original);
 Reject(TEXT("Duplicate original property rows fail closed"),{OldProperty,OldProperty},{NewProperty},Original);
 Before=OldProperty;Before.GarageSlots.Add(OldSlot);
 Reject(TEXT("Duplicate original slot indices fail closed"),{Before},{NewProperty},Original);
 After=NewProperty;
 const FNeonCityGarageSaveData DuplicateNewSlot=After.GarageSlots[0];
 After.GarageSlots.Add(DuplicateNewSlot);
 Reject(TEXT("Duplicate replacement slot indices fail closed"),{OldProperty},{After},Original);
 Saved=Original;Saved.Condition=-.2f;
 Reject(TEXT("Malformed condition stays under ordinary vehicle validation"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.RoofProgress=2;
 Reject(TEXT("Malformed roof state stays under ordinary vehicle validation"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.DataAssetID=TEXT("UnknownProfile");
 Reject(TEXT("Unknown profile/type records are not rescued before validation"),{OldProperty},{NewProperty},Saved);
 Saved=Original;Saved.LegacyFleetStored=true;
 Reject(TEXT("An already stored owned collectible stays in its existing collection"),{OldProperty},{NewProperty},Saved);
 TArray<FNeonCityVehicleSaveData> Duplicates={Original,Original};
 const TArray<FNeonCityPropertySaveData> BeforeDuplicateTest={OldProperty},AfterDuplicateTest={NewProperty};
 TestEqual(TEXT("Duplicate saved vehicle identities fail closed"),
  MigrateAuthoredMainlandGarageParking(World,BeforeDuplicateTest,AfterDuplicateTest,Duplicates),0);
 auto* BlockedBay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,140),FVector(40,500,200));
 if(!TestNotNull(TEXT("Static wall inside the new garage bay"),BlockedBay))return false;
 Reject(TEXT("A supported garage bay must also have real chassis clearance"),{OldProperty},{NewProperty},Original);
 BlockedBay->Destroy();Bay->Destroy();
 auto* PartialBay=MainlandRecovery920::Support(*World,Cube,FVector(1200,800,40),FVector(40,40,40));
 if(!TestNotNull(TEXT("Insufficient one-point destination support"),PartialBay))return false;
 Reject(TEXT("A garage destination must support the whole footprint, not a tiny centre island"),{OldProperty},{NewProperty},Original);
 PartialBay->Destroy();Site->Destroy();
 return !HasAnyErrors();
}
#endif
