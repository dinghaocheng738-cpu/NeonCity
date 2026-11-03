#pragma once
#include "Core/Persistence/NeonCitySaveTypes.h"

namespace NeonCityVehiclePersistence
{
 // Bounded version2 placement migration. Operates on values only, before any
 // world mutation; it cannot change a save file or a player's moved vehicle.
 bool MigrateDefaultIslandParking(FNeonCityVehicleSaveData& Data);
}
