#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI/NPCInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "NeonCityNPCCharacter.generated.h"
class UNeonCityCombatAIComponent;
class UHealthComponent;
class UCombatComponent;
class UDamageableCharacterComponent;
class UCharacterDamageFeedbackComponent;
class UNPCStateComponent;
class UNPCData;
class UNPCThreatComponent;
class UAnimSequence;
class APedestrianSpawnPoint;

UCLASS()
class NEONCITY_API ANeonCityNPCCharacter : public ACharacter, public INPCInterface, public IDamageInterface, public ICombatInterface
{
	GENERATED_BODY()
public:
	virtual UHealthComponent* GetDamageHealth_Implementation() const override { return DamageHealth; }
	virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
	virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
	virtual ENeonCityCombatState GetCombatState_Implementation() const override;
	UHealthComponent* GetHealthComponent() const { return DamageHealth; }
	ANeonCityNPCCharacter();
	virtual FName GetNPCId_Implementation() const override { return NPCId; }
	void InitializePedestrian(FName Id, UNPCData* Profile, const TArray<FVector>& Points, int32 Seed, APedestrianSpawnPoint* SpawnPoint=nullptr);
	/** Actor Owner belongs to possession. This weak backlink retains the actual
	 * authoring marker through AI possession without changing network ownership. */
	APedestrianSpawnPoint* GetSpawnPoint() const;
	const TArray<FVector>& GetRoute() const { return Route; }
	const UNPCData* GetNPCData() const;
	UNPCStateComponent* GetNPCState() const { return State; }
 UNeonCityCombatAIComponent* GetCombatAI() const { return CombatAI; }
	UNPCThreatComponent* GetThreatComponent() const { return Threat; }
	UCharacterDamageFeedbackComponent* GetDamageFeedback() const { return DamageFeedback; }
	int32 GetBehaviorSeed() const { return BehaviorSeed; }
	bool IsPopulationDormant() const { return bPopulationDormant; }
	bool CanSuspendPopulation() const;
	bool UsesPopulationBehaviourBudget() const;
	void SetPopulationDormant(bool Dormant);
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UHealthComponent> DamageHealth;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UNeonCityCombatAIComponent> CombatAI;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UDamageableCharacterComponent> BodyState;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCharacterDamageFeedbackComponent> DamageFeedback;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|NPC")
	TObjectPtr<UNPCStateComponent> State;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|NPC") TObjectPtr<UNPCThreatComponent> Threat;
	UPROPERTY()
	TObjectPtr<UNPCData> Data;
	UPROPERTY(Transient) TWeakObjectPtr<APedestrianSpawnPoint> SourceSpawnPoint;
	UPROPERTY()
	TArray<FVector> Route;
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnimation;
	FName NPCId = NAME_None;
	int32 BehaviorSeed = 1;
	bool bPlayingWalk = false;
	struct FSuspendedTick { TWeakObjectPtr<UActorComponent> Component; bool Enabled=false; };
	TArray<FSuspendedTick> SuspendedTicks;
	TWeakObjectPtr<AController> SuspendedController;
	bool bPopulationDormant=false,bSavedActorTick=false,bSavedControllerTick=false,bSavedCollision=false,bSavedHidden=false;
};
