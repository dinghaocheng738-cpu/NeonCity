#pragma once
// Read-only diagnostic used immediately before the existing view test capture.
// Read current runtime scene/material state; no save, input, visibility, shader
// compilation, texture readback, physics, time, camera or console-variable writes.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Water/CoastalOceanSurface.h"
#include "Water/CoastalWaveData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"

namespace C923MarinaWaterAttribution
{
inline TArray<TSharedPtr<FJsonValue>> Vector(const FVector& V)
{return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}

inline TSharedPtr<FJsonObject> Material(UMaterialInterface* M,UWorld* W)
{
    auto J=MakeShared<FJsonObject>();if(!M){J->SetBoolField(TEXT("missing"),true);return J;}
    J->SetStringField(TEXT("path"),M->GetPathName());J->SetNumberField(TEXT("blend_mode"),int32(M->GetBlendMode()));
    auto* Base=M->GetMaterial();J->SetStringField(TEXT("base"),GetPathNameSafe(Base));
    const auto* Resource=Base?Base->GetMaterialResource(GShaderPlatformForFeatureLevel[W->GetFeatureLevel()]):nullptr;
    J->SetBoolField(TEXT("base_resource_present"),Resource!=nullptr);
    J->SetBoolField(TEXT("base_shader_map_complete"),Resource && Resource->IsGameThreadShaderMapComplete());
    TArray<TSharedPtr<FJsonValue>> Errors;if(Resource)for(const FString& E:Resource->GetCompileErrors())Errors.Add(MakeShared<FJsonValueString>(E));
    J->SetArrayField(TEXT("base_compile_errors"),Errors);
    auto Parameters=MakeShared<FJsonObject>();
    for(const TCHAR* Name:{TEXT("OceanOrigin"),TEXT("WaterBounds"),TEXT("WakeControls"),TEXT("WakeBoat"),TEXT("WakeHullScale"),
        TEXT("Riviera915DryLandBounds"),TEXT("Riviera915MarinaNotch"),TEXT("Riviera915RoadConnector"),TEXT("OceanAbsorption"),TEXT("ShoreControls"),TEXT("SourceCamera")})
    {FLinearColor C;if(M->GetVectorParameterValue(FMaterialParameterInfo(Name),C))Parameters->SetArrayField(Name,
        {MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B),MakeShared<FJsonValueNumber>(C.A)});}
    for(const TCHAR* Name:{TEXT("GridSize"),TEXT("Amplitude"),TEXT("WindSpeed"),TEXT("FoamBias")})
    {float V=0;if(M->GetScalarParameterValue(FMaterialParameterInfo(Name),V))Parameters->SetNumberField(Name,V);}
    for(const TCHAR* Name:{TEXT("Bathymetry"),TEXT("HeightField"),TEXT("ShoreField"),TEXT("NormalRock"),TEXT("Displacement"),TEXT("Derivatives"),TEXT("WakeDisplay"),TEXT("WakeAeration"),TEXT("WakeNear")})
    {UTexture* T=nullptr;if(M->GetTextureParameterValue(FMaterialParameterInfo(Name),T))Parameters->SetStringField(Name,GetPathNameSafe(T));}
    J->SetObjectField(TEXT("actual_parameters"),Parameters);return J;
}

