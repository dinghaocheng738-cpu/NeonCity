#include "IslandLandCollisionLibrary.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "StaticMeshCompiler.h"
#include "PhysicsEngine/BodySetup.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Misc/SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace IslandLandCollision
{
const FString RoadBase=TEXT("/Game/NeonCity/Art/Polish/Adventure/Road/");
const TArray<FName> Canonical={TEXT("M_RoadAsphalt"),TEXT("M_RoadConcrete"),TEXT("M_RoadGalvanized"),TEXT("M_RoadWhite"),TEXT("M_RoadYellow"),TEXT("M_RoadReflector"),TEXT("M_RoadBlack")};

// JSON is written with Core only: this editor module has no Json dependency.
FString Quote(const FString& S)
{
 FString Out=TEXT("\"");
 for(TCHAR C:S)
 {
  switch(C)
  {
   case TEXT('"'):Out+=TEXT("\\\"");break;case TEXT('\\'):Out+=TEXT("\\\\");break;
   case TEXT('\n'):Out+=TEXT("\\n");break;case TEXT('\r'):Out+=TEXT("\\r");break;case TEXT('\t'):Out+=TEXT("\\t");break;
   case TEXT('\b'):Out+=TEXT("\\b");break;case TEXT('\f'):Out+=TEXT("\\f");break;
   default:if(uint32(C)<32)Out+=FString::Printf(TEXT("\\u%04x"),uint32(C));else Out.AppendChar(C);
  }
 }
 return Out+TEXT("\"");
}
FString FinalHash(FSHA1& H){H.Final();uint8 Bytes[FSHA1::DigestSize];H.GetHash(Bytes);return BytesToHex(Bytes,UE_ARRAY_COUNT(Bytes));}
bool AuthoringScope(FString& Error)
{
 FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Root);
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityGuardAuthoringQA")) || !Root.Contains(TEXT("/Saved/IslandLandPass1/")) || !IFileManager::Get().FileExists(*(Root/TEXT("qa-source-snapshot.json"))))
 {Error=TEXT("Opt-in isolated QA project and qa-source-snapshot marker required.");return false;}
#if PLATFORM_WINDOWS
 // The Python entry point additionally verifies final paths/source hashes. This
 // native guard rejects all Content ancestors that are still shared junctions.
 for(const TCHAR* Suffix:{TEXT(""),TEXT("Content"),TEXT("Content/NeonCity"),TEXT("Content/NeonCity/Art"),TEXT("Content/NeonCity/Art/Polish"),TEXT("Content/NeonCity/Art/Polish/Adventure"),TEXT("Content/NeonCity/Art/Polish/Adventure/Road"),TEXT("Content/NeonCity/Art/Polish/Adventure/Road/Collision15")})
 {
  FString Directory=Root/Suffix;FPaths::MakePlatformFilename(Directory);const uint32 Attributes=GetFileAttributesW(*Directory);
  if(Attributes==INVALID_FILE_ATTRIBUTES || !(Attributes&FILE_ATTRIBUTE_DIRECTORY) || (Attributes&FILE_ATTRIBUTE_REPARSE_POINT))
  {Error=TEXT("QA Content/Road and every Content ancestor must be a real isolated directory, never a junction.");return false;}
 }
#else
 Error=TEXT("This isolated native authoring scope currently supports Windows only.");return false;
#endif
 return true;
}
FString Slots(UStaticMesh* M)
{
 TArray<FString> Out;int32 Index=0;
 for(const FStaticMaterial& S:M->GetStaticMaterials())
  Out.Add(FString::Printf(TEXT("{\"index\":%d,\"slot\":%s,\"importedSlot\":%s,\"material\":%s}"),Index++,*Quote(S.MaterialSlotName.ToString()),*Quote(S.ImportedMaterialSlotName.ToString()),*Quote(GetPathNameSafe(S.MaterialInterface.Get()))));
 return TEXT("[")+FString::Join(Out,TEXT(","))+TEXT("]");
}
struct FRenderProof
{
 FString Hash,Nanite,MaterialSlots,DerivedDataKey;int64 Vertices=0,Triangles=0;int32 LODs=0;
 FString Json()const{return FString::Printf(TEXT("{\"sha1GeometryUvTangentsColorsSections\":%s,\"vertices\":%lld,\"triangles\":%lld,\"lods\":%d,\"naniteSettings\":%s,\"materialSlots\":%s,\"derivedDataKey\":%s}"),*Quote(Hash),Vertices,Triangles,LODs,*Quote(Nanite),*MaterialSlots,*Quote(DerivedDataKey));}
 bool operator==(const FRenderProof& R)const{return Hash==R.Hash && Nanite==R.Nanite && MaterialSlots==R.MaterialSlots && DerivedDataKey==R.DerivedDataKey && Vertices==R.Vertices && Triangles==R.Triangles && LODs==R.LODs;}
};
bool RenderProof(UStaticMesh* M,FRenderProof& Out,FString& Error)
{
 const auto* Data=M?M->GetRenderData():nullptr;
 if(!Data || Data->LODResources.IsEmpty()){Error=TEXT("Missing native render LOD resources.");return false;}
 FSHA1 H;Out.LODs=Data->LODResources.Num();Out.DerivedDataKey=Data->DerivedDataKey;Out.MaterialSlots=Slots(M);
 const FProperty* P=FindFProperty<FProperty>(UStaticMesh::StaticClass(),UStaticMesh::GetNaniteSettingsMemberName());
 if(!P){Error=TEXT("Native Nanite property is unavailable.");return false;}
 P->ExportText_InContainer(0,Out.Nanite,M,nullptr,M,PPF_None);H.Update(Out.LODs);
 for(const auto& L:Data->LODResources)
 {
  const auto& V=L.VertexBuffers;const uint32 N=V.PositionVertexBuffer.GetNumVertices(),UVs=V.StaticMeshVertexBuffer.GetNumTexCoords();
  if(!N || V.StaticMeshVertexBuffer.GetNumVertices()!=N || !V.StaticMeshVertexBuffer.GetTangentData() || !V.StaticMeshVertexBuffer.GetTexCoordData() || !L.IndexBuffer.GetAllowCPUAccess())
  {Error=TEXT("Full native CPU vertex/index/UV/tangent evidence is unavailable; parent is not modified.");return false;}
  const auto Indices=L.IndexBuffer.GetArrayView();H.Update(N);H.Update(UVs);const int32 IndexCount=Indices.Num();H.Update(IndexCount);
  for(uint32 I=0;I<N;++I)
  {
   H.Update(V.PositionVertexBuffer.VertexPosition(I));H.Update(V.StaticMeshVertexBuffer.VertexTangentX(I));H.Update(V.StaticMeshVertexBuffer.VertexTangentY(I));H.Update(V.StaticMeshVertexBuffer.VertexTangentZ(I));
   for(uint32 C=0;C<UVs;++C)H.Update(V.StaticMeshVertexBuffer.GetVertexUV(I,C));
  }
  const uint32 Colors=V.ColorVertexBuffer.GetNumVertices();H.Update(Colors);for(uint32 I=0;I<Colors;++I)H.Update(V.ColorVertexBuffer.VertexColor(I));
  for(int32 I=0;I<Indices.Num();++I){const uint32 Index=Indices[I];H.Update(Index);}
  const int32 Sections=L.Sections.Num();H.Update(Sections);
  for(const auto& S:L.Sections)
  {
   H.Update(S.MaterialIndex);H.Update(S.FirstIndex);H.Update(S.NumTriangles);H.Update(S.MinVertexIndex);H.Update(S.MaxVertexIndex);
   const uint8 Collision=S.bEnableCollision,Shadow=S.bCastShadow;H.Update(Collision);H.Update(Shadow);
  }
  Out.Vertices+=N;Out.Triangles+=Indices.Num()/3;
 }
 Out.Hash=FinalHash(H);return true;
}
bool ParentSlots(UStaticMesh* Parent,TMap<FName,int32>& Indices,FString& Error)
{
 const auto& Materials=Parent->GetStaticMaterials();
 if(Materials.Num()<6 || Materials.Num()>7){Error=TEXT("Parent must retain its six used canonical road slots; unused source Black may be the seventh.");return false;}
 for(int32 I=0;I<Materials.Num();++I)
 {
  const auto& S=Materials[I];if(!Canonical.Contains(S.MaterialSlotName) || Indices.Contains(S.MaterialSlotName) || !S.MaterialInterface)
  {Error=TEXT("Parent has unknown/duplicate/unbound canonical material slots.");return false;}
  Indices.Add(S.MaterialSlotName,I);
 }
 for(int32 I=0;I<6;++I)if(!Indices.Contains(Canonical[I])){Error=TEXT("Parent is missing a required original road slot.");return false;}
 return true;
}
struct FPhysicsProof
{
 int32 SourceVertices=0,SourceTriangles=0,SourceMin=MAX_int32,SourceMax=INDEX_NONE,CookedMeshCount=0;
 int64 BakedTriangles=0;int32 BakedMin=MAX_int32,BakedMax=INDEX_NONE;TArray<int64> Histogram;
 FString Json()const
 {
  TArray<FString> Counts;for(int64 C:Histogram)Counts.Add(FString::Printf(TEXT("%lld"),C));
  return FString::Printf(TEXT("{\"sourceVertices\":%d,\"sourceTriangles\":%d,\"sourceMaterialMin\":%d,\"sourceMaterialMax\":%d,\"bakedMeshCount\":%d,\"bakedPhysicsTriMeshTriangles\":%lld,\"bakedMaterialMin\":%d,\"bakedMaterialMax\":%d,\"bakedMaterialTriangleCounts\":[%s]}"),SourceVertices,SourceTriangles,SourceMin,SourceMax,CookedMeshCount,BakedTriangles,BakedMin,BakedMax,*FString::Join(Counts,TEXT(",")));
 }
};
bool PhysicsProof(UStaticMesh* Parent,FPhysicsProof& Out,FString& Error)
{
 FTriMeshCollisionData Source;
 if(!Parent->GetPhysicsTriMeshData(&Source,false) || Source.Indices.IsEmpty() || Source.MaterialIndices.Num()!=Source.Indices.Num())
 {Error=TEXT("Native parent collision provider did not return complete proxy triangles/material indices.");return false;}
 Out.SourceVertices=Source.Vertices.Num();Out.SourceTriangles=Source.Indices.Num();const int32 SlotsCount=Parent->GetStaticMaterials().Num();Out.Histogram.SetNumZeroed(SlotsCount);
 for(uint16 Material:Source.MaterialIndices)
 {
  if(Material>=SlotsCount){Error=TEXT("Collision source material index exceeds parent material array.");return false;}
  Out.SourceMin=FMath::Min(Out.SourceMin,int32(Material));Out.SourceMax=FMath::Max(Out.SourceMax,int32(Material));
 }
 const auto* Body=Parent->GetBodySetup();
 if(!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes || Body->TriMeshGeometries.IsEmpty())
 {Error=TEXT("BodySetup did not create actual cooked Chaos triangle geometry.");return false;}
 Out.CookedMeshCount=Body->TriMeshGeometries.Num();
 for(const auto& Mesh:Body->TriMeshGeometries)
 {
  if(!Mesh){Error=TEXT("Null cooked Chaos triangle geometry.");return false;}
  const int32 N=Mesh->Elements().GetNumTriangles();Out.BakedTriangles+=N;
  for(int32 I=0;I<N;++I)
  {
   const int32 Material=Mesh->GetMaterialIndex(I);
   if(Material<0 || Material>=SlotsCount){Error=TEXT("Cooked Chaos material index exceeds parent array.");return false;}
   ++Out.Histogram[Material];Out.BakedMin=FMath::Min(Out.BakedMin,Material);Out.BakedMax=FMath::Max(Out.BakedMax,Material);
  }
 }
 if(!Out.BakedTriangles){Error=TEXT("Cooked Chaos triangle geometry is empty.");return false;}
 return true;
}
FString Failure(const FString& Error,const FString& Detail=TEXT(""))
{return TEXT("{\"passed\":false,\"error\":")+Quote(Error)+TEXT(",\"detail\":")+Quote(Detail)+TEXT(",\"packagesSaved\":false}");}
}

