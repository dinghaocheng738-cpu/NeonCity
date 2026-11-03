#include "ReplicaAssetLibrary.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "Adventure/IslandSupercarPresentationData.h"

FString UReplicaAssetLibrary::ConfigureReplicaRoofContact(UStaticMesh* Mesh,UIslandSupercarPresentationData* Profile)
{
    if (!GEditor || GEditor->PlayWorld || !Mesh || !Profile ||
        Mesh->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/Assembly393/SM_McLaren720S_RHTContact.SM_McLaren720S_RHTContact") ||
        !Profile->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/Assembly393/")))
        return TEXT("Owned replica roof contact copy and profile required");
    UBodySetup* Body=Mesh->GetBodySetup();
    if (!Body) return TEXT("Roof body setup absent");
    Mesh->Modify();Body->Modify();Body->AggGeom.EmptyElements();
    // A non-simulating kinematic panel follows the actual manufactured shell.
    // It must not use a solid cabin box that survives in the open cockpit.
    Body->CollisionTraceFlag=CTF_UseComplexAsSimple;
    Body->bDoubleSidedGeometry=true;Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
    Mesh->MarkPackageDirty();Profile->Modify();Profile->Roof=Mesh;Profile->MarkPackageDirty();
    return FString();
}

FString UReplicaAssetLibrary::ConfigureRoofPresentationMesh(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials)
{
    if (!GEditor || GEditor->PlayWorld || !Mesh ||
        (!Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/")) &&
         !Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland914/Cars/SM_Carrera914_")) &&
         !Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland916/Fleet/SM_GrandTourer916_"))))
        return TEXT("An owned replica presentation mesh in an editor authoring session is required");
    if (Mesh->GetNumSourceModels() < 1 || Materials.Num() != Mesh->GetStaticMaterials().Num())
        return TEXT("Imported source model/material slots do not match the requested bindings");
    for (const UMaterialInterface* Material : Materials)
        if (!Material) return TEXT("Missing presentation material");
    const int32 SourceSections = Mesh->GetSectionInfoMap().GetSectionNumber(0);
    for (int32 Section = 0; Section < SourceSections; ++Section)
        if (!Mesh->GetStaticMaterials().IsValidIndex(Mesh->GetSectionInfoMap().Get(0, Section).MaterialIndex))
            return TEXT("Source section has an invalid material index");

    Mesh->Modify();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
        Mesh->GetStaticMaterials()[Index].MaterialInterface = Materials[Index];
    const FMeshBuildSettings SourceBuild = Mesh->GetSourceModel(0).BuildSettings;
    const FMeshReductionSettings SourceReduction = Mesh->GetSourceModel(0).ReductionSettings;
    const float Ratios[] = {1.f, .5f, .18f, .06f};
    const float Screens[] = {1.f, .35f, .12f, .03f};
    Mesh->SetNumSourceModels(4);
    for (int32 Level = 0; Level < 4; ++Level)
    {
        auto& Model = Mesh->GetSourceModel(Level);
        Model.BuildSettings = SourceBuild;
        Model.BuildSettings.bUseFullPrecisionUVs = true;
        Model.BuildSettings.bGenerateLightmapUVs = false;
        Model.ReductionSettings = SourceReduction;
        Model.ReductionSettings.PercentTriangles = Ratios[Level];
        Model.ReductionSettings.BaseLODModel = 0;
        Model.ScreenSize = Screens[Level];
    }
    Mesh->SetAutoComputeLODScreenSize(false);
    Mesh->GetNaniteSettings().bEnabled = false;
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->AggGeom.EmptyElements();
        Body->InvalidatePhysicsData();
    }

    // Seed source section flags before generation. Reduced section numbering
    // can differ, so check the actual generated sections in one bounded pass.
    for (int32 Section = 0; Section < SourceSections; ++Section)
    {
        FMeshSectionInfo Info = Mesh->GetSectionInfoMap().Get(0, Section);
        if (Mesh->GetStaticMaterials()[Info.MaterialIndex].MaterialSlotName.ToString().Contains(TEXT("glass"), ESearchCase::IgnoreCase))
            Info.bCastShadow = false;
        for (int32 Level = 0; Level < 4; ++Level)
        {
            Mesh->GetSectionInfoMap().Set(Level, Section, Info);
            Mesh->GetOriginalSectionInfoMap().Set(Level, Section, Info);
        }
    }
    Mesh->PostEditChange();
    TArray<UStaticMesh*> OwnedMeshes{Mesh};
    FStaticMeshCompilingManager::Get().FinishCompilation(OwnedMeshes);
    const FStaticMeshRenderData* Render = Mesh->GetRenderData();
    if (!Render || Render->LODResources.Num() != 4)
        return TEXT("Four native LOD resources were not built");
    bool bChangedFlags = false;
    for (int32 Level = 0; Level < 4; ++Level)
    {
        for (int32 Section = 0; Section < Render->LODResources[Level].Sections.Num(); ++Section)
        {
            const int32 MaterialIndex = Render->LODResources[Level].Sections[Section].MaterialIndex;
            if (!Mesh->GetStaticMaterials().IsValidIndex(MaterialIndex))
                return TEXT("Generated section has an invalid material index");
            if (!Mesh->GetStaticMaterials()[MaterialIndex].MaterialSlotName.ToString().Contains(TEXT("glass"), ESearchCase::IgnoreCase))
                continue;
            FMeshSectionInfo Info = Mesh->GetSectionInfoMap().Get(Level, Section);
            if (Info.bCastShadow)
            {
                Info.bCastShadow = false;
                Mesh->GetSectionInfoMap().Set(Level, Section, Info);
                Mesh->GetOriginalSectionInfoMap().Set(Level, Section, Info);
                bChangedFlags = true;
            }
        }
    }
    if (bChangedFlags)
    {
        Mesh->PostEditChange();
        FStaticMeshCompilingManager::Get().FinishCompilation(OwnedMeshes);
    }
    Mesh->MarkPackageDirty();
    return FString();
}