inline TSharedPtr<FJsonObject> Snapshot(UWorld* W,APlayerController* PC,int32 Fixture,bool DiagnosticBoatShadowOff=false)
{
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("status"),TEXT("READ_ONLY_RUNTIME_WATER_ATTRIBUTION_SNAPSHOT"));
    Root->SetNumberField(TEXT("fixture_index"),Fixture);Root->SetStringField(TEXT("world"),GetPathNameSafe(W));
    Root->SetBoolField(TEXT("diagnostic_b_boat_shadow_off"),DiagnosticBoatShadowOff);
    Root->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);
    Root->SetStringField(TEXT("scope"),TEXT("Actual current primitives, materials, Ocean nodes and existing water samples only; not a root-cause, beauty or performance PASS. No mutation or shader/asset compilation."));
    if(!W || !PC){Root->SetBoolField(TEXT("missing_world_or_controller"),true);return Root;}
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);Root->SetArrayField(TEXT("actual_eye_cm"),Vector(Eye));Root->SetStringField(TEXT("view_rotation"),View.ToString());
    const FBox Region(FVector(-9000,-9000,-5000),FVector(6000,3000,5000));
    TArray<TSharedPtr<FJsonValue>> Primitives,Oceans,Boats;auto Materials=MakeShared<FJsonObject>();
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components(*It);
        for(UPrimitiveComponent* C:Components)
        {
            if(!C || !C->Bounds.GetBox().Intersect(Region))continue;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),It->GetPathName());J->SetStringField(TEXT("component"),C->GetPathName());
            J->SetStringField(TEXT("class"),C->GetClass()->GetPathName());J->SetBoolField(TEXT("actor_hidden"),It->IsHidden());
            J->SetBoolField(TEXT("visible"),C->IsVisible());J->SetBoolField(TEXT("hidden_in_game"),C->bHiddenInGame);J->SetBoolField(TEXT("registered"),C->IsRegistered());
            J->SetBoolField(TEXT("cast_shadow"),C->CastShadow);J->SetBoolField(TEXT("cast_hidden_shadow"),C->bCastHiddenShadow);
            J->SetBoolField(TEXT("render_main_pass"),C->bRenderInMainPass);J->SetBoolField(TEXT("render_depth_pass"),C->bRenderInDepthPass);
            J->SetBoolField(TEXT("visible_in_ray_tracing"),C->bVisibleInRayTracing);J->SetBoolField(TEXT("affect_distance_field_lighting"),C->bAffectDistanceFieldLighting);
            J->SetArrayField(TEXT("world_bounds_min_cm"),Vector(C->Bounds.GetBox().Min));J->SetArrayField(TEXT("world_bounds_max_cm"),Vector(C->Bounds.GetBox().Max));
            J->SetNumberField(TEXT("collision_enabled"),int32(C->GetCollisionEnabled()));J->SetNumberField(TEXT("object_type"),int32(C->GetCollisionObjectType()));
            TArray<TSharedPtr<FJsonValue>> Slots;
            for(int32 I=0;I<C->GetNumMaterials();++I)
            {UMaterialInterface* M=C->GetMaterial(I);Slots.Add(MakeShared<FJsonValueString>(GetPathNameSafe(M)));if(M && !Materials->HasField(M->GetPathName()))Materials->SetObjectField(M->GetPathName(),Material(M,W));}
            J->SetArrayField(TEXT("materials"),Slots);
            if(auto* S=Cast<UStaticMeshComponent>(C))J->SetStringField(TEXT("static_mesh"),GetPathNameSafe(S->GetStaticMesh()));
            if(auto* I=Cast<UInstancedStaticMeshComponent>(C))
            {
                J->SetNumberField(TEXT("instance_count"),I->GetInstanceCount());
                if(C->GetFName()==TEXT("OceanRenderGrid") || C->GetFName()==TEXT("MarineWaterParticles"))
                {
                    TArray<TSharedPtr<FJsonValue>> Instances;
                    for(int32 N=0;N<I->GetInstanceCount();++N)
                    {
                        FTransform T;if(!I->GetInstanceTransform(N,T,true))continue;auto P=MakeShared<FJsonObject>();
                        P->SetNumberField(TEXT("index"),N);P->SetArrayField(TEXT("location_cm"),Vector(T.GetLocation()));P->SetArrayField(TEXT("scale"),Vector(T.GetScale3D()));
                        if(auto* Mesh=I->GetStaticMesh().Get())
                        {const FBox B=Mesh->GetBoundingBox().TransformBy(T);P->SetArrayField(TEXT("undeformed_bounds_min_cm"),Vector(B.Min));P->SetArrayField(TEXT("undeformed_bounds_max_cm"),Vector(B.Max));}
                        TArray<TSharedPtr<FJsonValue>> Custom;
                        for(int32 K=0;K<I->NumCustomDataFloats;++K)
                        {const int32 V=N*I->NumCustomDataFloats+K;if(I->PerInstanceSMCustomData.IsValidIndex(V))Custom.Add(MakeShared<FJsonValueNumber>(I->PerInstanceSMCustomData[V]));}
                        P->SetArrayField(TEXT("custom_data"),Custom);Instances.Add(MakeShared<FJsonValueObject>(P));
                    }
                    J->SetArrayField(TEXT("instances"),Instances);
                }
            }
            Primitives.Add(MakeShared<FJsonValueObject>(J));
        }
        if(auto* O=Cast<ACoastalOceanSurface>(*It))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),O->GetPathName());J->SetBoolField(TEXT("ready"),O->IsReady());
            J->SetNumberField(TEXT("query_sequence"),double(O->GetQuerySequence()));J->SetNumberField(TEXT("surface_node_count"),O->GetSurfaceNodeCount());
            if(const auto* F=O->GetField())
            {J->SetNumberField(TEXT("mean_water_z_cm"),F->SeaLevel);J->SetStringField(TEXT("field"),F->GetPathName());
                J->SetStringField(TEXT("depth_texture"),GetPathNameSafe(F->DepthTexture));J->SetStringField(TEXT("depth_bounds_min_cm"),F->GridMinimum.ToString());J->SetStringField(TEXT("depth_bounds_max_cm"),F->GridMaximum.ToString());}
            Oceans.Add(MakeShared<FJsonValueObject>(J));
        }
        if(auto* B=Cast<ANeonCityMarineVehicleBase>(*It);B && Region.IsInsideOrOn(B->GetActorLocation()))
        {
            auto J=MakeShared<FJsonObject>();const auto T=B->GetMarineMovement()->GetTelemetry();J->SetStringField(TEXT("actor"),B->GetPathName());
            J->SetArrayField(TEXT("position_cm"),Vector(B->GetActorLocation()));J->SetBoolField(TEXT("engine_running"),T.EngineRunning);J->SetBoolField(TEXT("afloat"),T.Afloat);
            J->SetNumberField(TEXT("speed_kmh"),T.SpeedKmh);J->SetNumberField(TEXT("thrust"),T.SignedThrust);
            if(auto* FX=B->FindComponentByClass<UMarineWaterEffectsComponent>())
            {J->SetNumberField(TEXT("active_foam"),FX->GetActiveWakeCount());J->SetNumberField(TEXT("active_spray"),FX->GetActiveSprayCount());J->SetStringField(TEXT("foam_material"),FX->FoamMaterial.ToSoftObjectPath().ToString());}
            FWaterSurfaceSample S;if(auto* Registry=W->GetSubsystem<UMarineWorldSubsystem>();Registry && Registry->QueryWater(B->GetActorLocation(),S))
            {J->SetArrayField(TEXT("existing_sample_water_point_cm"),Vector(S.SurfacePoint));J->SetArrayField(TEXT("existing_sample_normal"),Vector(S.Normal));}
            Boats.Add(MakeShared<FJsonValueObject>(J));
        }
    }
    Root->SetArrayField(TEXT("primitives_in_marina_aabb_including_hidden"),Primitives);Root->SetObjectField(TEXT("actual_materials"),Materials);
    Root->SetArrayField(TEXT("ocean_providers"),Oceans);Root->SetArrayField(TEXT("boats"),Boats);
    auto CVars=MakeShared<FJsonObject>();for(const TCHAR* Name:{TEXT("r.Water.SingleLayer"),TEXT("r.Water.SingleLayer.Reflection"),TEXT("r.Water.SingleLayer.RefractionDownsampleFactor"),TEXT("r.Water.SingleLayer.DepthPrepass"),TEXT("r.Water.SingleLayer.ShadersSupportVSMFiltering"),TEXT("r.Water.SingleLayer.VSMFiltering"),TEXT("r.Water.SingleLayer.ShadersSupportDistanceFieldShadow"),TEXT("r.Water.SingleLayer.DistanceFieldShadow"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.SSR.Quality"),TEXT("r.Lumen.Reflections.Allow")})
    {if(const auto* V=IConsoleManager::Get().FindConsoleVariable(Name))CVars->SetStringField(Name,V->GetString());else CVars->SetStringField(Name,TEXT("UNAVAILABLE"));}
    Root->SetObjectField(TEXT("read_only_render_cvars"),CVars);return Root;
}

inline bool Write(UWorld* W,APlayerController* PC,int32 Fixture,const FString& UniqueDirectory,bool DiagnosticBoatShadowOff=false)
{
    const FString File=UniqueDirectory/FString::Printf(TEXT("water-attribution-%02d.json"),Fixture);
    if(IFileManager::Get().FileExists(*File))return false;
    IFileManager::Get().MakeDirectory(*UniqueDirectory,true);FString Json;
    if(!FJsonSerializer::Serialize(Snapshot(W,PC,Fixture,DiagnosticBoatShadowOff).ToSharedRef(),TJsonWriterFactory<>::Create(&Json)))return false;
    return FFileHelper::SaveStringToFile(Json,*File);
}
}
#endif
