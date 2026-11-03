#pragma once
// Opt-in, read-only companion to the existing 923 water snapshot. This header
// does not change render state, compile shaders, read GPU textures, issue water
// queries, move actors, save assets, or change the production ocean/clock.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityMarinaWaterAttribution923.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "StaticMeshResources.h"
#include "Misc/SecureHash.h"
#include "World/NeonCityDayNightCycle.h"

namespace C923MarinaWaterAttribution56
{
using C923MarinaWaterAttribution::Vector;

inline FString Sha1(const void* Data,uint64 Bytes)
{
    const uint8 Empty=0;uint8 Digest[20];
    FSHA1::HashBuffer(Bytes?Data:&Empty,Bytes,Digest);
    return BytesToHex(Digest,20).ToLower();
}

inline TArray<TSharedPtr<FJsonValue>> XY(const FVector2D& V)
{return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y)};}

inline TSharedPtr<FJsonObject> Texture(UTexture* T)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),GetPathNameSafe(T));
    if(!T)return J;
    J->SetStringField(TEXT("class"),T->GetClass()->GetPathName());
    J->SetBoolField(TEXT("render_resource_present"),T->GetResource()!=nullptr);
    J->SetBoolField(TEXT("srgb"),T->SRGB);
    if(auto* D=Cast<UTexture2D>(T))
    {
        J->SetNumberField(TEXT("size_x"),D->GetSizeX());J->SetNumberField(TEXT("size_y"),D->GetSizeY());
        J->SetNumberField(TEXT("mips"),D->GetNumMips());J->SetNumberField(TEXT("pixel_format_enum"),int32(D->GetPixelFormat()));
    }
    if(auto* R=Cast<UTextureRenderTarget2D>(T))
    {J->SetNumberField(TEXT("size_x"),R->SizeX);J->SetNumberField(TEXT("size_y"),R->SizeY);}
    J->SetStringField(TEXT("pixel_content"),TEXT("NOT_READ_BACK; dimensions/resource presence do not verify pixel values"));
    return J;
}

inline TSharedPtr<FJsonObject> Parameters(UMaterialInterface* M)
{
    auto J=MakeShared<FJsonObject>();if(!M)return J;
    J->SetStringField(TEXT("path"),M->GetPathName());J->SetStringField(TEXT("base"),GetPathNameSafe(M->GetMaterial()));
    J->SetBoolField(TEXT("single_layer_water"),M->GetShadingModels().HasShadingModel(MSM_SingleLayerWater));
    TArray<FMaterialParameterInfo> Infos;TArray<FGuid> Ids;
    TArray<TSharedPtr<FJsonValue>> Scalars,Vectors,Textures;
    auto Info=[](const FMaterialParameterInfo& P)
    {auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),P.Name.ToString());R->SetNumberField(TEXT("association"),int32(P.Association));R->SetNumberField(TEXT("index"),P.Index);return R;};
    M->GetAllScalarParameterInfo(Infos,Ids);
    for(const auto& P:Infos){float V=0;auto R=Info(P);const bool Found=M->GetScalarParameterValue(P,V);R->SetBoolField(TEXT("resolved"),Found);if(Found)R->SetNumberField(TEXT("value"),V);Scalars.Add(MakeShared<FJsonValueObject>(R));}
    Infos.Reset();Ids.Reset();M->GetAllVectorParameterInfo(Infos,Ids);
    for(const auto& P:Infos){FLinearColor V;auto R=Info(P);const bool Found=M->GetVectorParameterValue(P,V);R->SetBoolField(TEXT("resolved"),Found);if(Found)R->SetArrayField(TEXT("value"),{MakeShared<FJsonValueNumber>(V.R),MakeShared<FJsonValueNumber>(V.G),MakeShared<FJsonValueNumber>(V.B),MakeShared<FJsonValueNumber>(V.A)});Vectors.Add(MakeShared<FJsonValueObject>(R));}
    Infos.Reset();Ids.Reset();M->GetAllTextureParameterInfo(Infos,Ids);
    for(const auto& P:Infos){UTexture* V=nullptr;auto R=Info(P);const bool Found=M->GetTextureParameterValue(P,V);R->SetBoolField(TEXT("resolved"),Found);if(Found)R->SetObjectField(TEXT("value"),Texture(V));Textures.Add(MakeShared<FJsonValueObject>(R));}
    J->SetArrayField(TEXT("all_resolved_scalar_parameters"),Scalars);J->SetArrayField(TEXT("all_resolved_vector_parameters"),Vectors);J->SetArrayField(TEXT("all_resolved_texture_parameters"),Textures);
    return J;
}

