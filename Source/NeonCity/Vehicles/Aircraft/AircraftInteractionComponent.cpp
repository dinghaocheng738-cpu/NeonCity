#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Wanted/SearchSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
UAircraftInteractionComponent::UAircraftInteractionComponent() { PrimaryComponentTick.bCanEverTick=false; }
ACharacter* UAircraftInteractionComponent::GetDriver() const { return Driver.Get(); }
bool UAircraftInteractionComponent::CanEnter(AActor* Actor) const
{
 const auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(GetOwner()); const auto* Character=Cast<ACharacter>(Actor);
 const auto* PC=Character?Cast<APlayerController>(Character->GetController()):nullptr;
 if(!Aircraft || !Character || !PC || !PC->IsLocalController() || !Aircraft->HasAuthority() || Aircraft->GetWorld()!=Character->GetWorld()
  || Aircraft->GetController() || Driver.IsValid() || bTransition || Aircraft->GetAircraftMovement()->GetCondition()==EAircraftCondition::Disabled || Character->GetAttachParentActor()) return false;
 if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 return Aircraft->GetAircraftMovement()->IsGrounded() && Aircraft->GetVelocity().Size()<100 && FVector::Dist(Aircraft->GetActorLocation(),Character->GetActorLocation())<420;
}
bool UAircraftInteractionComponent::Enter(AActor* Actor)
{
 if(!CanEnter(Actor)) return false;
 TGuardValue<bool> Guard(bTransition,true);
 auto* Character=CastChecked<ACharacter>(Actor); auto* PC=CastChecked<APlayerController>(Character->GetController());
 auto* Aircraft=CastChecked<ANeonCityAircraftVehicleBase>(GetOwner());
 EntryTransform=Character->GetActorTransform(); bHidden=Character->IsHidden(); bCollision=Character->GetActorEnableCollision();
 bVolumeUpdates=Character->GetCapsuleComponent()->GetShouldUpdatePhysicsVolume();
 // CMC's native volume callback can otherwise switch a disabled, seated character into swimming.
 Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);
 Driver=Character; DriverController=PC;
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleTransition(Character,Aircraft);
 Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->DisableMovement();
 Character->SetActorEnableCollision(false); Character->SetActorHiddenInGame(true);
 Character->AttachToActor(Aircraft,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 PC->Possess(Aircraft);
 if(PC->GetPawn()!=Aircraft) { Restore(EntryTransform,true); return false; }
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->ShowDriver(Character);
 return true;
}
bool UAircraftInteractionComponent::FindExitTransform(FTransform& Out) const
{
 const auto* Character=Driver.Get(); const auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(GetOwner());
 if(!Character || !Aircraft || !Aircraft->GetAircraftMovement()->IsGrounded() || Aircraft->GetVelocity().Size()>100 || FMath::Abs(Aircraft->GetActorRotation().Roll)>15 || FMath::Abs(Aircraft->GetActorRotation().Pitch)>15) return false;
 const auto* Capsule=Character->GetCapsuleComponent(); const float Radius=Capsule->GetScaledCapsuleRadius(),Half=Capsule->GetScaledCapsuleHalfHeight();
 const auto Shape=FCollisionShape::MakeCapsule(Radius,Half); const FVector Extent=Aircraft->GetProfile()->BodyExtent;
 const FRotator Yaw(0,Aircraft->GetActorRotation().Yaw,0);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(AircraftExit),false,Character);
 // Only stationary external walkable ground. No midair exit or rotor/cabin spawning.
 TArray<FVector> Candidates={FVector(0,Extent.Y+Radius+130,0),FVector(0,-Extent.Y-Radius-130,0),FVector(-Extent.X-Radius-65,0,0)};
 for(const auto& Local:Candidates)
 {
  const FVector Point=Aircraft->GetActorTransform().TransformPosition(Local);
  FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,250),Point-FVector(0,0,200),ECC_Pawn,Query)
   || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ() || Floor.GetActor()==Aircraft || !Floor.GetComponent() || Floor.GetComponent()->GetCollisionObjectType()!=ECC_WorldStatic || Floor.GetComponent()->IsSimulatingPhysics()) continue;
  const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query)) continue;
  FHitResult Route;
  const FVector Start=Aircraft->GetActorLocation()+FVector(0,0,Extent.Z+Half+6);
  // The seated origin is inside this vehicle. Ignore only its own cabin along departure;
  // destination overlap above still checks the hull and all external blockers.
  FCollisionQueryParams RouteQuery=Query; RouteQuery.AddIgnoredActor(Aircraft);
  if(GetWorld()->SweepSingleByChannel(Route,Start,Destination,FQuat::Identity,ECC_Pawn,Shape,RouteQuery)) continue;
  Out=FTransform(Yaw,Destination); return true;
 }
 return false;
}
void UAircraftInteractionComponent::Restore(const FTransform& Transform,bool Possess)
{
 auto* Character=Driver.Get(); auto* PC=DriverController.Get(); if(!Character) return;
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->HideDriver();
 Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 Character->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);
 Character->SetActorHiddenInGame(bHidden); Character->SetActorEnableCollision(bCollision);
 auto* Move=Character->GetCharacterMovement(); Move->StopMovementImmediately();
 if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>()) Body->RestoreLocomotionMode(MOVE_Falling,0);
 else Move->SetMovementMode(MOVE_Falling);
 Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(bVolumeUpdates);
 Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 if(Possess && PC) { PC->Possess(Character); PC->SetControlRotation(Transform.Rotator()); }
 Driver.Reset(); DriverController.Reset();
}
bool UAircraftInteractionComponent::Exit()
{
 if(bTransition || !Driver.IsValid() || !DriverController.IsValid()) return false;
 FTransform ExitTransform; if(!FindExitTransform(ExitTransform)) return false;
 TGuardValue<bool> Guard(bTransition,true);
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleExit(Driver.Get(),GetOwner());
 Restore(ExitTransform,true); return true;
}
void UAircraftInteractionComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Driver.IsValid())
 {
  FTransform Destination=EntryTransform; FindExitTransform(Destination);
  Restore(Destination,Reason==EEndPlayReason::Destroyed);
 }
 Super::EndPlay(Reason);
}
