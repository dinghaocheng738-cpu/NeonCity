#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vehicles/VehicleInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "NeonCityTrafficVehicle.generated.h"
class UHealthComponent;
class UCombatComponent;
class ANeonCityTrafficRoute;
class UBoxComponent;
class UStaticMeshComponent;

/** Ambient kinematic traffic, deliberately separate from the player's Chaos simulation. */
UCLASS()
class NEONCITY_API ANeonCityTrafficVehicle : public AActor, public IVehicleInterface, public IDamageInterface, public ICombatInterface
{
	GENERATED_BODY()
public:
	virtual UHealthComponent* GetDamageHealth_Implementation() const override { return DamageHealth; }
	virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
	virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
	virtual ENeonCityCombatState GetCombatState_Implementation() const override;
	UHealthComponent* GetHealthComponent() const { return DamageHealth; }
	ANeonCityTrafficVehicle();
	void InitializeRoute(ANeonCityTrafficRoute* InRoute, float InDistance);
	virtual FVector GetVelocity() const override { return CurrentVelocity; }
	virtual FName GetVehicleId_Implementation() const override { return TEXT("NeonCity.AmbientSedan"); }
	UFUNCTION(BlueprintPure, Category="NeonCity|Traffic")
	bool IsYielding() const { return bYielding; }
	UFUNCTION(BlueprintPure, Category="NeonCity|Traffic")
	AActor* GetBlockingActor() const { return BlockingActor.Get(); }
	UPROPERTY(EditAnywhere, Category="NeonCity|Traffic", meta=(ClampMin="100", ClampMax="800"))
	float CruiseSpeed = 280.f;
protected:
	virtual void Tick(float DeltaSeconds) override;
private:
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UHealthComponent> DamageHealth;
	UPROPERTY(VisibleAnywhere,Category="NeonCity|Combat") TObjectPtr<UCombatComponent> Combat;
	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
	TWeakObjectPtr<ANeonCityTrafficRoute> Route;
	TWeakObjectPtr<AActor> BlockingActor;
	FVector CurrentVelocity = FVector::ZeroVector;
	float Distance = 0.f;
	float WheelAngle = 0.f;
	float PresentationWheelRadiusCm = 32.f;
	bool bYielding = false;
};
