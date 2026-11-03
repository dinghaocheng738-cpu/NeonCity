#pragma once
#include "CoreMinimal.h"

/** Read-only, bounded samples of authored road lanes. Order is travel direction. */
struct FNeonCityGuideRoad
{
 TArray<FVector> Points;
 bool Closed=false;
 // Only explicitly drivable authored roads participate in vehicle control.
 bool CruiseEligible=true;
 bool Bidirectional=false;
 // Endpoint-to-interior joins are allowed only from physically measured
 // linear connectors, rather than inferred at arbitrary nearby roads.
 bool NativeMeasured=false;
 float LaneOffsetCm=0;
 float SpeedLimitKmh=45;
 FName RoadID;
};

/** Presentation route query, independent of traffic simulation and actor lifetime. */
class NEONCITY_API FNeonCityRoadGuide
{
public:
 static constexpr int32 MaxRoadCount=32;
 bool AddRoad(FNeonCityGuideRoad Road);
 bool FindRoute(const FVector& Origin,const FVector& Goal,TArray<FVector>& Route) const;
 /** No off-road origin/goal connectors. Returns an ordered, elevation-aware lane. */
 bool FindDriveRoute(const FVector& Origin,const FVector& Forward,const FVector* Goal,
  TArray<FVector>& Route,float& SpeedLimitKmh,bool& Looped,TArray<float>* PointSpeedLimits=nullptr) const;
 const TArray<FNeonCityGuideRoad>& GetRoads() const { return Roads; }
 void Reset() { Roads.Reset();PointCount=0; }
private:
 TArray<FNeonCityGuideRoad> Roads;
 int32 PointCount=0;
};
