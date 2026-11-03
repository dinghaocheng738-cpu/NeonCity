#pragma once
// Test-only B: remove exactly the two observed cosmetic seabed renderers.
// Never use these captures as normal production, beauty or visual acceptance.
// Only raw component visibility changes, without child propagation. Explicit
// cleanup and RAII restore it; every other observed render/physics flag stays
// unchanged. The existing FViews camera/observer/clock fixture is retained.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityMarinaWaterAttribution923.h"
#include "Components/StaticMeshComponent.h"

namespace C923MarinaWaterAttribution
{
class FSeabedAB62
{
    struct FState
    {
        TWeakObjectPtr<UStaticMeshComponent> Component;
        FString Path;
        TWeakObjectPtr<UStaticMesh> Mesh;
        TWeakObjectPtr<UMaterialInterface> Material;
        FTransform Transform;
        bool Visible=false,Hidden=false,ActorHidden=false,Shadow=false,HiddenShadow=false;
        bool MainPass=false,DepthPass=false,RayTracing=false,DistanceField=false,Indirect=false,Tick=false;
        ECollisionEnabled::Type Collision=ECollisionEnabled::NoCollision;
        ECollisionChannel ObjectType=ECC_WorldStatic;
        FName Profile;
        FCollisionResponseContainer Responses;
        bool Overlap=false;
    };
    TArray<FState> States;
    TWeakObjectPtr<ACoastalOceanSurface> Ocean;
    const UCoastalWaveData* Field=nullptr;
    const UTexture2D* Depth=nullptr;
    TSharedPtr<FJsonObject> Receipt=MakeShared<FJsonObject>();
    FString ReceiptFile;
    bool Applied=false,Restored=false,RestorePassed=true;
    int32 HideWrites=0,RestoreWrites=0;

