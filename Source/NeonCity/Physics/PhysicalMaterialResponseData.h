#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterialResponseData.generated.h"
USTRUCT(BlueprintType)
struct FPhysicalSurfaceResponse
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) float DamageScale=1;
 UPROPERTY(EditAnywhere) float Friction=.7f;
 UPROPERTY(EditAnywhere) float Restitution=.05f;
};
/** Surface IDs use UE physical materials; no new global collision channels are required. */
UCLASS(BlueprintType)
class NEONCITY_API UPhysicalMaterialResponseData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) TMap<TEnumAsByte<EPhysicalSurface>,FPhysicalSurfaceResponse> Surfaces;
 FPhysicalSurfaceResponse Get(EPhysicalSurface Type) const
 {
  FPhysicalSurfaceResponse R; if(const auto* Found=Surfaces.Find(Type)) R=*Found;
  R.DamageScale=FMath::IsFinite(R.DamageScale)?FMath::Clamp(R.DamageScale,.1f,2.f):1;
  R.Friction=FMath::IsFinite(R.Friction)?FMath::Clamp(R.Friction,.1f,1.f):.7f;
  R.Restitution=FMath::IsFinite(R.Restitution)?FMath::Clamp(R.Restitution,0.f,.15f):.05f; return R;
 }
};
