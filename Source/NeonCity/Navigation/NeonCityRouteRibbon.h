#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityRouteRibbon.generated.h"
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** The active route drawn in the world: glowing chevrons laid on the ground along the planned path ahead of the
 * player, flowing toward the destination. Local presentation only (spawned by the navigation component, no
 * collision, no shadows); it re-lays itself every frame from the route the minimap shows. */
UCLASS(NotPlaceable)
class NEONCITY_API ANeonCityRouteRibbon : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityRouteRibbon();
 /** Lay the chevrons from the point of Route nearest Origin, along the route, ahead of the player. */
 void Present(const TArray<FVector>& Route,const FVector& Origin,bool bDriving,float Time);
 void Clear();
 int32 GetChevronCount() const { return Shown/2; }
 /** World transform of a laid chevron arm (diagnostics). */
 bool GetArm(int32 Index,FTransform& Out) const;
private:
 UStaticMeshComponent* Arm(int32 Index);
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Arms;
 UPROPERTY() TObjectPtr<UStaticMesh> Cube;
 UPROPERTY() TObjectPtr<UMaterialInterface> Glow;
 int32 Shown=0;
};
