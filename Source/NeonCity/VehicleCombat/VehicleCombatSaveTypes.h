#pragma once
#include "CoreMinimal.h"
#include "VehicleCombatSaveTypes.generated.h"
/** Stable vehicle ID + authored hardpoint ID; never restore arbitrary classes or mount transforms from disk. */
USTRUCT()
struct FVehicleWeaponSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName VehicleID;
 UPROPERTY(SaveGame) FName HardpointID;
 UPROPERTY(SaveGame) FName WeaponID;
 UPROPERTY(SaveGame) int32 Ammo=0;
 UPROPERTY(SaveGame) int32 Reserve=0;
 UPROPERTY(SaveGame) bool Selected=false;
};
