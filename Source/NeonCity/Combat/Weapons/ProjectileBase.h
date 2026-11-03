#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/DamageEvent.h"
#include "ProjectileBase.generated.h"
class USphereComponent;
class UProjectileMovementComponent;
/** Shared launch, swept collision and single-impact lifecycle for all projectile payloads. */
UCLASS(Abstract)
class NEONCITY_API AProjectileBase : public AActor
{
	GENERATED_BODY()
public:
	AProjectileBase();
	bool Launch(FVector Direction,float Speed,const FNeonCityDamageEvent& Payload);
protected:
 virtual void ResolveImpact(AActor* Other,const FHitResult& Hit);
	UFUNCTION() void Impact(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY() FNeonCityDamageEvent Damage;
	bool bLaunched=false,bImpacted=false;
};
