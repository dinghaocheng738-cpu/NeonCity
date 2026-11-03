#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "NeonCityNPCController.generated.h"
class ANeonCityNPCCharacter;

UENUM(BlueprintType)
enum class ENeonCityNPCReaction : uint8 { None, ObservePlayer, EvadeVehicle };

/** Direct movement on authored safe segments. No BehaviorTree or navigation generation. */
UCLASS()
class NEONCITY_API ANeonCityNPCController : public AAIController
{
	GENERATED_BODY()
public:
	ANeonCityNPCController();
	UFUNCTION(BlueprintPure, Category="NeonCity|NPC")
	ENeonCityNPCReaction GetReaction() const { return Reaction; }
	UFUNCTION(BlueprintPure, Category="NeonCity|NPC")
	AActor* GetAttentionActor() const { return Attention.Get(); }
	bool HasObservedPlayer() const { return bObservedPlayer; }
	bool HasEvadedVehicle() const { return bEvadedVehicle; }
	/** Destination selected by the real vehicle reaction; observation only. */
	FVector GetReactionGoal() const { return DodgeTarget; }
	/** Read-only evidence for the bounded seated vehicle sensor repro. */
	FString GetVehicleReactionDiagnostic()const{return FString::Printf(TEXT("sense=[%s] dodge_attempts=%u dodge=[%s]"),*VehicleSenseDiagnostic,DodgeAttempts,*DodgeDiagnostic);}
protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	static constexpr float PointArrivalRadius = 35.f;
	void Sense();
	bool TrySocial();
	void ReactTo(AActor* Actor, ENeonCityNPCReaction Kind);
	bool FindSafeDodge(AActor* Vehicle, FVector& OutLocation) const;
	bool MoveToPoint(FVector Point);
	void LookAt(FVector Point, float DeltaSeconds);
	TWeakObjectPtr<ANeonCityNPCCharacter> Pedestrian;
	TWeakObjectPtr<ANeonCityNPCCharacter> SocialPartner;
	TWeakObjectPtr<AActor> Attention;
	FRandomStream Random;
	int32 RouteIndex = 1;
	float SensorSeconds = 0.f;
	float ReactionCooldown = 0.f;
	float IdleDuration = 2.5f;
	float LookCountdown = 0.f;
	float LookYaw = 0.f;
	float BlockedSeconds = 0.f;
	FVector DodgeTarget = FVector::ZeroVector;
	ENeonCityNPCReaction Reaction = ENeonCityNPCReaction::None;
	bool bObservedPlayer = false;
	bool bEvadedVehicle = false;
	FString VehicleSenseDiagnostic;
	mutable FString DodgeDiagnostic;
	mutable uint32 DodgeAttempts=0;
};
