#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "NeonCityOceanWake.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIGPUReadback.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <atomic>

namespace {
struct FWakeCapture {FString Name;uint32 Size;TUniquePtr<FRHIGPUBufferReadback> Readback;TArray<uint8> Data;};
struct FWakeAudit {
 TUniquePtr<FNeonCityOceanWake> Sim;
 FTextureRHIRef Island,Hull,Piles;
 TArray<uint8> HullBytes,PileBytes;
 TArray<FWakeCapture> Captures;
 std::atomic<bool> Busy{false},Ready{false};
 int32 Frame=0;
 bool Slept=false;
};
class FWakeAuditCommand:public IAutomationLatentCommand {
 FAutomationTestBase* Test;FString Path;TSharedRef<FWakeAudit,ESPMode::ThreadSafe> S;
 double Started=FPlatformTime::Seconds();
public:
 FWakeAuditCommand(FAutomationTestBase* T,FString P,TArray<uint8> H,TArray<uint8> B):Test(T),Path(P),S(MakeShared<FWakeAudit,ESPMode::ThreadSafe>()){S->HullBytes=MoveTemp(H);S->PileBytes=MoveTemp(B);}
 bool Update()override {
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Wake GPU audit timeout; no visual acceptance implied"));Release();return true;}
  if(S->Ready){
   TArray<TSharedPtr<FJsonValue>> Rows;
   for(const auto& C:S->Captures){
    const float* F=reinterpret_cast<const float*>(C.Data.GetData());float Peak=0;double Sum=0;bool Finite=true;
    for(int32 I=0;I<C.Data.Num()/4;++I){Finite&=FMath::IsFinite(F[I]);Peak=FMath::Max(Peak,FMath::Abs(F[I]));Sum+=FMath::Abs(F[I]);}
    Test->TestTrue(C.Name+TEXT(" finite"),Finite);Test->TestTrue(C.Name+TEXT(" bounded physical field"),Peak<30);
    if(C.Name==TEXT("300-Aeration"))Test->TestTrue(TEXT("Propeller aeration exists after turning"),Sum>1);
    Test->TestTrue(TEXT("Save GPU field"),FFileHelper::SaveArrayToFile(C.Data,*(Path/(C.Name+TEXT(".bin")))));
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),C.Name);Row->SetNumberField(TEXT("peak"),Peak);Row->SetNumberField(TEXT("absolute_sum"),Sum);Rows.Add(MakeShared<FJsonValueObject>(Row));
   }
   Test->TestTrue(TEXT("Wake stops dispatching after forty idle seconds"),S->Slept);
   auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("adapter"),GRHIAdapterName);Report->SetArrayField(TEXT("fields"),Rows);Report->SetStringField(TEXT("status"),TEXT("Native GPU numerical capture; manual game acceptance pending"));
   FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Path/TEXT("Report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   Release();return true;
  }
  if(S->Busy.exchange(true))return false;
  ENQUEUE_RENDER_COMMAND(NeonCityWakeAudit)([A=S](FRHICommandListImmediate& Cmd){
   if(!A->Sim){
    A->Sim=MakeUnique<FNeonCityOceanWake>();
    auto Upload=[&](const TCHAR* Name,int32 X,int32 Y,const uint8* Bytes){
     auto T=Cmd.CreateTexture(FRHITextureCreateDesc::Create2D(Name,X,Y,PF_R32_FLOAT).SetFlags(ETextureCreateFlags::ShaderResource).SetInitialState(ERHIAccess::SRVMask));
     Cmd.UpdateTexture2D(T,0,FUpdateTextureRegion2D(0,0,0,0,X,Y),X*4,Bytes);return T;
    };
    float Depth[4]={-12,-12,-12,-12};A->Island=Upload(TEXT("WakeAudit.Depth"),2,2,reinterpret_cast<uint8*>(Depth));
    A->Hull=Upload(TEXT("WakeAudit.Hull"),24,112,A->HullBytes.GetData());A->Piles=Upload(TEXT("WakeAudit.Piles"),158,1028,A->PileBytes.GetData());
   }
   for(int32 Batch=0;Batch<12 && A->Frame<1560;++Batch){
    const int32 I=A->Frame++;const float Angle=FMath::Clamp((I-180)/30.f,0.f,4.f)*.25f;
    FNeonCityWakeInput B;B.BoatID=1;B.Driven=I<300;B.Speed=I<300?7:0;B.Throttle=B.Driven?.65f:0;B.RPM=.8f;
    B.Position=I<180?FVector2f(200,100+I*7.f/30):FVector2f(200+28*(1-FMath::Cos(Angle)),142+28*FMath::Sin(Angle));
    B.Forward=FVector2f(FMath::Sin(Angle),FMath::Cos(Angle));
    FRDGBuilder G(Cmd);auto Register=[&](FTextureRHIRef T,const TCHAR* Name){return G.RegisterExternalTexture(CreateRenderTarget(T,Name));};
    auto R=A->Sim->AddFrame_RenderThread(G,B,(I+1)/30.f,1.f/30,Register(A->Island,TEXT("Audit.Island")),Register(A->Hull,TEXT("Audit.Hull")),Register(A->Piles,TEXT("Audit.Piles")));
    if(A->Frame==1 || A->Frame==180 || A->Frame==300 || A->Frame==420 || A->Frame==1200){
     auto Capture=[&](FRDGBufferRef Buffer,const TCHAR* Name){FWakeCapture C;C.Name=FString::Printf(TEXT("%d-%s"),A->Frame,Name);C.Size=Buffer->Desc.GetSize();C.Readback=MakeUnique<FRHIGPUBufferReadback>(FName(*C.Name));AddEnqueueCopyPass(G,C.Readback.Get(),Buffer,C.Size);A->Captures.Add(MoveTemp(C));};
     Capture(R.State,TEXT("State"));Capture(R.AerA,TEXT("Aeration"));Capture(R.NearBuffer,TEXT("Near"));
    }
    if(A->Frame==1560)A->Slept=!R.Stepped && R.Controls.Z==0;
    G.Execute();
   }
   if(A->Frame>=1560){
    bool Ready=true;for(auto& C:A->Captures)Ready&=C.Readback->IsReady();
    if(Ready){for(auto& C:A->Captures){const void* P=C.Readback->Lock(C.Size);C.Data.Append(static_cast<const uint8*>(P),C.Size);C.Readback->Unlock();C.Readback.Reset();}A->Ready=true;}
   }
   A->Busy=false;
  });
  return false;
 }
 void Release(){ENQUEUE_RENDER_COMMAND(NeonCityWakeAuditRelease)([A=S](FRHICommandListImmediate&){A->Sim.Reset();A->Captures.Reset();A->Island.SafeRelease();A->Hull.SafeRelease();A->Piles.SafeRelease();});}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanWakeGPU,"NeonCity.OceanExperience.WakeGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityOceanWakeGPU::RunTest(const FString&){
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 if(Label.IsEmpty()||Label.Contains(TEXT("/"))||Label.Contains(TEXT("\\"))||Label.Contains(TEXT("..")))return false;
 const FString Path=FPaths::ProjectSavedDir()/TEXT("VerticalSlice")/Label/TEXT("WakeNative");
 if(IFileManager::Get().DirectoryExists(*Path)){AddError(TEXT("Use a new evidence label"));return false;}
 TArray<uint8> H,P;const FString Src=FPaths::ProjectDir()/TEXT("ArtSource/OceanExperience/Wake/Tidewater");
 if(!FFileHelper::LoadFileToArray(H,*(Src/TEXT("Hull.f32"))) || !FFileHelper::LoadFileToArray(P,*(Src/TEXT("Piles.f32"))) || H.Num()!=24*112*4 || P.Num()!=158*1028*4)return false;
 IFileManager::Get().MakeDirectory(*Path,true);ADD_LATENT_AUTOMATION_COMMAND(FWakeAuditCommand(this,Path,MoveTemp(H),MoveTemp(P)));return true;
}
#endif
