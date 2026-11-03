#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"

bool NeonCityDynamicSurfaces::IsProvider(const AActor* Actor)
{ return IsValid(Actor) && Actor->Implements<UDynamicSurfaceInteractionInterface>(); }

bool NeonCityDynamicSurfaces::Query(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface)
{
 auto* Part=Hit.GetComponent(); auto* Actor=Part?Part->GetOwner():nullptr;
 if(!Character || !IsProvider(Actor) || !Hit.IsValidBlockingHit() || Hit.ImpactNormal.ContainsNaN()
  || Part->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block) return false;
 if(!IDynamicSurfaceInteractionInterface::Execute_QueryStandingSurface(Actor,Hit,Character,Surface)) return false;
 return !Surface.PointVelocity.ContainsNaN() && FMath::IsFinite(Surface.MaxLandingHorizontalSpeed)
  && FMath::IsFinite(Surface.MaxLandingVerticalSpeed) && FMath::IsFinite(Surface.ReleaseAcceleration)
  && FMath::IsFinite(Surface.MaxDepartureSpeed) && Surface.MaxLandingHorizontalSpeed>0
  && Surface.MaxLandingVerticalSpeed>0 && Surface.ReleaseAcceleration>0 && Surface.MaxDepartureSpeed>0;
}

bool NeonCityDynamicSurfaces::IsSafeLanding(const FVector& RelativeVelocity,const FDynamicStandingSurface& Surface)
{
 return !RelativeVelocity.ContainsNaN() && RelativeVelocity.SizeSquared2D()<=FMath::Square(Surface.MaxLandingHorizontalSpeed)
  && RelativeVelocity.Z<=100.f && RelativeVelocity.Z>=-Surface.MaxLandingVerticalSpeed;
}

bool NeonCityDynamicSurfaces::IsSupportContact(AActor* SurfaceActor,ACharacter* Character,const FHitResult& SourceHit)
{
 if(!Character || !IsProvider(SurfaceActor)) return false;
 auto* Movement=Character->GetCharacterMovement();
 if(!Movement || (!Movement->IsFalling() && !Movement->IsMovingOnGround())) return false;
 auto* Root=Cast<UPrimitiveComponent>(SurfaceActor->GetRootComponent());
 FHitResult SurfaceHit(SurfaceActor,Root,SourceHit.ImpactPoint,-SourceHit.ImpactNormal);
 SurfaceHit.bBlockingHit=true; SurfaceHit.BoneName=SourceHit.MyBoneName;
 FDynamicStandingSurface Surface;
 if(!Query(SurfaceHit,Character,Surface)) return false;
 const float FeetZ=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(FeetZ<SourceHit.ImpactPoint.Z-15.f) return false; // A roof-height side hit is not a landing.
 auto* Base=Cast<UPrimitiveComponent>(Character->GetMovementBaseObject());
 const bool BasedHere=Movement->IsMovingOnGround() && Base && Base->GetOwner()==SurfaceActor;
 const FVector Relative=BasedHere?Movement->Velocity:Movement->Velocity-Surface.PointVelocity;
 return IsSafeLanding(Relative,Surface);
}