    bool Persist()const
    {
        if(ReceiptFile.IsEmpty())return false;
        FString Json;
        return FJsonSerializer::Serialize(Receipt.ToSharedRef(),TJsonWriterFactory<>::Create(&Json))
            && FFileHelper::SaveStringToFile(Json,*ReceiptFile);
    }
    bool OtherStateUnchanged(const FState& S)const
    {
        const auto* C=S.Component.Get();
        return C && C->IsRegistered() && C->GetPathName()==S.Path && C->GetOwner()
            && C->GetStaticMesh()==S.Mesh.Get() && C->GetNumMaterials()==1 && C->GetMaterial(0)==S.Material.Get()
            && C->GetComponentTransform().Equals(S.Transform,.000001)
            && C->bHiddenInGame==S.Hidden && C->GetOwner()->IsHidden()==S.ActorHidden
            && bool(C->CastShadow)==S.Shadow && bool(C->bCastHiddenShadow)==S.HiddenShadow
            && bool(C->bRenderInMainPass)==S.MainPass && bool(C->bRenderInDepthPass)==S.DepthPass
            && bool(C->bVisibleInRayTracing)==S.RayTracing && bool(C->bAffectDistanceFieldLighting)==S.DistanceField
            && bool(C->bAffectDynamicIndirectLighting)==S.Indirect && C->IsComponentTickEnabled()==S.Tick
            && C->GetCollisionEnabled()==S.Collision && C->GetCollisionObjectType()==S.ObjectType
            && C->GetCollisionProfileName()==S.Profile && C->GetCollisionResponseToChannels()==S.Responses
            && C->GetGenerateOverlapEvents()==S.Overlap;
    }
    bool FieldUnchanged()const
    {return Ocean.IsValid() && Field && Depth && Ocean->GetField()==Field && Field->DepthTexture.Get()==Depth;}
public:
    FSeabedAB62()=default;
    FSeabedAB62(const FSeabedAB62&)=delete;
    FSeabedAB62& operator=(const FSeabedAB62&)=delete;
    ~FSeabedAB62(){Restore();}
    int32 GetHideWrites()const{return HideWrites;}
    int32 GetRestoreWrites()const{return RestoreWrites;}
    bool Begin(UWorld* W,const FString& UniqueDirectory)
    {
        if(Applied || !States.IsEmpty() || !W || !IsInGameThread())return false;
        ReceiptFile=UniqueDirectory/TEXT("diagnostic-seabed-ab62.json");
        if(IFileManager::Get().FileExists(*ReceiptFile)){ReceiptFile.Reset();return false;}
        struct FExpected{const TCHAR* Actor;const TCHAR* Mesh;};
        // Exact persistent paths and assets recorded in native normal A's
        // WaterSternFix1 snapshots; no generated runtime actor ID is guessed.
        const FExpected Expected[]={
            {TEXT("StaticMeshActor_71"),TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/SM_C921_ContinuousCoastBed2.SM_C921_ContinuousCoastBed2")},
            {TEXT("StaticMeshActor_40"),TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_CoastalSeabedApron.SM_C920_CoastalSeabedApron")}};
        const FString Prefix=TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test.NeonCity_SouthBeach_Test:PersistentLevel.");
        TArray<TSharedPtr<FJsonValue>> Components;
        for(const auto& E:Expected)
        {
            UStaticMeshComponent* Found=nullptr;int32 Matches=0;
            for(TActorIterator<AActor> I(W);I;++I)
            {
                if(I->GetLevel()!=W->PersistentLevel)continue;
                TInlineComponentArray<UStaticMeshComponent*> Meshes(*I);
                for(auto* C:Meshes)
                    if(C && C->GetStaticMesh() && C->GetStaticMesh()->GetPathName()==E.Mesh)
                    {++Matches;if(I->GetPathName()==Prefix+E.Actor && C->GetPathName()==Prefix+E.Actor+TEXT(".StaticMeshComponent0"))Found=C;}
            }
            if(Matches!=1 || !Found || !Found->IsRegistered() || !Found->GetVisibleFlag() || !Found->IsVisible()
                || Found->bHiddenInGame || Found->GetOwner()->IsHidden() || Found->bCastHiddenShadow
                || Found->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || Found->GetNumMaterials()!=1
                || GetPathNameSafe(Found->GetMaterial(0))!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/M_C920_CoastalBlend.M_C920_CoastalBlend"))
            {States.Reset();ReceiptFile.Reset();return false;}
            FState S;S.Component=Found;S.Path=Found->GetPathName();S.Mesh=Found->GetStaticMesh().Get();S.Material=Found->GetMaterial(0);
            S.Transform=Found->GetComponentTransform();S.Visible=Found->GetVisibleFlag();S.Hidden=Found->bHiddenInGame;
            S.ActorHidden=Found->GetOwner()->IsHidden();S.Shadow=Found->CastShadow;S.HiddenShadow=Found->bCastHiddenShadow;
            S.MainPass=Found->bRenderInMainPass;S.DepthPass=Found->bRenderInDepthPass;S.RayTracing=Found->bVisibleInRayTracing;
            S.DistanceField=Found->bAffectDistanceFieldLighting;S.Indirect=Found->bAffectDynamicIndirectLighting;S.Tick=Found->IsComponentTickEnabled();
            S.Collision=Found->GetCollisionEnabled();S.ObjectType=Found->GetCollisionObjectType();S.Profile=Found->GetCollisionProfileName();
            S.Responses=Found->GetCollisionResponseToChannels();S.Overlap=Found->GetGenerateOverlapEvents();States.Add(S);
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("component"),S.Path);J->SetStringField(TEXT("mesh"),E.Mesh);
            J->SetStringField(TEXT("material"),GetPathNameSafe(S.Material.Get()));J->SetBoolField(TEXT("original_visible_flag"),S.Visible);
            J->SetBoolField(TEXT("original_hidden_in_game"),S.Hidden);J->SetBoolField(TEXT("original_actor_hidden"),S.ActorHidden);
            J->SetBoolField(TEXT("original_cast_shadow"),S.Shadow);J->SetBoolField(TEXT("original_cast_hidden_shadow"),S.HiddenShadow);
            J->SetBoolField(TEXT("original_render_main_pass"),S.MainPass);J->SetBoolField(TEXT("original_render_depth_pass"),S.DepthPass);
            J->SetBoolField(TEXT("original_visible_in_ray_tracing"),S.RayTracing);J->SetBoolField(TEXT("original_affect_distance_field_lighting"),S.DistanceField);
            J->SetBoolField(TEXT("original_affect_dynamic_indirect_lighting"),S.Indirect);J->SetBoolField(TEXT("original_tick_enabled"),S.Tick);
            J->SetNumberField(TEXT("original_collision_enabled"),int32(S.Collision));J->SetStringField(TEXT("original_transform"),S.Transform.ToString());
            Components.Add(MakeShared<FJsonValueObject>(J));
        }
        int32 Providers=0;
        for(TActorIterator<ACoastalOceanSurface> I(W);I;++I){++Providers;Ocean=*I;}
        if(Providers!=1 || !Ocean.IsValid() || !Ocean->IsReady() || !Ocean->GetField())
        {States.Reset();ReceiptFile.Reset();return false;}
        Field=Ocean->GetField();Depth=Field->DepthTexture.Get();
        if(GetPathNameSafe(Field)!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/DA_C923_HarborWaves26.DA_C923_HarborWaves26")
            || GetPathNameSafe(Depth)!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_HarborDepth26.T_C923_HarborDepth26"))
        {States.Reset();ReceiptFile.Reset();return false;}
        Receipt->SetStringField(TEXT("scope"),TEXT("DIAGNOSTIC B ONLY: fixed normal cameras40/41,20s settle, exactly two observed NoCollision cosmetic seabed components SetVisibility(false,false), then restored. Same Ocean/Field/depth, boat gameplay and production quality. No seabed/Ocean/boat transform, material, shadow-flag, collision, physics, input, CVar, asset or save writes. Existing observer/camera/clock fixture cleanup matches normal A. Never beauty, art acceptance or performance PASS."));
        Receipt->SetBoolField(TEXT("diagnostic_seabed_hidden"),true);Receipt->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);
        Receipt->SetArrayField(TEXT("exact_components"),Components);Receipt->SetStringField(TEXT("ocean"),GetPathNameSafe(Ocean.Get()));
        Receipt->SetStringField(TEXT("unchanged_field"),GetPathNameSafe(Field));Receipt->SetStringField(TEXT("unchanged_depth_texture"),GetPathNameSafe(Depth));
        Receipt->SetStringField(TEXT("status"),TEXT("DIAGNOSTIC_PREPARED_NO_VISIBILITY_MUTATION_YET"));
        if(!IFileManager::Get().MakeDirectory(*UniqueDirectory,true) || !Persist()){States.Reset();return false;}
        Applied=true;
        for(const FState& S:States){S.Component->SetVisibility(false,false);++HideWrites;}
        Receipt->SetNumberField(TEXT("actual_hide_writes"),HideWrites);
        Receipt->SetStringField(TEXT("status"),TEXT("DIAGNOSTIC_RUNTIME_TWO_SEABEDS_HIDDEN_RESTORE_REQUIRED"));
        if(!IsApplied() || !Persist()){Restore();return false;}return true;
    }
    bool IsApplied()const
    {
        if(!Applied || Restored || States.Num()!=2 || HideWrites!=2 || !FieldUnchanged())return false;
        for(const FState& S:States)if(!OtherStateUnchanged(S) || S.Component->GetVisibleFlag() || S.Component->IsVisible())return false;
        return true;
    }
    bool Restore()
    {
        if(!Applied)return true;if(Restored)return RestorePassed;
        bool Valid=FieldUnchanged();TArray<TSharedPtr<FJsonValue>> Results;
        for(const FState& S:States)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("component"),S.Path);
            const bool Exists=S.Component.IsValid();J->SetBoolField(TEXT("component_still_exists"),Exists);
            if(Exists)
            {
                const bool Other=OtherStateUnchanged(S);S.Component->SetVisibility(S.Visible,false);++RestoreWrites;
                const bool Visible=S.Component->GetVisibleFlag()==S.Visible;
                J->SetBoolField(TEXT("original_visible_flag"),S.Visible);J->SetBoolField(TEXT("restored_visible_flag"),S.Component->GetVisibleFlag());
                const bool OtherAfter=OtherStateUnchanged(S);
                J->SetBoolField(TEXT("all_other_observed_flags_unchanged"),Other && OtherAfter);Valid&=Other && OtherAfter && Visible;
            }
            else Valid=false;Results.Add(MakeShared<FJsonValueObject>(J));
        }
        Restored=true;Valid&=RestoreWrites==2;
        Receipt->SetArrayField(TEXT("restore_results"),Results);Receipt->SetBoolField(TEXT("all_original_visibility_restored"),Valid);
        Receipt->SetBoolField(TEXT("ocean_field_and_depth_unchanged"),FieldUnchanged());Receipt->SetNumberField(TEXT("actual_restore_writes"),RestoreWrites);
        Receipt->SetStringField(TEXT("status"),Valid?TEXT("DIAGNOSTIC_COMPLETE_OR_ABORTED_TWO_SEABEDS_RESTORED"):TEXT("DIAGNOSTIC_RESTORE_OR_UNCHANGED_STATE_FAILED"));
        RestorePassed=Persist() && Valid;return RestorePassed;
    }
    bool WriteSnapshot(UWorld* W,APlayerController* PC,int32 Fixture,const FString& UniqueDirectory)const
    {
        if(!IsApplied() || (Fixture!=40 && Fixture!=41))return false;
        const FString File=UniqueDirectory/FString::Printf(TEXT("water-attribution-%02d.json"),Fixture);
        if(IFileManager::Get().FileExists(*File))return false;
        auto J=Snapshot(W,PC,Fixture,false);J->SetBoolField(TEXT("diagnostic_seabed_hidden"),true);
        J->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);J->SetNumberField(TEXT("actual_hidden_seabed_components"),2);
        J->SetStringField(TEXT("scope"),TEXT("DIAGNOSTIC B ONLY, exact two cosmetic seabed component visibility disabled; actual Ocean/Field/depth unchanged. Never normal beauty/visual acceptance or performance evidence."));
        IFileManager::Get().MakeDirectory(*UniqueDirectory,true);FString Json;
        return FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json,*File);
    }
};
}
#endif
