#include "Interaction/InteractionComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Interaction/InteractableInterface.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"

const FName UInteractionComponent::ProximityTag(TEXT("NeonCity.ProximityInteract"));

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

bool UInteractionComponent::CanInteractWith(AActor* Candidate) const
{
	AActor* Owner = GetOwner();
	return IsValid(Owner) && IsValid(Candidate) && Candidate != Owner
		&& Candidate->GetWorld() == GetWorld()
		&& Candidate->Implements<UInteractableInterface>()
		&& IInteractableInterface::Execute_CanInteract(Candidate, Owner);
}

AActor* UInteractionComponent::DetectInteractable(FVector Start, FVector End, FHitResult& OutHit) const
{
	OutHit = FHitResult();
	UWorld* World = GetWorld();
	if (!World || !IsValid(GetOwner()) || Start.ContainsNaN() || End.ContainsNaN() || Start.Equals(End))
	{
		return nullptr;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NeonCityInteraction), false, GetOwner());
	if (!World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params))
	{
		return nullptr;
	}

	AActor* Candidate = OutHit.GetActor();
	return CanInteractWith(Candidate) ? Candidate : nullptr;
}

AActor* UInteractionComponent::RefreshFocusedInteractable()
{
	FocusedInteractable.Reset();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsPlayerControlled() || !Pawn->IsLocallyControlled()
		|| !FMath::IsFinite(InteractionDistance) || InteractionDistance <= 0.f)
	{
		return nullptr;
	}

	FVector Start;
	FRotator Rotation;
	Pawn->GetActorEyesViewPoint(Start, Rotation);
	FHitResult Hit;
	FocusedInteractable = DetectInteractable(Start, Start + Rotation.Vector() * InteractionDistance, Hit);
	if (!FocusedInteractable.IsValid())
	{
		// Standing on it: a boat's deck offers its helm / going ashore wherever the player looks.
		if (const ACharacter* Character = Cast<ACharacter>(Pawn); Character && Character->GetMovementBase())
		{
			AActor* Base = Character->GetMovementBase()->GetOwner();
			if (Base && Base->ActorHasTag(ProximityTag) && CanInteractWith(Base)) { FocusedInteractable = Base; }
		}
	}
	if (!FocusedInteractable.IsValid() && ProximityRadius > 0.f)
	{
		// Beside it: the nearest tagged actor in reach (boarding from the quay, climbing out of the water).
		TArray<FOverlapResult> Near;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NeonCityProximityInteraction), false, Pawn);
		GetWorld()->OverlapMultiByObjectType(Near, Pawn->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams::AllObjects,
			FCollisionShape::MakeSphere(ProximityRadius), Params);
		float Best = TNumericLimits<float>::Max();
		for (const FOverlapResult& Result : Near)
		{
			AActor* Candidate = Result.GetActor();
			if (!Candidate || !Candidate->ActorHasTag(ProximityTag) || !CanInteractWith(Candidate)) { continue; }
			const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), Pawn->GetActorLocation());
			if (Distance < Best) { Best = Distance; FocusedInteractable = Candidate; }
		}
	}
	return FocusedInteractable.Get();
}

bool UInteractionComponent::TryInteract()
{
	if (auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { return false; }
	AActor* Candidate = RefreshFocusedInteractable();
	if (!CanInteractWith(Candidate))
	{
		return false;
	}

	IInteractableInterface::Execute_Interact(Candidate, GetOwner());
	FocusedInteractable.Reset();
	return true;
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFocusedInteractable();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FocusedInteractable.Reset();
	Super::EndPlay(EndPlayReason);
}