FString UIslandLandCollisionLibrary::ConfigureGuardCollision(UStaticMesh* Parent,UStaticMesh* Proxy)
{
 using namespace IslandLandCollision;
 if(!IsInGameThread() || (GEditor && GEditor->PlayWorld))return Failure(TEXT("Only editor game-thread authoring outside PIE is permitted."));
 FString ScopeError;if(!AuthoringScope(ScopeError))return Failure(ScopeError);
 if(!Parent || !Proxy || Parent==Proxy)return Failure(TEXT("Distinct native parent and independent collision proxy are required."));
 if(!Parent->GetPathName().StartsWith(RoadBase+TEXT("SM_AdventureRoad_")) || !Proxy->GetPathName().StartsWith(RoadBase+TEXT("Collision15/SM_AdventureRoad_")))return Failure(TEXT("Candidate package scope is restricted to the owned road/Collision15 folder."));
 FStaticMeshCompilingManager::Get().FinishCompilation({Parent,Proxy});
 if(Proxy->ComplexCollisionMesh || Proxy->GetNaniteSettings().bEnabled || Proxy->LODForCollision!=0)return Failure(TEXT("Collision-only proxy must be non-Nanite LOD0 with no chained complex proxy."));
 auto* Body=Parent->GetBodySetup();
 if(!Body || Body->CollisionTraceFlag!=CTF_UseComplexAsSimple || Body->bNeverNeedsCookedCollisionData || Body->bSharedCookedData || Body->CurrentCookHelper)
  return Failure(TEXT("Parent must retain its existing synchronous, unshared ComplexAsSimple BodySetup."));
 FString Error;TMap<FName,int32> ParentMap;FRenderProof Before;
 if(!ParentSlots(Parent,ParentMap,Error) || !RenderProof(Parent,Before,Error))return Failure(Error);
 struct FRemap{int32 LOD,Section,ParentIndex;FName Name;};TArray<FRemap> Remaps;
 const auto ProxySlots=Proxy->GetStaticMaterials();const auto* Data=Proxy->GetRenderData();
 if(!Data || Data->LODResources.IsEmpty())return Failure(TEXT("Proxy render resources are unavailable for native collision cooking."));
 for(int32 LOD=0;LOD<Data->LODResources.Num();++LOD)
  for(int32 Section=0;Section<Data->LODResources[LOD].Sections.Num();++Section)
  {
   const auto& S=Data->LODResources[LOD].Sections[Section];
   if(!ProxySlots.IsValidIndex(S.MaterialIndex))return Failure(TEXT("Proxy imported section has an invalid material index."));
   const FName Name=ProxySlots[S.MaterialIndex].MaterialSlotName;const int32* Target=ParentMap.Find(Name);
   if(!Target)return Failure(TEXT("Proxy section has no matching canonical parent slot."),Name.ToString());
   Remaps.Add({LOD,Section,*Target,Name});
  }
 Proxy->Modify();Proxy->SetStaticMaterials(Parent->GetStaticMaterials());
 for(const auto& R:Remaps)
 {
  auto Info=Proxy->GetSectionInfoMap().Get(R.LOD,R.Section);Info.MaterialIndex=R.ParentIndex;Info.bEnableCollision=true;
  Proxy->GetSectionInfoMap().Set(R.LOD,R.Section,Info);
 }
 // Native editor subsystem SetLODMaterialSlot follows this same SectionInfoMap
 // route. Build only the newly imported proxy; OutErrors forces synchronous Build.
 TArray<FText> Errors;Proxy->Build(true,&Errors);FStaticMeshCompilingManager::Get().FinishCompilation({Proxy});
 if(!Errors.IsEmpty())return Failure(TEXT("Collision-only proxy native Build failed."),Errors[0].ToString());
 Data=Proxy->GetRenderData();
 if(!Data || Data->LODResources.IsEmpty())return Failure(TEXT("Proxy Build returned no native LODs."));
 TArray<FString> SectionJSON;
 for(const auto& R:Remaps)
 {
  if(!Data->LODResources.IsValidIndex(R.LOD) || !Data->LODResources[R.LOD].Sections.IsValidIndex(R.Section))return Failure(TEXT("Proxy Build changed imported section topology."));
  const auto& S=Data->LODResources[R.LOD].Sections[R.Section];
  if(S.MaterialIndex!=R.ParentIndex || !S.bEnableCollision)return Failure(TEXT("Native Build did not retain canonical section material/collision mapping."),R.Name.ToString());
  SectionJSON.Add(FString::Printf(TEXT("{\"lod\":%d,\"section\":%d,\"slot\":%s,\"parentIndex\":%d}"),R.LOD,R.Section,*Quote(R.Name.ToString()),R.ParentIndex));
 }
 UStaticMesh* Previous=Parent->ComplexCollisionMesh;
 Parent->Modify();Body->Modify();Parent->ComplexCollisionMesh=Proxy;
 // UE5.8 UStaticMesh::PostEditChangeProperty does this native BodySetup pair
 // then unconditionally rebuilds render/UV data. Call the pair directly so the
 // parent's existing render mesh, Nanite settings and UV data stay exact.
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
 FPhysicsProof Physics;FRenderProof After;
 const bool Valid=PhysicsProof(Parent,Physics,Error) && RenderProof(Parent,After,Error) && Before==After;
 if(!Valid)
 {
  Parent->ComplexCollisionMesh=Previous;Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
  if(Error.IsEmpty())Error=TEXT("Parent render/Nanite/material proof changed; new collision binding was reverted and nothing saved.");
  return Failure(Error);
 }
 int32 Refreshed=0;for(TObjectIterator<UStaticMeshComponent> It;It;++It)if(It->GetStaticMesh()==Parent && It->IsRegistered()){It->RecreatePhysicsState();++Refreshed;}
 Proxy->MarkPackageDirty();Parent->MarkPackageDirty();
 return FString::Printf(TEXT("{\"passed\":true,\"parent\":%s,\"proxy\":%s,\"parentRenderUnchanged\":true,\"parentRenderBuildCalled\":false,\"packagesSaved\":false,\"refreshedPhysicsComponents\":%d,\"parentBefore\":%s,\"parentAfter\":%s,\"sectionRemap\":[%s],\"physics\":%s,\"nativeReloadAndGameplayRequired\":true,\"packagedCookRequired\":true,\"visualFaceMaterialQueryCompatible\":false}"),
  *Quote(Parent->GetPathName()),*Quote(Proxy->GetPathName()),Refreshed,*Before.Json(),*After.Json(),*FString::Join(SectionJSON,TEXT(",")),*Physics.Json());
}

