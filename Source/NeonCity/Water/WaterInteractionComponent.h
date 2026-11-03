#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Water/WaterStateInterface.h"
#include "WaterInteractionComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaterStateChanged,EWaterInteractionState,Previous,EWaterInteractionState,Current);
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UWaterInteractionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UWaterInteractionComponent();
 UFUNCTION(BlueprintPure) EWaterInteractionState GetWaterState() const { return State; }
 UFUNCTION(BlueprintPure) bool IsFullySubmerged() const { return State==EWaterInteractionState::Underwater; }
 const FWaterSurfaceSample& GetSample() const { return Sample; }
 bool HasShallowFloor() const { return bShallowFloor; }
 bool HasSeafloorSupport() const { return bSeafloorSupport; }
 UFUNCTION(BlueprintPure) FVector GetBreathingLocation() const { return BreathingLocation; }
 void Refresh();
 void ResetForLoad();
 UPROPERTY(BlueprintAssignable) FWaterStateChanged OnWaterStateChanged;
 static bool IsSubmergedActor(const AActor* Actor);
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 FWaterSurfaceSample Sample;
 EWaterInteractionState State=EWaterInteractionState::Dry;
 bool bShallowFloor=false;
 bool bSeafloorSupport=false;
 FVector BreathingLocation=FVector::ZeroVector;
 FVector PreviousCenter=FVector::ZeroVector;
 float FilteredHeadDepth=0;
 double LastHeadSample=-1;
};
