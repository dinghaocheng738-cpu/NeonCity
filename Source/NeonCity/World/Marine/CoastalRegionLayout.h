#pragma once
#include "CoreMinimal.h"

/** World placement of the Tidewater reference island: its streaming-level transform and the
 * ANeonCityCoastalRegion actor in NeonCity_SouthBeach_Test sit here. The ocean field and the
 * coastline bathymetry are authored in island-local metres around this origin.
 * Moved 800 m south of its first site so the island clears the West Lake landscape. */
namespace NeonCityCoastalLayout
{
 inline const FVector IslandOrigin(50000,-250000,-130);

 /** Where the island stood when its content, older saves and older tests were authored. */
 inline const FVector LegacyIslandOrigin(50000,-170000,-130);
 /** The island landscape's footprint at that site (world cm): everything authored inside it
  * belonged to the island or its surrounding sea and moves with it. */
 inline const FBox2D LegacyIslandFootprint(FVector2D(-52400,-272400),FVector2D(152400,-67600));

 /** A world position authored against the legacy island site, at the island's current site. */
 inline FVector FromLegacyIsland(const FVector& P){return P+(IslandOrigin-LegacyIslandOrigin);}
 inline FVector2D FromLegacyIsland(const FVector2D& P){return P+FVector2D(IslandOrigin-LegacyIslandOrigin);}
 inline FBox FromLegacyIsland(const FBox& B){return FBox(FromLegacyIsland(B.Min),FromLegacyIsland(B.Max));}

 /** Save migration: a point inside the legacy footprint moves with the island. */
 inline bool RelocateLegacyIslandPoint(FVector& P)
 {
  if(!LegacyIslandFootprint.IsInside(FVector2D(P)))return false;
  P=FromLegacyIsland(P);return true;
 }
 inline bool RelocateLegacyIslandTransform(FTransform& T)
 {
  FVector P=T.GetLocation();
  if(!RelocateLegacyIslandPoint(P))return false;
  T.SetLocation(P);return true;
 }
}
