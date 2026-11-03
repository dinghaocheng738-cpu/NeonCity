#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferenceIslandLibrary.generated.h"

class ALandscape;
class UMaterialInterface;
class UMaterial;
class USkeletalMeshComponent;
class UAnimSequence;
class UStaticMesh;
class UTexture2D;

/** Editor-only, bounded heightfield import. Runtime uses standard Landscape collision and LOD. */
UCLASS()
class NEONCITYARTEDITOR_API UReferenceIslandLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Import the verified 2041-square reference field into an empty editor level.
     * Canonical metres (X,Y-up,Z) become UE centimetres (X,-Z,Y), sea level zero.
     * Placement relative to South Beach belongs to the streaming level transform.
     */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static ALandscape* ImportReferenceLandscape(const FString& HeightmapPath, UMaterialInterface* Material);
    /** Apply the reviewed road-only heightfield to the owned island, including native collision. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static bool ApplyAdventureRoadEarthworks(ALandscape* Landscape, const FString& HeightmapPath);
    /** Collision height in metres at canonical UE-local XY; -1e9 means no landscape hit. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static double SampleReferenceGroundHeight(ALandscape* Landscape, FVector2D PositionCm);
    /** Finish editor compilation and return actual material errors, not fallback-render success. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceMaterialErrors(UMaterial* Material);
    /** Rebuild and validate every owned Landscape component's real material permutation. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString ApplyReferenceLandscapeMaterial(ALandscape* Landscape, UMaterial* Material);
    /** Flush deferred editor component transforms before an offline capture; never used at runtime. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static void FlushReferenceEditorRendering();
    /** Editor-only source/platform texture diagnostics. Optionally finish only
     * this texture's pending compilation; never import, modify settings or
     * save a package. Pending platform fields remain null until safe to read. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceTexturePlatformStats(UTexture2D* Texture, bool bFinishCompilation=false);
    /** Editor intake recovery for the exact Ground32/34 data textures only.
     * Rebuilds missing derived platform payload synchronously using UTexture's
     * documented resource API. Does not change source/settings or save assets. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString RebuildReferenceGround32Texture(UTexture2D* Texture);
    /** Read-only editor digest of fallback triangle attributes/section flags and
     * persistent Nanite pages/hierarchy; independent of recalculated bounds. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceStaticMeshGeometryDigest(UStaticMesh* Mesh);
    /** Original imported LOD0 positions, independent of Nanite's conservative
     * render envelope. Read-only centimetre bounds for offline source checks. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceStaticMeshSourceBounds(UStaticMesh* Mesh);
    /** Read-only JSON statistics of the actual final fallback render T/B/N.
     * Finishes pending compilation, requires resident CPU tangent data, and
     * reports source MeshDescription separately. Does not inspect Nanite's
     * internal basis, rebuild a mesh, enable CPU access or save a package. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceStaticMeshTangentBasisStats(UStaticMesh* Mesh);
    /** Read the actual imported float32 corner inputs for the shipped Mikk
     * diagnostic. Writes a fresh Saved/Phase205cUpgrade binary only; never
     * changes mesh attributes, build settings, geometry or packages. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static bool ExportReferenceStaticMeshMikkInputs(UStaticMesh* Mesh,const FString& Filename);
    /** Restore canonical wind/culling bounds only on the exact C920 or C923
     * palm copy/source pairs after material binding. Requires identical geometry and bounds
     * extensions; never saves a package or changes the canonical source. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static bool CopyReferencePalmBounds(UStaticMesh* Candidate, UStaticMesh* Source);
    /** Exact fallback render-attribute union, including section flags. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceStaticMeshFallbackUnionDigest(const TArray<UStaticMesh*>& Meshes, bool ExportDiagnostic=false);
    /** Offline candidate only: retain one of six dominant face directions.
     * The caller supplies an owned duplicate under Garage888; source is never edited. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static int32 RetainReferenceMeshOrientation(UStaticMesh* Candidate, int32 Direction);
    /** Read-only native render versus ray-tracing-proxy surface inventory. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static FString GetReferenceRayTracingProxyInventory(UStaticMesh* Mesh);
    /** Evaluate a native reference clip for offline pose inspection, without advancing gameplay. */
    UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
    static bool SetReferenceCharacterPose(USkeletalMeshComponent* Component, UAnimSequence* Sequence, float Seconds);
};
