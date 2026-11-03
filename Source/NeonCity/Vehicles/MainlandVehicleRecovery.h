#pragma once

#include "Core/Persistence/NeonCitySaveTypes.h"

class UWorld;
class ANeonCityBaseVehicle;
class ANeonCityMainlandSite;

namespace NeonCityVehiclePersistence
{
 enum class EMainlandVehicleRecovery : uint8 { Unchanged, AuthoredParking, OwnedStorage };

 /** Post-validation recovery within the explicitly retired mainland only.
  * Supported, unobstructed choices are preserved. A matching, fully supported
  * and clear authored default is supplied only for a live actor. */
 EMainlandVehicleRecovery RecoverUnsupportedMainlandVehicle(UWorld* World,
  FNeonCityVehicleSaveData& Data,const FNeonCityVehicleSaveData* AuthoredDefault);

 /** Shared migration-only collision query. Ignores pawns, nonblocking query
  * fixtures and simulated props; does not alter any gameplay collision. */
 bool HasStaticVehicleObstruction(UWorld& World,const FTransform& Transform,
  FVector HalfExtents,FVector LocalOffset=FVector::ZeroVector);

 /** Caller validates the saved record first. An authored exact-ID/profile row
  * changes only the profile/type, preserving every other saved value. */
 bool MigrateAuthoredMainlandVehicleProfile(const ANeonCityMainlandSite* Site,FNeonCityVehicleSaveData& Data);

 /** Released C920 default-bay correction only. Exact audited identities and
  * old XY/yaw must match, with a live car's sealed current authored default.
  * Preserves every non-pose value and player-chosen parking. A candidate whose
  * destination/link fails validation is unchanged with a diagnostic reason. */
 bool MigrateDefaultMainlandParking(UWorld* World,FNeonCityVehicleSaveData& Data,
  const FNeonCityVehicleSaveData* AuthoredDefault,FString* Diagnostic=nullptr,
  TConstArrayView<FNeonCityVehicleSaveData> SavedParkingChoices={});

 /** Reconcile the two relocated mainland garages before vehicle normalization.
  * Only a previously valid, still-owned slot binding may move to its authored
  * replacement bay. Returns the number of poses changed; never writes a save. */
 int32 MigrateAuthoredMainlandGarageParking(UWorld* World,
  TConstArrayView<FNeonCityPropertySaveData> OldProperties,
  TConstArrayView<FNeonCityPropertySaveData> NormalizedProperties,
  TArray<FNeonCityVehicleSaveData>& Vehicles);

 /** Shared by normal save restore and owned-storage retrieval. */
 TSubclassOf<ANeonCityBaseVehicle> RestoreVehicleClass(uint8 VehicleType,FName ProfileName);
}
