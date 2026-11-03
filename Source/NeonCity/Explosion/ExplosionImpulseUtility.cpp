#include "Explosion/ExplosionImpulseUtility.h"
#include "Components/PrimitiveComponent.h"
float FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind Kind,float Falloff,float ImpulseStrength,float OcclusionFraction,float WeatherScale,const FNeonCityImpulsePolicy& Policy)
{
 if(!FMath::IsFinite(Falloff) || !FMath::IsFinite(ImpulseStrength) || !FMath::IsFinite(OcclusionFraction) || !FMath::IsFinite(WeatherScale)) return 0;
 float Limit=0;
 switch(Kind)
 {
 case EImpactTargetKind::Character: Limit=Policy.CharacterDeltaVelocity; break;
 case EImpactTargetKind::Vehicle: Limit=Policy.VehicleDeltaVelocity; break;
 case EImpactTargetKind::Prop: Limit=Policy.PropDeltaVelocity; break;
 default: Limit=Policy.WorldDeltaVelocity; break;
 }
 if(Limit<=0) return 0;
 const float Scale=FMath::Clamp(FMath::Clamp(Falloff,0.f,1.f)*FMath::Clamp(ImpulseStrength,0.f,4.f)*FMath::Clamp(OcclusionFraction,0.f,1.f)*FMath::Clamp(WeatherScale,0.f,2.f),0.f,1.f);
 return FMath::Min(Limit*Scale,FMath::Max(0.f,Policy.MaximumDeltaVelocity));
}
float FExplosionImpulseUtility::ImpulseMagnitude(float TargetMass,float DeltaVelocity,const FNeonCityImpulsePolicy& Policy)
{
 if(!FMath::IsFinite(TargetMass) || !FMath::IsFinite(DeltaVelocity) || TargetMass<=0 || DeltaVelocity<=0) return 0;
 return FMath::Clamp(TargetMass*DeltaVelocity,0.f,FMath::Max(0.f,Policy.MaximumImpulse));
}
FVector FExplosionImpulseUtility::PushDirection(const FVector& Center,const FVector& Target,float UpwardBias)
{
 FVector Radial=Target-Center;
 // A blast centred inside or under the body pushes it straight up rather than not at all.
 if(Radial.SizeSquared()<1) Radial=FVector::UpVector;
 return (Radial.GetSafeNormal()+FVector::UpVector*FMath::Clamp(UpwardBias,0.f,1.f)).GetSafeNormal();
}
bool FExplosionImpulseUtility::Apply(UPrimitiveComponent* Body,const FRequest& Request,const FNeonCityImpulsePolicy& Policy)
{
 if(!IsValid(Body) || !Body->IsSimulatingPhysics() || Request.Center.ContainsNaN() || Request.Target.ContainsNaN()) return false;
 FNeonCityImpulsePolicy Limits=Policy;
 if(Request.bLooseBody) Limits.PropDeltaVelocity=Policy.LooseBodyDeltaVelocity;
 const float Delta=VelocityDelta(Request.bLooseBody?EImpactTargetKind::Prop:Request.Kind,Request.Falloff,Request.ImpulseStrength,Request.OcclusionFraction,Request.WeatherScale,Limits);
 if(Delta<=0) return false;
 const FVector Direction=PushDirection(Request.Center,Request.Target,Policy.UpwardBias);
 if(Direction.IsNearlyZero()) return false;
 // A body already moving faster than the runaway gate is never accelerated further.
 if(Body->GetPhysicsLinearVelocity().Size()>=RunawaySpeed) return false;
 const float Magnitude=ImpulseMagnitude(Request.TargetMass>0?Request.TargetMass:Body->GetMass(),Delta,Policy);
 if(Magnitude<=0) return false;
 // Off-centre on the blast-facing side: the lifting part of the push raises the near side first and the body rolls away.
 const FVector Toward=(Request.Center-Request.Target).GetSafeNormal2D();
 const FVector Point=Request.Target+Toward*Body->Bounds.SphereRadius*FMath::Clamp(Policy.TumbleOffset,0.f,1.f)*.5f;
 Body->AddImpulseAtLocation(Direction*Magnitude,Point,NAME_None);
 Body->WakeAllRigidBodies();
 return true;
}