#pragma once
#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TidewaterOceanTexture.generated.h"

/** Material-facing reference to the shared FFT output; never serialized. */
UCLASS(Transient)
class NEONCITY_API UTidewaterOceanTexture:public UTexture
{
 GENERATED_BODY()
public:
 UTidewaterOceanTexture();
 virtual FTextureResource* CreateResource()override;
 virtual EMaterialValueType GetMaterialType()const override{return MCT_Texture2DArray;}
 virtual ETextureClass GetTextureClass()const override{return ETextureClass::Array;}
 virtual float GetSurfaceWidth()const override{return 256;}
 virtual float GetSurfaceHeight()const override{return 256;}
 virtual float GetSurfaceDepth()const override{return 0;}
 virtual uint32 GetSurfaceArraySize()const override{return 4;}
 // Called on render thread after the producing graph has executed.
 static void Publish_RenderThread(FRHICommandListBase& Cmd,FTextureResource* Resource,FRHITexture* Texture);
};
