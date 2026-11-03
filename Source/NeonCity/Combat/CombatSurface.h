#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/WeaponData.h"

namespace NeonCityCombat
{
 /** Resolve the actual hit body first, retaining legacy actor fallbacks.
  *  No dependency on player, foliage, or a particular weapon implementation. */
 NEONCITY_API ENeonCityImpactSurface ResolveImpactSurface(const FHitResult& Hit);
}
