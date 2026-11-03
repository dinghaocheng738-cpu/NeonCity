#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "WeaponEffectsSubsystem.generated.h"
class UParticleSystem;
class USoundBase;
class USoundAttenuation;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;
class AController;
class UNiagaraSystem;

/** What a shot sounds and looks like at the muzzle. */
UENUM()
enum class ENeonCityWeaponReport : uint8 { SmallArms, MachineGun, Cannon, Rocket };

/** Muzzle flash: a star of hot gas along the bore and a short light pulse. */
UCLASS(NotPlaceable)
class NEONCITY_API ANeonCityMuzzleFlash : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityMuzzleFlash();
 void Configure(float Scale,UMaterialInterface* Material);
protected:
 virtual void Tick(float Delta) override;
private:
 UPROPERTY() TObjectPtr<UPointLightComponent> Light;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Petals;
 float Peak=0,Age=0,Size=1;
 static constexpr float Lifetime=.06f;
};

/** Presentation of every shot and round strike, for any shooter (player, AI, vehicle mounts):
 * muzzle flash, report and its echo, round impacts by surface (sparks / dust / splinters, a hole decal,
 * a crack or thud), water entry splashes, and the round's momentum delivered to whatever simulates physics.
 * Bounded per frame window so a long burst never floods emitters or voices. */
UCLASS()
class NEONCITY_API UNeonCityWeaponEffectsSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 void Report(const FTransform& Muzzle,ENeonCityWeaponReport Kind);
 /** Calibre: 1 = heavy machine gun round; impulse, effect size and decal scale with it. */
 void Impact(const FHitResult& Hit,const FVector& Velocity,float Calibre,const AController* Instigator=nullptr);
 void WaterEntry(const FVector& Point,float Calibre);
 /** Smoke a rocket / missile motor leaves behind it (call along the flight path; Density 1 = burning motor). */
 void TrailPuff(const FVector& Point,float Density=1.f);
 /** The launch cloud at a rail / tube. */
 void LaunchSmoke(const FVector& Point,const FVector& Direction);
 /** A warhead's detonation look: fireball, smoke column, sparks (Scale 1 = a Hellfire). */
 /** Fireball, smoke column and sparks of a blast (Niagara); false while those assets are missing. */
 bool Detonation(const FVector& Point,float Scale);
 /** Last time a round from this shooter struck something that takes damage (hit marker). */
 double GetLastConfirmedHit(const AController* Instigator) const;
 int32 GetImpactCount() const { return ImpactCount; }
 int32 GetReportCount() const { return ReportCount; }
 /** Momentum a round of this calibre delivers (kg cm/s): heavy MG ~5x the real 12.7 mm round so a burst visibly
  * rocks a parked car and throws loose props; cannon shells (calibre 3) a real 30 mm round's ~3x. */
 static float RoundImpulse(float Calibre) { return 20000.f*Calibre; }
private:
 void Load();
 bool Budget(int32& Used,int32 Max);
 void Burst(UParticleSystem* System,const FVector& Location,const FRotator& Rotation,float Scale,float Seconds);
 void Niagara(UNiagaraSystem* System,const FVector& Location,const FRotator& Rotation,float Scale);
 void Sound(USoundBase* Sound,const FVector& Location,float Volume,float Pitch,USoundAttenuation* Falloff);
 bool bLoaded=false,bFXLoaded=false;
 UPROPERTY() TObjectPtr<UParticleSystem> Sparks;
 UPROPERTY() TObjectPtr<UParticleSystem> Smoke;
 UPROPERTY() TObjectPtr<UParticleSystem> Steam;
 UPROPERTY() TObjectPtr<UParticleSystem> Blast;
 UPROPERTY() TObjectPtr<UMaterialInterface> Hole;
 UPROPERTY() TObjectPtr<UNiagaraSystem> PuffFX;
 UPROPERTY() TObjectPtr<UNiagaraSystem> LaunchFX;
 UPROPERTY() TObjectPtr<UNiagaraSystem> FireballFX;
 UPROPERTY() TObjectPtr<UNiagaraSystem> BlastSmokeFX;
 UPROPERTY() TObjectPtr<UNiagaraSystem> SparksFX;
 UPROPERTY() TObjectPtr<UMaterialInterface> Flash;
 UPROPERTY() TObjectPtr<USoundBase> MGShot;
 UPROPERTY() TObjectPtr<USoundBase> CannonShot;
 UPROPERTY() TObjectPtr<USoundBase> RocketLaunch;
 UPROPERTY() TArray<TObjectPtr<USoundBase>> HardHits;
 UPROPERTY() TObjectPtr<USoundBase> SoftHit;
 UPROPERTY() TObjectPtr<USoundBase> WaterHit;
 UPROPERTY() TObjectPtr<USoundAttenuation> ShotFalloff;
 UPROPERTY() TObjectPtr<USoundAttenuation> HitFalloff;
 TMap<TWeakObjectPtr<const AController>,double> ConfirmedHits;
 double WindowStart=-1;int32 EmittersUsed=0,VoicesUsed=0,DecalsUsed=0;
 int32 ImpactCount=0,ReportCount=0;
};
