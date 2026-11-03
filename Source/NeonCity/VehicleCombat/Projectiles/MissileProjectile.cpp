#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Components/StaticMeshComponent.h"
#include "VehicleCombat/TargetingComponent.h"
#include "VehicleCombat/VehicleLaserMark.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
AMissileProjectile::AMissileProjectile()
{
 // the AGM-114 Hellfire airframe (as on the rails), nozzle origin, centred on the round
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Hellfire(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_Hellfire.SM_NC_Hellfire"));
 BodyLength=163.f;
 if(Hellfire.Succeeded()){TracerMesh->SetStaticMesh(Hellfire.Object);TracerMesh->SetRelativeLocation(FVector(-BodyLength*.5f,0,0));TracerMesh->SetRelativeScale3D(FVector(1));}
 Plume->SetRelativeScale3D(FVector(1.3f));
}
void AMissileProjectile::Tick(float Delta)
{
 Super::Tick(Delta); if(!bLaunched || bImpacted || !Profile || IsActorBeingDestroyed()) return;
 AActor* Victim=Target.Get();
 if(bLost || Age>=Profile->FuelTime || !UCombatTargetComponent::IsAliveTarget(Victim)) { bLost=true; Target.Reset(); return; }
 // A laser spot is followed wherever the designator holds it (the laser, not the missile, needs the view). A
 // vehicle or person is given up only after the missile's own view of it stays blocked for a moment.
 if(!Victim->IsA<AVehicleLaserMark>())
 {
  SightBlocked=UTargetingComponent::HasLineOfSight(GetWorld(),this,Victim,GetActorLocation())?0.f:SightBlocked+Delta;
  if(SightBlocked>.8f) { bLost=true; Target.Reset(); return; }
 }
 const FVector DeltaTarget=Victim->GetActorLocation()-GetActorLocation();
 // While it drops off the rail its velocity points down; steer from the airframe until the motor has it moving.
 const float Speed=Movement->Velocity.Size();
 const FVector Forward=Speed>600?Movement->Velocity/Speed:GetActorForwardVector();
 // A target off to the side of the rail is acquired after launch; only one passed (behind it) is given up.
 if(DeltaTarget.Size()>Profile->Range || (Age>.6f && FVector::DotProduct(Forward,DeltaTarget.GetSafeNormal())<.25f)) { bLost=true; Target.Reset(); return; }
 // Lofted (top-attack) profile: steer for a point above the target that sinks onto it as the range closes, so the
 // missile clears trees, walls and rises between it and a target seen from the air, then dives in.
 const float Horizontal=FVector(DeltaTarget.X,DeltaTarget.Y,0).Size();
 const FVector Steer=DeltaTarget+FVector(0,0,Horizontal>1500.f?FMath::Min(Horizontal*.22f,3000.f):0.f);
 Movement->Velocity=FMath::VInterpNormalRotationTo(Forward,Steer.GetSafeNormal(),Delta,Profile->TurnRate)*FMath::Max(Speed,1.f);
 if(DeltaTarget.Size()<80)
 {
  FHitResult Hit; Hit.ImpactPoint=GetActorLocation(); Hit.ImpactNormal=-Forward; bImpacted=true;
  ResolveImpact(Victim,Hit); Movement->StopMovementImmediately(); Destroy();
 }
}
