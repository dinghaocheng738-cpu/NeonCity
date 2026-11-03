#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"
#include "Water/SwimmingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"

void UNeonCitySurfaceMovementComponent::PhysicsRotation(float DeltaTime)
{
 // The motor already turns the body smoothly toward the camera in ApplyPropulsion.
 // Vertical swim input must not replace that heading with a zero-yaw dive direction.
 if(IsSwimming() && CharacterOwner)
  if(const auto* Device=CharacterOwner->FindComponentByClass<UDiveEquipmentComponent>();Device && Device->IsDPVDeployed() && Device->GetMotorSpool()>.08f)return;
 Super::PhysicsRotation(DeltaTime);
}

bool UNeonCitySurfaceMovementComponent::DoJump(bool bReplayingMoves,float DeltaTime)
{
 if(CharacterOwner)
  if(auto* Swim=CharacterOwner->FindComponentByClass<USwimmingComponent>();Swim && Swim->PushOffSeafloor())return true;
 return Super::DoJump(bReplayingMoves,DeltaTime);
}

bool UNeonCitySurfaceMovementComponent::BeginIslandGlide(const FVector& Heading)
{
 if(!CharacterOwner || !IsFalling() || Heading.ContainsNaN())return false;
 IslandTurn=IslandDive=0;bIslandCanopy=false;SetMovementMode(MOVE_Custom,42);
 Velocity=Heading.GetSafeNormal2D()*2100+FVector(0,0,100);return true;
}
void UNeonCitySurfaceMovementComponent::SetIslandFlightControls(float Turn,float Dive,bool Canopy)
{
 IslandTurn=FMath::IsFinite(Turn)?FMath::Clamp(Turn,-1.f,1.f):0;
 IslandDive=FMath::IsFinite(Dive)?FMath::Clamp(Dive,-1.f,1.f):0;bIslandCanopy=Canopy;
}
void UNeonCitySurfaceMovementComponent::PhysCustom(float DeltaTime,int32 Iterations)
{
 if(!IsIslandGliding()){Super::PhysCustom(DeltaTime,Iterations);return;}
 if(!HasValidData() || DeltaTime<MIN_TICK_TIME)return;
 float Remaining=DeltaTime;
 while(Remaining>=MIN_TICK_TIME && Iterations<MaxSimulationIterations && IsIslandGliding())
 {
  ++Iterations;const float Step=GetSimulationTimeStep(Remaining,Iterations);Remaining-=Step;
  const float Yaw=CharacterOwner->GetActorRotation().Yaw+IslandTurn*(bIslandCanopy?45.f:58.f)*Step;
  const FVector Forward=FRotator(0,Yaw,0).Vector();
  const float TargetSpeed=bIslandCanopy?450.f:(IslandDive>=0?3000+1100*IslandDive:3000+800*IslandDive);
  const FVector Horizontal(Velocity.X,Velocity.Y,0);
  const FVector Drag=(Forward*TargetSpeed-Horizontal)*(bIslandCanopy?1.7f:.85f);
  // Gravity, speed-dependent lift and vertical drag are integrated continuously.
  const float LiftFactor=IslandDive>=0?1-.45f*IslandDive:1-.9f*IslandDive;
  const float Stall=FMath::Lerp(.6f,1.f,FMath::Clamp((Horizontal.Size()-1800)/800,0.f,1.f));
  const float Lift=bIslandCanopy?780.f:FMath::Min(Horizontal.SizeSquared()*.000065f,850.f)*LiftFactor*Stall;
  const float VerticalDrag=bIslandCanopy?.95f:(IslandDive>=0?.55f-.2f*IslandDive:.55f-.4f*IslandDive);
  const float VerticalAcceleration=GetGravityZ()+Lift-Velocity.Z*VerticalDrag;
  Velocity+=FVector(Drag.X,Drag.Y,VerticalAcceleration)*Step;
  Velocity.Z=FMath::Clamp(Velocity.Z,bIslandCanopy?-550.f:-1800.f,300.f);
  FHitResult Hit;SafeMoveUpdatedComponent(Velocity*Step,FRotator(0,Yaw,0).Quaternion(),true,Hit);
  if(Hit.bBlockingHit)
  {
   if(IsWalkable(Hit) && Velocity.Z<=0)
   {
    // UE's landing pipeline changes movement mode only from Falling.
    SetMovementMode(MOVE_Falling);
    ProcessLanded(Hit,Remaining,Iterations);return;
   }
   HandleImpact(Hit,Step,Velocity*Step);SlideAlongSurface(Velocity*Step,1-Hit.Time,Hit.Normal,Hit,true);
  }
  if(GetPhysicsVolume() && GetPhysicsVolume()->bWaterVolume){SetMovementMode(MOVE_Swimming);return;}
  UpdateComponentVelocity();
 }
}

