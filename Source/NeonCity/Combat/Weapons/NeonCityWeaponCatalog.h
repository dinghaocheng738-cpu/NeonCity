#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/Weapons/WeaponData.h"
#include "NeonCityWeaponCatalog.generated.h"
// Shared primitive preview geometry; each native type supplies an independent data profile.
UCLASS()
class NEONCITY_API ANeonCitySMG : public ANeonCityPistol { GENERATED_BODY() public: ANeonCitySMG(); };
UCLASS()
class NEONCITY_API ANeonCityRifle : public ANeonCityPistol { GENERATED_BODY() public: ANeonCityRifle(); };
UCLASS()
class NEONCITY_API ANeonCityShotgun : public ANeonCityPistol { GENERATED_BODY() public: ANeonCityShotgun(); };
namespace NeonCityWeapons
{
 NEONCITY_API TSubclassOf<AWeaponBase> Resolve(FName ID);
 NEONCITY_API const UWeaponData* Profile(FName ID);
 NEONCITY_API FName Slot(int32 Index);
 NEONCITY_API FName AssetID(FName ID);
}