FString UIslandLandCollisionLibrary::ValidateGuardAuthoringWorld(UWorld* World)
{
 using namespace IslandLandCollision;
 FString Error;if(!AuthoringScope(Error))return Failure(Error);
 if(!IsInGameThread() || !World || World->WorldType!=EWorldType::Editor || !World->PersistentLevel
  || World->GetOutermost()->GetName()!=TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")
  || World->IsPartitionedWorld() || World->PersistentLevel->IsUsingExternalActors())return Failure(TEXT("Only the isolated copied non-partitioned SouthBeach map without external actors is permitted."));
#if PLATFORM_WINDOWS
 FString Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())/TEXT("Content/NeonCity/Maps");FPaths::MakePlatformFilename(Directory);
 const uint32 Attributes=GetFileAttributesW(*Directory);
 if(Attributes==INVALID_FILE_ATTRIBUTES || !(Attributes&FILE_ATTRIBUTE_DIRECTORY) || (Attributes&FILE_ATTRIBUTE_REPARSE_POINT))return Failure(TEXT("QA Maps must be an independent ordinary directory."));
#endif
 return TEXT("{\"passed\":true,\"partitionedWorld\":false,\"externalActors\":false,\"mapsDirectoryIndependent\":true,\"packagesSaved\":false}");
}

