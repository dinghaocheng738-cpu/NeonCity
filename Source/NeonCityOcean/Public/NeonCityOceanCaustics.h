#pragma once
#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

// Tidewater Caustics.js photon splatting, adapted to UE RDG. Inputs and sun
// direction retain the source's metre / Y-up coordinates.
NEONCITYOCEAN_API void AddNeonCityCaustics(FRDGBuilder& Graph,FRDGTextureRef Derivatives,
 FRDGTextureRef Fine,FRDGTextureRef Broad,FVector3f SunDirection,FVector2f TileSizes,float Amplitude);
