// Source algorithm and notice: see NeonCityOceanCDLOD.h. No engine modifications.
#include "NeonCityOceanCDLOD.h"
#include "Algo/StableSort.h"

namespace
{
double AxisDistance(double Lo, double Hi, double P)
{
    return FMath::Max3(Lo - P, 0.0, P - Hi);
}
bool IsFiniteVector(const FVector3d& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
}
}

FNeonCityOceanCDLOD::FNeonCityOceanCDLOD()
{
    Nodes.Reserve(MaxNodes);
    double Previous = 0;
    for (int32 L = 0; L < LevelCount; ++L)
    {
        const double Size = LeafSize * double(1 << L);
        const double Range = Size * 2.5;
        const double Start = Previous + (Range - Previous) * 0.66;
        Ranges[L] = Range;
        Morph[L] = FVector4f(float(Start), float(1.0 / FMath::Max(1e-3, Range - Start)), float(Size / GridSize), 0);
        LevelCounts[L] = 0;
        Previous = Range;
    }
}

void FNeonCityOceanCDLOD::BuildGrid(TArray<FVector3f>& Vertices, TArray<uint32>& Indices)
{
    Vertices.Reset((GridSize + 1) * (GridSize + 1));
    Indices.Reset(GridSize * GridSize * 6);
    for (int32 J = 0; J <= GridSize; ++J)
        for (int32 I = 0; I <= GridSize; ++I)
            Vertices.Emplace(float(I) / GridSize, 0, float(J) / GridSize);
    for (int32 I0 = 0; I0 < GridSize; I0 += 8)
        for (int32 J = 0; J < GridSize; ++J)
            for (int32 I = I0; I < FMath::Min(I0 + 8, GridSize); ++I)
            {
                const uint32 A = J * (GridSize + 1) + I, B = A + 1, C = A + GridSize + 1, D = C + 1;
                if ((I + J) % 2 == 0) Indices.Append({A, C, B, B, C, D});
                else Indices.Append({A, C, D, A, D, B});
            }
}

bool FNeonCityOceanCDLOD::Select(const FVector3d& CameraMetres, const TStaticArray<FVector4d, 6>& InwardPlanes)
{
    Nodes.Reset();
    for (uint32& Count : LevelCounts) Count = 0;
    bExceededCapacity = false;
    if (!IsFiniteVector(CameraMetres)) return false;
    for (const FVector4d& P : InwardPlanes)
        if (!IsFiniteVector(FVector3d(P.X, P.Y, P.Z)) || !FMath::IsFinite(P.W)
            || P.X * P.X + P.Y * P.Y + P.Z * P.Z < 1e-16) return false;
    Camera = CameraMetres;
    Planes = InwardPlanes;
    const double RootSize = LeafSize * double(1 << (LevelCount - 1));
    const double CX = FMath::FloorToDouble(Camera.X / RootSize);
    const double CZ = FMath::FloorToDouble(Camera.Z / RootSize);
    for (int32 J = -1; J <= 1; ++J)
        for (int32 I = -1; I <= 1; ++I)
            SelectNode((CX + I) * RootSize, (CZ + J) * RootSize, RootSize, LevelCount - 1);

    // Source JS sort is stable: preserve child traversal order for equal distances.
    Algo::StableSort(Nodes, [this](const FVector4f& A, const FVector4f& B)
    {
        auto Distance = [this](const FVector4f& N)
        {
            const double DX = AxisDistance(N.X, double(N.X) + N.Z, Camera.X);
            const double DZ = AxisDistance(N.Y, double(N.Y) + N.Z, Camera.Z);
            return DX * DX + DZ * DZ;
        };
        return Distance(A) < Distance(B);
    });
    return !bExceededCapacity;
}

bool FNeonCityOceanCDLOD::IntersectsRange(double X, double Z, double Size, double Radius) const
{
    const double DX = AxisDistance(X, X + Size, Camera.X);
    const double DY = AxisDistance(MinHeight, MaxHeight, Camera.Y);
    const double DZ = AxisDistance(Z, Z + Size, Camera.Z);
    return DX * DX + DY * DY + DZ * DZ <= Radius * Radius;
}

bool FNeonCityOceanCDLOD::IntersectsFrustum(double X, double Z, double Size) const
{
    for (const FVector4d& P : Planes)
    {
        const double PX = P.X > 0 ? X + Size : X;
        const double PY = P.Y > 0 ? MaxHeight : MinHeight;
        const double PZ = P.Z > 0 ? Z + Size : Z;
        if (P.X * PX + P.Y * PY + P.Z * PZ + P.W < 0) return false;
    }
    return true;
}

void FNeonCityOceanCDLOD::AddNode(double X, double Z, double Size, int32 Level)
{
    if (Nodes.Num() == MaxNodes) { bExceededCapacity = true; return; }
    Nodes.Emplace(float(X), float(Z), float(Size), float(Level));
    ++LevelCounts[Level];
}

int32 FNeonCityOceanCDLOD::ExclusionOverlap(double X, double Z, double Size) const
{
    int32 Result = 0;
    for (const FBox2D& E : Exclusions)
    {
        if (X >= E.Min.X && X + Size <= E.Max.X && Z >= E.Min.Y && Z + Size <= E.Max.Y) return 2;
        if (X < E.Max.X && X + Size > E.Min.X && Z < E.Max.Y && Z + Size > E.Min.Y) Result = 1;
    }
    return Result;
}

void FNeonCityOceanCDLOD::AddClipped(double X, double Z, double Size, int32 Level)
{
    const int32 Overlap = ExclusionOverlap(X, Z, Size);
    if (Overlap == 2) return;
    // Split straddling nodes finer near the viewer; far away a coarse rim over the (land) edge of the
    // footprint is invisible. Split children keep the parent's morph level, as CDLOD's own fill-in does.
    const double DX = AxisDistance(X, X + Size, Camera.X), DZ = AxisDistance(Z, Z + Size, Camera.Z);
    if (Overlap == 1 && Size > FMath::Max(ExclusionLeafSize, FMath::Sqrt(DX * DX + DZ * DZ) * 0.125))
    {
        const double Half = Size * 0.5;
        for (int32 J = 0; J < 2; ++J)
            for (int32 I = 0; I < 2; ++I)
                if (IntersectsFrustum(X + I * Half, Z + J * Half, Half)) AddClipped(X + I * Half, Z + J * Half, Half, Level);
        return;
    }
    AddNode(X, Z, Size, Level);
}

bool FNeonCityOceanCDLOD::SelectNode(double X, double Z, double Size, int32 Level)
{
    if (!IntersectsRange(X, Z, Size, Ranges[Level])) return false;
    if (!IntersectsFrustum(X, Z, Size)) return true;
    if (ExclusionOverlap(X, Z, Size) == 2) return true;
    if (Level == 0 || !IntersectsRange(X, Z, Size, Ranges[Level - 1]))
    {
        AddClipped(X, Z, Size, Level);
        return true;
    }
    const double Half = Size * 0.5;
    for (int32 J = 0; J < 2; ++J)
        for (int32 I = 0; I < 2; ++I)
        {
            const double ChildX = X + I * Half, ChildZ = Z + J * Half;
            if (!SelectNode(ChildX, ChildZ, Half, Level - 1) && IntersectsFrustum(ChildX, ChildZ, Half))
                AddClipped(ChildX, ChildZ, Half, Level);
        }
    return true;
}