inline TSharedPtr<FJsonObject> Mesh(UStaticMesh* M,bool DumpSmallCpuGrid=false)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),GetPathNameSafe(M));if(!M)return J;
    const FBox B=M->GetBoundingBox();J->SetArrayField(TEXT("extended_local_bounds_min_cm"),Vector(B.Min));J->SetArrayField(TEXT("extended_local_bounds_max_cm"),Vector(B.Max));
    const auto* R=M->GetRenderData();J->SetBoolField(TEXT("render_data_present"),R!=nullptr);if(!R)return J;
    TArray<TSharedPtr<FJsonValue>> Lods;
    for(int32 I=0;I<R->LODResources.Num();++I)
    {
        const auto& L=R->LODResources[I];auto K=MakeShared<FJsonObject>();K->SetNumberField(TEXT("index"),I);
        K->SetNumberField(TEXT("vertices"),L.VertexBuffers.PositionVertexBuffer.GetNumVertices());K->SetNumberField(TEXT("triangles"),L.GetNumTriangles());K->SetNumberField(TEXT("sections"),L.Sections.Num());
        const bool P=L.VertexBuffers.PositionVertexBuffer.GetAllowCPUAccess() && L.VertexBuffers.PositionVertexBuffer.GetVertexData()!=nullptr;
        const bool Q=L.IndexBuffer.GetAllowCPUAccess();K->SetBoolField(TEXT("cpu_positions_readable"),P);K->SetBoolField(TEXT("cpu_indices_retained"),Q);
        if(DumpSmallCpuGrid && I==0 && P && Q && L.VertexBuffers.PositionVertexBuffer.GetNumVertices()<=20000 && L.IndexBuffer.GetNumIndices()<=150000)
        {
            TArray<TSharedPtr<FJsonValue>> Positions,Indices;
            for(uint32 N=0;N<L.VertexBuffers.PositionVertexBuffer.GetNumVertices();++N)Positions.Add(MakeShared<FJsonValueArray>(Vector(FVector(L.VertexBuffers.PositionVertexBuffer.VertexPosition(N)))));
            for(int32 N=0;N<L.IndexBuffer.GetNumIndices();++N)Indices.Add(MakeShared<FJsonValueNumber>(L.IndexBuffer.GetIndex(N)));
            K->SetArrayField(TEXT("undeformed_local_positions_cm"),Positions);K->SetArrayField(TEXT("triangle_indices"),Indices);
        }
        Lods.Add(MakeShared<FJsonValueObject>(K));
    }
    J->SetArrayField(TEXT("lods"),Lods);J->SetStringField(TEXT("geometry_scope"),TEXT("Stored undeformed render geometry; ocean WPO/morph heights are not evaluated by this snapshot"));return J;
}

inline TSharedPtr<FJsonObject> Primitive(AActor* A,UPrimitiveComponent* C)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),GetPathNameSafe(A));J->SetStringField(TEXT("component"),GetPathNameSafe(C));J->SetStringField(TEXT("class"),C->GetClass()->GetPathName());
    J->SetBoolField(TEXT("actor_hidden"),A->IsHidden());J->SetBoolField(TEXT("visible"),C->IsVisible());J->SetBoolField(TEXT("hidden_in_game"),C->bHiddenInGame);J->SetBoolField(TEXT("registered"),C->IsRegistered());
    J->SetBoolField(TEXT("render_main_pass"),C->bRenderInMainPass);J->SetBoolField(TEXT("render_depth_pass"),C->bRenderInDepthPass);J->SetBoolField(TEXT("cast_shadow"),C->CastShadow);
    J->SetArrayField(TEXT("bounds_min_cm"),Vector(C->Bounds.GetBox().Min));J->SetArrayField(TEXT("bounds_max_cm"),Vector(C->Bounds.GetBox().Max));
    J->SetArrayField(TEXT("component_location_cm"),Vector(C->GetComponentLocation()));J->SetArrayField(TEXT("component_scale"),Vector(C->GetComponentScale()));J->SetStringField(TEXT("component_rotation"),C->GetComponentRotation().ToString());
    J->SetNumberField(TEXT("collision_enabled"),int32(C->GetCollisionEnabled()));J->SetNumberField(TEXT("collision_object_type"),int32(C->GetCollisionObjectType()));
    TArray<TSharedPtr<FJsonValue>> Materials;for(int32 S=0;S<C->GetNumMaterials();++S)Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(C->GetMaterial(S))));J->SetArrayField(TEXT("effective_materials"),Materials);
    if(auto* S=Cast<UStaticMeshComponent>(C))J->SetStringField(TEXT("mesh"),GetPathNameSafe(S->GetStaticMesh()));
    if(auto* I=Cast<UInstancedStaticMeshComponent>(C))J->SetNumberField(TEXT("instances"),I->GetInstanceCount());return J;
}

