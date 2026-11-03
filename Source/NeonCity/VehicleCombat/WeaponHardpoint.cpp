#include "VehicleCombat/WeaponHardpoint.h"
FVector UWeaponHardpointComponent::ClampAim(FVector Direction) const
{
 const FVector Forward=GetForwardVector();
 if(FixedForward || Direction.ContainsNaN() || Direction.IsNearlyZero()) return Forward;
 return FMath::VInterpNormalRotationTo(Forward,Direction.GetSafeNormal(),1,FMath::Clamp(AimAngle,0.f,90.f));
}
