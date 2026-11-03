#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Destruction/DestructionTypes.h"
#include "BreakableSurfaceData.generated.h"
UENUM(BlueprintType)
enum class ENeonCitySurfaceKind : uint8 { Concrete, Glass, StorefrontGlass, Door, Sign, StreetLight, Billboard, Barrier, Fence, TrashCan };
/** Authored break policy for one environmental surface class. Health itself stays in HealthComponent. */
UCLASS(BlueprintType)
class NEONCITY_API UBreakableSurfaceData : public UDataAsset
{
 GENERATED_BODY()
public:
 UBreakableSurfaceData();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") ENeonCitySurfaceKind Kind=ENeonCitySurfaceKind::Concrete;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") FName SurfaceID=TEXT("NeonCity.Surface.Concrete");
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") bool bGlass=false;
 /** Health fraction at or below which the surface shows the intermediate damaged state. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface",meta=(ClampMin="0.05",ClampMax="1")) float DamagedThreshold=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface",meta=(ClampMin="0.01",ClampMax="0.95")) float BrokenThreshold=.3f;
 /** Contact speed at which a direct collision may shatter the surface without waiting for health. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface",meta=(ClampMin="0",ClampMax="6000")) float ShatterSpeed=1600;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface",meta=(ClampMin="0",ClampMax="6000")) float MinimumImpactSpeed=250;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") bool bCollapseWhenBroken=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") bool bHiddenWhenBroken=false;
 /** Cracks keep the mesh visible so the player can read the damage before it breaks. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Surface") bool bHiddenWhenCracked=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Budget",meta=(ClampMin="0",ClampMax="4")) float FragmentBudget=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Budget",meta=(ClampMin="0",ClampMax="1200")) float MaximumSimulationDistance=6000;
 /** Opt-in flag for the future persistent-destruction interface; transient city damage is restored on load. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Persistence") bool bPersistent=false;
 bool IsValidProfile() const;
 EPhysicsPropState StateFor(float HealthFraction,float DamageAmount) const;
 ENeonCityGlassState GlassStateFor(EPhysicsPropState State) const;
};
namespace NeonCitySurfaces
{
 NEONCITY_API const UBreakableSurfaceData& Profile(ENeonCitySurfaceKind Kind);
 NEONCITY_API TArray<ENeonCitySurfaceKind> AllKinds();
}