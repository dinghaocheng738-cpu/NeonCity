#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Actor.h"
#include "Explosion/ExplosionEvent.h"
#include "ExplosionPresentationSubsystem.generated.h"
class UParticleSystem;
class USoundBase;
class USoundAttenuation;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

/** Sharp jolt that decays over the duration; scale comes from the camera's distance to the blast. */
UCLASS()
class NEONCITY_API UNeonCityExplosionShakePattern : public UCameraShakePattern
{
 GENERATED_BODY()
public:
 float Duration=.9f;
 float RotationAmplitude=2.2f;
 float LocationAmplitude=9.f;
 float Frequency=17.f;
protected:
 virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
 virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
 virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,FCameraShakePatternUpdateResult& OutResult) override;
 virtual bool IsFinishedImpl() const override { return Elapsed>=Duration; }
private:
 float Elapsed=0;
 FVector Seed=FVector::ZeroVector;
};

UCLASS()
class NEONCITY_API UNeonCityExplosionCameraShake : public UCameraShakeBase
{
 GENERATED_BODY()
public:
 UNeonCityExplosionCameraShake(const FObjectInitializer& Initializer);
};

/** Short white-orange light pulse at the fireball. */
UCLASS(NotPlaceable)
class NEONCITY_API ANeonCityExplosionFlash : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityExplosionFlash();
 void Configure(float Scale);
protected:
 virtual void Tick(float Delta) override;
private:
 UPROPERTY() TObjectPtr<UPointLightComponent> Light;
 float Peak=0,Age=0;
 static constexpr float Lifetime=.45f;
};

/** Ground fragments thrown by a surface blast: simulated chunks that bounce, roll and settle, then expire. */
UCLASS(NotPlaceable)
class NEONCITY_API ANeonCityExplosionDebris : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityExplosionDebris();
 void Throw(const FVector& SurfaceNormal,float Scale,int32 Count,UMaterialInterface* Material);
 int32 GetChunkCount() const { return Chunks.Num(); }
 static constexpr int32 MaximumChunks=18;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Chunks;
};

/** Presents every blast the explosion subsystem resolves: fireball, smoke column, lingering fire, light flash,
 * distance-delayed sound, camera shake for nearby players, thrown ground debris and a scorch mark. Visual only:
 * damage and impulse stay with UNeonCityExplosionSubsystem. */
UCLASS()
class NEONCITY_API UNeonCityExplosionPresentationSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void OnWorldBeginPlay(UWorld& InWorld) override;
 virtual void Deinitialize() override;
 int32 GetPresentedCount() const { return Presented; }
 int32 GetActiveDebrisCount() const;
 static constexpr int32 MaximumDebrisActors=6;
private:
 UFUNCTION() void Present(const FNeonCityExplosionEvent& Event);
 void LoadAssets();
 UPROPERTY() TObjectPtr<UParticleSystem> Fireball;
 UPROPERTY() TObjectPtr<UParticleSystem> Smoke;
 UPROPERTY() TObjectPtr<UParticleSystem> Fire;
 UPROPERTY() TObjectPtr<USoundBase> Boom;
 UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
 UPROPERTY() TObjectPtr<UMaterialInterface> Scorch;
 UPROPERTY() TObjectPtr<UMaterialInterface> Charred;
 TArray<TWeakObjectPtr<ANeonCityExplosionDebris>> Debris;
 int32 Presented=0;
 bool bLoaded=false;
};
