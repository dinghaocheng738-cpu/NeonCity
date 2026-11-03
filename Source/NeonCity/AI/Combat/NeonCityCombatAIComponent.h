#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/ThreatEvent.h"
#include "AI/Combat/CombatPerceptionData.h"
#include "NeonCityCombatAIComponent.generated.h"
class UCombatAIData;
class UCombatComponent;
class UHealthComponent;
class ACharacter;
class ACoverPoint;
class AWeaponBase;
class USceneComponent;
UENUM(BlueprintType)
enum class ENeonCityCombatAIState : uint8 { Inactive, AcquireTarget, MoveToCover, Engage, Reposition, Chase, Retreat, Downed, Dead };
UENUM(BlueprintType)
enum class ENeonCityCombatHealth : uint8 { Healthy, Injured, Critical, Downed, Dead };
/** Decisions only: the existing combat/weapon/damage components execute all attacks. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityCombatAIComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityCombatAIComponent();
 bool Configure(UCombatAIData* Profile,int32 Seed=1);
 bool ConfigureWeapon(TSubclassOf<AWeaponBase> Type);
 bool CanFight() const;
 bool ControlsLocomotion() const { return bActive; }
 void UpdateLocomotion(float DeltaSeconds);
 void SetDistantLOD(bool Distant) { bDistantLOD=Distant; }
 UFUNCTION(BlueprintPure) ENeonCityCombatAIState GetState() const { return State; }
 UFUNCTION(BlueprintPure) ENeonCityCombatHealth GetHealthState() const;
 UFUNCTION(BlueprintPure) FCombatPerceptionData GetPerception() const { return Perception; }
 AActor* GetTarget() const { return Target.Get(); }
 ACoverPoint* GetCover() const { return Cover.Get(); }
 uint32 GetVisitedStates() const { return Visited; }
 int32 GetShotsFired() const { return Shots; }
 int32 GetReloadCount() const { return Reloads; }
 int32 GetPerceptionQueries() const { return PerceptionQueries; }
 int32 GetCoverQueries() const { return CoverQueries; }
 float GetLastAimErrorDegrees() const { return LastAimError; }
 FVector GetMovementGoal() const { return MoveGoal; }
 bool IsMovingToGoal() const { return bMove; }
 void Disengage();
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* Function) override;
private:
 UFUNCTION() void OnThreat(const FNeonCityThreatEvent& Event);
 UFUNCTION() void HealthChanged(float Current,float Maximum);
 UFUNCTION() void OnShot(FTransform Muzzle);
 UFUNCTION() void ResetForLoad();
 void SetState(ENeonCityCombatAIState Value);
 void RefreshTick();
 void Sense();
 bool CanSee(AActor* Candidate,FVector& Position,FVector& Aim) const;
 void Acquire(AActor* Candidate,FVector Evidence,bool Seen);
 void SeekCover(bool Replace=false);
 void ReleaseCover();
 bool StartMove(FVector Point);
 void TryShoot();
 void Retreat();
 UPROPERTY() TObjectPtr<UCombatAIData> Data;
 UPROPERTY() TObjectPtr<USceneComponent> WeaponMount;
 TWeakObjectPtr<ACharacter> Character;
 TWeakObjectPtr<UCombatComponent> Combat;
 TWeakObjectPtr<UHealthComponent> Health;
 TWeakObjectPtr<AActor> Target;
 TWeakObjectPtr<ACoverPoint> Cover;
 FCombatPerceptionData Perception;
 FRandomStream Random;
 FVector Home=FVector::ZeroVector,MoveGoal=FVector::ZeroVector,LastMovePosition=FVector::ZeroVector;
 ENeonCityCombatAIState State=ENeonCityCombatAIState::Inactive;
 uint32 Visited=1;
 int32 Shots=0,Reloads=0,BurstShots=0,PerceptionQueries=0,CoverQueries=0;
 float StartedAt=0,StateAt=0,NextSense=0,NextCover=0,NextShot=0,NextMovePlan=0,ReacquireAfter=0,BlockedSince=0,LastAimError=0,PreviousMoveSpeed=140;
 bool bActive=false,bMove=false,bAtPeek=false,bDistantLOD=false;
};
