#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityReferenceVillage.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** Native licensed island village: static support plus bounded decorative wind/light. */
UCLASS()
class NEONCITY_API ANeonCityReferenceVillage : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityReferenceVillage();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void BeginPlay() override;
 UPROPERTY(EditAnywhere,Category="NeonCity|Marine",meta=(ClampMin="0",ClampMax="20")) float WindSpeed=7;
 UPROPERTY(EditAnywhere,Category="NeonCity|Marine") FVector2D WindDirection=FVector2D(.6,.8);
 /** Local revisions retain the canonical village and dock assets. */
 UPROPERTY(EditAnywhere,Category="NeonCity|Land") TMap<int32,TObjectPtr<UStaticMesh>> LandSurfaceOverrides;
 UPROPERTY(EditAnywhere,Category="NeonCity|Land") TArray<int32> OpenBuildingContacts;
 /** Open rail proxies protect bodies; their empty visual gaps must not mask interactions. */
 UPROPERTY(EditAnywhere,Category="NeonCity|Land") TArray<int32> SightTransparentContacts;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Surfaces;
 UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
 struct FSwing { double T=0,A=0,AV=0,B=0,BV=0,Phase=0; };
 TArray<FSwing> Swings;
 FSwing Sign;
 float Refresh=0;
};
