#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiveBreathingComponent.generated.h"
class UAudioComponent;
class USoundBase;
class UNiagaraSystem;
class UNiagaraComponent;

/** Recorded regulator cycles and world-space exhaust share one breathing clock. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UDiveBreathingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDiveBreathingComponent();
 bool IsBreathing()const{return bBreathing;}
 int32 GetExhaustCount()const{return ExhaustCount;}
 FVector GetRegulatorLocation() const;
 FString DescribeExhaust() const;
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> Breath;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Cycles;
 UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> Bubbles;
 UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Plumes;
 float CycleAge=0,CycleDuration=0,ExhaustClock=0,PlaybackRate=1,Exertion=0,FirstPersonMix=0;
 int32 CycleIndex=0,ExhaustCount=0;
 bool bBreathing=false;
 void StopBreathing();
};
