#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Wanted/SearchSystem.h"
#include "Combat/HealthComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"

namespace NeonCityVehicleInteraction
{
	constexpr float EnterDistance = 350.f;
	constexpr float MaximumTransitionSpeed = 150.f;
}
UVehicleInteraction::UVehicleInteraction() { PrimaryComponentTick.bCanEverTick = false; }
ACharacter* UVehicleInteraction::GetDriver() const { return Driver.Get(); }

bool UVehicleInteraction::CanEnter(AActor* Interactor) const
{
	const APawn* Vehicle = Cast<APawn>(GetOwner());
	if (auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) { return false; }
	if (IsValid(Interactor)) { if (auto* Body=Interactor->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { return false; } }
	const ACharacter* Character = Cast<ACharacter>(Interactor);
	const APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	return !bRestoring && !Driver.IsValid() && IsValid(Vehicle) && IsValid(Character)
		&& Vehicle->GetWorld() == Character->GetWorld() && !Vehicle->GetController()
		&& Controller && Controller->IsLocalController() && Controller->GetPawn() == Character
		&& Character->GetCharacterMovement() && Character->GetCapsuleComponent()
		&& FVector::DistSquared(Vehicle->GetActorLocation(), Character->GetActorLocation()) <= FMath::Square(NeonCityVehicleInteraction::EnterDistance)
		&& Vehicle->GetVelocity().SizeSquared() <= FMath::Square(NeonCityVehicleInteraction::MaximumTransitionSpeed);
}
bool UVehicleInteraction::Enter(AActor* Interactor)
{
	if (!CanEnter(Interactor)) { return false; }
	ACharacter* Character = CastChecked<ACharacter>(Interactor);
	APlayerController* Controller = CastChecked<APlayerController>(Character->GetController());
	APawn* Vehicle = CastChecked<APawn>(GetOwner());
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	GetWorld()->GetSubsystem<USearchSystem>()->ObserveVehicleTransition(Character,Vehicle);
	EntryTransform = Character->GetActorTransform();
	bOriginalHidden = Character->IsHidden();
	bOriginalCollision = Character->GetActorEnableCollision();
	OriginalMovementMode = static_cast<uint8>(Movement->MovementMode);
	OriginalCustomMovementMode = Movement->CustomMovementMode;
	OriginalAttachParent = Character->GetRootComponent()->GetAttachParent();
	OriginalAttachSocket = Character->GetRootComponent()->GetAttachSocketName();
	Driver = Character;
	DriverController = Controller;
 bOriginalVolumeUpdates=Character->GetCapsuleComponent()->GetShouldUpdatePhysicsVolume();
 Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->SetActorHiddenInGame(true);
	Character->AttachToActor(Vehicle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Controller->Possess(Vehicle);
	if (Controller->GetPawn() != Vehicle) { RestoreDriver(true); return false; }
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->ShowDriver(Character);
	return true;
}

bool UVehicleInteraction::FindExitTransform(const ACharacter* Character, FTransform& OutTransform, bool AllowWater) const
{
	const AActor* Vehicle = GetOwner();
	UWorld* World = GetWorld();
	if (!Vehicle || !Character || !World || !Character->GetCapsuleComponent()) { return false; }
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NeonCityVehicleExit), false, Vehicle);
	Query.AddIgnoredActor(Character);
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Vehicle->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	// A rotated world AABB counts part of the vehicle length as side clearance.
	// On a narrow road that can put every exit beyond the guardrail even when
	// there is ample space beside the door. Use the chassis-local envelope.
	const FBox LocalBounds = Vehicle->CalculateComponentsBoundingBoxInLocalSpace(false);
	if (LocalBounds.IsValid) { BoundsOrigin = Vehicle->GetActorTransform().TransformPosition(LocalBounds.GetCenter()); }
	const FRotator YawRotation(0.f, Vehicle->GetActorRotation().Yaw, 0.f);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector Back = -FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Forward = -Back;
	// Doors first, then behind, then ahead (a car parked nose-in between others in a garage leaves only the
	// space in front or a tight gap at the door). Each direction is tried at a comfortable step-out distance,
	// then squeezing out close beside the body.
	const FVector Directions[] = {Right, -Right, Back, (Back + Right).GetSafeNormal(), (Back - Right).GetSafeNormal(),
		Forward, (Forward + Right).GetSafeNormal(), (Forward - Right).GetSafeNormal()};
	for (const float Gap : {55.f, 25.f, 8.f})
	for (const FVector& Direction : Directions)
	{
		const float SideExtent = LocalBounds.IsValid
			? FVector::DotProduct(Vehicle->GetActorTransform().InverseTransformVectorNoScale(Direction).GetAbs(), LocalBounds.GetExtent())
			: FVector::DotProduct(Direction.GetAbs(), BoundsExtent);
		const float Clearance = SideExtent + Radius + Gap;
		const FVector Candidate = BoundsOrigin + Direction * Clearance;
  if(AllowWater && World->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(Candidate))
  {
   // Escape beside the actual sinking car, never teleport to an arbitrary shore/surface.
   const FVector Destination=Candidate+FVector(0,0,HalfHeight*.5f);
   FHitResult Route;
   if(!World->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query)
    && !World->SweepSingleByChannel(Route,Vehicle->GetActorLocation(),Destination,FQuat::Identity,ECC_Pawn,Shape,Query))
   { OutTransform=FTransform(YawRotation,Destination,EntryTransform.GetScale3D()); return true; }
  }
		FHitResult GroundHit;
		// Start at the height the exiting player can occupy. A trace starting
		// above a garage ceiling finds the upper slab instead of the real floor.
		FVector GroundTraceStart=Candidate;
		GroundTraceStart.Z=FMath::Min(Candidate.Z+300.f,Vehicle->GetActorLocation().Z+HalfHeight+25.f);
		if (!World->LineTraceSingleByChannel(GroundHit, GroundTraceStart,
			Candidate - FVector(0.f,0.f,600.f), ECC_Visibility, Query)
			|| !GroundHit.IsValidBlockingHit()
			|| GroundHit.ImpactNormal.Z < Character->GetCharacterMovement()->GetWalkableFloorZ()
			|| FMath::Abs(GroundHit.ImpactPoint.Z - BoundsOrigin.Z) > 250.f) { continue; }
		const FVector Destination = GroundHit.ImpactPoint + FVector(0.f,0.f,HalfHeight + 3.f);
		if (World->OverlapBlockingTestByChannel(Destination, FQuat::Identity, ECC_Pawn, Shape, Query)) { continue; }
		FVector Start = Vehicle->GetActorLocation();
		Start.Z = Destination.Z;
		FHitResult RouteHit;
		if (World->SweepSingleByChannel(RouteHit, Start, Destination, FQuat::Identity, ECC_Pawn, Shape, Query)) { continue; }
		OutTransform = FTransform(YawRotation, Destination, EntryTransform.GetScale3D());
		return true;
	}
	return false;
}