inline TSharedPtr<FJsonObject> Field(const UCoastalWaveData* F)
{
    auto J=MakeShared<FJsonObject>();if(!F)return J;J->SetStringField(TEXT("path"),F->GetPathName());J->SetNumberField(TEXT("grid_size"),F->GridSize);
    J->SetArrayField(TEXT("minimum_cm"),XY(F->GridMinimum));J->SetArrayField(TEXT("maximum_cm"),XY(F->GridMaximum));J->SetNumberField(TEXT("mean_sea_level_cm"),F->SeaLevel);
    J->SetNumberField(TEXT("depth_float_count"),F->DepthCm.Num());J->SetStringField(TEXT("depth_raw_float32_sha1"),Sha1(F->DepthCm.GetData(),uint64(F->DepthCm.Num())*sizeof(float)));
    float Lo=MAX_flt,Hi=-MAX_flt;int32 NonFinite=0;for(float D:F->DepthCm){if(FMath::IsFinite(D)){Lo=FMath::Min(Lo,D);Hi=FMath::Max(Hi,D);}else ++NonFinite;}
    J->SetNumberField(TEXT("depth_nonfinite_count"),NonFinite);if(F->DepthCm.Num()>NonFinite){J->SetNumberField(TEXT("depth_minimum_cm"),Lo);J->SetNumberField(TEXT("depth_maximum_cm"),Hi);}
    TArray<TSharedPtr<FJsonValue>> Profiles;for(uint8 State=0;State<3;++State)
    {
        const auto& P=F->GetProfile(State);auto K=MakeShared<FJsonObject>();K->SetNumberField(TEXT("sea_state"),State);K->SetNumberField(TEXT("bytes"),P.Num());K->SetStringField(TEXT("raw_sha1"),Sha1(P.GetData(),P.Num()));K->SetStringField(TEXT("raw_hex"),BytesToHex(P.GetData(),P.Num()));Profiles.Add(MakeShared<FJsonValueObject>(K));
    }
    J->SetArrayField(TEXT("immutable_fft_profiles"),Profiles);J->SetObjectField(TEXT("bathymetry"),Texture(F->DepthTexture));J->SetObjectField(TEXT("island_height"),Texture(F->IslandHeight));J->SetObjectField(TEXT("island_shore"),Texture(F->IslandShore));J->SetObjectField(TEXT("island_normal"),Texture(F->IslandNormal));return J;
}

