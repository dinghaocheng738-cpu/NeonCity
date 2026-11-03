#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "MarineWaterEffectsComponent.generated.h"
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ACoastalOceanSurface;
class UNiagaraSystem;
class UNiagaraComponent;
class ANeonCityDayNightCycle;

/** Bounded cosmetic water particles; no collision, damage or hull forces. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UMarineWaterEffectsComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UMarineWaterEffectsComponent();
 void UpdateEffects(const FMarineDriveTelemetry& State,FVector HullExtent,float Delta);
 int32 GetActiveWakeCount() const;
 int32 GetActiveSprayCount() const;
 int32 GetParticleCapacity() const { return 72+64*48; }
 int32 GetSprayLayerBurstCount(int32 Kind) const { return Kind>=0 && Kind<5?LayerBursts[Kind]:0; }
 int32 GetContactBurstCount() const { return TotalContactBursts; }
 /** Cosmetic opt-in, anchored to the visible transom rather than the physics box. */
 void ConfigureTransomWash(FVector LocalAnchor,float HalfBeamCm);
 bool HasTransomWash() const { return bTransomWash; }
 FVector GetTransomWashLocalAnchor() const { return TransomAnchor; }
 int32 GetTransomWashEmissionCount() const { return TransomEmissions; }
 UPROPERTY(EditDefaultsOnly,Category="Marine|Effects") TSoftObjectPtr<UMaterialInterface> FoamMaterial;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 struct FParticle
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
  float Age=0,Life=0,Size=1,Angle=0;
  bool bSpray=false,bTransom=false;
 };
 void Clear();
 void Emit(bool Spray,FVector Position,FVector Velocity,float Size,float Angle,bool Transom=false);
 void UpdateTransomWash(const FMarineDriveTelemetry& State,FVector Forward,FVector Right,float Delta);
 void UpdateContactSpray(const FMarineDriveTelemetry& State,FVector Extent,float Delta);
 struct FContactState { float Depth=0,Rate=0,Carry[5]={}; bool Primed=false; };
 FContactState Contacts[6];
 UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> ContactSpray;
 UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> SprayLayers;
 UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> SprayBursts;
 int32 TotalContactBursts=0;
 int32 LayerBursts[5]={};
 float LastSpraySpeed=0,SmoothedAcceleration=0,ImpactBurst=0,WashCarry=0;
 UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Particles;
 TArray<FParticle> Pool;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;
 TWeakObjectPtr<ANeonCityDayNightCycle> DayNight;
 TArray<FTransform> Transforms;
 FVector LastPosition=FVector::ZeroVector;
 float WakeClock=0,SprayClock=0;
 int32 WakeCursor=0,SprayCursor=48;
 bool bVisible=false;
 bool bTransomWash=false;
 FVector TransomAnchor=FVector::ZeroVector;
 float TransomHalfBeamCm=0,TransomClock=0;
 int32 TransomEmissions=0;
};
