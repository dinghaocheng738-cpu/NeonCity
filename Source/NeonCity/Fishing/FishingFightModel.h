// Adapted from Tidewater CatchMinigame.js, commit 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT; see Docs/ThirdParty/Tidewater-LICENSE.txt.
#pragma once
#include "CoreMinimal.h"

enum class ENeonCityFishFightResult : uint8 { Fighting, Caught, Snapped, Escaped, Invalid };

struct NEONCITY_API FNeonCityFishFightConfig
{
 double WeightKg=.8, SpeciesMaximumKg=1.2, SpeciesFight=.25, SpeciesStaminaSeconds=4;
 double LineRatingKg=7, ReelSpeedMps=1.1, DistanceM=18;
 bool IsValid() const;
};

/** Faithful line-tension fight in metres/seconds/kg. Caller supplies fixed bounded steps.
 * Uses a portable seeded stream for reproducible replays/reference comparisons, no global RNG.
 * Tension may exceed one: it must overload for >.45 seconds before breaking. */
struct NEONCITY_API FNeonCityFishFight
{
 ENeonCityFishFightResult Result=ENeonCityFishFightResult::Invalid;
 double Tension=.3,DistanceM=0,Stamina=1,Surge=0,Elapsed=0;
 double SlackSeconds=0,OverloadSeconds=0;
 static constexpr double BandLow=.3;
 static constexpr double BandHigh=.85;
 bool Reset(const FNeonCityFishFightConfig& Config,uint32 Seed);
 ENeonCityFishFightResult Step(double Seconds,bool Reeling);
private:
 double Random01();
 uint32 RandomState=1;
 double Power=0,StaminaMax=1,ReelSpeedMps=1,MaximumDistanceM=60,NextSurge=0,SurgeRemaining=0;
};
