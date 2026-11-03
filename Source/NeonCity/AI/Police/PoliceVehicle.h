#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI/Police/PoliceState.h"
#include "Vehicles/VehicleInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "Explosion/ExplosionResponseInterface.h"
#include "PoliceVehicle.generated.h"
class UHealthComponent;
class UCombatComponent;
class UPoliceSensorComponent;
class UBoxComponent;
class UVehicleDamageComponent;
class UStaticMeshComponent;
class ANeonCityTrafficRoute;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCitySirenChanged,bool,bEnabled);
/** Road-node response vehicle: swept movement, finite acceleration, no access to a live suspect. */
UCLASS()
class NEONCITY_API APoliceVehicle : public AActor, public IVehicleInterface, public IDamageInterface, public ICombatInterface, public IExplosionResponseInterface
{
 GENERATED_BODY()
public:
 APoliceVehicle();
 void InitializeRoute(ANeonCityTrafficRoute* InRoute,float InDistance);
 void SetResponding(bool Value) { bResponding=Value; }
 bool IsResponding() const { return bResponding; }
 void SetRoadblock(bool Value) { bRoadblock=Value; }
 void SetDispatchSlot(int32 Value) { DispatchSlot=Value; }
 void SetResponseSpeed(float Value) { ResponseSpeed=FMath::Clamp(Value,300.f,2000.f); }
 UFUNCTION(BlueprintPure,Category="NeonCity|Police") ENeonCityPoliceState GetPoliceState() const { return State; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Police") bool IsSirenEnabled() const { return bSiren; }
 UFUNCTION(BlueprintCallable,Category="NeonCity|Police") void SetSiren(bool Value);
 UPROPERTY(BlueprintAssignable) FNeonCitySirenChanged OnSirenChanged;
 UPoliceSensorComponent* GetSensor() const { return Sensor; }
 UVehicleDamageComponent* GetDamageState() const;
 /** Wrecked response vehicles may burn down; the fuse is opt-in per spawned unit. */
 virtual bool ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event) override;
 virtual FVector GetVelocity() const override { return Velocity; }
 virtual FName GetVehicleId_Implementation() const override { return TEXT("NeonCity.PoliceSedan"); }
 virtual UHealthComponent* GetDamageHealth_Implementation() const override { return Health; }
 virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
 virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
 virtual ENeonCityCombatState GetCombatState_Implementation() const override;
protected:
 virtual void Tick(float DeltaSeconds) override;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPoliceSensorComponent> Sensor;
 UPROPERTY() TObjectPtr<UBoxComponent> Collision;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Beacon;
 TWeakObjectPtr<ANeonCityTrafficRoute> Route;
 FVector Velocity=FVector::ZeroVector;
 ENeonCityPoliceState State=ENeonCityPoliceState::Patrol;
 float Distance=0,Speed=0,WheelAngle=0,ResponseSpeed=1500,BlockedSince=-1,RecoveryRemaining=0,NextRecovery=0;
 int32 DispatchSlot=0;
 float PresentationWheelRadiusCm=32.f;
 bool bRoadblock=false;
 bool bResponding=false,bSiren=false;
};