void UVehicleInteraction::RestoreCharacterState(ACharacter* Character, const FTransform& Transform)
{
 if(auto* Presentation=GetOwner()->FindComponentByClass<UTransportOccupantComponent>()) Presentation->HideDriver();
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (USceneComponent* AttachParent = OriginalAttachParent.Get())
	{
		Character->AttachToComponent(AttachParent, FAttachmentTransformRules::KeepWorldTransform, OriginalAttachSocket);
	}
	Character->SetActorHiddenInGame(bOriginalHidden);
	Character->SetActorEnableCollision(bOriginalCollision);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		if (auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>()) { Body->RestoreLocomotionMode(OriginalMovementMode,OriginalCustomMovementMode); }
		else { Movement->SetMovementMode(static_cast<EMovementMode>(OriginalMovementMode), OriginalCustomMovementMode); }
  Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(bOriginalVolumeUpdates);
  Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
  if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(Transform.GetLocation(),0))
  {
   if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>()) Body->RestoreLocomotionMode(MOVE_Swimming,0);
   else Movement->SetMovementMode(MOVE_Swimming);
  }
	}
}
bool UVehicleInteraction::Exit() { return RestoreDriver(false); }
bool UVehicleInteraction::RestoreDriver(bool bForce)
{
	ACharacter* Character = Driver.Get();
	APlayerController* Controller = DriverController.Get();
	APawn* Vehicle = Cast<APawn>(GetOwner());
	if (bRestoring || !IsValid(Character)) { return false; }
	if (!bForce && (!Vehicle || !Controller || Controller->GetPawn() != Vehicle
		|| (Vehicle->GetVelocity().SizeSquared() > FMath::Square(NeonCityVehicleInteraction::MaximumTransitionSpeed)
   && !GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(Vehicle->GetActorLocation())))) { return false; }
	FTransform ExitTransform;
	if (!FindExitTransform(Character, ExitTransform,true))
	{
		if (!bForce) { return false; }
		ExitTransform = EntryTransform;
		// Destruction fallback searches around the last safe entry point; never use an occupied capsule.
		FCollisionQueryParams Query(SCENE_QUERY_STAT(NeonCityVehicleRecovery), false, Vehicle);
		Query.AddIgnoredActor(Character);
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
		bool bFound = false;
		for (int32 Ring = 0; Ring <= 6 && !bFound; ++Ring)
		{
			for (int32 Index = 0; Index < 8 && !bFound; ++Index)
			{
				const float Angle = Index * PI / 4.f;
				FVector Candidate = EntryTransform.GetLocation() + FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.f) * Ring * 150.f;
				FHitResult Ground;
				if (GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,250),Candidate-FVector(0,0,500),ECC_Visibility,Query)
					&& Ground.ImpactNormal.Z >= Character->GetCharacterMovement()->GetWalkableFloorZ())
				{
					Candidate = Ground.ImpactPoint + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+3.f);
					if (!GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,Shape,Query))
					{
						ExitTransform.SetLocation(Candidate);
						bFound = true;
					}
				}
			}
		}
		if (!bFound) { return false; }
	}
	TGuardValue<bool> RestoreGuard(bRestoring, true);
	RestoreCharacterState(Character, ExitTransform);
 if(auto* Search=GetWorld()->GetSubsystem<USearchSystem>()) Search->ObserveVehicleExit(Character,Vehicle);
	Driver.Reset();
	DriverController.Reset();
	OriginalAttachParent.Reset();
	if (IsValid(Controller) && (!Controller->GetPawn() || Controller->GetPawn() == Vehicle)) { Controller->Possess(Character); }
	return true;
}
void UVehicleInteraction::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Driver.IsValid())
	{
		if (EndPlayReason == EEndPlayReason::Destroyed) { RestoreDriver(true); }
		else
		{
			RestoreCharacterState(Driver.Get(), EntryTransform);
			Driver.Reset();
			DriverController.Reset();
			OriginalAttachParent.Reset();
		}
	}
	Super::EndPlay(EndPlayReason);
}
