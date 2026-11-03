// Adapted from Tidewater Bites.js / FishTable.js, commit 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT; see Docs/ThirdParty/Tidewater-LICENSE.txt.
#pragma once
#include "CoreMinimal.h"

enum class ENeonCityFishActivity : uint8 { Any, Day, DawnDusk, Night };

/** Overlapping habitat weights; distances and depth are metres, time is game hours. */
struct NEONCITY_API FNeonCityFishingHabitat
{
 double Shallows=0, Reef=0, Pier=0, Bay=0, Deep=0;
 double Richness() const { return Shallows+Reef+Pier+Bay+Deep; }
 double Dot(const FNeonCityFishingHabitat& Other) const;
};

/** Pure reference mathematics. No Actors, inventory, currency, save or renderer dependencies. */
namespace NeonCityFishingEcology
{
 NEONCITY_API FNeonCityFishingHabitat HabitatAt(double DepthM,double ReefEdgeDistanceM,double PierDistanceM);
 NEONCITY_API double Activity(ENeonCityFishActivity Preference,double Hour);
 NEONCITY_API double BiteDelay(const FNeonCityFishingHabitat& Habitat,double Hour,double Random01);
 NEONCITY_API double RollWeight(double MinimumKg,double MaximumKg,double Random01);
 NEONCITY_API double LengthCm(double WeightKg,double CoefficientA,double ExponentB);
 NEONCITY_API int64 SaleValue(double WeightKg,double MinimumKg,double MaximumKg,double PricePerKg);
}
