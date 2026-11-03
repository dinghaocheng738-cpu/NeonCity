#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "MarineFeedbackComponent.generated.h"
class USoundBase;
class UAudioComponent;

/** Local presentation driven by movement telemetry; never controls the hull. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UMarineFeedbackComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UMarineFeedbackComponent();
 void UpdateFeedback(const FMarineDriveTelemetry& State,float MaxSpeed);
 UAudioComponent* GetEngineAudio() const { return EngineAudio; }
 UPROPERTY(EditDefaultsOnly,Category="Marine|Audio") TSoftObjectPtr<USoundBase> EngineLoop;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> EngineAudio;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> WaterAudio;
 float WaterGains[3]{};
 bool bPlaybackRequested=false;
};
