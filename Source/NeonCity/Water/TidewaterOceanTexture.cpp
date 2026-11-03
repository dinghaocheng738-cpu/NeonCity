#include "Water/TidewaterOceanTexture.h"
#include "TextureResource.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"

namespace
{
class FTidewaterTextureResource:public FTextureResource
{
public:
 explicit FTidewaterTextureResource(UTidewaterOceanTexture* T):Owner(T){}
 virtual uint32 GetSizeX()const override{return 256;}
 virtual uint32 GetSizeY()const override{return 256;}
 virtual void InitRHI(FRHICommandListBase& Cmd)override
 {
  // Hidden until first executed FFT frame. The real output has all nine mips.
  TextureRHI=Cmd.CreateTexture(FRHITextureCreateDesc::Create2DArray(TEXT("NeonCity.OceanPending"))
   .SetExtent(256,256).SetArraySize(4).SetNumMips(9).SetFormat(PF_FloatRGBA)
   .SetFlags(ETextureCreateFlags::ShaderResource).SetInitialState(ERHIAccess::SRVMask));
  SamplerStateRHI=TStaticSamplerState<SF_AnisotropicLinear,AM_Wrap,AM_Wrap,AM_Wrap,0,4>::GetRHI();
  Cmd.UpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);
 }
 virtual void ReleaseRHI()override
 {RHIClearTextureReference(Owner->TextureReference.TextureReferenceRHI);FTextureResource::ReleaseRHI();}
 void Publish(FRHICommandListBase& Cmd,FRHITexture* Texture)
 {check(IsInRenderingThread());TextureRHI=Texture;Cmd.UpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);}
private:
 UTidewaterOceanTexture* Owner;
};
}
UTidewaterOceanTexture::UTidewaterOceanTexture(){SRGB=false;NeverStream=true;Filter=TF_Trilinear;}
FTextureResource* UTidewaterOceanTexture::CreateResource(){return new FTidewaterTextureResource(this);}
void UTidewaterOceanTexture::Publish_RenderThread(FRHICommandListBase& Cmd,FTextureResource* Resource,FRHITexture* Texture)
{if(Resource && Texture)static_cast<FTidewaterTextureResource*>(Resource)->Publish(Cmd,Texture);}
