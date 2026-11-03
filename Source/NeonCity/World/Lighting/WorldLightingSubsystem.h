#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldLightingSubsystem.generated.h"

class ULocalLightComponent;
class UMaterialInstanceDynamic;

/** Photometry stays in the component's explicitly authored units. Distances are centimetres. */
struct FNeonCityLocalLightPolicy
{
 float Intensity=1000;
 float DayMultiplier=0;
 float ShadowDistance=1800;
 float CullDistance=6000;
 float VolumetricScattering=.35f;
 float MidDistanceMultiplier=1;
 bool bCastShadows=true;
 bool bEnabled=true;
 // Local player headlamp / searchlight may reserve priority, still inside the global budget.
 int32 Priority=0;
 bool IsValid() const;
};

/** One world clock input, one bounded light scheduler, no per-lamp time polling. */
UCLASS()
class NEONCITY_API UWorldLightingSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void OnWorldBeginPlay(UWorld& InWorld) override;
 virtual void Deinitialize() override;
 bool RegisterLight(ULocalLightComponent* Light,const FNeonCityLocalLightPolicy& Policy);
 void UnregisterLight(ULocalLightComponent* Light);
 void SetLightEnabled(ULocalLightComponent* Light,bool bEnabled);
 void RegisterNightMaterial(UMaterialInstanceDynamic* Material,FName Parameter=TEXT("Night"));
 /** Called only by the world clock; also safe before actors register or after streaming. */
 void SetTimeOfDay(float Hours);
 void RefreshLighting();
 UFUNCTION(BlueprintPure,Category="NeonCity|Lighting") float GetNightAmount() const { return NightAmount; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Lighting") int32 GetActiveLightCount() const { return ActiveLightCount; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Lighting") int32 GetShadowLightCount() const { return ShadowLightCount; }
 int32 GetRegisteredLightCount() const { return Lights.Num(); }
 static constexpr int32 DynamicLightBudget=24;
 static constexpr int32 ShadowLightBudget=6;
 static constexpr float UpdateInterval=.25f;
private:
 struct FLocalLight
 {
  TWeakObjectPtr<ULocalLightComponent> Component;
  FNeonCityLocalLightPolicy Policy;
 };
 struct FNightMaterial
 {
  TWeakObjectPtr<UMaterialInstanceDynamic> Material;
  FName Parameter;
 };
 TArray<FLocalLight> Lights;
 TArray<FNightMaterial> Materials;
 FTimerHandle RefreshTimer;
 /** The West Lake city's shared parameters (MPC_WL_City): Night drives tower windows, shop fronts and screens. */
 UPROPERTY(Transient) TObjectPtr<class UMaterialParameterCollection> CityParameters;
 void ApplyCityParameters();
 float NightAmount=0;
 int32 ActiveLightCount=0,ShadowLightCount=0;
 void ApplyClockState();
};
