#include "Physics/CharacterImpactComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PhysicsEngine/PhysicsAsset.h"
UCharacterImpactComponent::UCharacterImpactComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UCharacterImpactComponent::BeginPlay()
{
 Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner());
 if(!Character.IsValid()) return;
 if(auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>()) Health->OnHealthChanged.AddDynamic(this,&UCharacterImpactComponent::HealthChanged);
 GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->OnReset.AddDynamic(this,&UCharacterImpactComponent::ResetForLoad);
}
void UCharacterImpactComponent::SetState(ECharacterImpactState Value)
{ if(State!=Value) { State=Value; Visited|=1u<<uint8(Value); OnStateChanged.Broadcast(Value); } }
void UCharacterImpactComponent::HealthChanged(float Current,float Maximum)
{
 auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 if(Health->IsDead()) SetState(ECharacterImpactState::Dead);
 else if(Current<=Maximum*.2f) SetState(ECharacterImpactState::Downed);
 else if(State==ECharacterImpactState::Downed || State==ECharacterImpactState::Dead) { Finish(); SetState(ECharacterImpactState::Normal); }
}
void UCharacterImpactComponent::BeginPhysics(FVector Direction,float Speed)
{
 auto* Mesh=Character->GetMesh();
 if(bPhysics || !bAllowPhysics || !Mesh || !Mesh->GetPhysicsAsset()) return;
 PhysicsBone=NAME_None;
 for(const FName Bone:{FName(TEXT("spine_01")),FName(TEXT("spine_02")),FName(TEXT("spine_03"))})
  if(Mesh->GetPhysicsAsset()->FindBodyIndex(Bone)!=INDEX_NONE) { PhysicsBone=Bone; break; }
 if(PhysicsBone.IsNone()) return;
 MeshTransform=Mesh->GetRelativeTransform(); MeshCollision=Mesh->GetCollisionEnabled(); MeshResponses=Mesh->GetCollisionResponseToChannels();
 Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Mesh->SetCollisionResponseToAllChannels(ECR_Ignore); Mesh->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 Mesh->SetAllBodiesBelowSimulatePhysics(PhysicsBone,true,true); Mesh->SetAllBodiesBelowPhysicsBlendWeight(PhysicsBone,.45f);
 Mesh->SetAllBodiesBelowLinearVelocity(PhysicsBone,Direction.GetSafeNormal()*FMath::Clamp(Speed*.1f,0.f,180.f));
 bPhysics=true; ++PhysicsActivations;
}
void UCharacterImpactComponent::EndPhysics()
{
 if(!bPhysics || !Character.IsValid()) return;
 auto* Mesh=Character->GetMesh(); Mesh->SetAllBodiesBelowLinearVelocity(PhysicsBone,FVector::ZeroVector);
 Mesh->SetAllBodiesBelowPhysicsBlendWeight(PhysicsBone,0); Mesh->SetAllBodiesBelowSimulatePhysics(PhysicsBone,false,true);
 Mesh->SetCollisionEnabled(MeshCollision); Mesh->SetCollisionResponseToChannels(MeshResponses); Mesh->SetRelativeTransform(MeshTransform);
 bPhysics=false;
}
void UCharacterImpactComponent::ReceiveImpact(const FPhysicalImpactEvent& Event)
{
 if(!Character.IsValid() || !GetOwner()->HasAuthority() || Event.AppliedDamage<=0) return;
 if(!bActive) VehicleResponse=Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Vehicle);
 bActive=true; StartedAt=GetWorld()->GetTimeSeconds(); LockSeconds=Event.Severity==EImpactSeverity::Low?.45f:1.3f;
 Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Ignore);
 if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>()) Body->SetPhysicalLock(true);
 const auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 SetState(Health->IsDead()?ECharacterImpactState::Dead:Health->GetCurrentHealth()<=Health->GetMaxHealth()*.2f?ECharacterImpactState::Downed:Event.Severity==EImpactSeverity::Low?ECharacterImpactState::Stagger:ECharacterImpactState::Knockdown);
 // A small swept horizontal displacement, not a ballistic launch. The authoritative capsule stays on its floor.
 const FVector Push=Event.ImpactDirection.GetSafeNormal2D()*FMath::Clamp(Event.ImpactSpeed*.06f,5.f,100.f);
 FHitResult Hit; Character->SetActorLocation(Character->GetActorLocation()+Push,true,&Hit);
 if(Event.Severity>=EImpactSeverity::Medium) BeginPhysics(Event.ImpactDirection,Event.ImpactSpeed);
 GetWorld()->GetTimerManager().SetTimer(Timer,this,&UCharacterImpactComponent::UpdateReaction,.05f,true);
}
void UCharacterImpactComponent::UpdateReaction()
{
 if(!Character.IsValid()) return;
 const float Age=GetWorld()->GetTimeSeconds()-StartedAt;
 if(bPhysics)
 {
  auto* Mesh=Character->GetMesh(); const FVector Velocity=Mesh->GetPhysicsLinearVelocity(PhysicsBone);
  if(Velocity.SizeSquared()>90000) Mesh->SetAllBodiesBelowLinearVelocity(PhysicsBone,Velocity.GetClampedToMaxSize(300));
  if(Age>.8f) EndPhysics();
 }
 if(Age<LockSeconds) return;
 if(State==ECharacterImpactState::Downed || State==ECharacterImpactState::Dead)
 { EndPhysics(); if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>()) Body->SetPhysicalLock(false); GetWorld()->GetTimerManager().ClearTimer(Timer); return; }
 SetState(ECharacterImpactState::Recover);
 if(Age<LockSeconds+.35f) return;
 // Do not re-enable vehicle blocking while a car is still covering the character.
 auto* Capsule=Character->GetCapsuleComponent(); FCollisionQueryParams Q(SCENE_QUERY_STAT(ImpactRecovery),false,GetOwner());
 if(GetWorld()->OverlapAnyTestByObjectType(Character->GetActorLocation(),FQuat::Identity,FCollisionObjectQueryParams(ECC_Vehicle),
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2),Q)) return;
 Finish(); SetState(ECharacterImpactState::Normal);
}
void UCharacterImpactComponent::Finish()
{
 EndPhysics(); GetWorld()->GetTimerManager().ClearTimer(Timer);
 if(Character.IsValid() && bActive)
 {
  Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle,VehicleResponse);
  if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>()) Body->SetPhysicalLock(false);
 }
 bActive=false;
}
void UCharacterImpactComponent::ResetForLoad() { Finish(); const auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); if(Health) HealthChanged(Health->GetCurrentHealth(),Health->GetMaxHealth()); if(State!=ECharacterImpactState::Downed && State!=ECharacterImpactState::Dead) SetState(ECharacterImpactState::Normal); }
void UCharacterImpactComponent::EndPlay(EEndPlayReason::Type Reason)
{
 Finish(); if(auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>()) Health->OnHealthChanged.RemoveAll(this);
 if(auto* System=GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()) System->OnReset.RemoveAll(this);
 Super::EndPlay(Reason);
}
