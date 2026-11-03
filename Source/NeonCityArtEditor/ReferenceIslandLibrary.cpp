#include "ReferenceIslandLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "LandscapeComponent.h"
#include "Materials/MaterialInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/SecureHash.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "Nanite/NaniteFixupChunk.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"
#include "PixelFormat.h"

namespace
{
constexpr double ReferenceTangentZeroThreshold = 1.e-4;

FString ReferenceStatsJsonString(FString Value)
{
    Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
    Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
    Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
    Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
    Value.ReplaceInline(TEXT("\t"), TEXT("\\t"));
    return TEXT("\"")+Value+TEXT("\"");
}

bool ReferenceStatsFinite(const FVector3f& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

struct FReferenceTangentAxisStats
{
    uint32 FiniteCount = 0;
    uint32 NonfiniteCount = 0;
    uint32 ZeroLengthCount = 0;
    uint32 NearZeroComponentsCount = 0;
    uint32 ShortLengthCount = 0;
    double Minimum = TNumericLimits<double>::Max();
    double Maximum = 0.;

    void Add(const FVector3f& Value)
    {
        if (!ReferenceStatsFinite(Value))
        {
            ++NonfiniteCount;
            return;
        }
        // Accumulate in double, so malformed large float components cannot
        // overflow a float SizeSquared and masquerade as a usable unit axis.
        const double Length = FVector3d(Value).Length();
        ++FiniteCount;
        Minimum = FMath::Min(Minimum, Length);
        Maximum = FMath::Max(Maximum, Length);
        ZeroLengthCount += Length < ReferenceTangentZeroThreshold;
        NearZeroComponentsCount += FMath::Abs(Value.X) <= ReferenceTangentZeroThreshold &&
            FMath::Abs(Value.Y) <= ReferenceTangentZeroThreshold && FMath::Abs(Value.Z) <= ReferenceTangentZeroThreshold;
        // Packed 8-bit zero input can decode to a small nonzero vector. Retain
        // this separate diagnostic instead of declaring that quantized zero OK.
        ShortLengthCount += Length < .5;
    }

    FString Json() const
    {
        const FString Min = FiniteCount ? FString::Printf(TEXT("%.17g"), Minimum) : TEXT("null");
        const FString Max = FiniteCount ? FString::Printf(TEXT("%.17g"), Maximum) : TEXT("null");
        return FString::Printf(TEXT("{\"finite_count\":%u,\"nonfinite_count\":%u,\"zero_length_lt_1e_4_count\":%u,\"near_zero_components_le_1e_4_count\":%u,\"short_length_lt_0_5_count\":%u,\"min_length\":%s,\"max_length\":%s}"),
            FiniteCount, NonfiniteCount, ZeroLengthCount, NearZeroComponentsCount, ShortLengthCount, *Min, *Max);
    }
};

struct FReferenceTangentPairStats
{
    uint32 ValidCount = 0;
    uint32 UnavailableCount = 0;
    double Minimum = 1.;
    double Maximum = -1.;
    double MaximumAbsolute = 0.;

    void Add(const FVector3f& A, const FVector3f& B)
    {
        if (!ReferenceStatsFinite(A) || !ReferenceStatsFinite(B))
        {
            ++UnavailableCount;
            return;
        }
        const FVector3d DA(A), DB(B);
        const double LengthA = DA.Length(), LengthB = DB.Length();
        if (LengthA < ReferenceTangentZeroThreshold || LengthB < ReferenceTangentZeroThreshold)
        {
            ++UnavailableCount;
            return;
        }
        const double Dot = FVector3d::DotProduct(DA/LengthA, DB/LengthB);
        ++ValidCount;
        Minimum = FMath::Min(Minimum, Dot);
        Maximum = FMath::Max(Maximum, Dot);
        MaximumAbsolute = FMath::Max(MaximumAbsolute, FMath::Abs(Dot));
    }

    FString Json() const
    {
        const FString Min = ValidCount ? FString::Printf(TEXT("%.17g"), Minimum) : TEXT("null");
        const FString Max = ValidCount ? FString::Printf(TEXT("%.17g"), Maximum) : TEXT("null");
        const FString Abs = ValidCount ? FString::Printf(TEXT("%.17g"), MaximumAbsolute) : TEXT("null");
        return FString::Printf(TEXT("{\"valid_pair_count\":%u,\"unavailable_pair_count\":%u,\"min_dot\":%s,\"max_dot\":%s,\"max_abs_dot\":%s}"),
            ValidCount, UnavailableCount, *Min, *Max, *Abs);
    }
};

struct FReferenceTangentBasisStats
{
    FReferenceTangentAxisStats Axes[3];
    FReferenceTangentPairStats Pairs[3];

    bool Add(const FVector3f& X, const FVector3f& Y, const FVector3f& Z)
    {
        Axes[0].Add(X);
        Axes[1].Add(Y);
        Axes[2].Add(Z);
        Pairs[0].Add(X, Y);
        Pairs[1].Add(X, Z);
        Pairs[2].Add(Y, Z);
        return !ReferenceStatsFinite(X) || !ReferenceStatsFinite(Y) || !ReferenceStatsFinite(Z) ||
            FVector3d(X).Length() < .5 || FVector3d(Y).Length() < .5 || FVector3d(Z).Length() < .5;
    }

    FString Json() const
    {
        return FString::Printf(TEXT("\"axes\":{\"tangent_x\":%s,\"tangent_y\":%s,\"tangent_z\":%s},\"orthogonality\":{\"xy\":%s,\"xz\":%s,\"yz\":%s}"),
            *Axes[0].Json(), *Axes[1].Json(), *Axes[2].Json(), *Pairs[0].Json(), *Pairs[1].Json(), *Pairs[2].Json());
    }
};
}

FString UReferenceIslandLibrary::GetReferenceStaticMeshTangentBasisStats(UStaticMesh* Mesh)
{
    if (!GEditor || !Mesh || !IsInGameThread())
        return TEXT("{\"schema\":923,\"status\":\"ERROR\",\"errors\":[\"Editor, valid mesh and game-thread context required\"],\"lods\":[],\"source_mesh_descriptions\":[],\"nanite_internal_inspected\":false}");
    FAssetCompilingManager::Get().FinishAllCompilation();
    FlushRenderingCommands();
    const FStaticMeshRenderData* Data = Mesh->GetRenderData();
    TArray<FString> Errors, Lods, Sources;
    if (!Data || Data->LODResources.IsEmpty())
        Errors.Add(ReferenceStatsJsonString(TEXT("Final fallback render LOD data unavailable")));
    if (Data)
    {
        for (int32 Lod = 0; Lod < Data->LODResources.Num(); ++Lod)
        {
            const auto& Resource = Data->LODResources[Lod];
            const auto& Buffer = Resource.VertexBuffers.StaticMeshVertexBuffer;
            const uint32 Count = Buffer.GetNumVertices();
            // GetAllowCPUAccess is a resource-data retention policy, distinct
            // from Mesh.bAllowCPUAccess. Require a real resident tangent pointer
            // as well; never set either policy or rebuild to manufacture access.
            const bool CpuPolicy = Buffer.GetAllowCPUAccess();
            const bool CpuAvailable = CpuPolicy && Buffer.GetTangentData() != nullptr && Count > 0;
            if (!CpuAvailable)
            {
                Errors.Add(ReferenceStatsJsonString(FString::Printf(TEXT("LOD%d CPU tangent data unavailable"), Lod)));
                Lods.Add(FString::Printf(TEXT("{\"lod\":%d,\"vertex_count\":%u,\"cpu_tangent_data_available\":false,\"cpu_access_policy\":%s}"),
                    Lod, Count, CpuPolicy ? TEXT("true") : TEXT("false")));
                continue;
            }
            FReferenceTangentBasisStats Stats;
            TArray<FString> Examples;
            uint32 InvalidCount = 0;
            const auto& Positions = Resource.VertexBuffers.PositionVertexBuffer;
            const bool PositionAvailable = Positions.GetAllowCPUAccess() && Positions.GetVertexData() && Positions.GetNumVertices() == Count;
            for (uint32 Vertex = 0; Vertex < Count; ++Vertex)
            {
                const FVector3f X(Buffer.VertexTangentX(Vertex));
                const FVector3f Y(Buffer.VertexTangentY(Vertex));
                const FVector3f Z(Buffer.VertexTangentZ(Vertex));
                if (Stats.Add(X, Y, Z))
                {
                    ++InvalidCount;
                    if (Examples.Num() < 32)
                    {
                        FString Position = TEXT("null");
                        if (PositionAvailable)
                        {
                            const FVector3f P = Positions.VertexPosition(Vertex);
                            if (ReferenceStatsFinite(P))
                                Position = FString::Printf(TEXT("[%.9g,%.9g,%.9g]"), double(P.X), double(P.Y), double(P.Z));
                        }
                        Examples.Add(FString::Printf(TEXT("{\"vertex\":%u,\"position_cm\":%s}"), Vertex, *Position));
                    }
                }
            }
            Lods.Add(FString::Printf(TEXT("{\"lod\":%d,\"vertex_count\":%u,\"cpu_tangent_data_available\":true,\"cpu_access_policy\":%s,\"high_precision_tangent_basis\":%s,\"full_precision_uvs\":%s,\"invalid_or_short_vertex_count\":%u,\"invalid_examples_limit\":32,\"invalid_examples\":[%s],%s}"),
                Lod, Count, CpuPolicy ? TEXT("true") : TEXT("false"), Buffer.GetUseHighPrecisionTangentBasis() ? TEXT("true") : TEXT("false"),
                Buffer.GetUseFullPrecisionUVs() ? TEXT("true") : TEXT("false"), InvalidCount, *FString::Join(Examples, TEXT(",")), *Stats.Json()));
        }
    }
    for (int32 Lod = 0; Lod < Mesh->GetNumSourceModels(); ++Lod)
    {
        const auto& Build = Mesh->GetSourceModel(Lod).BuildSettings;
        const FString Settings = FString::Printf(TEXT("{\"recompute_normals\":%s,\"recompute_tangents\":%s,\"use_mikk_t_space\":%s,\"full_precision_uvs\":%s,\"high_precision_tangent_basis\":%s}"),
            Build.bRecomputeNormals ? TEXT("true") : TEXT("false"), Build.bRecomputeTangents ? TEXT("true") : TEXT("false"),
            Build.bUseMikkTSpace ? TEXT("true") : TEXT("false"), Build.bUseFullPrecisionUVs ? TEXT("true") : TEXT("false"),
            Build.bUseHighPrecisionTangentBasis ? TEXT("true") : TEXT("false"));
        const FMeshDescription* Description = Mesh->GetMeshDescription(Lod);
        if (!Description)
        {
            Sources.Add(FString::Printf(TEXT("{\"lod\":%d,\"available\":false,\"error\":\"Source MeshDescription absent; optional input diagnostic\",\"build_settings\":%s}"), Lod, *Settings));
            continue;
        }
        const FStaticMeshConstAttributes Attributes(*Description);
        const auto Tangents = Attributes.GetVertexInstanceTangents();
        const auto Normals = Attributes.GetVertexInstanceNormals();
        const auto Signs = Attributes.GetVertexInstanceBinormalSigns();
        if (!Tangents.IsValid() || !Normals.IsValid() || !Signs.IsValid())
        {
            Sources.Add(FString::Printf(TEXT("{\"lod\":%d,\"available\":false,\"error\":\"Source basis attributes absent; optional input diagnostic\",\"build_settings\":%s}"), Lod, *Settings));
            continue;
        }
        FReferenceTangentBasisStats Stats;
        for (const FVertexInstanceID ID : Description->VertexInstances().GetElementIDs())
        {
            const FVector3f X = Tangents[ID], Z = Normals[ID];
            const FVector3f Y = FVector3f::CrossProduct(Z, X)*Signs[ID];
            Stats.Add(X, Y, Z);
        }
        Sources.Add(FString::Printf(TEXT("{\"lod\":%d,\"available\":true,\"vertex_instance_count\":%d,\"binormal_is_derived\":true,\"source_input_is_not_final_render_basis\":true,\"build_settings\":%s,%s}"),
            Lod, Description->VertexInstances().Num(), *Settings, *Stats.Json()));
    }
    return FString::Printf(TEXT("{\"schema\":923,\"status\":\"%s\",\"mesh\":%s,\"zero_threshold\":%.9g,\"asset_compilation_finished\":true,\"read_only\":true,\"nanite_enabled\":%s,\"nanite_internal_inspected\":false,\"scope\":\"Final fallback render vertex buffer, decoded packed X/Z and derived Y; optional source inputs reported separately. No Nanite internal tangent inspection or visual PASS.\",\"lods\":[%s],\"source_mesh_descriptions\":[%s],\"errors\":[%s]}"),
        Errors.IsEmpty() ? TEXT("DIAGNOSTIC_COMPLETE") : TEXT("ERROR"), *ReferenceStatsJsonString(Mesh->GetPathName()), ReferenceTangentZeroThreshold,
        Mesh->GetNaniteSettings().bEnabled ? TEXT("true") : TEXT("false"), *FString::Join(Lods, TEXT(",")), *FString::Join(Sources, TEXT(",")), *FString::Join(Errors, TEXT(",")));
}

bool UReferenceIslandLibrary::ExportReferenceStaticMeshMikkInputs(UStaticMesh* Mesh,const FString& Filename)
{
    if(!GEditor || !Mesh || !Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/")))return false;
    FString Output=FPaths::ConvertRelativePathToFull(Filename);FPaths::NormalizeFilename(Output);
    FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade"));FPaths::NormalizeFilename(Allowed);
    if(!FPaths::IsUnderDirectory(Output,Allowed) || FPaths::FileExists(Output))return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FMeshDescription* D=Mesh->GetMeshDescription(0);if(!D)return false;
    const FStaticMeshConstAttributes A(*D);const auto P=A.GetVertexPositions();const auto N=A.GetVertexInstanceNormals();const auto UV=A.GetVertexInstanceUVs();
    if(!P.IsValid() || !N.IsValid() || !UV.IsValid() || UV.GetNumChannels()<1)return false;
    struct FCorner {uint32 Point,Loop;float Position[3],Normal[3],Texcoord[2];};
    struct FTriangle {uint32 Face,Fan;FCorner Corners[3];};static_assert(sizeof(FTriangle)==128);
    TArray<uint8> Bytes;const uint32 Count=D->Triangles().Num();Bytes.Append(reinterpret_cast<const uint8*>(&Count),sizeof(Count));
    for(const FTriangleID Triangle:D->Triangles().GetElementIDs())
    {
        FTriangle Row{};Row.Face=Triangle.GetValue();Row.Fan=1;const auto Corners=D->GetTriangleVertexInstances(Triangle);
        if(Corners.Num()!=3)return false;
        for(int32 I=0;I<3;++I)
        {
            const FVertexInstanceID Instance=Corners[I];const FVertexID Vertex=D->GetVertexInstanceVertex(Instance);
            const FVector3f Position=P[Vertex],Normal=N[Instance];const FVector2f Texcoord=UV.Get(Instance,0);
            Row.Corners[I].Point=Vertex.GetValue();Row.Corners[I].Loop=Instance.GetValue();
            for(int32 K=0;K<3;++K){Row.Corners[I].Position[K]=Position[K];Row.Corners[I].Normal[K]=Normal[K];}
            Row.Corners[I].Texcoord[0]=Texcoord.X;Row.Corners[I].Texcoord[1]=Texcoord.Y;
        }
        Bytes.Append(reinterpret_cast<const uint8*>(&Row),sizeof(Row));
    }
    return Bytes.Num()==4+int64(Count)*128 && FFileHelper::SaveArrayToFile(Bytes,*Output);
}

FString UReferenceIslandLibrary::GetReferenceRayTracingProxyInventory(UStaticMesh* Mesh)
{
    if(!GEditor || !Mesh || !Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/")))return FString();
    FAssetCompilingManager::Get().FinishAllCompilation();const auto* Data=Mesh->GetRenderData();if(!Data)return FString();
    FString Result=FString::Printf(TEXT("ray_proxy_settings enabled%d target%d percent%.6f relative_error%.6f\n"),Mesh->GetRayTracingProxySettings().bEnabled,int32(Mesh->GetRayTracingProxySettings().FallbackTarget),Mesh->GetRayTracingProxySettings().FallbackPercentTriangles,Mesh->GetRayTracingProxySettings().FallbackRelativeError);
    auto Describe=[&](const FString& Name,const FPositionVertexBuffer& Positions,const FRawStaticIndexBuffer& Indices)
    {
        const int32 Count=Positions.GetNumVertices();
        const uint64 RequiredIndexBytes=uint64(Indices.GetNumIndices())*(Indices.Is32Bit()?4:2);
        if(!Positions.GetVertexData() || uint64(Indices.GetIndexDataSize())<RequiredIndexBytes)
        {Result+=Name+TEXT(" CPU_DATA_UNAVAILABLE\n");return;}
        FBox Bounds(ForceInit);double Areas[6]={};
        for(int32 I=0;I<Count;++I)Bounds+=FVector(Positions.VertexPosition(I));
        struct FTri{FVector A,B,C;};TArray<FTri> Horizontal;
        for(int32 I=0;I+2<int32(Indices.GetNumIndices());I+=3)
        {
            const uint32 IA=Indices.GetIndex(I),IB=Indices.GetIndex(I+1),IC=Indices.GetIndex(I+2);if(IA>=uint32(Count) || IB>=uint32(Count) || IC>=uint32(Count)){Result+=TEXT("INVALID_INDEX\n");return;}
            const FVector A(Positions.VertexPosition(IA)),B(Positions.VertexPosition(IB)),C(Positions.VertexPosition(IC)),N=FVector::CrossProduct(B-A,C-A);
            int32 Axis=0;for(int32 K=1;K<3;++K)if(FMath::Abs(N[K])>FMath::Abs(N[Axis]))Axis=K;Areas[Axis*2+(N[Axis]<0?1:0)]+=N.Length()*.5;
            if(FMath::Abs(N.Z)>.99*N.Length() && FMath::Min3(A.Z,B.Z,C.Z)>250 && FMath::Max3(A.Z,B.Z,C.Z)<330)Horizontal.Add({A,B,C});
        }
        int32 GarageHits=0,GarageRays=0;
        for(double X=-4250;X<=-2450;X+=100)for(double Y=-1550;Y<=1950;Y+=100)
        {
            ++GarageRays;
            for(const auto& T:Horizontal)
            {
                const FVector U=T.B-T.A,V=T.C-T.A;const double D=U.X*V.Y-U.Y*V.X;if(FMath::Abs(D)<1.e-8)continue;
                const double PX=X-T.A.X,PY=Y-T.A.Y,S=(PX*V.Y-PY*V.X)/D,Q=(U.X*PY-U.Y*PX)/D;
                if(S>=-1.e-7 && Q>=-1.e-7 && S+Q<=1+1.e-7){++GarageHits;break;}
            }
        }
        Result+=FString::Printf(TEXT("%s vertices%d triangles%u bounds%s / %s area_cm2[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f] garage_ceiling_hits%d/%d\n"),*Name,Count,Indices.GetNumIndices()/3,*Bounds.Min.ToString(),*Bounds.Max.ToString(),Areas[0],Areas[1],Areas[2],Areas[3],Areas[4],Areas[5],GarageHits,GarageRays);
    };
    for(int32 I=0;I<Data->LODResources.Num();++I)Describe(FString::Printf(TEXT("render_LOD%d"),I),Data->LODResources[I].VertexBuffers.PositionVertexBuffer,Data->LODResources[I].IndexBuffer);
    if(const auto* Proxy=Data->RayTracingProxy)
    {
        Result+=FString::Printf(TEXT("proxy_uses_render_lods%d\n"),Proxy->bUsingRenderingLODs);
        for(int32 I=0;I<Proxy->LODs.Num();++I)if(Proxy->LODs[I].VertexBuffers && Proxy->LODs[I].IndexBuffer)Describe(FString::Printf(TEXT("ray_proxy_LOD%d"),I),Proxy->LODs[I].VertexBuffers->PositionVertexBuffer,*Proxy->LODs[I].IndexBuffer);
    }
    else Result+=TEXT("RAY_PROXY_ABSENT\n");
    return Result;
}

FString UReferenceIslandLibrary::GetReferenceStaticMeshFallbackUnionDigest(const TArray<UStaticMesh*>& Meshes,bool ExportDiagnostic)
{
    if (!GEditor || Meshes.IsEmpty()) return FString();
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<FString> Triangles;
    for(UStaticMesh* Mesh:Meshes)
    {
    if(!Mesh)return FString();
    const FStaticMeshRenderData* Data=Mesh->GetRenderData();
    if (!Data || Data->LODResources.IsEmpty()) return FString();
    for (int32 LOD=0;LOD<Data->LODResources.Num();++LOD)
    {
        const auto& Resource=Data->LODResources[LOD];
        const auto& Positions=Resource.VertexBuffers.PositionVertexBuffer;
        const auto Indices=Resource.IndexBuffer.GetArrayView();
        if (!Positions.GetNumVertices() || !Indices.Num()) return FString();
        for (const auto& Section:Resource.Sections)
        {
            if (uint64(Section.FirstIndex)+uint64(Section.NumTriangles)*3>uint64(Indices.Num())) return FString();
            for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
            {
                FString Points[3];
                for (int32 Corner=0;Corner<3;++Corner)
                {
                    const uint32 Vertex=Indices[Section.FirstIndex+Triangle*3+Corner];
                    if (Vertex>=Positions.GetNumVertices()) return FString();
                    const FVector3f P=Positions.VertexPosition(Vertex);
                    if (P.ContainsNaN()) return FString();
                    Points[Corner]=FString::Printf(TEXT("%.5f,%.5f,%.5f"),P.X,P.Y,P.Z);
                    const auto& Attributes=Resource.VertexBuffers.StaticMeshVertexBuffer;
                    for (const FVector3f Direction:{FVector3f(Attributes.VertexTangentX(Vertex)),Attributes.VertexTangentY(Vertex),FVector3f(Attributes.VertexTangentZ(Vertex))})
                        Points[Corner]+=FString::Printf(TEXT("/%.7f,%.7f,%.7f"),Direction.X,Direction.Y,Direction.Z);
                    for (uint32 UV=0;UV<Attributes.GetNumTexCoords();++UV)
                    {const FVector2f T=Attributes.GetVertexUV(Vertex,UV);Points[Corner]+=FString::Printf(TEXT("/%.7f,%.7f"),T.X,T.Y);}
                    const auto& Colors=Resource.VertexBuffers.ColorVertexBuffer;
                    Points[Corner]+=Colors.GetNumVertices()?FString::Printf(TEXT("/%u"),Colors.VertexColor(Vertex).DWColor()):TEXT("/NoVertexColors");
                    Points[Corner].ReplaceInline(TEXT("-0.0000000"),TEXT("0.0000000"),ESearchCase::CaseSensitive);
                }
                // Canonical cyclic rotation preserves face winding while
                // tolerating harmless vertex/triangle reordering by a rebuild.
                int32 First=0;for(int32 I=1;I<3;++I)if(Points[I]<Points[First])First=I;
                Triangles.Add(FString::Printf(TEXT("%d/%d/%d%d%d%d/%s/%s/%s"),LOD,Section.MaterialIndex,Section.bEnableCollision,Section.bCastShadow,Section.bVisibleInRayTracing,Section.bAffectDistanceFieldLighting,*Points[First],*Points[(First+1)%3],*Points[(First+2)%3]));
            }
        }
    }
    }
    Triangles.Sort();const FString Text=FString::Join(Triangles,TEXT("\n"));const FString Hash=FMD5::HashAnsiString(*Text);
    if(ExportDiagnostic)
    {
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/GaragePartitionAttributes");IFileManager::Get().MakeDirectory(*Directory,true);
        const FString File=Directory/(Meshes[0]->GetName()+TEXT("-")+Hash+TEXT(".txt"));
        if(!IFileManager::Get().FileExists(*File) && !FFileHelper::SaveStringToFile(Text,*File))return FString();
    }
    return FString::Printf(TEXT("%d:%s"),Triangles.Num(),*Hash);
}

int32 UReferenceIslandLibrary::RetainReferenceMeshOrientation(UStaticMesh* Candidate,int32 Direction)
{
    if(!GEditor || !Candidate || Direction<0 || Direction>5 || !Candidate->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Garage888/")) || Candidate->GetNumSourceModels()!=1)return -1;
    FAssetCompilingManager::Get().FinishAllCompilation();
    FMeshDescription* Description=Candidate->GetMeshDescription(0);if(!Description)return -1;
    TMap<int32,FMeshSectionInfo> OriginalByMaterial;
    const auto& OriginalSections=Candidate->GetRenderData()->LODResources[0].Sections;
    for(int32 I=0;I<OriginalSections.Num();++I)
    {
        FMeshSectionInfo Info=Candidate->GetSectionInfoMap().Get(0,I);Info.MaterialIndex=OriginalSections[I].MaterialIndex;
        if(const auto* Previous=OriginalByMaterial.Find(Info.MaterialIndex);Previous &&
           (Previous->bEnableCollision!=Info.bEnableCollision || Previous->bCastShadow!=Info.bCastShadow ||
            Previous->bVisibleInRayTracing!=Info.bVisibleInRayTracing || Previous->bAffectDistanceFieldLighting!=Info.bAffectDistanceFieldLighting || Previous->bForceOpaque!=Info.bForceOpaque))return -1;
        OriginalByMaterial.Add(Info.MaterialIndex,Info);
    }
    FStaticMeshAttributes Attributes(*Description);auto Positions=Attributes.GetVertexPositions();
    TArray<FPolygonID> Remove;
    for(const FPolygonID Polygon:Description->Polygons().GetElementIDs())
    {
        const auto Triangles=Description->GetPolygonTriangles(Polygon);if(Triangles.IsEmpty())return -1;
        const auto Vertices=Description->GetTriangleVertices(Triangles[0]);
        const FVector3f N=FVector3f::CrossProduct(Positions[Vertices[1]]-Positions[Vertices[0]],Positions[Vertices[2]]-Positions[Vertices[0]]);
        int32 Axis=0;for(int32 I=1;I<3;++I)if(FMath::Abs(N[I])>FMath::Abs(N[Axis]))Axis=I;
        if(Axis*2+(N[Axis]<0?1:0)!=Direction)Remove.Add(Polygon);
    }
    if(Remove.Num()==Description->Polygons().Num())return 0;
    Description->DeletePolygons(Remove);FElementIDRemappings Remapping;Description->Compact(Remapping);
    // Polygon-group compaction changes section ordering. Bind each remaining
    // imported slot explicitly to its original material and section policy.
    Candidate->GetSectionInfoMap().Clear();Candidate->GetOriginalSectionInfoMap().Clear();
    FStaticMeshAttributes Remaining(*Description);auto SlotNames=Remaining.GetPolygonGroupMaterialSlotNames();
    for(const FPolygonGroupID Group:Description->PolygonGroups().GetElementIDs())
    {
        const FName SlotName=SlotNames[Group];int32 Material=INDEX_NONE;
        for(int32 I=0;I<Candidate->GetStaticMaterials().Num();++I)
        {const auto& Slot=Candidate->GetStaticMaterials()[I];if(Slot.ImportedMaterialSlotName==SlotName || Slot.MaterialSlotName==SlotName){Material=I;break;}}
        const auto* Info=OriginalByMaterial.Find(Material);if(!Info)return -1;
        Candidate->GetSectionInfoMap().Set(0,Group.GetValue(),*Info);Candidate->GetOriginalSectionInfoMap().Set(0,Group.GetValue(),*Info);
    }
    Candidate->CommitMeshDescription(0);Candidate->Build(false);FAssetCompilingManager::Get().FinishAllCompilation();
    return Candidate->GetRenderData() && !Candidate->GetRenderData()->LODResources.IsEmpty()?Candidate->GetRenderData()->LODResources[0].GetNumTriangles():-1;
}

namespace
{
bool CanonicalReferenceNaniteHierarchy(const Nanite::FResources& Resources,TArray<Nanite::FPackedHierarchyNode>& Hierarchy)
{
    using FFixup=Nanite::FFixupChunk;
    if (!Resources.NumRootPages || Resources.NumRootPages>uint32(Resources.PageStreamingStates.Num()) || Resources.HierarchyNodes.IsEmpty())return false;
    Hierarchy=Resources.HierarchyNodes;
    TMap<uint64,uint32> CanonicalReferences;
    const uint64 RootSize=Resources.RootData.Num();
    for(uint32 Page=0;Page<Resources.NumRootPages;++Page)
    {
        const auto& State=Resources.PageStreamingStates[Page];
        if(uint64(State.BulkOffset)+State.BulkSize>RootSize || State.BulkSize<sizeof(FFixup::FHeader))return false;
        const uint8* Data=Resources.RootData.GetData()+State.BulkOffset;
        FFixup::FHeader Header;FMemory::Memcpy(&Header,Data,sizeof(Header));
        // Bound every count in uint64 before interpreting the typed fixup
        // records. Memcpy also supports unaligned page/chunk offsets safely.
        const uint64 GroupsOffset=sizeof(Header);
        const uint64 PartsOffset=GroupsOffset+uint64(Header.NumGroupFixups)*sizeof(FFixup::FGroupFixup);
        const uint64 ParentsOffset=PartsOffset+uint64(Header.NumPartFixups)*sizeof(FFixup::FPartFixup);
        const uint64 LocationsOffset=ParentsOffset+uint64(Header.NumParentFixups)*sizeof(FFixup::FParentFixup);
        const uint64 ChunkSize=LocationsOffset+uint64(Header.NumHierarchyLocations)*sizeof(FFixup::FHierarchyLocation)
            +uint64(Header.NumReconsiderPages)*sizeof(uint16)+uint64(Header.NumClusterIndices)*sizeof(uint8);
        if(Header.Magic!=NANITE_FIXUP_MAGIC || ChunkSize>State.BulkSize || State.BulkSize-ChunkSize!=State.PageSize)return false;
        for(uint32 GroupIndex=0;GroupIndex<Header.NumGroupFixups;++GroupIndex)
        {
            FFixup::FGroupFixup Group;
            FMemory::Memcpy(&Group,Data+GroupsOffset+uint64(GroupIndex)*sizeof(Group),sizeof(Group));
            if(uint64(Group.FirstPartFixup)+Group.NumPartFixups>Header.NumPartFixups ||
               uint64(Group.FirstParentFixup)+Group.NumParentFixups>Header.NumParentFixups)return false;
            // Match ProcessNewResources exactly: only these root-only groups
            // have CPU hierarchy addresses rewritten by the streaming manager.
            if(Group.PageDependencies.HasStreamingPages())continue;
            for(uint32 PartIndex=Group.FirstPartFixup;PartIndex<uint32(Group.FirstPartFixup)+Group.NumPartFixups;++PartIndex)
            {
                FFixup::FPartFixup Part;
                FMemory::Memcpy(&Part,Data+PartsOffset+uint64(PartIndex)*sizeof(Part),sizeof(Part));
                if(Part.PageIndex>=Resources.NumRootPages || Part.StartClusterIndex>=NANITE_MAX_CLUSTERS_PER_PAGE ||
                   uint64(Part.FirstHierarchyLocation)+Part.NumHierarchyLocations>Header.NumHierarchyLocations)return false;
                const uint32 Canonical=(uint32(Part.PageIndex)<<NANITE_MAX_CLUSTERS_PER_PAGE_BITS)|Part.StartClusterIndex;
                for(uint32 LocationIndex=0;LocationIndex<Part.NumHierarchyLocations;++LocationIndex)
                {
                    FFixup::FHierarchyLocation Location(0,0);
                    FMemory::Memcpy(&Location,Data+LocationsOffset+(uint64(Part.FirstHierarchyLocation)+LocationIndex)*sizeof(Location),sizeof(Location));
                    const uint32 Node=Location.GetNodeIndex(),Child=Location.GetChildIndex();
                    if(Node>=uint32(Hierarchy.Num()) || Child>=NANITE_MAX_BVH_NODE_FANOUT)return false;
                    const uint64 Key=uint64(Node)*NANITE_MAX_BVH_NODE_FANOUT+Child;
                    if(const uint32* Previous=CanonicalReferences.Find(Key))
                    {if(*Previous!=Canonical)return false;continue;}
                    auto& Reference=Hierarchy[Node].Misc1[Child].ChildStartReference;
                    // The encoder stores MAX_uint32 before registration. Once
                    // patched, all page addresses must share one allocation
                    // base and retain the immutable local page/cluster mapping.
                    if(Reference!=MAX_uint32)
                    {
                        const uint32 GPUPage=Reference>>NANITE_MAX_CLUSTERS_PER_PAGE_BITS;
                        if((Reference&NANITE_MAX_CLUSTERS_PER_PAGE_MASK)!=Part.StartClusterIndex ||
                           GPUPage<Part.PageIndex || Resources.RootPageIndex<0)return false;
                        const uint64 ExpectedGPUPage=uint64(Nanite::GStreamingManager.GetMaxStreamingPages())
                            +uint32(Resources.RootPageIndex)+Part.PageIndex;
                        if(uint64(GPUPage)!=ExpectedGPUPage)return false;
                    }
                    CanonicalReferences.Add(Key,Canonical);Reference=Canonical;
                }
            }
        }
    }
    return true;
}
}

FString UReferenceIslandLibrary::GetReferenceTexturePlatformStats(UTexture2D* Texture, bool bFinishCompilation)
{
    if (!GEditor || !IsInGameThread())
    {
        return TEXT("{\"status\":\"INVALID_EDITOR_CONTEXT\",\"package_saved\":false}");
    }
    if (!Texture)
    {
        return TEXT("{\"status\":\"INVALID_TEXTURE\",\"package_saved\":false}");
    }
    const auto ReadStats = [Texture]() -> FString
    {
        const bool bCompiling = Texture->IsCompiling();
        const bool bManagerCompiling = FTextureCompilingManager::Get().IsCompilingTexture(Texture);
        // GetPlatformData waits on pending work. The read-only before snapshot
        // must preserve that state instead of silently finishing compilation.
        const bool bSkipPlatform = bCompiling || bManagerCompiling;
        const FTexturePlatformData* Platform = bSkipPlatform ? nullptr : Texture->GetPlatformData();
        FString PlatformJson = TEXT("null");
        if (Platform)
        {
            const int32 Format = static_cast<int32>(Platform->PixelFormat);
            const FString FormatName = Format >= 0 && Format < PF_MAX ?
                FString(GPixelFormats[Format].Name) : TEXT("INVALID_PIXEL_FORMAT");
            const FString FirstMip = Platform->Mips.Num() > 0 ? FString::Printf(
                TEXT("{\"size_x\":%d,\"size_y\":%d}"), Platform->Mips[0].SizeX, Platform->Mips[0].SizeY) : TEXT("null");
            PlatformJson = FString::Printf(
                TEXT("{\"size_x\":%d,\"size_y\":%d,\"mips\":%d,\"pixel_format\":%d,\"pixel_format_name\":%s,\"first_mip\":%s}"),
                Platform->SizeX, Platform->SizeY, Platform->Mips.Num(), Format,
                *ReferenceStatsJsonString(FormatName), *FirstMip);
        }
        const FIntPoint Logical = Texture->Source.GetLogicalSize();
        return FString::Printf(
            TEXT("{\"is_compiling\":%s,\"manager_is_compiling\":%s,\"source\":{\"size_x\":%lld,\"size_y\":%lld,\"logical_size_x\":%d,\"logical_size_y\":%d,\"mips\":%d,\"layers\":%d,\"format\":%d},\"accessor_size_x\":%d,\"accessor_size_y\":%d,\"accessor_mips\":%d,\"platform_read_skipped_while_compiling\":%s,\"platform\":%s}"),
            bCompiling ? TEXT("true") : TEXT("false"), bManagerCompiling ? TEXT("true") : TEXT("false"),
            static_cast<long long>(Texture->Source.GetSizeX()), static_cast<long long>(Texture->Source.GetSizeY()),
            Logical.X, Logical.Y, Texture->Source.GetNumMips(), Texture->Source.GetNumLayers(),
            static_cast<int32>(Texture->Source.GetFormat()), Texture->GetSizeX(), Texture->GetSizeY(),
            Texture->GetNumMips(), bSkipPlatform ? TEXT("true") : TEXT("false"), *PlatformJson);
    };
    const FString Before = ReadStats();
    const double Start = FPlatformTime::Seconds();
    if (bFinishCompilation)
    {
        TArray<UTexture*> OneTexture;
        OneTexture.Add(Texture);
        FTextureCompilingManager::Get().FinishCompilation(OneTexture);
        // Complete the final edited data texture and its transient resource
        // synchronously, as recommended by UTexture's resource-update API.
        // A save can leave an unregistered proxy from an earlier property edit.
        Texture->UpdateResourceWithParams(UTexture::EUpdateResourceFlags::Synchronous);
        Texture->BlockOnAnyAsyncBuild();
        FlushRenderingCommands();
    }
    const FString After = ReadStats();
    return FString::Printf(
        TEXT("{\"status\":\"DIAGNOSTIC_COMPLETE\",\"texture\":%s,\"finish_requested\":%s,\"finish_scope_texture_count\":%d,\"finish_seconds\":%.9f,\"before\":%s,\"after\":%s,\"settings_modified\":false,\"package_saved\":false}"),
        *ReferenceStatsJsonString(Texture->GetPathName()), bFinishCompilation ? TEXT("true") : TEXT("false"),
        bFinishCompilation ? 1 : 0, FPlatformTime::Seconds() - Start, *Before, *After);
}

FString UReferenceIslandLibrary::RebuildReferenceGround32Texture(UTexture2D* Texture)
{
    if (!GEditor || !IsInGameThread() || !Texture)
        return TEXT("{\"status\":\"INVALID_EDITOR_TEXTURE_CONTEXT\"}");
    const FString Base=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_Ground32");
    const FString Broad=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_Ground34");
    const FString Path=Texture->GetPathName();
    const bool Height=Path==Base+TEXT("Heights.T_C923_Ground32Heights") || Path==Broad+TEXT("Heights.T_C923_Ground34Heights");
    const bool Mask=Path==Base+TEXT("Diagonals.T_C923_Ground32Diagonals") || Path==Base+TEXT("Density.T_C923_Ground32Density")
        || Path==Broad+TEXT("Diagonals.T_C923_Ground34Diagonals") || Path==Broad+TEXT("Density.T_C923_Ground34Density");
    if (!(Height || Mask) || !Texture->Source.IsValid() || Texture->SRGB || Texture->VirtualTextureStreaming
        || !Texture->NeverStream || Texture->MipGenSettings!=TMGS_NoMipmaps || Texture->Filter!=TF_Nearest
        || Texture->CompressionSettings!=(Height?TC_VectorDisplacementmap:TC_Grayscale)
        || Texture->Source.GetFormat()!=(Height?TSF_BGRA8:TSF_G8))
        return TEXT("{\"status\":\"REJECTED_OWNED_DATA_TEXTURE_POLICY\"}");
    const FGuid SourceId=Texture->Source.GetId();
    const FString Before=GetReferenceTexturePlatformStats(Texture,true);
    // Texture.h recommends these flags instead of directly calling
    // ForceRebuildPlatformData/FinishCachePlatformData on compiler proxies.
    const auto Flags=static_cast<UTexture::EUpdateResourceFlags>(
        static_cast<uint32>(UTexture::EUpdateResourceFlags::Synchronous) | static_cast<uint32>(UTexture::EUpdateResourceFlags::ForceRebuild));
    Texture->UpdateResourceWithParams(Flags);
    Texture->BlockOnAnyAsyncBuild();
    TArray<UTexture*> OneTexture;OneTexture.Add(Texture);
    FTextureCompilingManager::Get().FinishCompilation(OneTexture);
    FlushRenderingCommands();
    const FString After=GetReferenceTexturePlatformStats(Texture,true);
    if (Texture->Source.GetId()!=SourceId)
        return TEXT("{\"status\":\"UNEXPECTED_SOURCE_ID_CHANGE\"}");
    return FString::Printf(TEXT("{\"status\":\"OWNED_GROUND32_DERIVED_PAYLOAD_REBUILT\",\"before\":%s,\"after\":%s,\"source_settings_modified\":false,\"package_saved\":false}"),*Before,*After);
}

FString UReferenceIslandLibrary::GetReferenceStaticMeshSourceBounds(UStaticMesh* Mesh)
{
    if(!Mesh)return FString();
    const FMeshDescription* Description=Mesh->GetMeshDescription(0);
    if(!Description || Description->Vertices().Num()==0)return FString();
    const TVertexAttributesConstRef<FVector3f> Positions=FStaticMeshConstAttributes(*Description).GetVertexPositions();
    FBox Bounds(ForceInit);
    for(const FVertexID ID:Description->Vertices().GetElementIDs())
    {
        const FVector Position(Positions[ID]);
        if(Position.ContainsNaN())return FString();
        Bounds+=Position;
    }
    return FString::Printf(TEXT("{\"vertices\":%d,\"minimum_cm\":[%.9f,%.9f,%.9f],\"maximum_cm\":[%.9f,%.9f,%.9f]}"),
        Description->Vertices().Num(),Bounds.Min.X,Bounds.Min.Y,Bounds.Min.Z,Bounds.Max.X,Bounds.Max.Y,Bounds.Max.Z);
}

FString UReferenceIslandLibrary::GetReferenceStaticMeshGeometryDigest(UStaticMesh* Mesh)
{
    const FString Fallback=GetReferenceStaticMeshFallbackUnionDigest({Mesh});if(Fallback.IsEmpty())return FString();
    FlushRenderingCommands();
    auto* Resources=Mesh->GetRenderData()->NaniteResourcesPtr.Get();
    if (!Resources || Resources->RootData.IsEmpty()) return Fallback+TEXT(":NONE");
    bool Failed=false;const double Deadline=FPlatformTime::Seconds()+30;
    while (!Resources->RebuildBulkDataFromCacheAsync(Mesh,Failed))
    {if(FPlatformTime::Seconds()>Deadline)return FString();FPlatformProcess::Sleep(.01f);}
    if(Failed)return FString();
    // Nanite patches only root-fixup-addressed ChildStartReference fields with
    // per-resource GPU page addresses. Hash their immutable local references
    // in a copy, retaining every other hierarchy byte and all original pages.
    TArray<Nanite::FPackedHierarchyNode> Hierarchy;
    if(!CanonicalReferenceNaniteHierarchy(*Resources,Hierarchy))return FString();
    FMD5 Nanite;
    auto Hash=[&](const void* Bytes,uint64 Size){if(Size)Nanite.Update(static_cast<const uint8*>(Bytes),Size);};
    Hash(Resources->RootData.GetData(),Resources->RootData.Num());
    const int64 BulkSize=Resources->StreamablePages.GetBulkDataSize();
    if(BulkSize>0)
    {const void* Bulk=Resources->StreamablePages.LockReadOnly();if(!Bulk){Resources->StreamablePages.Unlock();return FString();}Hash(Bulk,BulkSize);Resources->StreamablePages.Unlock();}
    else if(Resources->HasStreamingData())return FString();
    Hash(Hierarchy.GetData(),Hierarchy.Num()*sizeof(Nanite::FPackedHierarchyNode));
    Hash(Resources->HierarchyRootOffsets.GetData(),Resources->HierarchyRootOffsets.Num()*sizeof(uint32));
    Hash(Resources->PageStreamingStates.GetData(),Resources->PageStreamingStates.Num()*sizeof(Nanite::FPageStreamingState));
    Hash(Resources->PageDependencies.GetData(),Resources->PageDependencies.Num()*sizeof(uint16));
    Hash(Resources->AssemblyTransforms.GetData(),Resources->AssemblyTransforms.Num()*sizeof(FMatrix3x4));
    Hash(Resources->AssemblyBoneAttachmentData.GetData(),Resources->AssemblyBoneAttachmentData.Num()*sizeof(uint32));
    Hash(Resources->BoneIndices.GetData(),Resources->BoneIndices.Num()*sizeof(uint32));
    Hash(Resources->PageRangeLookup.GetData(),Resources->PageRangeLookup.Num()*sizeof(Nanite::FPageRangeKey));
    if(Resources->MeshBounds.ContainsNaN())return FString();
    Hash(&Resources->MeshBounds.Origin,sizeof(Resources->MeshBounds.Origin));
    Hash(&Resources->MeshBounds.BoxExtent,sizeof(Resources->MeshBounds.BoxExtent));
    Hash(&Resources->MeshBounds.SphereRadius,sizeof(Resources->MeshBounds.SphereRadius));
    Hash(&Resources->VoxelMaterialsMask,sizeof(Resources->VoxelMaterialsMask));
    Hash(&Resources->NumInputCurves,sizeof(Resources->NumInputCurves));
    const FString Precision=FString::Printf(TEXT("%d/%d/%d/%u/%u/%u/%u"),Resources->PositionPrecision,Resources->NormalPrecision,Resources->TangentPrecision,Resources->NumInputTriangles,Resources->NumInputVertices,Resources->NumClusters,Resources->NumRootPages);
    FTCHARToUTF8 PrecisionBytes(*Precision);Hash(PrecisionBytes.Get(),PrecisionBytes.Length());
    uint8 Digest[16];Nanite.Final(Digest);
    return Fallback+TEXT(":")+BytesToHex(Digest,16);
}

bool UReferenceIslandLibrary::CopyReferencePalmBounds(UStaticMesh* Candidate,UStaticMesh* Source)
{
    if(!GEditor || GEditor->PlayWorld || !Candidate || !Source || Candidate==Source)return false;
    const FString CandidatePath=Candidate->GetPathName(),SourcePath=Source->GetPathName();
    const bool Near=CandidatePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_PalmNearWithLeafGaps.SM_C920_PalmNearWithLeafGaps") &&
        SourcePath==TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_PalmNear.SM_NCTW_PalmNear");
    const bool Far=CandidatePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_PalmFarWithLeafGaps.SM_C920_PalmFarWithLeafGaps") &&
        SourcePath==TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_PalmFar.SM_NCTW_PalmFar");
    const bool BarkNear=CandidatePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SM_C923_PalmNearBark23.SM_C923_PalmNearBark23") &&
        SourcePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_PalmNearWithLeafGaps.SM_C920_PalmNearWithLeafGaps");
    const bool BarkFar=CandidatePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SM_C923_PalmFarBark23.SM_C923_PalmFarBark23") &&
        SourcePath==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_PalmFarWithLeafGaps.SM_C920_PalmFarWithLeafGaps");
    if(!Near && !Far && !BarkNear && !BarkFar)return false;
    FAssetCompilingManager::Get().FinishAllCompilation();FlushRenderingCommands();
    const FString SourceDigest=GetReferenceStaticMeshGeometryDigest(Source);
    if(SourceDigest.IsEmpty() || GetReferenceStaticMeshGeometryDigest(Candidate)!=SourceDigest ||
       Candidate->GetPositiveBoundsExtension()!=Source->GetPositiveBoundsExtension() ||
       Candidate->GetNegativeBoundsExtension()!=Source->GetNegativeBoundsExtension())return false;
    const FBoxSphereBounds Bounds=Source->GetExtendedBounds();
    if(Bounds.ContainsNaN() || Bounds.BoxExtent.GetMin()<=0. || Bounds.SphereRadius<=0.)return false;
    // A material PostEditChange can recalculate from RenderData.Bounds instead
    // of the canonical cached mesh-description bounds. Restore the exact
    // authored wind envelope directly; PostEditChange would recalculate it again.
    Candidate->Modify();Candidate->SetExtendedBounds(Bounds);Candidate->MarkPackageDirty();
    const auto& Actual=Candidate->GetExtendedBounds();
    return Actual.Origin==Bounds.Origin && Actual.BoxExtent==Bounds.BoxExtent && Actual.SphereRadius==Bounds.SphereRadius;
}

bool UReferenceIslandLibrary::ApplyAdventureRoadEarthworks(ALandscape* Landscape,const FString& HeightmapPath)
{
 if(!Landscape || !Landscape->GetWorld() || Landscape->GetWorld()->WorldType!=EWorldType::Editor
  || !Landscape->ActorHasTag(TEXT("NeonCity.Tidewater.Island")) || Landscape->LandscapeComponents.Num()!=64)return false;
 const FString Expected=FPaths::ConvertRelativePathToFull(HeightmapPath);
 const FString Road=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/road-landscape-2041.r16"));
 const FString Layby=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/Road/layby-landscape-2041.r16"));
 if(!FPaths::IsSamePath(Expected,Road) && !FPaths::IsSamePath(Expected,Layby))return false;
 auto* Info=Landscape->GetLandscapeInfo();int32 X1,Y1,X2,Y2;
 if(!Info || !Info->GetLandscapeExtent(X1,Y1,X2,Y2) || X1!=0 || Y1!=0 || X2!=2040 || Y2!=2040)return false;
 TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Expected) || Bytes.Num()!=2041*2041*2)return false;
 TArray<uint16> Heights;Heights.SetNumUninitialized(2041*2041);
 for(int32 Y=0;Y<2041;++Y)for(int32 X=0;X<2041;++X)
 {
  const int32 Index=((2040-Y)*2041+X)*2;Heights[Y*2041+X]=uint16(Bytes[Index])|(uint16(Bytes[Index+1])<<8);
 }
 Landscape->Modify();
 {
  // UE 5.8 composes final height textures from persistent edit layers. Writing
  // only the final texture appears to work, then is lost when the level reloads.
  const auto* Base=Landscape->GetEditLayerConst(0);if(!Base)return false;
  FScopedSetLandscapeEditingLayer Scope(Landscape,Base->GetGuid());
  FLandscapeEditDataInterface Edit(Info);Edit.SetEditLayer(Base->GetGuid());
  Edit.SetHeightData(0,0,2040,2040,Heights.GetData(),0,true);Edit.Flush();
 }
 Landscape->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
 Landscape->ForceUpdateLayersContent();
 Landscape->RecreateCollisionComponents();
 Landscape->MarkPackageDirty();return true;
}

bool UReferenceIslandLibrary::SetReferenceCharacterPose(USkeletalMeshComponent* Component, UAnimSequence* Sequence, float Seconds)
{
    if(!Component || !Sequence || !Component->GetWorld() || Component->GetWorld()->WorldType!=EWorldType::Editor
        || !Sequence->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/Characters/"))
        || !FMath::IsFinite(Seconds)) return false;
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetAnimation(Sequence);
    Component->SetPosition(FMath::Clamp(Seconds,0.f,Sequence->GetPlayLength()),false);
    Component->TickAnimation(0.f,false);
    Component->RefreshBoneTransforms();
    Component->UpdateBounds();
    Component->MarkRenderTransformDirty();
    Component->MarkRenderDynamicDataDirty();
    return true;
}

void UReferenceIslandLibrary::FlushReferenceEditorRendering()
{
    if(!GEditor) return;
    if(UWorld* World=GEditor->GetEditorWorldContext().World())
    {
        if(World->WorldType!=EWorldType::Editor) return;
        World->SendAllEndOfFrameUpdates();
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        FlushRenderingCommands();
    }
}

FString UReferenceIslandLibrary::ApplyReferenceLandscapeMaterial(ALandscape* Landscape, UMaterial* Material)
{
    if(!Landscape || !Material || !Landscape->GetWorld() || Landscape->GetWorld()->WorldType!=EWorldType::Editor
        || !Landscape->ActorHasTag(TEXT("NeonCity.Tidewater.Island"))) return TEXT("Not an owned reference landscape");
    // A loaded Landscape can retain component combination instances after a Python
    // property assignment. Recreate these explicitly before compiling/checking them.
    Landscape->LandscapeMaterial=Material;
    Landscape->UpdateAllComponentMaterialInstances(true);
    Landscape->MarkComponentsRenderStateDirty();
    // MaterialUpdateContext can initialize static permutations with None; an
    // unattended authoring script has no following editor frame to request them.
    for(ULandscapeComponent* Component:Landscape->LandscapeComponents)
        if(Component)
            for(UMaterialInstance* Instance:Component->GetGeneratedMaterialInstances())
                if(Instance) Instance->CacheShaders(EMaterialShaderPrecompileMode::Default);
    FlushReferenceEditorRendering();
    const EShaderPlatform Platform=GShaderPlatformForFeatureLevel[Landscape->GetWorld()->GetFeatureLevel()];
    for(ULandscapeComponent* Component:Landscape->LandscapeComponents)
    {
        if(!Component || Component->GetMaterialInstanceCount(false)<1) return TEXT("Missing landscape component material");
        for(int32 Index=0;Index<Component->GetMaterialInstanceCount(false);++Index)
        {
            UMaterialInstance* Instance=Component->GetMaterialInstance(Index,false);
            if(!Instance || Instance->GetMaterial()!=Material) return TEXT("Landscape instance has the wrong parent");
            const FMaterialResource* Resource=Instance->GetMaterialResource(Platform);
            if(!Resource || !Resource->IsGameThreadShaderMapComplete()) return FString::Printf(
                TEXT("Landscape shader map incomplete: %s platform %d resource %s"),*Instance->GetPathName(),int32(Platform),Resource?TEXT("present"):TEXT("missing"));
            if(Resource->GetCompileErrors().Num()) return FString::Join(Resource->GetCompileErrors(),TEXT("\n"));
        }
    }
    Landscape->MarkPackageDirty();
    return FString();
}

FString UReferenceIslandLibrary::GetReferenceMaterialErrors(UMaterial* Material)
{
    if(!Material || !GEditor) return TEXT("Missing editor material");
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if(!World) return TEXT("Missing editor world");
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    const FMaterialResource* Resource=Material->GetMaterialResource(GShaderPlatformForFeatureLevel[World->GetFeatureLevel()]);
    if(!Resource) return TEXT("Material has no compiled resource for the current feature level");
    return FString::Join(Resource->GetCompileErrors(),TEXT("\n"));
}

double UReferenceIslandLibrary::SampleReferenceGroundHeight(ALandscape* Landscape, FVector2D PositionCm)
{
    if(!Landscape || !Landscape->GetWorld() || PositionCm.ContainsNaN()) return -1.e9;
    FHitResult Hit;
    const bool bHit=Landscape->GetWorld()->LineTraceSingleByChannel(Hit,
        FVector(PositionCm.X,PositionCm.Y,40000),FVector(PositionCm.X,PositionCm.Y,-10000),
        ECC_Visibility,FCollisionQueryParams(SCENE_QUERY_STAT(ReferenceIslandCollision),true));
    return bHit && Hit.GetActor()==Landscape ? Hit.ImpactPoint.Z/100.0 : -1.e9;
}

ALandscape* UReferenceIslandLibrary::ImportReferenceLandscape(const FString& HeightmapPath, UMaterialInterface* Material)
{
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(!World || World->WorldType!=EWorldType::Editor || !Material) return nullptr;
    // The authoring script verifies the manifest SHA first. Restrict this bridge to the reviewed export.
    const FString Expected=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/IslandWithVillage/height-landscape-2041.r16"));
    if(!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(HeightmapPath),Expected)) return nullptr;
    for(TActorIterator<ALandscape> It(World);It;++It) return nullptr; // Never overwrite an existing landscape.
    constexpr int32 Resolution=2041;
    TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToArray(Bytes,*Expected) || Bytes.Num()!=Resolution*Resolution*2) return nullptr;
    TArray<uint16> Heights; Heights.SetNumUninitialized(Resolution*Resolution);
    for(int32 Y=0;Y<Resolution;++Y)
        for(int32 X=0;X<Resolution;++X)
        {
            const int32 Source=((Resolution-1-Y)*Resolution+X)*2;
            Heights[Y*Resolution+X]=uint16(Bytes[Source]) | (uint16(Bytes[Source+1])<<8);
        }
    constexpr double SpanCm=204700.0;
    ALandscape* Landscape=World->SpawnActor<ALandscape>(FVector(-SpanCm*.5,-SpanCm*.5,12800),FRotator::ZeroRotator);
    if(!Landscape) return nullptr;
    Landscape->SetActorLabel(TEXT("NeonCity Tidewater Reference Island"));
    Landscape->Tags.Add(TEXT("NeonCity.Tidewater.Island"));
    Landscape->LandscapeMaterial=Material;
    Landscape->SetActorScale3D(FVector(SpanCm/(Resolution-1),SpanCm/(Resolution-1),100));
    Landscape->StaticLightingLOD=1;
    TMap<FGuid,TArray<uint16>> HeightLayers;
    HeightLayers.Add(FGuid(),MoveTemp(Heights));
    TMap<FGuid,TArray<FLandscapeImportLayerInfo>> MaterialLayers;
    MaterialLayers.Add(FGuid(),TArray<FLandscapeImportLayerInfo>());
    Landscape->Import(FGuid::NewGuid(),0,0,Resolution-1,Resolution-1,1,255,HeightLayers,
        *Expected,MaterialLayers,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
    ULandscapeInfo* Info=Landscape->GetLandscapeInfo();
    if(!Info || Landscape->LandscapeComponents.Num()!=64)
    {
        UE_LOG(LogTemp,Error,TEXT("Reference island import did not produce the expected 64 components; level remains unsaved."));
        return nullptr;
    }
    Info->UpdateLayerInfoMap(Landscape);
    Landscape->MarkPackageDirty();
    return Landscape;
}