inline TSharedPtr<FJsonObject> Snapshot(UWorld* W,APlayerController* PC,int32 Fixture)
{
    auto Root=C923MarinaWaterAttribution::Snapshot(W,PC,Fixture);
    Root->SetStringField(TEXT("status"),TEXT("READ_ONLY_RUNTIME_WATER_ATTRIBUTION56_NOT_ROOT_CAUSE_PASS"));
    Root->SetStringField(TEXT("diagnostic_scope56"),TEXT("Actual parameter/light/layer/field/undeformed grid reads. No shader evaluation or GPU texture readback; cached ocean samples can use the mean-sea fallback. No render-state, gameplay, profile or clock mutation."));
    if(!W || !PC)return Root;
    Root->SetNumberField(TEXT("world_seconds"),W->GetTimeSeconds());Root->SetNumberField(TEXT("frame_counter"),double(GFrameCounter));
    // These coordinates are mean-plane inverse projections of the observed
    // screenshot features, not proof of the deformed pixel's final position.
    const FVector2D Points[]={FVector2D(-42.432554,-28.684493),FVector2D(-33.989384,-29.210803),FVector2D(-25.179914,-29.601769),FVector2D(37.780680,-63.788141),FVector2D(93.027649,-66.462386),FVector2D(115.148625,-106.641876)};
    const TCHAR* Names[]={TEXT("green_near_approx_pixel_520_800"),TEXT("green_centre_approx_pixel_600_600"),TEXT("green_shadow_approx_pixel_640_520"),TEXT("white_mask_core_approx_pixel_200_401"),TEXT("white_mask_fade_approx_pixel_400_380"),TEXT("white_outside_mask_approx_pixel_100_376")};
    const FBox MaritimeRegion(FVector(-20000,-22000,-20000),FVector(25000,10000,15000));
    TArray<TSharedPtr<FJsonValue>> Layers,Underlays,Suns,Clocks,Oceans;auto DetailedMaterials=MakeShared<FJsonObject>();auto Meshes=MakeShared<FJsonObject>();
    for(TActorIterator<AActor> It(W);It;++It)
    {
        AActor* A=*It;TInlineComponentArray<UPrimitiveComponent*> Components(A);
        for(UPrimitiveComponent* C:Components)
        {
            if(!C)continue;bool Slw=false,NamedWater=false,Translucent=false;
            for(int32 S=0;S<C->GetNumMaterials();++S)if(auto* M=C->GetMaterial(S))
            {
                Slw|=M->GetShadingModels().HasShadingModel(MSM_SingleLayerWater);
                const FString N=M->GetPathName()+TEXT(" ")+GetPathNameSafe(M->GetMaterial());
                NamedWater|=N.Contains(TEXT("Ocean")) || N.Contains(TEXT("CoastalWater")) || N.Contains(TEXT("HarborAmbientFoam"));
                Translucent|=M->GetBlendMode()==BLEND_Translucent;
            }
            const bool IsOceanGrid=C->GetFName()==TEXT("OceanRenderGrid");
            // SLW/named water is inventoried globally, including hidden layers.
            // Other translucent layers are only ambiguous local candidates.
            if(Slw || NamedWater || IsOceanGrid || (Translucent && C->Bounds.GetBox().Intersect(MaritimeRegion)))
            {
                auto J=Primitive(A,C);J->SetBoolField(TEXT("single_layer_water"),Slw);J->SetBoolField(TEXT("named_water_candidate"),NamedWater);J->SetBoolField(TEXT("ocean_grid"),IsOceanGrid);J->SetBoolField(TEXT("translucent_only_ambiguous_candidate"),!Slw && !NamedWater && !IsOceanGrid);
                Layers.Add(MakeShared<FJsonValueObject>(J));
                if(Slw || NamedWater || IsOceanGrid)
                {
                    for(int32 S=0;S<C->GetNumMaterials();++S)if(auto* M=C->GetMaterial(S);M && !DetailedMaterials->HasField(M->GetPathName()))DetailedMaterials->SetObjectField(M->GetPathName(),Parameters(M));
                    if(auto* S=Cast<UStaticMeshComponent>(C))if(auto* M=S->GetStaticMesh().Get();M && !Meshes->HasField(M->GetPathName()))Meshes->SetObjectField(M->GetPathName(),Mesh(M,IsOceanGrid));
                }
            }
            // Bounds witnesses deliberately include NoCollision seabeds. Bounds
            // intersection alone is not a finite triangle/depth intersection.
            if(!IsOceanGrid && C->Bounds.GetBox().Intersect(MaritimeRegion))
            {
                TArray<TSharedPtr<FJsonValue>> At;const FBox B=C->Bounds.GetBox();
                for(int32 P=0;P<UE_ARRAY_COUNT(Points);++P)if(Points[P].X*100>=B.Min.X && Points[P].X*100<=B.Max.X && Points[P].Y*100>=B.Min.Y && Points[P].Y*100<=B.Max.Y && B.Min.Z<0)
                    At.Add(MakeShared<FJsonValueNumber>(P));
                if(!At.IsEmpty()){auto J=Primitive(A,C);J->SetArrayField(TEXT("probe_xy_bounds_overlap_indices"),At);J->SetStringField(TEXT("proof_scope"),TEXT("AABB candidate only; does not establish visible pixel, triangle intersection or depth"));Underlays.Add(MakeShared<FJsonValueObject>(J));}
            }
        }
        TInlineComponentArray<UDirectionalLightComponent*> Lights(A);
        for(auto* L:Lights)if(L)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),A->GetPathName());J->SetStringField(TEXT("component"),L->GetPathName());J->SetBoolField(TEXT("actor_hidden"),A->IsHidden());J->SetBoolField(TEXT("visible"),L->IsVisible());
            J->SetArrayField(TEXT("direction_world"),Vector(L->GetDirection()));J->SetStringField(TEXT("rotation"),L->GetComponentRotation().ToString());J->SetNumberField(TEXT("intensity"),L->Intensity);
            const auto V=L->GetLightColor();J->SetArrayField(TEXT("linear_light_color"),{MakeShared<FJsonValueNumber>(V.R),MakeShared<FJsonValueNumber>(V.G),MakeShared<FJsonValueNumber>(V.B),MakeShared<FJsonValueNumber>(V.A)});
            J->SetBoolField(TEXT("cast_shadow"),L->CastShadows);J->SetBoolField(TEXT("cast_cloud_shadows"),L->bCastCloudShadows);J->SetNumberField(TEXT("cloud_shadow_strength"),L->CloudShadowStrength);Suns.Add(MakeShared<FJsonValueObject>(J));
        }
        if(auto* Clock=Cast<ANeonCityDayNightCycle>(A))
        {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),Clock->GetPathName());J->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());J->SetBoolField(TEXT("cycle_enabled"),Clock->bCycleEnabled);J->SetStringField(TEXT("sun_component"),GetPathNameSafe(Clock->GetSun()));Clocks.Add(MakeShared<FJsonValueObject>(J));}
        if(auto* O=Cast<ACoastalOceanSurface>(A))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),O->GetPathName());J->SetBoolField(TEXT("ready"),O->IsReady());J->SetNumberField(TEXT("sea_state"),int32(O->GetSeaState()));J->SetNumberField(TEXT("strength"),O->GetStrength());J->SetNumberField(TEXT("wave_seconds"),O->GetWaveSeconds());J->SetNumberField(TEXT("query_sequence"),double(O->GetQuerySequence()));
            const auto D=O->GetSurfaceInstanceDiagnostic();auto K=MakeShared<FJsonObject>();K->SetNumberField(TEXT("tracked_nodes"),D.TrackedNodes);K->SetNumberField(TEXT("rendered_instances"),D.RenderedInstances);K->SetNumberField(TEXT("changed_selections"),double(D.ChangedSelections));K->SetNumberField(TEXT("order_only_selections"),double(D.OrderOnlySelections));K->SetNumberField(TEXT("topology_updates"),double(D.TopologyUpdates));K->SetNumberField(TEXT("added_nodes"),double(D.AddedNodes));K->SetNumberField(TEXT("removed_nodes"),double(D.RemovedNodes));J->SetObjectField(TEXT("cdlod_actual_counters"),K);
            if(const auto* F=O->GetField())
            {
                const FString Before=Sha1(F->DepthCm.GetData(),uint64(F->DepthCm.Num())*sizeof(float));J->SetObjectField(TEXT("actual_field"),Field(F));TArray<TSharedPtr<FJsonValue>> Probes;
                for(int32 P=0;P<UE_ARRAY_COUNT(Points);++P)
                {
                    auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("index"),P);V->SetStringField(TEXT("label"),Names[P]);V->SetArrayField(TEXT("world_xy_m"),XY(Points[P]));const FVector2D Pos=Points[P]*100;FVector2D Grad;
                    V->SetNumberField(TEXT("native_field_bilinear_depth_cm"),F->GetDepth(Pos,&Grad));V->SetArrayField(TEXT("native_field_depth_gradient_cm_per_cm"),XY(Grad));V->SetBoolField(TEXT("inside_native_depth_bounds"),Pos.X>=F->GridMinimum.X && Pos.X<=F->GridMaximum.X && Pos.Y>=F->GridMinimum.Y && Pos.Y<=F->GridMaximum.Y);
                    if(F->GridSize>=2 && F->DepthCm.Num()==F->GridSize*F->GridSize)
                    {
                        const FVector2D Step=(F->GridMaximum-F->GridMinimum)/(F->GridSize-1),G=(Pos-F->GridMinimum)/Step;
                        const int32 X=FMath::Min(FMath::FloorToInt(FMath::Clamp(G.X,0.,double(F->GridSize-1))),F->GridSize-2),Y=FMath::Min(FMath::FloorToInt(FMath::Clamp(G.Y,0.,double(F->GridSize-1))),F->GridSize-2);
                        V->SetArrayField(TEXT("depth_grid_cell_xy"),XY(FVector2D(X,Y)));V->SetArrayField(TEXT("depth_grid_step_cm"),XY(Step));V->SetArrayField(TEXT("depth_grid_uv_clamped"),XY(FVector2D(FMath::Clamp(G.X,0.,double(F->GridSize-1))-X,FMath::Clamp(G.Y,0.,double(F->GridSize-1))-Y)));
                        V->SetArrayField(TEXT("depth_cell_abcd_cm"),{MakeShared<FJsonValueNumber>(F->DepthCm[Y*F->GridSize+X]),MakeShared<FJsonValueNumber>(F->DepthCm[Y*F->GridSize+X+1]),MakeShared<FJsonValueNumber>(F->DepthCm[(Y+1)*F->GridSize+X]),MakeShared<FJsonValueNumber>(F->DepthCm[(Y+1)*F->GridSize+X+1])});
                    }
                    float Height=F->SeaLevel;FVector Normal,Velocity;O->Sample(FVector(Pos.X,Pos.Y,F->SeaLevel),Height,Normal,Velocity);
                    V->SetNumberField(TEXT("existing_cached_or_fallback_height_cm"),Height);V->SetArrayField(TEXT("existing_cached_or_fallback_normal"),Vector(Normal));V->SetArrayField(TEXT("existing_cached_or_fallback_velocity_cm_s"),Vector(Velocity));V->SetStringField(TEXT("sample_limitation"),TEXT("Sample is read-only: nearest existing <=5m, <=1s lease or mean sea fallback; no public validity flag. Not fresh FFT/GPU or per-pixel shader truth."));
                    Probes.Add(MakeShared<FJsonValueObject>(V));
                }
                J->SetArrayField(TEXT("feature_probes"),Probes);J->SetBoolField(TEXT("depth_raw_bytes_unchanged_during_snapshot"),Before==Sha1(F->DepthCm.GetData(),uint64(F->DepthCm.Num())*sizeof(float)));
            }
            Oceans.Add(MakeShared<FJsonValueObject>(J));
        }
    }
    Root->SetArrayField(TEXT("global_water_and_local_translucent_layer_candidates"),Layers);Root->SetObjectField(TEXT("actual_water_material_full_parameters"),DetailedMaterials);Root->SetObjectField(TEXT("water_mesh_render_data"),Meshes);
    Root->SetArrayField(TEXT("underlay_probe_bounds_candidates_including_no_collision"),Underlays);Root->SetArrayField(TEXT("directional_lights"),Suns);Root->SetArrayField(TEXT("day_night_clocks"),Clocks);Root->SetArrayField(TEXT("ocean_attribution56"),Oceans);
    Root->SetArrayField(TEXT("unresolved_attribution_requires_pixel_evidence"),{MakeShared<FJsonValueString>(TEXT("White: ambient shore/whitecap coverage versus direct sun specular versus physical wake.")),MakeShared<FJsonValueString>(TEXT("Green triangles: interpolated shore/depth/aeration versus underlay/SLW depth/shadow versus CDLOD normals. Snapshot alone does not choose a cause."))});
    return Root;
}

inline bool Write(UWorld* W,APlayerController* PC,int32 Fixture,const FString& UniqueDirectory)
{
    const FString File=UniqueDirectory/FString::Printf(TEXT("water-attribution56-%02d.json"),Fixture);
    if(IFileManager::Get().FileExists(*File))return false;
    IFileManager::Get().MakeDirectory(*UniqueDirectory,true);FString Json;
    if(!FJsonSerializer::Serialize(Snapshot(W,PC,Fixture).ToSharedRef(),TJsonWriterFactory<>::Create(&Json)))return false;
    return FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}
#endif
