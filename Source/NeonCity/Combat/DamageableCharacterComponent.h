#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/DamageEvent.h"
#include "DamageableCharacterComponent.generated.h"
class UHealthComponent;
UENUM(BlueprintType)
enum class ENeonCityBodyState : uint8 { Normal, HitReaction, Injured, Downed, Dead };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityBodyStateChanged,ENeonCityBodyState,Previous,ENeonCityBodyState,Current);
/** Biological response only. Medical/crime/relationship systems may observe this without owning health. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UDamageableCharacterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDamageableCharacterComponent();
	UFUNCTION(BlueprintPure,Category="NeonCity|Health") ENeonCityBodyState GetBodyState() const { return State; }
	/** Possession adapters supply the locomotion mode to resume after an injury lock. */
	void RestoreLocomotionMode(uint8 Mode,uint8 CustomMode);
	void SetPhysicalLock(bool Locked);
 bool CanAct() const { return !bPhysicalLock && (State==ENeonCityBodyState::Normal || State==ENeonCityBodyState::Injured); }
	UPROPERTY(BlueprintAssignable) FNeonCityBodyStateChanged OnBodyStateChanged;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void Damaged(const FNeonCityDamageEvent& Event);
	UFUNCTION() void HealthChanged(float Current,float Maximum);
	void Resolve();
 void UpdateMovementLock();
	void SetState(ENeonCityBodyState Value);
	UPROPERTY() TObjectPtr<UHealthComponent> Health;
	ENeonCityBodyState State=ENeonCityBodyState::Normal;
	FTimerHandle ReactionTimer;
	uint8 SavedMovementMode=0,SavedCustomMode=0;
	bool bStopped=false,bPhysicalLock=false;
};
