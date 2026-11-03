#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Vehicles/IslandParkingMigration.h"
#include "Vehicles/VehicleData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParkingMigration808,"NeonCity.Persistence.IslandParkingDefaults808",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FParkingMigration808::RunTest(const FString&)
{
 FNeonCityVehicleSaveData Old;FGuid::ParseExact(TEXT("5FF7803645833C8D593E77A50B006245"),EGuidFormats::Digits,Old.VehicleID);Old.VehicleType=uint8(ENeonCityVehicleType::SportsCar);Old.DataAssetID=TEXT("DA_HeroKestrel");Old.Condition=.72f;Old.Owned=true;Old.Transform=FTransform(FRotator(-.4874,14.04705,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(59249.2771,-153126.5078,727.9137)));
 auto Moved=Old;Moved.Transform.AddToTranslation(FVector(300,0,0));const FTransform MovedPose=Moved.Transform;
 TestFalse(TEXT("A player-moved car keeps its saved pose"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Moved));TestTrue(TEXT("Moved transform exact"),Moved.Transform.Equals(MovedPose));
 auto Stored=Old;Stored.StoredPropertyID=TEXT("Estate.Garage");Stored.GarageSlot=1;const FTransform StoredPose=Stored.Transform;TestFalse(TEXT("Garage car is preserved"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Stored));TestTrue(TEXT("Stored pose exact"),Stored.Transform.Equals(StoredPose));
 auto Turned=Old;Turned.Transform.SetRotation(FRotator(0,45,0).Quaternion());TestFalse(TEXT("Player changed orientation is preserved"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Turned));
 auto Other=Old;Other.VehicleID=FGuid::NewGuid();TestFalse(TEXT("Unrelated car at same position is preserved"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Other));
 TestTrue(TEXT("Untouched old default adopts supported new parking orientation"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Old));TestTrue(TEXT("Faces the authored exit"),FMath::Abs(FMath::FindDeltaAngleDegrees(Old.Transform.Rotator().Yaw,105.07603f))<.01f);TestTrue(TEXT("Health and ownership survive"),Old.Owned && Old.Condition==.72f);TestEqual(TEXT("Migration version retained"),int32(Old.IslandFleetMigrationVersion),2);
 const FTransform Once=Old.Transform;TestFalse(TEXT("Repeated normalization does not migrate again"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Old));TestTrue(TEXT("Repeated pose exact"),Old.Transform.Equals(Once));
 // All three exact identities are exercised, including the retained roof state.
 FNeonCityVehicleSaveData Aster;FGuid::ParseExact(TEXT("7F242340469FEA4D1A7D21A5D5DED892"),EGuidFormats::Digits,Aster.VehicleID);Aster.DataAssetID=TEXT("DA_HeroAster");Aster.Transform=FTransform(FRotator(1.66,23.1138,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(57072.7507,-153875.1075,714.1264)));TestTrue(TEXT("Aster default migrates"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Aster));TestTrue(TEXT("Aster faces its measured exit"),FMath::Abs(FMath::FindDeltaAngleDegrees(Aster.Transform.Rotator().Yaw,180.f))<.01f);
 FNeonCityVehicleSaveData Spider;FGuid::ParseExact(TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8"),EGuidFormats::Digits,Spider.VehicleID);Spider.DataAssetID=TEXT("DA_HeroNacre");Spider.Transform=FTransform(FRotator(3.3,10.65951,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(60384.6401,-152870.3611,747.2213)));Spider.RoofProgress=.6f;Spider.RoofTargetOpen=true;TestTrue(TEXT("Spider default migrates"),NeonCityVehiclePersistence::MigrateDefaultIslandParking(Spider));TestTrue(TEXT("Roof state preserved"),Spider.RoofProgress==.6f && Spider.RoofTargetOpen);return true;
}
#endif
