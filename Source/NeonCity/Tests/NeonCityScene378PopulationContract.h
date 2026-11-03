#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "AI/PedestrianSpawnPoint.h"

/** Independent expected authored contract for Current346 + Scene375.
 * This is a test-only snapshot of the approved 20 existing enabled identities.
 * Counts are not inferred from the world under test or from its runtime cap.
 * No normal population scheduling, movement or animation is changed here. */
namespace NeonCityScene378Population
{
inline constexpr int32 Retained = 81;
inline constexpr int32 Enabled = 20;
inline constexpr int32 Disabled = 61;
inline constexpr int32 Stroll = 8;
inline constexpr int32 Cafe = 10;
inline constexpr int32 Viewpoint = 1;
inline constexpr int32 Staff = 1;
inline constexpr int32 ConversationGroups = 5;
inline constexpr int32 ForegroundTags = 4;
inline constexpr int32 LiveBudget = 20;
inline constexpr const TCHAR* Contract = TEXT("Current346-Scene375-existing20-v1");

struct FExpectedMarker
{
 const TCHAR* ID;
 ECoastalAmbientActivity Activity;
 const TCHAR* Group;
};

inline const FExpectedMarker* Find(FName ID)
{
 // Source identities from Current346ActorStates, with only Scene376's declared
 // route/skin/dwell deltas. No markers are enabled or disabled by Scene375.
 static const FExpectedMarker Expected[] = {
  {TEXT("SouthBeach.Pedestrian.01"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.02"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.03"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.04"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.05"),ECoastalAmbientActivity::CafeConversation,TEXT("C921.Cafe.Umbrella01")},
  {TEXT("SouthBeach.Pedestrian.06"),ECoastalAmbientActivity::CafeConversation,TEXT("C921.Cafe.Umbrella01")},
  {TEXT("SouthBeach.Pedestrian.07"),ECoastalAmbientActivity::Viewpoint,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.08"),ECoastalAmbientActivity::HarborStaff,TEXT("None")},
  {TEXT("SouthBeach.Pedestrian.09"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe.Foreground18.West")},
  {TEXT("SouthBeach.Pedestrian.10"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe.Foreground18.West")},
  {TEXT("SouthBeach.Pedestrian.11"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe.Foreground18.East")},
  {TEXT("SouthBeach.Pedestrian.12"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe.Foreground18.East")},
  {TEXT("Mainland916.Riviera.00"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe52.Table0")},
  {TEXT("Mainland916.Riviera.01"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe52.Table0")},
  {TEXT("Mainland916.Riviera.02"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe52.Table1")},
  {TEXT("Mainland916.Riviera.03"),ECoastalAmbientActivity::CafeConversation,TEXT("C923.Cafe52.Table1")},
  {TEXT("Mainland916.Riviera.08"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("Mainland916.Riviera.09"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("Mainland916.Riviera.10"),ECoastalAmbientActivity::Stroll,TEXT("None")},
  {TEXT("Mainland916.Riviera.11"),ECoastalAmbientActivity::Stroll,TEXT("None")}
 };
 static_assert(UE_ARRAY_COUNT(Expected)==Enabled);
 for(const auto& Row:Expected)if(ID==FName(Row.ID))return &Row;
 return nullptr;
}
}
#endif
