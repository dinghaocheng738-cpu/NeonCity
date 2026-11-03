// Adapted from Tidewater src/core/CDLOD.js, commit 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT; Docs/ThirdParty/Tidewater-LICENSE.txt.
#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

/** Source-space open-ocean quadtree. Positions are metres in X/Y-up/Z, not UE centimetres.
 * Frustum planes use dot(N, P) + W >= 0 inside (NOT FPlane's PlaneDot convention).
 * Own one selector per view; selection mutates bounded scratch state. No UObject/GPU work.
 */
class NEONCITYOCEAN_API FNeonCityOceanCDLOD
{
public:
    static constexpr int32 GridSize = 32;
    static constexpr int32 LevelCount = 12;
    static constexpr int32 MaxNodes = 1500;
    static constexpr double LeafSize = 8.0;
    static constexpr double MinHeight = -25.0;
    static constexpr double MaxHeight = 25.0;

    FNeonCityOceanCDLOD();

    /** False for invalid input or capacity exhaustion; never silently reports a complete mesh. */
    bool Select(const FVector3d& CameraMetres, const TStaticArray<FVector4d, 6>& InwardPlanes);
    const TArray<FVector4f>& GetNodes() const { return Nodes; }
    const TStaticArray<uint32, LevelCount>& GetLevelCounts() const { return LevelCounts; }
    const TStaticArray<FVector4f, LevelCount>& GetMorphParameters() const { return Morph; }
    bool DidExceedCapacity() const { return bExceededCapacity; }

    /** Inland water footprints (source-space X/Z rectangles) where no open ocean is drawn: nodes inside
     * one are skipped, nodes straddling an edge are split (down to a distance-scaled size) and only
     * their outside parts kept. The ocean lies below a raised lake's bed and would show through it. */
    void SetExclusions(const TArray<FBox2D>& SourceRects) { Exclusions = SourceRects; }

    /** Exact source grid/winding/8-column strip order, to upload once and instance per node. */
    static void BuildGrid(TArray<FVector3f>& Vertices, TArray<uint32>& Indices);

private:
    bool SelectNode(double X, double Z, double Size, int32 Level);
    bool IntersectsRange(double X, double Z, double Size, double Radius) const;
    bool IntersectsFrustum(double X, double Z, double Size) const;
    void AddNode(double X, double Z, double Size, int32 Level);
    /** 0 outside every exclusion, 1 straddling one, 2 inside one. */
    int32 ExclusionOverlap(double X, double Z, double Size) const;
    void AddClipped(double X, double Z, double Size, int32 Level);
    static constexpr double ExclusionLeafSize = 16.0;
    TArray<FBox2D> Exclusions;

    FVector3d Camera = FVector3d::ZeroVector;
    TStaticArray<FVector4d, 6> Planes;
    TStaticArray<double, LevelCount> Ranges;
    TStaticArray<FVector4f, LevelCount> Morph;
    TStaticArray<uint32, LevelCount> LevelCounts;
    TArray<FVector4f> Nodes;
    bool bExceededCapacity = false;
};
