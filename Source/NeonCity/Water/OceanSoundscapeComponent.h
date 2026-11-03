#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OceanSoundscapeComponent.generated.h"
class UAudioComponent;
class UNeonCityReferenceSoundscapeData;
class ACoastalOceanSurface;

/** One ocean-owned listener mix, also active when the player possesses a boat.
 * Recorded sources, fixed six-voice budget, shoreline and habitat driven events. */
UCLASS()
class NEONCITY_API UOceanSoundscapeComponent:public UActorComponent
{
 GENERATED_BODY()
public:
 UOceanSoundscapeComponent();
 virtual void BeginPlay()override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
 float GetShoreDistance()const{return ShoreDistance;}
 int32 GetShoreEventCount()const{return ShoreEvents;}
 int32 GetHabitatEventCount()const{return HabitatEvents;}
 int32 GetVoiceCount()const{return Voices.Num();}
 bool IsAudioReady()const{return bAudioReady;}
 int32 GetPlayingVoiceCount()const;
private:
 UPROPERTY(Transient) TObjectPtr<UNeonCityReferenceSoundscapeData> Data;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Voices;
 TArray<FVector> ShorePoints;
 TSharedPtr<struct FStreamableHandle> ResidentAudio;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;
 FRandomStream Random;
 FVector Shore=FVector::ZeroVector,PreviousEye=FVector::ZeroVector;
 float ShoreDistance=1e9f,Survey=0,SurfWait=1,WashWait=-1,LifeWait=2,LifeEnd=-1,MoveWait=0;
 float Gains[2]{};
 float VoiceNormalization[6]{1,1,1,1,1,1};
 bool bHadEye=false,bUnderwater=false,bAudioReady=false;
 int32 ShoreEvents=0,HabitatEvents=0,SurfSlice=-1;
 UAudioComponent* Voice(int32 Index,bool Spatial);
 float Play(int32 Index,FName Bank,int32 Slice,float Volume,const FVector& Position,float Pitch=1);
 void Bed(int32 Index,FName Bank,float Target,float Delta,float LowPass);
 void SurveyShore(const FVector& Eye);
};