bool UNeonCitySurfaceMovementComponent::IsWalkable(const FHitResult& Hit) const
{
 if(!Super::IsWalkable(Hit)) return false;
 if(!NeonCityDynamicSurfaces::IsProvider(Hit.GetActor())) return true;
 FDynamicStandingSurface Surface;
 return NeonCityDynamicSurfaces::Query(Hit,CharacterOwner,Surface);
}

bool UNeonCitySurfaceMovementComponent::IsValidLandingSpot(const FVector& Location,const FHitResult& Hit) const
{
 if(!Super::IsValidLandingSpot(Location,Hit)) return false;
 if(!NeonCityDynamicSurfaces::IsProvider(Hit.GetActor())) return true;
 FDynamicStandingSurface Surface;
 return !(ReleasedBase.Get()==Hit.GetActor() && GetWorld()->GetTimeSeconds()<ReleaseUntil)
  && NeonCityDynamicSurfaces::Query(Hit,CharacterOwner,Surface)
  && NeonCityDynamicSurfaces::IsSafeLanding(Velocity-Surface.PointVelocity,Surface);
}

void UNeonCitySurfaceMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
 FDynamicStandingSurface Surface;
 const bool Dynamic=IsFalling() && NeonCityDynamicSurfaces::Query(Hit,CharacterOwner,Surface);
 const FVector Relative=Velocity-Surface.PointVelocity;
 if(Dynamic)
 {
  // Falling velocity is world-relative; walking velocity is relative to the new base.
  Velocity.X=Relative.X; Velocity.Y=Relative.Y;
 }
 Super::SetPostLandedPhysics(Hit);
 if(Dynamic && IsMovingOnGround())
 {
  SampleBase=Hit.GetActor(); SamplePointVelocity=Surface.PointVelocity; SampleTime=GetWorld()->GetTimeSeconds();
  MotionFilter.Reset();
  ++LandingCount; OnDynamicSurfaceLanded.Broadcast(Hit.GetActor(),Relative);
 }
}

FVector UNeonCitySurfaceMovementComponent::GetImpartedMovementBaseVelocity() const
{
 FVector Result=Super::GetImpartedMovementBaseVelocity();
 auto* Base=CharacterOwner?Cast<UPrimitiveComponent>(CharacterOwner->GetMovementBaseObject()):nullptr;
 if(!Base || !NeonCityDynamicSurfaces::IsProvider(Base->GetOwner())) return Result;
 // Keep UE's linear + angular tangential inheritance, with a finite departure safety envelope.
 FHitResult Contact(Base->GetOwner(),Base,CharacterOwner->GetActorLocation()+GetGravityDirection()*CharacterOwner->GetSimpleCollisionHalfHeight(),Base->GetUpVector());
 Contact.bBlockingHit=true; Contact.BoneName=CharacterOwner->GetBasedMovement().BoneName;
 FDynamicStandingSurface Surface;
 const bool Supported=NeonCityDynamicSurfaces::Query(Contact,CharacterOwner,Surface);
 const float Limit=Supported?Surface.MaxDepartureSpeed:2200.f;
 if(Supported && Base!=Base->GetOwner()->GetRootComponent())
 {
  const auto* Root=Cast<UPrimitiveComponent>(Base->GetOwner()->GetRootComponent());
  Result=bImpartBaseAngularVelocity || !Root?Surface.PointVelocity:Root->GetPhysicsLinearVelocity();
  if(!bImpartBaseVelocityX)Result.X=0;if(!bImpartBaseVelocityY)Result.Y=0;if(!bImpartBaseVelocityZ)Result.Z=0;
 }
 const float Vertical=FMath::Clamp(Result.Z,-250.f,250.f); Result.Z=0;
 Result=Result.GetClampedToMaxSize(Limit); Result.Z=Vertical;
 return Result;
}

void UNeonCitySurfaceMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
 const FQuat PreviousBaseRotation=OldBaseQuat;
 const FVector PreviousFloorNormal=CurrentFloor.HitResult.ImpactNormal;
 Super::UpdateBasedMovement(DeltaSeconds); // UE handles relative transforms and post-physics ordering.
 auto* Base=CharacterOwner?Cast<UPrimitiveComponent>(CharacterOwner->GetMovementBaseObject()):nullptr;
 if(!IsMovingOnGround() || !Base || !NeonCityDynamicSurfaces::IsProvider(Base->GetOwner())) { SampleBase.Reset(); SampleTime=-1; MotionFilter.Reset(); return; }
 FHitResult Contact=CurrentFloor.HitResult;
 Contact.ImpactPoint=CharacterOwner->GetActorLocation()+GetGravityDirection()*CharacterOwner->GetSimpleCollisionHalfHeight();
 // Floor normal was sampled before physics; rotate it with the current base pose for release checks.
 FVector BaseLocation; FQuat BaseRotation;
 MovementBaseUtility::GetMovementBaseTransform(CharacterOwner->GetMovementBaseInterfaceData(),CharacterOwner->GetBasedMovement().BoneName,BaseLocation,BaseRotation);
 Contact.ImpactNormal=BaseRotation.RotateVector(PreviousBaseRotation.UnrotateVector(PreviousFloorNormal));
 FDynamicStandingSurface Surface;
 const bool Supported=NeonCityDynamicSurfaces::Query(Contact,CharacterOwner,Surface);
 const double Now=GetWorld()->GetTimeSeconds(),Elapsed=Now-SampleTime;
 const bool HasInterval=Supported && SampleBase.Get()==Base->GetOwner() && Elapsed>UE_SMALL_NUMBER && Elapsed<.25;
 if(HasInterval)MotionFilter.Update(Surface.PointVelocity-SamplePointVelocity,Elapsed);
 else if(SampleBase.Get()!=Base->GetOwner() || Elapsed>=.25)MotionFilter.Reset();
 // CMC may call this before and after physics in one game frame. Filter once
 // per measured interval; retain the existing release threshold and floor/
 // tumble rules. Short wave/contact jitter is not a collision-sized impulse.
 const bool Shock=HasInterval && MotionFilter.Acceleration.Size()>Surface.ReleaseAcceleration;
 const bool Tumbling=Base->GetPhysicsAngularVelocityInRadians().Size()>2.f;
 if(!Supported || Shock || Tumbling)
 {
  LastSurfaceReleaseDetails=FString::Printf(TEXT("supported=%d shock=%d tumble=%d dt=%.4f acceleration=%.1f localContact=%s localNormal=%s omega=%s previousVelocity=%s pointVelocity=%s normal=%s balancedAcceleration=%s base=%s contactPart=%s actorRotation=%s baseRotation=%s oldBaseRotation=%s cachedNormal=%s cachedPoint=%s capsuleFoot=%s"),
   Supported,Shock,Tumbling,Elapsed,Elapsed>UE_SMALL_NUMBER?(Surface.PointVelocity-SamplePointVelocity).Size()/Elapsed:0.,
   *Base->GetComponentTransform().InverseTransformPosition(Contact.ImpactPoint).ToString(),
   *Base->GetComponentTransform().InverseTransformVectorNoScale(Contact.ImpactNormal).ToString(),
   *Base->GetPhysicsAngularVelocityInRadians().ToString(),*SamplePointVelocity.ToString(),*Surface.PointVelocity.ToString(),*Contact.ImpactNormal.ToString(),*MotionFilter.Acceleration.ToString(),
   *Base->GetName(),*GetNameSafe(Contact.GetComponent()),*Base->GetOwner()->GetActorRotation().ToString(),*BaseRotation.Rotator().ToString(),*PreviousBaseRotation.Rotator().ToString(),
   *PreviousFloorNormal.ToString(),*CurrentFloor.HitResult.ImpactPoint.ToString(),*Contact.ImpactPoint.ToString());
  ReleasedBase=Base->GetOwner(); ReleaseUntil=Now+.35; ++ReleaseCount;
  SetMovementMode(MOVE_Falling); // Standard departure imparts velocity and clears the base.
  SampleBase.Reset(); SampleTime=-1; MotionFilter.Reset(); return;
 }
 if(Elapsed>UE_SMALL_NUMBER || SampleBase.Get()!=Base->GetOwner())
 { SampleBase=Base->GetOwner(); SamplePointVelocity=Surface.PointVelocity; SampleTime=Now; }
}

void UNeonCitySurfaceMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Hit,const FVector& Accel,const FVector& Speed)
{
 // Kinematic walking must not add an artificial force/torque to heavy mobile surfaces.
 if(!NeonCityDynamicSurfaces::IsProvider(Hit.GetActor())) Super::ApplyImpactPhysicsForces(Hit,Accel,Speed);
}
void UNeonCitySurfaceMovementComponent::ApplyDownwardForce(float DeltaSeconds)
{
 if(!NeonCityDynamicSurfaces::IsProvider(CurrentFloor.HitResult.GetActor())) Super::ApplyDownwardForce(DeltaSeconds);
}
void UNeonCitySurfaceMovementComponent::ApplyRepulsionForce(float DeltaSeconds)
{
 // Do not run the engine overlap routine's velocity-zeroing/radial-force branch on a platform.
 // Only current capsule overlaps are inspected; no scene scan or additional traces.
 if(UpdatedPrimitive) for(const auto& Overlap:UpdatedPrimitive->GetOverlapInfos())
  if(auto* Part=Overlap.OverlapInfo.Component.Get(); Part && NeonCityDynamicSurfaces::IsProvider(Part->GetOwner())) return;
 Super::ApplyRepulsionForce(DeltaSeconds);
}
void UNeonCitySurfaceMovementComponent::CapsuleTouched(UPrimitiveComponent* Part,AActor* Other,UPrimitiveComponent* OtherPart,int32 Index,bool Swept,const FHitResult& Hit)
{
 if(!NeonCityDynamicSurfaces::IsProvider(Other)) Super::CapsuleTouched(Part,Other,OtherPart,Index,Swept,Hit);
}

void UNeonCitySurfaceMovementComponent::BeginPlay()
{
 Super::BeginPlay();
 GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->OnReset.AddDynamic(this,&UNeonCitySurfaceMovementComponent::ResetForLoad);
}
void UNeonCitySurfaceMovementComponent::ResetForLoad()
{
 SampleBase.Reset(); ReleasedBase.Reset(); SampleTime=-1; ReleaseUntil=0;
 MotionFilter.Reset();
 LastSurfaceReleaseDetails.Reset();
 auto* Base=CharacterOwner?Cast<UPrimitiveComponent>(CharacterOwner->GetMovementBaseObject()):nullptr;
 if(Base && NeonCityDynamicSurfaces::IsProvider(Base->GetOwner()))
 {
  // The existing load coordinator teleports/restores velocity after this reset signal.
  // Discard the old relative base before a saved vehicle is moved back to its saved transform.
  SetMovementMode(MOVE_Falling);
  StopMovementImmediately();
 }
}
void UNeonCitySurfaceMovementComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* System=GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()) System->OnReset.RemoveAll(this);
 Super::EndPlay(Reason);
}