FString UIslandLandCollisionLibrary::ConfigureIndependentGuardCollision(UStaticMesh* Parent,UStaticMesh* Guard)
{
 using namespace IslandLandCollision;
 if(!IsInGameThread() || (GEditor && GEditor->PlayWorld))return Failure(TEXT("Independent guard authoring requires the editor game thread outside PIE."));
 FString Error;if(!AuthoringScope(Error))return Failure(Error);
 if(!Parent || !Guard || Parent==Guard || !Parent->GetPathName().StartsWith(RoadBase+TEXT("SM_AdventureRoad_"))
  || !Guard->GetPathName().StartsWith(RoadBase+TEXT("Collision15/SM_AdventureGuard_")))return Failure(TEXT("Distinct scoped road and new independent guard are required."));
 FStaticMeshCompilingManager::Get().FinishCompilation({Parent,Guard});
 TMap<FName,int32> Indices;if(!ParentSlots(Parent,Indices,Error))return Failure(Error);
 if(Guard->ComplexCollisionMesh || Guard->GetNaniteSettings().bEnabled || Guard->LODForCollision!=0 || Guard->GetStaticMaterials().Num()!=1
  || Guard->GetStaticMaterials()[0].MaterialSlotName!=TEXT("M_RoadGalvanized"))return Failure(TEXT("Independent guard must be non-Nanite LOD0, one canonical galvanized slot, and have no chained proxy."));
 auto* Body=Guard->GetBodySetup();if(!Body || Body->CurrentCookHelper || Body->bSharedCookedData)return Failure(TEXT("Independent guard requires its own synchronous BodySetup."));
 Guard->Modify();Body->Modify();
 Guard->SetStaticMaterials({FStaticMaterial(Parent->GetMaterial(Indices[TEXT("M_RoadGalvanized")]),TEXT("M_RoadGalvanized"),TEXT("M_RoadGalvanized"))});
 Body->CollisionTraceFlag=CTF_UseComplexAsSimple;Body->bDoubleSidedGeometry=true;
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
 FPhysicsProof Physics;if(!PhysicsProof(Guard,Physics,Error))return Failure(Error);
 Guard->MarkPackageDirty();
 return FString::Printf(TEXT("{\"passed\":true,\"guard\":%s,\"physics\":%s,\"tracePolicy\":\"ComplexAsSimple\",\"doubleSided\":true,\"nanite\":false,\"roadParentModified\":false,\"packagesSaved\":false,\"gameplayCertified\":false}"),*Quote(Guard->GetPathName()),*Physics.Json());
}

