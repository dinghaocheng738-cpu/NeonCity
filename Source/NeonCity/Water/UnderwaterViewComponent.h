#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnderwaterViewComponent.generated.h"
class UMaterialInstanceDynamic;
class UCameraComponent;
class ANeonCityDayNightCycle;
class UNiagaraSystem;
class UNiagaraComponent;
class ACoastalOceanSurface;

/** Camera-local optical water path. Independent of the character's breathing point. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UUnderwaterViewComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UUnderwaterViewComponent();
 float GetImmersionWeight() const { return ImmersionWeight; }
 float GetLensWetness() const { return LensWetness; }
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WaterOptics;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LensOptics;
 UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> MarineSnow;
 UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> SnowPatches;
 void UpdateParticulates(float Delta,bool Active,const FVector& Eye,float SurfaceZ);
 FVector LastSnowOrigin=FVector::ZeroVector;
 float SnowClock=6;
 uint32 OpticalFrame=0;
 TWeakObjectPtr<ANeonCityDayNightCycle> DayNight;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;
 float ImmersionWeight=0;
 float OriginalMinEV=0,WaterMinEV=-1.5f,ExposureWeight=0;
 bool bOriginalMinEVOverride=false;
 float LensWetness=0,SubmergedSeconds=0,ExitAge=0,ExitSeed=0;
 FVector PreviousEye=FVector::ZeroVector;
 bool bEyeInitialized=false,bWasUnderwater=false;
};
