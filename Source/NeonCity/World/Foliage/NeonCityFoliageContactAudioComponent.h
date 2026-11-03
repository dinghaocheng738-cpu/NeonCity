#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeonCityFoliageContactAudioComponent.generated.h"

class UNeonCityFoliageInteractionComponent;
class UAudioComponent;
class USoundWave;

/** Event-driven brush foley. Two reusable voices per vegetation layer, no Tick.
 *  Wind never triggers this component; only moving contact from the spring solver.
 */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UNeonCityFoliageContactAudioComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityFoliageContactAudioComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 int32 GetContactEventCount() const { return Events; }
 int32 GetVoiceAllocationCount() const;
 int32 GetPlayingVoiceCount() const;
 static constexpr int32 VoiceBudget=2;
 UPROPERTY(EditAnywhere,Category="NeonCity|Foliage") TSoftObjectPtr<USoundWave> BrushSound;
private:
 void OnSoftContact(const FVector& Position,float Speed,float Strength);
 TWeakObjectPtr<UNeonCityFoliageInteractionComponent> Interaction;
 FDelegateHandle ContactHandle;
 UPROPERTY(Transient) TObjectPtr<USoundWave> Wave;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Voices;
 float AvailableAt[VoiceBudget]{};
 float NextContact=0;
 int32 Events=0;
 FRandomStream Random;
};