FString UIslandLandCollisionLibrary::InspectIndependentGuardCollision(UStaticMesh* Guard)
{
 using namespace IslandLandCollision;
 if(!Guard || !IsInGameThread())return Failure(TEXT("Independent native guard on the editor game thread is required."));
 FStaticMeshCompilingManager::Get().FinishCompilation({Guard});
 const auto* Body=Guard->GetBodySetup();FString Error;FPhysicsProof Physics;
 if(!Body || Body->GetCollisionTraceFlag()!=CTF_UseComplexAsSimple || !Body->bDoubleSidedGeometry || Guard->ComplexCollisionMesh
  || Guard->GetNaniteSettings().bEnabled || !PhysicsProof(Guard,Physics,Error))return Failure(Error.IsEmpty()?TEXT("Independent guard policy was not preserved."):Error);
 return FString::Printf(TEXT("{\"passed\":true,\"guard\":%s,\"physics\":%s,\"tracePolicy\":\"ComplexAsSimple\",\"doubleSided\":true,\"nanite\":false,\"roadParentModified\":false,\"packagesSaved\":false,\"gameplayCertified\":false}"),*Quote(Guard->GetPathName()),*Physics.Json());
}

FString UIslandLandCollisionLibrary::InspectGuardCollision(UStaticMesh* Parent)
{
 using namespace IslandLandCollision;
 if(!Parent || !IsInGameThread())return Failure(TEXT("Native parent on editor game thread is required."));
 if(Parent->ComplexCollisionMesh)FStaticMeshCompilingManager::Get().FinishCompilation({Parent,Parent->ComplexCollisionMesh.Get()});
 else FStaticMeshCompilingManager::Get().FinishCompilation({Parent});
 FString Error;FRenderProof Render;FPhysicsProof Physics;TMap<FName,int32> ParentMap;
 if(!ParentSlots(Parent,ParentMap,Error) || !RenderProof(Parent,Render,Error) || !PhysicsProof(Parent,Physics,Error))return Failure(Error);
 return FString::Printf(TEXT("{\"passed\":true,\"parent\":%s,\"proxy\":%s,\"render\":%s,\"physics\":%s,\"packagesSaved\":false,\"gameplayCertified\":false}"),*Quote(Parent->GetPathName()),*Quote(GetPathNameSafe(Parent->ComplexCollisionMesh.Get())),*Render.Json(),*Physics.Json());
}
