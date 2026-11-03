#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "World/Audio/NeonCityForestSoundMath.h"
#include "NeonCityReferenceSoundscape.generated.h"

class USoundWave;
class UAudioComponent;
class ACharacter;
class ANeonCityDayNightCycle;
class ANeonCityReferenceVegetation;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FNeonCityReferenceSoundBank
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) FName Id;
 UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<USoundWave>> Clips;
 UPROPERTY(EditAnywhere) TArray<float> LoudnessLUFS;
 UPROPERTY(EditAnywhere) TArray<float> Durations;
 UPROPERTY(EditAnywhere) int32 Channels=1;
 UPROPERTY(EditAnywhere) bool bLoop=false;
};

/** Reviewed original recordings, cut at the original soundBank.js boundaries. */
UCLASS(BlueprintType)
class NEONCITY_API UNeonCityReferenceSoundscapeData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) FString ReferenceCommit;
 UPROPERTY(EditAnywhere) TArray<FNeonCityReferenceSoundBank> Banks;
 const FNeonCityReferenceSoundBank* Find(FName Id) const;
 bool IsValidLibrary() const;
};

/** Local island ambience and surface steps. No change to movement/animation.
 *  One canonical owner, four beds, three footstep and four spatial bird voices.
 *  Source wave/boat/fishing/underwater audio is deliberately a separate adapter.
 */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UNeonCityReferenceSoundscapeComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityReferenceSoundscapeComponent();
 bool InitializeSoundscape();
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 int32 GetStepEventCount() const { return StepEvents; }
 int32 GetBirdEventCount() const { return BirdEvents; }
 int32 GetVoiceAllocationCount() const;
 int32 GetPlayingVoiceCount() const;
 float GetLoopGain(int32 Index) const { return Index>=0 && Index<4?LoopGain[Index]:0; }
 FName GetLastStepSurface() const { return LastStepSurface; }
 /** The listener is inside this island's soundscape (its beds and surface steps are playing). */
 bool IsActive() const { return bActiveNow; }
 bool IsAudioEnabled() const;
 static constexpr int32 VoiceBudget=11;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") TSoftObjectPtr<UNeonCityReferenceSoundscapeData> SoundLibrary;
private:
 struct FSinger
 {
  FVector Location=FVector::ZeroVector;
  FName Bank;
  int32 Remaining=0,Group=0;
  float Wait=0,Pitch=1,TargetLufs=-31;
 };
 UPROPERTY(Transient) TObjectPtr<UNeonCityReferenceSoundscapeData> Data;
 UPROPERTY(Transient) TObjectPtr<ANeonCityReferenceVegetation> Vegetation;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Beds;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Steps;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Birds;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 bool bActiveNow=false;
 TSharedPtr<struct FStreamableHandle> ResidentAudio;
 TWeakObjectPtr<ACharacter> PreviousWalker;
 TWeakObjectPtr<UPrimitiveComponent> PreviousBase;
 FVector PreviousFoot=FVector::ZeroVector;
 FTransform PreviousBaseTransform=FTransform::Identity;
 FVector Listener=FVector::ZeroVector;
 TArray<FSinger> Singers;
 TMap<FName,int32> LastSlice;
 FRandomStream Random;
 NeonCityForestSound::FEnvironment Environment;
 float LoopGain[4]{};
 float BirdGain[4]{};
 FVector BirdLocations[4]{};
 float Gust=.4f,GustTarget=.6f,GustTimer=0,ClockTimer=1,BirdTimer=2,StepDistance=0,StepTimer=0;
 int32 StepSlot=0,BirdSlot=0,StepEvents=0,BirdEvents=0;
 bool bReady=false;
 FName LastStepSurface;
 void UpdateBeds(float Delta,const NeonCityForestSound::FMix& Mix,bool Active);
 void UpdateFootsteps(ACharacter* Walker,float Delta,bool Active);
 void UpdateBirds(float Delta,const NeonCityForestSound::FMix& Mix,bool Active);
 bool FindPerch(FVector& Position);
 UAudioComponent* AllocateVoice(bool Spatial);
 float PlayPhrase(FName Bank,int32 Pick,float TargetLufs,float Pitch,const FVector& At,bool Spatial);
};
