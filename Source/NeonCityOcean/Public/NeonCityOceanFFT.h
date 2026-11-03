#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/** Source metres / X,Y-up,Z coordinates. No UObject, world or actor references.
 * Original layout remains exactly 224 bytes for immutable source profile audits. */
struct FNeonCityOceanParameters
{
    FVector4f Sizes[4];
    FVector4f Cuts[4];
    FVector4f SysA[2];
    FVector4f SysB[2];
    float Choppiness=0, FoamBias=0, FoamGain=0, FoamDecay=0;
    float FoamAdd=0, Time=0, Depth=0;
    uint32 Seed=0;
    FNeonCityOceanParameters()
    {
        for(auto& V:Sizes)V=FVector4f(0,0,0,0);
        for(auto& V:Cuts)V=FVector4f(0,0,0,0);
        for(auto& V:SysA)V=FVector4f(0,0,0,0);
        for(auto& V:SysB)V=FVector4f(0,0,0,0);
    }
};
static_assert(sizeof(FNeonCityOceanParameters)==224);
static_assert(STRUCT_OFFSET(FNeonCityOceanParameters,Time)==212);

/** RDG handles expire when the calling graph executes. Persistent resources
 * remain owned by the simulation, never by a boat or player-specific copy. */
struct FNeonCityOceanFrame
{
    FRDGBufferRef H0=nullptr, Wave=nullptr, Tmp=nullptr, Foam=nullptr, MipSrc=nullptr, MipMid=nullptr;
    FRDGTextureRef Displacement=nullptr, Derivatives=nullptr;
};

/** Optional offline audit hook, called immediately after the named source stage.
 * Normal rendering does not copy full textures or buffers back to the CPU. */
using FNeonCityOceanAudit=TFunction<void(FRDGBuilder&,const TCHAR*,const FNeonCityOceanFrame&)>;

/** Render-thread-owned, pooled four-cascade native implementation of Tidewater
 * OceanFFT. Each call adds six normal-frame dispatches; first call adds three
 * spectrum initialization passes. No scene integration or game-thread tick. */
class NEONCITYOCEAN_API FNeonCityOceanFFT final
{
public:
    FNeonCityOceanFFT();
    ~FNeonCityOceanFFT();
    FNeonCityOceanFFT(const FNeonCityOceanFFT&)=delete;
    FNeonCityOceanFFT& operator=(const FNeonCityOceanFFT&)=delete;

    // Execute the graph before submitting another frame on this instance.
    // Reset before changing spectral parameters; foam controls may change live.
    FNeonCityOceanFrame AddFrame_RenderThread(FRDGBuilder& Graph,
        const FNeonCityOceanParameters& Parameters,float Time,float DeltaSeconds,
        const FNeonCityOceanAudit& Audit={});
    void Reset_RenderThread();

private:
    struct FState;
    TUniquePtr<FState> State;
};
