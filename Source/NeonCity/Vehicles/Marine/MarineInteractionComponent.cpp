#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
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
UMarineInteractionComponent::UMarineInteractionComponent() { PrimaryComponentTick.bCanEverTick=false; }
ACharacter* UMarineInteractionComponent::GetDriver() const { return Driver.Get(); }
bool UMarineInteractionComponent::CanEnter(AActor* Actor) const
{
 const auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetOwner()); const auto* Character=Cast<ACharacter>(Actor);
 const auto* PC=Character?Cast<APlayerController>(Character->GetController()):nullptr;
 if(!Boat || !Character || !PC || !PC->IsLocalController() || !Boat->HasAuthority() || Boat->GetWorld()!=Character->GetWorld()
  || Boat->GetController() || Driver.IsValid() || bTransition || Boat->GetHealthComponent()->IsDead() || Character->GetAttachParentActor()) return false;
 if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 if(Boat->GetProfile()->bWalkableDeck)
 {
  const auto* Base=Cast<UPrimitiveComponent>(Character->GetMovementBaseObject());
  const FVector Helm=Boat->GetActorTransform().TransformPosition(Boat->GetProfile()->CameraRig.HelmEye);
  return Base && Base->GetOwner()==Boat && FVector::Dist(Helm,Character->GetActorLocation())<Boat->GetProfile()->HelmReachCm;
 }
 return Boat->GetVelocity().Size()<150 && FVector::Dist(Boat->GetActorLocation(),Character->GetActorLocation())<Boat->GetProfile()->InteractionReachCm;
}
bool UMarineInteractionComponent::Enter(AActor* Actor)
{
 if(!CanEnter(Actor)) return false;
 TGuardValue<bool> Guard(bTransition,true);
 auto* Character=CastChecked<ACharacter>(Actor); auto* PC=CastChecked<APlayerController>(Character->GetController());
 auto* Boat=CastChecked<ANeonCityMarineVehicleBase>(GetOwner());
 EntryTransform=Character->GetActorTransform(); bHidden=Character->IsHidden(); bCollision=Character->GetActorEnableCollision();
 bVolumeUpdates=Character->GetCapsuleComponent()->GetShouldUpdatePhysicsVolume();
 // CMC's native volume callback can otherwise switch a disabled, seated character into swimming.
 Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);
 Driver=Character; DriverController=PC;
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleTransition(Character,Boat);
 Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->DisableMovement();
 Character->SetActorEnableCollision(false); Character->SetActorHiddenInGame(true);
 Character->AttachToActor(Boat,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 PC->Possess(Boat);
 if(PC->GetPawn()!=Boat) { Restore(EntryTransform,true); return false; }
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->ShowDriver(Character);
 return true;
}
bool UMarineInteractionComponent::FindExitTransform(FTransform& Out,bool ShoreOnly) const
{
 const auto* Character=Driver.Get(); const auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetOwner());
 if(!Character || !Boat || (!Boat->GetProfile()->bWalkableDeck && Boat->GetSpeed_Implementation()>150) || FMath::Abs(Boat->GetActorRotation().Roll)>25 || FMath::Abs(Boat->GetActorRotation().Pitch)>25) return false;
 const auto* Capsule=Character->GetCapsuleComponent(); const float Radius=Capsule->GetScaledCapsuleRadius(),Half=Capsule->GetScaledCapsuleHalfHeight();
 const auto Shape=FCollisionShape::MakeCapsule(Radius,Half); const FVector Extent=Boat->GetProfile()->HullExtent;
 const FRotator Yaw(0,Boat->GetActorRotation().Yaw,0);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(MarineExit),false,Character);
 if(Boat->FindShoreLanding(Character,Out)) return true;
 // The compact yacht's aft deck is below its pier. Mirror its normal boarding
 // interaction only alongside the authored berth; sea exits still use the deck.
 // Survey real shore support and the entire landing capsule, and require sight
 // through the existing open-rail policy rather than ignoring village walls.
 if(Boat->GetProfile()->bPreferDeckExit && Boat->GetMarineMovement()->IsAtMooringBerth())
 {
  FCollisionQueryParams Sight=Query;Sight.AddIgnoredActor(Boat);
  const FVector Eye=Boat->GetActorTransform().TransformPosition(Boat->GetProfile()->CameraRig.HelmEye);
  for(float Side:{1.f,-1.f})for(float Along:{0.f,-110.f,110.f})for(float Clearance:{85.f,120.f,160.f})
  {
   const FVector Point=Boat->GetActorTransform().TransformPosition(FVector(Along,Side*(Extent.Y+Radius+Clearance),0));FHitResult Floor;
   if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,250),Point-FVector(0,0,200),ECC_Pawn,Query)
    || Floor.GetActor()==Boat || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())continue;
   const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);
   if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query)
    || GetWorld()->LineTraceTestByChannel(Eye,Destination+FVector(0,0,Half*.5f),ECC_Visibility,Sight))continue;
   Out=FTransform(Yaw,Destination);return true;
  }
  // A stern-in berth can use a short passerelle instead of a long side pier.
  // Keep the side exits above for existing marinas. Survey only real static
  // support behind the physical hull, while the yacht is at its authored berth.
  for(float AftClearance:{100.f,135.f,175.f})
  {
   const FVector Point=Boat->GetActorTransform().TransformPosition(FVector(-Extent.X-Radius-AftClearance,0,0));
   FHitResult Floor;
   if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,250),Point-FVector(0,0,200),ECC_Pawn,Query)
    || Floor.GetActor()==Boat || !Floor.GetComponent() || Floor.GetComponent()->GetCollisionObjectType()!=ECC_WorldStatic
    || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())continue;
   // Line-floor + capsule half-height is insufficient on a sloping gangway:
   // the rounded foot can already intersect it. Find the full capsule's first
   // genuine support, without ignoring the yacht or changing player dimensions.
   FHitResult Standing;
   const FVector Start=Floor.ImpactPoint+FVector(0,0,Half+Radius+50);
   const FVector End=Floor.ImpactPoint+FVector(0,0,Half-Radius-30);
   if(!GetWorld()->SweepSingleByChannel(Standing,Start,End,FQuat::Identity,ECC_Pawn,Shape,Query)
    || Standing.bStartPenetrating || Standing.GetActor()!=Floor.GetActor()
    || Standing.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())continue;
   const FVector Destination=Standing.Location+FVector(0,0,3);
   if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query)
    || GetWorld()->LineTraceTestByChannel(Eye,Destination+FVector(0,0,Half*.5f),ECC_Visibility,Sight))continue;
   Out=FTransform(Yaw,Destination);return true;
  }
 }
 if((Boat->GetProfile()->bWalkableDeck || Boat->GetProfile()->bPreferDeckExit) && !ShoreOnly)
 {
  // Leave the helm onto this deck even while coasting. Check the expected floor height,
  // not a ray from above the roof, and never choose an unseen shore across open water.
  for(FVector Local:{Boat->GetProfile()->DeckExit,Boat->GetProfile()->BoardingPoint})
  {
   const FVector Point=Boat->GetActorTransform().TransformPosition(Local);FHitResult Floor;
   if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,18),Point-FVector(0,0,25),ECC_Pawn,Query)
    || Floor.GetActor()!=Boat || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())continue;
   const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);
   if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query))continue;
   Out=FTransform(Yaw,Destination);return true;
  }
  return false;
 }
 // External floor first, then an unobstructed aft deck. Never teleport into water or the hull.
 TArray<FVector> Candidates={FVector(0,Extent.Y+Radius+65,0),FVector(0,-Extent.Y-Radius-65,0),FVector(-Extent.X-Radius-65,0,0)};
 if(!ShoreOnly) Candidates.Add(FVector(-Extent.X*.55f,0,0));
 for(const auto& Local:Candidates)
 {
  const FVector Point=Boat->GetActorTransform().TransformPosition(Local);
  FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,250),Point-FVector(0,0,200),ECC_Pawn,Query)
   || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ() || (ShoreOnly && Floor.GetActor()==Boat)) continue;
  const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query)) continue;
  FHitResult Route;
  // A heaving hull's pitch/roll can raise its physical top above Extent.Z.
  // Keep the complete route capsule above the actual collision bounds so
  // the boat itself cannot reject a safe shore route at its starting point.
  FVector Start=Boat->GetActorLocation()+FVector(0,0,Extent.Z+Half+6);
  Start.Z=FMath::Max(Start.Z,Boat->GetHull()->Bounds.GetBox().Max.Z+Half+6);
  if(GetWorld()->SweepSingleByChannel(Route,Start,Destination,FQuat::Identity,ECC_Pawn,Shape,Query)) continue;
  Out=FTransform(Yaw,Destination); return true;
 }
 return false;
}
void UMarineInteractionComponent::Restore(const FTransform& Transform,bool Possess)
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
bool UMarineInteractionComponent::Exit()
{
 if(bTransition || !Driver.IsValid() || !DriverController.IsValid()) return false;
 FTransform ExitTransform; if(!FindExitTransform(ExitTransform)) return false;
 TGuardValue<bool> Guard(bTransition,true);
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleExit(Driver.Get(),GetOwner());
 auto* Character=Driver.Get();auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetOwner());
 const FVector DeckVelocity=Boat && Boat->GetProfile()->bWalkableDeck?Boat->GetHull()->GetPhysicsLinearVelocityAtPoint(ExitTransform.GetLocation()):FVector::ZeroVector;
 Restore(ExitTransform,true);
 if(Character)Character->GetCharacterMovement()->Velocity=DeckVelocity;
 return true;
}
bool UMarineInteractionComponent::HandOver(UMarineInteractionComponent* To)
{
 if(bTransition || !To || To==this || To->bTransition || To->Driver.IsValid() || !Driver.IsValid() || !DriverController.IsValid()) return false;
 auto* Boat=Cast<ANeonCityMarineVehicleBase>(To->GetOwner()); if(!Boat || Boat->GetController()) return false;
 TGuardValue<bool> Guard(bTransition,true),Other(To->bTransition,true);
 auto* Character=Driver.Get(); auto* PC=DriverController.Get();
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleExit(Character,GetOwner());
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->HideDriver();
 // the receiving boat inherits where the driver originally boarded and their saved state
 To->EntryTransform=EntryTransform; To->bHidden=bHidden; To->bCollision=bCollision; To->bVolumeUpdates=bVolumeUpdates;
 To->Driver=Character; To->DriverController=PC; Driver.Reset(); DriverController.Reset();
 GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleTransition(Character,Boat);
 Character->AttachToActor(Boat,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 PC->Possess(Boat);
 if(PC->GetPawn()!=Boat)
 {
  // give the driver back to where they came from
  Driver=Character; DriverController=PC; To->Driver.Reset(); To->DriverController.Reset();
  Character->AttachToActor(GetOwner(),FAttachmentTransformRules::SnapToTargetNotIncludingScale); PC->Possess(CastChecked<APawn>(GetOwner()));
  return false;
 }
 if(auto* Presentation=Boat->FindComponentByClass<UTransportOccupantComponent>()) Presentation->ShowDriver(Character);
 return true;
}
void UMarineInteractionComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Driver.IsValid())
 {
  FTransform Destination=EntryTransform; FindExitTransform(Destination);
  Restore(Destination,Reason==EEndPlayReason::Destroyed);
 }
 Super::EndPlay(Reason);
}
