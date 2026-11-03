#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "CombatTestTarget.generated.h"
class UHealthComponent;
class UCombatComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
UCLASS()
class NEONCITY_API ACombatTestTarget : public AActor, public IDamageInterface, public ICombatInterface
{
	GENERATED_BODY()
public:
	ACombatTestTarget();
	virtual UHealthComponent* GetDamageHealth_Implementation() const override { return Health; }
	virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
	virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
	virtual ENeonCityCombatState GetCombatState_Implementation() const override;
	UHealthComponent* GetHealthComponent() const { return Health; }
	UCombatComponent* GetCombatComponent() const { return Combat; }
protected:
	virtual void BeginPlay() override;
private:
	UFUNCTION() void UpdateHealth(float Current,float Maximum);
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
};
