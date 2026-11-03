#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
ABulletProjectile::ABulletProjectile()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PrePhysics;
 TracerMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer")); TracerMesh->SetupAttachment(Collision);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/NeonCity/Art/WeaponFX/MI_NC_Tracer.MI_NC_Tracer"));
 // Tracer: a 1.8 m glowing streak trailing the round, readable at a few hundred metres in daylight.
 TracerMesh->SetStaticMesh(Mesh.Object); TracerMesh->SetRelativeLocation(FVector(-80,0,0)); TracerMesh->SetRelativeScale3D(FVector(1.8f,.07f,.07f));
 if(Glow.Succeeded()) TracerMesh->SetMaterial(0,Glow.Object);
 TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); TracerMesh->SetCastShadow(false);
 Movement->bRotationFollowsVelocity=true;
}
float ABulletProjectile::BallisticGravityScale(const UVehicleWeaponData& Data)
{ return Data.ProjectileType==EVehicleProjectileType::Bullet?1.f:0.f; }
float ABulletProjectile::RoundCalibre(const UVehicleWeaponData& Data)
{ return Data.WeaponType==EVehicleWeaponType::Cannon?3.f:Data.ProjectileType==EVehicleProjectileType::Bullet?1.f:6.f; }
bool ABulletProjectile::LaunchVehicle(FVector Direction,FVector InheritedVelocity,const UVehicleWeaponData& Data,const FNeonCityDamageEvent& Event,AActor* InTarget)
{
 if(!Data.IsValidProfile() || InheritedVelocity.ContainsNaN()) return false;
 Profile=DuplicateObject<UVehicleWeaponData>(&Data,this); Target=InTarget;
 if(!Launch(Direction,LaunchSpeed(Data),Event)) return false;
 Movement->Velocity+=InheritedVelocity.GetClampedToMaxSize(20000); Movement->MaxSpeed=Data.MuzzleVelocity+20000;
 Movement->ProjectileGravityScale=BallisticGravityScale(Data);
 PreviousLocation=GetActorLocation(); SetLifeSpan(Data.Lifetime); return true;
}
void ABulletProjectile::Tick(float Delta)
{
 Super::Tick(Delta); if(!bLaunched || bImpacted) return;
 const FVector Here=GetActorLocation();
 Age+=Delta; Travelled+=FVector::Distance(PreviousLocation,Here);
 // The water has no collision: find the surface crossing between last frame and this one.
 if(auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>())
 {
  float Surface=0; FVector Current;
  if(Water->SampleWater(Here,Surface,Current))
  {
   const bool bAbove=Here.Z>Surface;
   if(bWasAboveWater && !bAbove && PreviousLocation.Z>Surface)
   {
    const float T=(PreviousLocation.Z-Surface)/FMath::Max(1.f,PreviousLocation.Z-Here.Z);
    const FVector Entry=FMath::Lerp(PreviousLocation,Here,T);
    PreviousLocation=Here; bWasAboveWater=false; EnterWater(FVector(Entry.X,Entry.Y,Surface)); return;
   }
   bWasAboveWater=bAbove;
  }
 }
 PreviousLocation=Here;
 if(!Profile || Travelled>=Profile->Range || Age>=Profile->Lifetime) GetWorld()->GetSubsystem<UVehicleProjectilePool>()->Release(this);
}
void ABulletProjectile::EnterWater(const FVector& Point)
{
 if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>()) Effects->WaterEntry(Point,Profile?RoundCalibre(*Profile):1.f);
 bImpacted=true; Movement->StopMovementImmediately(); GetWorld()->GetSubsystem<UVehicleProjectilePool>()->Release(this);
}
FVector ABulletProjectile::GetFlightVelocity() const { return Movement->Velocity; }
void ABulletProjectile::ResolveImpact(AActor* Other,const FHitResult& Hit)
{
 Super::ResolveImpact(Other,Hit);
 if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>())
  Effects->Impact(Hit,Movement->Velocity,Profile?RoundCalibre(*Profile):1.f,Damage.Instigator);
}
