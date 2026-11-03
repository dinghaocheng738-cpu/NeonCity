#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityMainlandSite.generated.h"

class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UCoastalWaveData;

/** Explicit replacement of one retired mainland car's presentation profile. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityMainlandFleetReplacement
{
 GENERATED_BODY()

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mainland Fleet")
 FGuid VehicleID;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mainland Fleet")
 FName PreviousProfile;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mainland Fleet")
 FName ReplacementProfile;
};

/** Authored transport placements for the replacement mainland environment.
 * Does not spawn vehicles or own transport/persistence state. Worlds without a
 * persistent-level marker retain the existing isolated development fixtures. */
UCLASS()
class NEONCITY_API ANeonCityMainlandSite : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityMainlandSite();

 /** Centimetres relative to the site's position/rotation. Scale is not inherited. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Transport", meta=(MakeEditWidget=true))
 FTransform SpeedboatBerth = FTransform(FRotator(0,-90,0), FVector(5000,-4100,-134.5));

 /** Optional owned appearance derivative. Bootstrap changes only the existing
  * NoCollision visual component; physical hull/profile/save identity stay native. */
 UPROPERTY(EditInstanceOnly, Category="Mainland Site|Transport")
 TObjectPtr<UStaticMesh> SpeedboatVisualMesh = nullptr;

 /** Hull spawn pose; authored landing geometry must support the helicopter. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Transport", meta=(MakeEditWidget=true))
 FTransform HelicopterLanding = FTransform(FRotator(0,-90,0), FVector(12000,6000,137));

 /** The new authored scene supplies piers, steps, landing support and signage. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Transport")
 bool bReplaceLegacyTransportFixtures = true;

 /** The authored mainland replaces the retired rectangular dry-land mask.
  * Applied only to this world's ocean MID; the shared ocean and island retain
  * their existing material, bathymetry and generated coastline coverage. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Ocean")
 bool bReplaceLegacyMainlandOceanCoverage = false;

 /** Optional local seabed data for the existing runtime marine bootstrap.
  * Null retains its canonical field; no extra provider or fixture is spawned. */
 UPROPERTY(EditInstanceOnly, Category="Mainland Site|Ocean")
 TObjectPtr<UCoastalWaveData> OceanField = nullptr;

 /** Optional rendering derivative for the existing provider. Its material
  * must preserve the canonical ocean outside its authored local region. */
 UPROPERTY(EditInstanceOnly, Category="Mainland Site|Ocean")
 TObjectPtr<UMaterialInterface> OceanRenderingMaterial = nullptr;

 /** Explicit adoption opt-in. Does not affect supported saves or the island. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Save Recovery")
 bool bRecoverUnsupportedMainlandVehicles = false;

 /** Absolute world centimetres, independent of this site's transform. May narrow
  * the retired mainland envelope; cannot expand recovery into offshore regions. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Save Recovery")
 FVector RetiredMainlandMin = FVector(-160000,-3000,-20000);

 UPROPERTY(EditAnywhere, Category="Mainland Site|Save Recovery")
 FVector RetiredMainlandMax = FVector(160000,160000,40000);

 /** Empty by default. Each exact ID/previous-profile row explicitly opts one
  * retired mainland vehicle into the three selected hero presentations. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Fleet")
 TArray<FNeonCityMainlandFleetReplacement> FleetProfileReplacements;

 /** Explicit authored presentation for the existing weapon shop interface. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Commerce")
 TObjectPtr<UStaticMesh> WeaponStoreMesh = nullptr;

 UPROPERTY(EditAnywhere, Category="Mainland Site|Commerce", meta=(MakeEditWidget=true))
 FTransform WeaponStorePlacement = FTransform::Identity;

#if WITH_EDITORONLY_DATA
 /** Finish the authored scene's editor asset/shader preparation while loading.
  * Opt-in for uncooked presentation maps; cooked games use their built assets. */
 UPROPERTY(EditAnywhere, Category="Mainland Site|Loading")
 bool bPrepareEditorPresentationBeforePlay = false;
#endif

 FTransform GetSpeedboatBerth() const;
 FTransform GetHelicopterLanding() const;
 FTransform GetWeaponStorePlacement() const;
 void ApplyLegacyTransportFixturePolicy(AActor& Helper) const;
 void ApplyOceanCoverage(UMaterialInstanceDynamic& OceanMaterial) const;
 bool GetFleetProfileReplacement(FGuid VehicleID,FName SavedProfile,FName& ReplacementProfile) const;

 /** Value-only resolver, shared with authoring validation. Duplicate IDs and
  * canonical island vehicles can never request a mainland profile replacement. */
 static bool ResolveFleetProfileReplacement(TConstArrayView<FNeonCityMainlandFleetReplacement> Replacements,
  FGuid VehicleID,FName SavedProfile,FName& ReplacementProfile);

 /** Only the persistent mainland may opt in; streamed island markers are ignored. */
 static ANeonCityMainlandSite* Find(const UWorld* World);
protected:
#if WITH_EDITOR
 virtual void PostInitializeComponents() override;
#endif
private:
 FTransform ToWorldPose(const FTransform& LocalPose) const;
};
