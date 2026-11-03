#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionComponent.generated.h"

/** Local player focus detection; dispatches through the interface, never a concrete actor class. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UInteractionComponent();

	UFUNCTION(BlueprintCallable, Category="NeonCity|Interaction")
	AActor* DetectInteractable(FVector Start, FVector End, FHitResult& OutHit) const;

	UFUNCTION(BlueprintPure, Category="NeonCity|Interaction")
	bool CanInteractWith(AActor* Candidate) const;

	/** Eye/control-direction trace, constrained by InteractionDistance. */
	UFUNCTION(BlueprintCallable, Category="NeonCity|Interaction")
	AActor* RefreshFocusedInteractable();

	UFUNCTION(BlueprintPure, Category="NeonCity|Interaction")
	AActor* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	/** Retraces at execution time; single-player foundation, no networking/RPC yet. */
	UFUNCTION(BlueprintCallable, Category="NeonCity|Interaction")
	bool TryInteract();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NeonCity|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NeonCity|Interaction", meta=(ClampMin="0.0", Units="cm"))
	float InteractionDistance = 250.f;

	/** When the look trace finds nothing: the thing the player stands on (a boat's deck), then the nearest actor tagged
	 * ProximityTag within this radius (large vehicles used from beside them, e.g. a yacht from the quay or the water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NeonCity|Interaction", meta=(ClampMin="0.0", Units="cm"))
	float ProximityRadius = 650.f;
	static const FName ProximityTag;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AActor> FocusedInteractable;
};
