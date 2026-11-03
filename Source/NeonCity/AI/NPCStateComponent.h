#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCStateComponent.generated.h"

UENUM(BlueprintType)
enum class ENeonCityNPCState : uint8 { Idle, Walk, Social, React, Return, Alert, Flee, Fight, CallHelp, Recover };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityNPCStateChanged, ENeonCityNPCState, Previous, ENeonCityNPCState, Current);

/** State storage and notification only; controller owns decisions, character owns movement. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNPCStateComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNPCStateComponent();
	UFUNCTION(BlueprintPure, Category="NeonCity|NPC")
	ENeonCityNPCState GetState() const { return State; }
	UFUNCTION(BlueprintCallable, Category="NeonCity|NPC")
	void SetState(ENeonCityNPCState NewState);
	UFUNCTION(BlueprintPure, Category="NeonCity|NPC")
	float GetStateSeconds() const { return StateSeconds; }
	void AdvanceTime(float DeltaSeconds);
	uint32 GetVisitedStateMask() const { return VisitedStateMask; }
	UPROPERTY(BlueprintAssignable, Category="NeonCity|NPC")
	FNeonCityNPCStateChanged OnStateChanged;
private:
	UPROPERTY(VisibleAnywhere, Category="NeonCity|NPC")
	ENeonCityNPCState State = ENeonCityNPCState::Idle;
	float StateSeconds = 0.f;
	uint32 VisitedStateMask = 1;
};
