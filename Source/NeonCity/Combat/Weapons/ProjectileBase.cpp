#include "Combat/Weapons/ProjectileBase.h"
#include "Combat/DamageSystem.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
AProjectileBase::AProjectileBase()
{
	Collision=CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision"));
	SetRootComponent(Collision); Collision->InitSphereRadius(3); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->OnComponentHit.AddDynamic(this,&AProjectileBase::Impact);
	Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	Movement->SetUpdatedComponent(Collision); Movement->bAutoActivate=false; Movement->ProjectileGravityScale=0; Movement->bForceSubStepping=true;
}
bool AProjectileBase::Launch(FVector Direction,float Speed,const FNeonCityDamageEvent& Payload)
{
	if (!HasAuthority() || bLaunched || Direction.ContainsNaN() || Direction.IsNearlyZero()
		|| !FMath::IsFinite(Speed) || Speed<=0 || !FMath::IsFinite(Payload.DamageAmount) || Payload.DamageAmount<=0) { return false; }
	Damage=Payload; if (!Damage.SourceActor) { Damage.SourceActor=this; }
	Collision->IgnoreActorWhenMoving(GetOwner(),true);
	if (Damage.SourceActor!=this) { Collision->IgnoreActorWhenMoving(Damage.SourceActor,true); }
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Movement->Velocity=Direction.GetSafeNormal()*FMath::Clamp(Speed,1.f,100000.f);
	Movement->Activate(); bLaunched=true; SetLifeSpan(10); return true;
}
void AProjectileBase::Impact(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit)
{
	if (!HasAuthority() || !bLaunched || bImpacted || Other==GetOwner() || Other==Damage.SourceActor) { return; }
	bImpacted=true; Damage.HitLocation=Hit.ImpactPoint;
	ResolveImpact(Other,Hit);
	Movement->StopMovementImmediately(); Destroy(); // Only the projectile is consumed, never the victim.
}

void AProjectileBase::ResolveImpact(AActor* Other,const FHitResult& Hit)
{ GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(Other,Damage); }
