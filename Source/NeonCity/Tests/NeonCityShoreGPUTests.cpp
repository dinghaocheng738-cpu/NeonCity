#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Water/CoastalWaveData.h"
#include "NeonCityOceanFFT.h"
#include "NeonCityOceanQuery.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "UObject/StrongObjectPtr.h"
#include <atomic>

namespace
{
struct FShoreAuditState
{
 FNeonCityOceanFFT FFT;
 FNeonCityOceanQuery Queries;
 FNeonCityOceanQueryResult Result;
 std::atomic<bool> Submitted{false},Polling{false},Ready{false};
};
class FShoreAuditCommand:public IAutomationLatentCommand
{
public:
 FShoreAuditCommand(FAutomationTestBase* T,UCoastalWaveData* F,FString P):Test(T),Field(F),Path(MoveTemp(P)),State(MakeShared<FShoreAuditState,ESPMode::ThreadSafe>()){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>120){Test->AddError(TEXT("Shore readback timed out"));Release();return true;}
  if(!Waiting)
  {
   Waiting=true;State->Ready=false;State->Submitted=false;
   const int32 Sea=Index/6;const float Times[]={1,3,7,11,19,41};
   FNeonCityOceanQueryRequest Q;Q.Count=64;Q.Amplitude=0;Q.SimulationTime=Times[Index%6];
   // Disable FFT only in this isolated oracle to measure the new shore branch.
   // Normal-world boat tests retain the full FFT + shore composition.
   for(int32 I=0;I<64;++I)Q.Points[I]=FVector4f(-140+40*(I%8),-75+24*(I/8),0,0);
   FNeonCityOceanParameters P;FMemory::Memcpy(&P,Field->GetProfile(Sea).GetData(),sizeof(P));
   const FVector4f Controls(Q.SimulationTime,Sea==0?11.f:Sea==1?9.f:8.5f,Sea==0?.18f:Sea==1?.34f:.56f,1);
   auto* D=Field->DepthTexture->GetResource();auto* H=Field->IslandHeight->GetResource();auto* S=Field->IslandShore->GetResource();
   const FVector4f Bounds(Field->GridMinimum.X,Field->GridMinimum.Y,Field->GridMaximum.X,Field->GridMaximum.Y);
   ENQUEUE_RENDER_COMMAND(NeonCityShoreAuditSubmit)([R=State,D,H,S,Bounds,P,Q,Controls](FRHICommandListImmediate& Cmd)
   {
    FRDGBuilder Graph(Cmd);auto F=R->FFT.AddFrame_RenderThread(Graph,P,float(Q.SimulationTime),1.f/60);
    FNeonCityOceanBathymetry B;
    B.Depth=Graph.RegisterExternalTexture(CreateRenderTarget(D->TextureRHI,TEXT("ShoreAudit.Depth")));
    B.IslandHeight=Graph.RegisterExternalTexture(CreateRenderTarget(H->TextureRHI,TEXT("ShoreAudit.Height")));
    B.IslandShore=Graph.RegisterExternalTexture(CreateRenderTarget(S->TextureRHI,TEXT("ShoreAudit.Refraction")));
    B.Bounds=Bounds;B.Origin=FVector2f(FVector2D(NeonCityCoastalLayout::IslandOrigin));B.Resolution=513;B.ShoreControls=Controls;
    R->Queries.AddQueries_RenderThread(Graph,F.Displacement,P,Q,&B);Graph.Execute();R->Submitted=true;
   });
   return false;
  }
  if(State->Ready)
  {
   const auto& Q=State->Result;const FString File=FString::Printf(TEXT("shore-%02d.bin"),Index);
   TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>(Q.Values),sizeof(Q.Values));
   for(const auto V:Q.Values)if(!Test->TestFalse(TEXT("Finite shore GPU result"),V.ContainsNaN())){Release();return true;}
   Test->TestTrue(TEXT("Retain native shore samples"),FFileHelper::SaveArrayToFile(Bytes,*(Path/File)));
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("file"),File);Row->SetNumberField(TEXT("sea"),Index/6);
   Row->SetNumberField(TEXT("time"),Q.Request.SimulationTime);Rows.Add(MakeShared<FJsonValueObject>(Row));
   ++Index;Waiting=false;
   if(Index<18)return false;
   auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("runner"),TEXT("UE RDG real coastal query; FFT amplitude zero solely to isolate shore port"));
   Report->SetArrayField(TEXT("cases"),Rows);FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
   Test->TestTrue(TEXT("Shore evidence report"),FFileHelper::SaveStringToFile(Text,*(Path/TEXT("Report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
   Test->AddInfo(TEXT("Native capture requires independent original WGSL comparison by the test runner; not a visual acceptance."));Release();return true;
  }
  if(State->Submitted && !State->Polling.exchange(true))
  {
   ENQUEUE_RENDER_COMMAND(NeonCityShoreAuditPoll)([R=State](FRHICommandListImmediate&)
   {if(R->Queries.TryConsumeLatest_RenderThread(R->Result))R->Ready=true;R->Polling=false;});
  }
  return false;
 }
private:
 void Release(){ENQUEUE_RENDER_COMMAND(NeonCityShoreAuditRelease)([R=State](FRHICommandListImmediate&){R->Queries.Reset_RenderThread();R->FFT.Reset_RenderThread();});}
 FAutomationTestBase* Test;
 TStrongObjectPtr<UCoastalWaveData> Field;
 FString Path;
 TSharedRef<FShoreAuditState,ESPMode::ThreadSafe> State;
 TArray<TSharedPtr<FJsonValue>> Rows;
 double Start=0;int32 Index=0;bool Waiting=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityShoreGPUTest,"NeonCity.Immersion.ShoreGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityShoreGPUTest::RunTest(const FString&)
{
 auto* F=LoadObject<UCoastalWaveData>(nullptr,TEXT("/Game/NeonCity/Art/Immersion/Ocean/DA_CoastalWaves"));
 if(!TestTrue(TEXT("Native floating-point height/refraction textures"),F && F->IslandHeight && F->IslandShore && F->DepthTexture))return false;
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 if(Label.IsEmpty() || Label.Contains(TEXT("/")) || Label.Contains(TEXT("\\")) || Label.Contains(TEXT("..")))return false;
 const FString Path=FPaths::ProjectSavedDir()/TEXT("VerticalSlice")/Label/TEXT("ShoreNative");
 if(IFileManager::Get().DirectoryExists(*Path)){AddError(TEXT("Prior shore capture preserved"));return false;}
 IFileManager::Get().MakeDirectory(*Path,true);
 ADD_LATENT_AUTOMATION_COMMAND(FShoreAuditCommand(this,F,Path));return true;
}
#endif
