#include "Physics/PhysicalImpactComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
UPhysicalImpactComponent::UPhysicalImpactComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
float UPhysicalImpactComponent::GetWorldTime() const { return GetWorld()?GetWorld()->GetTimeSeconds():0; }
void UPhysicalImpactComponent::BeginPlay()
{
 Super::BeginPlay(); Body=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
 if(!Body || !GetOwner()->HasAuthority()) { SetComponentTickEnabled(false); return; }
 if(auto* Mesh=Cast<USkeletalMeshComponent>(Body)) PhysicsCreated=Mesh->RegisterOnPhysicsCreatedDelegate(FOnSkelMeshPhysicsCreated::CreateUObject(this,&UPhysicalImpactComponent::SetupBody));
 SetupBody(); Body->OnComponentHit.AddDynamic(this,&UPhysicalImpactComponent::Hit); Suppress();
}
void UPhysicalImpactComponent::SetupBody()
{
 if(auto* Mesh=Cast<USkeletalMeshComponent>(Body)) Mesh->SetAllBodiesNotifyRigidBodyCollision(true);
 else if(Body) Body->SetNotifyRigidBodyCollision(true);
 if(Body) if(auto* BI=Body->GetBodyInstance()) { BI->SetUseCCD(true); BI->SetMaxAngularVelocityInRadians(4.f,false); }
}
void UPhysicalImpactComponent::Suppress(float Seconds) { SuppressedUntil=GetWorldTime()+FMath::Clamp(Seconds,0.f,3.f); SampleVelocity=FVector::ZeroVector; SampleTime=-1; }
bool UPhysicalImpactComponent::IsSuppressed() const { return GetWorldTime()<SuppressedUntil; }
float UPhysicalImpactComponent::GetContactMass() const
{
 const auto* BI=Body?Body->GetBodyInstance():nullptr;
 const float Mass=Body && Body->IsSimulatingPhysics() && BI?BI->GetBodyMass():KinematicMass;
 return FMath::IsFinite(Mass)?FMath::Clamp(Mass,1.f,10000.f):1450.f;
}
FVector UPhysicalImpactComponent::GetContactVelocity() const
{ return GetWorldTime()-SampleTime<.15f?SampleVelocity:GetOwner()->GetVelocity().GetClampedToMaxSize(6000); }
void UPhysicalImpactComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if(!Body) return;
 // Kinematic adapters explicitly capture intended velocity immediately before their sweep.
 if(!Body->IsSimulatingPhysics()) { SetComponentTickEnabled(false); return; }
 SampleVelocity=Body->GetPhysicsLinearVelocity().GetClampedToMaxSize(6000); SampleTime=GetWorldTime();
 if(GetWorldTime()<StabilityUntil)
 {
  FVector Velocity=SampleVelocity; Velocity.Z=FMath::Clamp(Velocity.Z,-1200.f,1200.f);
  if(!Velocity.Equals(Body->GetPhysicsLinearVelocity(),.1f)) Body->SetPhysicsLinearVelocity(Velocity);
  const FVector Angular=Body->GetPhysicsAngularVelocityInRadians(); if(Angular.SizeSquared()>16) Body->SetPhysicsAngularVelocityInRadians(Angular.GetClampedToMaxSize(4));
 }
}
void UPhysicalImpactComponent::Hit(UPrimitiveComponent* HitBody,AActor* Other,UPrimitiveComponent* OtherBody,FVector Impulse,const FHitResult& Contact)
{ SubmitSweep(Other,Contact); }
void UPhysicalImpactComponent::SubmitSweep(AActor* Other,const FHitResult& Hit)
{ if(!IsSuppressed() && GetOwner()->HasAuthority()) GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->Submit(this,Other,Hit); }
void UPhysicalImpactComponent::Notify(const FPhysicalImpactEvent& Event)
{ StabilityUntil=GetWorldTime()+2; OnImpactFeedback.Broadcast(Event); OnCollisionAudioRequested.Broadcast(Event); }
void UPhysicalImpactComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Body) Body->OnComponentHit.RemoveAll(this);
 if(auto* Mesh=Cast<USkeletalMeshComponent>(Body)) Mesh->UnregisterOnPhysicsCreatedDelegate(PhysicsCreated);
 Super::EndPlay(Reason);
}
