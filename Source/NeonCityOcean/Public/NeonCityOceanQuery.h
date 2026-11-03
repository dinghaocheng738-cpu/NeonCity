#pragma once
#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

struct FNeonCityOceanParameters;
struct FNeonCityWakeFrame;
/** Optional native seabed adapter. Depth texture is centimetres in UE XY,
 * sampled at cell centres; the source wave equations consume metres. */
struct FNeonCityOceanBathymetry
{
    FRDGTextureRef Depth=nullptr;
    FRDGTextureRef IslandHeight=nullptr;
    FRDGTextureRef IslandShore=nullptr;
    FVector4f ShoreControls=FVector4f(0,9,.34f,0);
    FVector4f Bounds=FVector4f(0,0,1,1);
    FVector2f Origin=FVector2f::ZeroVector;
    float Resolution=0;
};

/** Source-coordinate metres. The world adapter reserves slot zero for camera,
 * allocates named actor sample ranges, and converts UE X/-Y centimetres once. */
struct FNeonCityOceanQueryRequest
{
    static constexpr uint32 Capacity=64;
    FVector4f Points[Capacity];
    uint32 Count=1;
    float SeaLevel=0,Amplitude=1;
    double SimulationTime=0;
    uint64 ClientRevision=0;
    FNeonCityOceanQueryRequest(){for(auto& P:Points)P=FVector4f(0,0,0,0);}
};

/** Values: source height, normal X, normal Z, sea floor. Request retains the
 * exact issued positions/time/revision; delayed results never imply current
 * actor positions. Normal Y is positive and reconstructed at the world edge. */
struct FNeonCityOceanQueryResult
{
    FNeonCityOceanQueryRequest Request;
    FVector4f Values[FNeonCityOceanQueryRequest::Capacity];
    uint64 Sequence=0;
};

struct FNeonCityOceanQuerySubmission
{
    FRDGBufferRef Results=nullptr; // Same-frame GPU camera/waterline consumers.
    bool bReadbackQueued=false;
};

/** Render-thread-only shared query bridge. Exactly three 1024-byte staging
 * buffers at most. A full ring skips CPU copying while GPU camera results still
 * update. No wait, flush, full-texture readback, or UObject lifetime coupling. */
class NEONCITYOCEAN_API FNeonCityOceanQuery final
{
public:
    FNeonCityOceanQuery();
    ~FNeonCityOceanQuery();
    FNeonCityOceanQuery(const FNeonCityOceanQuery&)=delete;
    FNeonCityOceanQuery& operator=(const FNeonCityOceanQuery&)=delete;
    FNeonCityOceanQuerySubmission AddQueries_RenderThread(FRDGBuilder& Graph,FRDGTextureRef Displacement,
        const FNeonCityOceanParameters& Ocean,const FNeonCityOceanQueryRequest& Request,
        const FNeonCityOceanBathymetry* Bathymetry=nullptr,const FNeonCityWakeFrame* Wake=nullptr);
    // Poll only between executed RDG graphs; returns the newest ready sequence.
    bool TryConsumeLatest_RenderThread(FNeonCityOceanQueryResult& Result);
    // Execute any graph referencing this instance before reset/destruction.
    void Reset_RenderThread();
private:
    struct FState;
    TUniquePtr<FState> State;
    uint64 NextSequence=1,LastDelivered=0;
};
