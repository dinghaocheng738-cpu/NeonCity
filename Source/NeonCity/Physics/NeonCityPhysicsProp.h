#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/DamageInterface.h"
#include "Explosion/ExplosionResponseInterface.h"
#include "Physics/DestructiblePropComponent.h"
#include "Destruction/BreakableSurfaceData.h"
#include "NeonCityPhysicsProp.generated.h"
class UStaticMeshComponent;
class UPointLightComponent;
class UHealthComponent;
class UPhysicalMaterial;
class UDestructibleStateComponent;
/** Road Cone, Trash Can, Road Barrier, Sign, Lamp Post, Fence, Glass, Static Block, Billboard, Door, Storefront Glass, Small Wall. */
UENUM(BlueprintType)
enum class ECityPhysicsProp : uint8 { RoadCone, TrashCan, RoadBarrier, Sign, LampPost, Fence, Glass, StaticBlock, Billboard, Door, StorefrontGlass, ConcreteWall };
/** Lightweight prototype geometry, no fracture assets. Configure before FinishSpawning. */
UCLASS()
class NEONCITY_API ANeonCityPhysicsProp : public AActor, public IDamageInterface, public IExplosionResponseInterface
{
 GENERATED_BODY()
public:
 ANeonCityPhysicsProp();
 void Configure(ECityPhysicsProp Type);
 void SetSurfaceProfile(UBreakableSurfaceData* Profile);
 virtual UHealthComponent* GetDamageHealth_Implementation() const override { return Destruction->Mode==EPhysicsPropMode::Static?nullptr:Health; }
 virtual bool ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event) override;
 UDestructiblePropComponent* GetDestruction() const { return Destruction; }
 UDestructibleStateComponent* GetSurfaceState() const { return Surface; }
 UStaticMeshComponent* GetMesh() const { return Mesh; }
 UPointLightComponent* GetLight() const { return Light; }
 UHealthComponent* GetHealthComponent() const { return Health; }
 float GetLastDamageTime() const { return Destruction?Destruction->GetLastDamageTime():-1000; }
	ECityPhysicsProp GetPropType() const { return PropType; }
private:
 UFUNCTION() void UpdateArtState(EPhysicsPropState Previous,EPhysicsPropState Current);
 UPROPERTY() TObjectPtr<UStaticMeshComponent> ArtSurface;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
 UPROPERTY() TObjectPtr<UPointLightComponent> Light;
 UPROPERTY() TObjectPtr<UHealthComponent> Health;
 UPROPERTY() TObjectPtr<UDestructiblePropComponent> Destruction;
 UPROPERTY() TObjectPtr<UDestructibleStateComponent> Surface;
 UPROPERTY() TObjectPtr<UPhysicalMaterial> Material;
 ECityPhysicsProp PropType=ECityPhysicsProp::RoadCone;
};
