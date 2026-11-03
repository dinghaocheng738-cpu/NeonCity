#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/ExplosionDamageComponent.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
namespace NeonCityRocketMotor
{
// Pod rockets light in the tube; a rail missile drops clear of the airframe first.
constexpr float MissileDrop=.12f,LaunchFraction=.35f,BoostTime=.45f;
// Fins weathervane the round back onto its launch line after the drop (deg/s) while it is unguided.
constexpr float FinAuthority=40.f;
}
using namespace NeonCityRocketMotor;
ARocketProjectile::ARocketProjectile()
{
 Explosion=CreateDefaultSubobject<UExplosionDamageComponent>(TEXT("Explosion"));
 // A rocket is a body, not a tracer streak: the Hydra 70 airframe (nozzle origin), centred on the round.
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Hydra(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_Hydra.SM_NC_Hydra"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Flame(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_MotorFlame.SM_NC_MotorFlame"));
 if(Hydra.Succeeded())
 {
  TracerMesh->SetStaticMesh(Hydra.Object);TracerMesh->SetRelativeScale3D(FVector(1));TracerMesh->SetRelativeLocation(FVector(-BodyLength*.5f,0,0));
  for(int32 I=0;I<TracerMesh->GetNumMaterials();++I)TracerMesh->SetMaterial(I,nullptr);
  TracerMesh->SetCastShadow(true);
 }
 Plume=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotorPlume")); Plume->SetupAttachment(TracerMesh);
 if(Flame.Succeeded())Plume->SetStaticMesh(Flame.Object);
 Plume->SetCollisionEnabled(ECollisionEnabled::NoCollision); Plume->SetCastShadow(false); Plume->SetVisibility(false);
 Glow=CreateDefaultSubobject<UPointLightComponent>(TEXT("ExhaustGlow")); Glow->SetupAttachment(TracerMesh);
 Glow->SetRelativeLocation(FVector(-40,0,0)); Glow->SetIntensityUnits(ELightUnits::Lumens); Glow->SetIntensity(0);
 Glow->SetLightColor(FLinearColor(1,.55f,.22f)); Glow->SetAttenuationRadius(1200); Glow->SetCastShadows(false); Glow->SetSourceRadius(20);
}
FVector ARocketProjectile::Nozzle() const { return TracerMesh->GetComponentLocation(); }
float ARocketProjectile::LaunchSpeed(const UVehicleWeaponData& Data) const { return Data.MuzzleVelocity*LaunchFraction; }
void ARocketProjectile::LightExhaust()
{
 bExhaustLit=true; Plume->SetVisibility(true); Glow->SetIntensity(16000); LastPuff=Nozzle();
 if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>()) Effects->LaunchSmoke(Nozzle(),GetActorForwardVector());
}
void ARocketProjectile::BurnOut()
{
 bBurnedOut=true; Plume->SetVisibility(false); Glow->SetIntensity(0); Movement->ProjectileGravityScale=1;
}
void ARocketProjectile::Tick(float Delta)
{
 Super::Tick(Delta);
 if(!bLaunched || bImpacted || IsActorBeingDestroyed() || !Profile) return;
 if(LaunchDir.IsZero()) LaunchDir=Movement->Velocity.GetSafeNormal();
 // A rail missile falls free for a moment, then the motor lights and drives it up to speed along its body;
 // the fins pull it back onto the launch line, thrust carries most of its weight, after burn-out it falls.
 const float Drop=Profile->ProjectileType==EVehicleProjectileType::Missile?MissileDrop:0.f;
 if(Age<Drop) { Movement->ProjectileGravityScale=1; return; }
 if(!bExhaustLit) { LightExhaust(); Movement->ProjectileGravityScale=.12f; }
 if(!bBurnedOut && Age>=Profile->FuelTime) BurnOut();
 if(!bBurnedOut)
 {
  float Speed=Movement->Velocity.Size();const float Boost=Profile->MuzzleVelocity*(1-LaunchFraction)/BoostTime;
  if(!HasGuidance() && Speed>1) Movement->Velocity=FMath::VInterpNormalRotationTo(Movement->Velocity/Speed,LaunchDir,Delta,FinAuthority)*Speed;
  if(Speed<Profile->MuzzleVelocity && Speed>1) Movement->Velocity*=FMath::Min(Profile->MuzzleVelocity,Speed+Boost*Delta)/Speed;
 }
 // Smoke: a puff every 45 cm of flight while the motor burns (thinner after burn-out), left in the world to grow,
 // drift and fade; the plume flickers in length.
 if(bExhaustLit)
 {
  auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>();
  const float Spacing=bBurnedOut?260.f:45.f;
  for(int32 Guard=0;Effects && Guard<12 && FVector::Dist(LastPuff,Nozzle())>Spacing;++Guard)
  {LastPuff+=(Nozzle()-LastPuff).GetSafeNormal()*Spacing;Effects->TrailPuff(LastPuff,bBurnedOut?.3f:1.f);}
  if(!bBurnedOut)Plume->SetRelativeScale3D(FVector(FMath::FRandRange(.8f,1.25f),FMath::FRandRange(.9f,1.1f),FMath::FRandRange(.9f,1.1f)));
 }
}
void ARocketProjectile::EndPlay(const EEndPlayReason::Type Reason) { Super::EndPlay(Reason); }
void ARocketProjectile::EnterWater(const FVector& Point)
{
 if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>()) Effects->WaterEntry(Point,Profile?RoundCalibre(*Profile):6.f);
 FHitResult Hit; Hit.ImpactPoint=Hit.Location=Point; Hit.ImpactNormal=Hit.Normal=FVector::UpVector;
 bImpacted=true; ResolveImpact(nullptr,Hit); Movement->StopMovementImmediately(); Destroy();
}
void ARocketProjectile::ResolveImpact(AActor* Other,const FHitResult& Hit)
{
 UWorld* World=GetWorld(); auto* Explosions=World?World->GetSubsystem<UNeonCityExplosionSubsystem>():nullptr;
 if(!Profile || !Explosions) return;
 // Radius, falloff, impulse and target caps come from the profile; only the payload scales the damage.
 UNeonCityExplosionSubsystem::FRequest Request;
 Request.Data=&NeonCityExplosions::Profile(Profile->ExplosionDataID);
 // Offset from the impacted face prevents an origin just inside a wall from leaking through it.
 Request.Center=Hit.ImpactPoint+Hit.ImpactNormal*4;
 AActor* FiringActor=Damage.SourceActor;
 Request.Instigator=Damage.Instigator; Request.SourceActor=FiringActor?FiringActor:GetOwner();
 Request.Ignored.Add(this); Request.Ignored.Add(GetOwner()); Request.DamageOverride=Damage.DamageAmount;
 Explosions->Detonate(Request);      // the explosion presentation shows the Niagara blast
}
