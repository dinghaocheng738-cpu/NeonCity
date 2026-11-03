#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "AssetCompilingManager.h"

// Compare imported native vertex-instance data with the locked source buffers.
// This catches UV aliasing prerequisites, tag gamma conversion and import flips;
// normal-game captures separately verify Nanite deformation on the GPU.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFoliageAttributeLayout,"NeonCity.Foliage.NativeAttributeLayout",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFoliageAttributeLayout::RunTest(const FString&)
{
 FString Text;TSharedPtr<FJsonObject> Data;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Vegetation/AttributeContract.json")))
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data)
 {AddError(TEXT("Missing canonical foliage attribute contract"));return false;}
 TestEqual(TEXT("Four-UV and exact-byte attribute schema"),Data->GetIntegerField(TEXT("version")),2);
 TestTrue(TEXT("Only cosmetic phase is rounded to 16 bits"),Data->GetNumberField(TEXT("max_material_phase_error"))<=1./131070.+1e-10);
 struct FVertex {FVector3f Position;float UV[8];uint8 Color[4];};
 int32 Total=0;
 const auto& Meshes=Data->GetArrayField(TEXT("meshes"));TestEqual(TEXT("All source mesh variants are covered"),Meshes.Num(),11);
 for(const auto& Entry:Meshes)
 {
  const auto Object=Entry->AsObject();const FString Name=Object->GetStringField(TEXT("name"));
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/")+Name));
  if(!TestNotNull(*Name,Mesh))continue;
  FAssetCompilingManager::Get().FinishAllCompilation();
  TestEqual(*(Name+TEXT(" supports Nanite's four UV fetch limit")),Mesh->GetNumUVChannels(0),4);
  if(Name!=TEXT("SM_NCTW_PalmFar"))
   TestTrue(*(Name+TEXT(" has built Nanite data and preserves semantic UVs")),Mesh->HasValidNaniteData()
    && !Mesh->GetNaniteSettings().bLerpUVs && Mesh->GetNaniteSettings().ShapePreservation==ENaniteShapePreservation::PreserveArea);
  const auto* Description=Mesh->GetMeshDescription(0);
  if(!TestNotNull(TEXT("Native source mesh description"),Description))continue;
  FStaticMeshConstAttributes Attributes(*Description);
  const auto Positions=Attributes.GetVertexPositions();const auto UVs=Attributes.GetVertexInstanceUVs();
  const auto Colors=Attributes.GetVertexInstanceColors();
  TArray<FVertex> Source;
  for(const auto& Value:Object->GetArrayField(TEXT("vertices")))
  {
   const auto V=Value->AsObject();FVertex R{};const auto P=V->GetArrayField(TEXT("p"));
   R.Position=FVector3f(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());
   const auto U=V->GetArrayField(TEXT("uv")),C=V->GetArrayField(TEXT("color"));
   for(int32 I=0;I<8;++I)R.UV[I]=U[I]->AsNumber();
   for(int32 I=0;I<4;++I)R.Color[I]=uint8(C[I]->AsNumber());Source.Add(R);
  }
  int32 Missing=0;FVector3f FirstPosition=FVector3f::ZeroVector;FVector4f FirstColor(0,0,0,0);
  for(const auto ID:Description->VertexInstances().GetElementIDs())
  {
   const auto P=Positions[Description->GetVertexInstanceVertex(ID)];const auto C=Colors[ID];
   // MeshDescription stores linear color; the native vertex buffer stores sRGB bytes.
   const FColor Packed=FLinearColor(C).ToFColor(true);
   const uint8 Channels[]={Packed.R,Packed.G,Packed.B,Packed.A};bool Found=false;
   for(const auto& V:Source)
   {
    if(!P.Equals(V.Position,.03f))continue;bool Match=true;
    for(int32 Channel=0;Channel<4;++Channel)
    {
     const FVector2f U=UVs.Get(ID,Channel);
     Match&=FMath::IsNearlyEqual(U.X,V.UV[2*Channel],.0002f) && FMath::IsNearlyEqual(U.Y,V.UV[2*Channel+1],.0002f);
     Match&=Channels[Channel]==V.Color[Channel];
    }
    if(Match){Found=true;break;}
   }
   if(!Found){if(Missing==0){FirstPosition=P;FirstColor=C;}++Missing;}++Total;
  }
  if(Missing)AddError(FString::Printf(TEXT("%s mismatches %d imported corners; first position=%s color=(%.8f %.8f %.8f %.8f)"),
   *Name,Missing,*FirstPosition.ToString(),FirstColor.X,FirstColor.Y,FirstColor.Z,FirstColor.W));
 }
 AddInfo(FString::Printf(TEXT("Compared %d native corners with canonical position, eight float attributes and four packed bytes"),Total));
 TestTrue(TEXT("Full source geometry is checked"),Total>10000);return true;
}
#endif
