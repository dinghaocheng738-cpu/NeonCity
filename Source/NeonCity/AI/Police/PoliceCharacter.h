#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI/NPCInterface.h"
#include "Combat/DamageInterface.h"
#include "Combat/CombatInterface.h"
#include "PoliceCharacter.generated.h"
class UHealthComponent;
class UCombatComponent;
class UDamageableCharacterComponent;
class UPoliceSensorComponent;
class UAnimSequence;
class UNeonCityCombatAIComponent;
class UCharacterDamageFeedbackComponent;
UENUM(BlueprintType)
enum class EPoliceForceState : uint8 { Observe, Investigate, Pursue, Threatened, Combat };
/** Evidence-driven response officer; civilian ecology and Epic player movement remain independent. */
UCLASS()
class NEONCITY_API APoliceCharacter : public ACharacter, public INPCInterface, public IDamageInterface, public ICombatInterface
{
 GENERATED_BODY()
public:
 APoliceCharacter();
 virtual FName GetNPCId_Implementation() const override { return TEXT("NeonCity.PoliceOfficer"); }
 virtual UHealthComponent* GetDamageHealth_Implementation() const override { return Health; }
 virtual bool Attack_Implementation(const FCombatAttackRequest& Request) override;
 virtual float TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) override;
 virtual ENeonCityCombatState GetCombatState_Implementation() const override;
 bool CanRespond() const;
 UNeonCityCombatAIComponent* GetCombatAI() const { return CombatAI; }
 UFUNCTION(BlueprintPure) EPoliceForceState GetUseOfForce() const;
 void SetDispatchSlot(int32 Value) { DispatchSlot=Value; }
 int32 GetDispatchSlot() const { return DispatchSlot; }
 void SetResponseEnabled(bool Value) { bResponseEnabled=Value; }
 bool IsResponseEnabled() const { return bResponseEnabled; }
 UPoliceSensorComponent* GetSensor() const { return Sensor; }
 UCharacterDamageFeedbackComponent* GetDamageFeedback() const { return DamageFeedback; }
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UHealthComponent> Health;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UDamageableCharacterComponent> BodyState;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCharacterDamageFeedbackComponent> DamageFeedback;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPoliceSensorComponent> Sensor;
 UPROPERTY() TObjectPtr<UAnimSequence> Idle;
 UPROPERTY() TObjectPtr<UAnimSequence> Walk;
 UPROPERTY() TObjectPtr<UNeonCityCombatAIComponent> CombatAI;
 int32 DispatchSlot=0;
 bool bResponseEnabled=true;
 bool bWalking=false;
};
