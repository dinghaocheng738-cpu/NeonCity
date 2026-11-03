#include "Water/OceanSoundscapeComponent.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Water/CoastalOceanSurface.h"
#include "Water/CoastalWaveData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "World/Audio/NeonCityReferenceCoast.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/CoastalWildlife.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UOceanSoundscapeComponent::UOceanSoundscapeComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.05f;}
void UOceanSoundscapeComponent::BeginPlay()
{
 Super::BeginPlay();Ocean=Cast<ACoastalOceanSurface>(GetOwner());
 if(GetWorld()->GetNetMode()==NM_DedicatedServer || !Ocean.IsValid() || !Ocean->GetField() || !Ocean->GetField()->IsValidField()){SetComponentTickEnabled(false);return;}
 Data=LoadObject<UNeonCityReferenceSoundscapeData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape"));
 if(!Data || !Data->IsValidLibrary()){SetComponentTickEnabled(false);return;}
 TArray<FSoftObjectPath> Clips;
 for(FName Bank:{TEXT("surf_far"),TEXT("surf_crash"),TEXT("surf_backwash"),TEXT("under_reef"),TEXT("submerge"),TEXT("emerge"),TEXT("uw_swim")})
 {
  const auto* B=Data->Find(Bank);if(!B){SetComponentTickEnabled(false);return;}
  for(const auto& Clip:B->Clips)Clips.AddUnique(Clip.ToSoftObjectPath());
 }
 ResidentAudio=UAssetManager::GetStreamableManager().RequestAsyncLoad(Clips);
 if(!ResidentAudio.IsValid()){SetComponentTickEnabled(false);return;}
 Random.GenerateNewSeed();Voices.SetNum(6);
 if(const auto* F=Ocean->GetField();F && F->IsValidField())
 {
  const FVector2D Step=(F->GridMaximum-F->GridMinimum)/(F->GridSize-1);
  for(int32 Y=1;Y<F->GridSize-1;++Y)for(int32 X=1;X<F->GridSize-1;++X)
  {
   const int32 I=Y*F->GridSize+X;
   if(F->DepthCm[I]>50)continue;
   if(F->DepthCm[I-1]<50 && F->DepthCm[I+1]<50 && F->DepthCm[I-F->GridSize]<50 && F->DepthCm[I+F->GridSize]<50)continue;
   const FVector2D P=F->GridMinimum+Step*FVector2D(X,Y);ShorePoints.Add(FVector(P.X,P.Y,F->SeaLevel));
  }
 }
}
UAudioComponent* UOceanSoundscapeComponent::Voice(int32 Index,bool Spatial)
{
 if(!Voices.IsValidIndex(Index) || !GetWorld()->AllowAudioPlayback() || !GetWorld()->GetAudioDeviceRaw())return nullptr;
 if(Voices[Index])return Voices[Index];
 auto* C=NewObject<UAudioComponent>(GetOwner());C->bAutoActivate=false;C->bAutoDestroy=false;
 C->bAllowSpatialization=Spatial;C->bOverrideAttenuation=true;C->AttenuationOverrides.bAttenuate=false;
 C->AttenuationOverrides.bSpatialize=Spatial;C->bShouldRemainActiveIfDropped=true;
 GetOwner()->AddInstanceComponent(C);C->RegisterComponent();Voices[Index]=C;return C;
}
float UOceanSoundscapeComponent::Play(int32 Index,FName Bank,int32 Slice,float Volume,const FVector& Position,float Pitch)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(OceanSoundPhrase);
 const auto* B=Data->Find(Bank);if(!B || B->Clips.IsEmpty())return 0;
 Slice=Slice<0?Random.RandRange(0,B->Clips.Num()-1):Slice%B->Clips.Num();
 auto* C=Voice(Index,Index==2 || Index==3 || Index==4);
 if(C)if(auto* W=B->Clips[Slice].Get())
 {
  C->Stop();C->SetSound(W);C->SetWorldLocation(Position);C->SetPitchMultiplier(Pitch);
  // Match source slices to -25 LUFS before applying distance/strength gains.
  VoiceNormalization[Index]=FMath::Pow(10.f,(-25.f-B->LoudnessLUFS[Slice])/20.f);
  C->SetVolumeMultiplier(Volume*VoiceNormalization[Index]);
  C->SetLowPassFilterEnabled(true);C->SetLowPassFilterFrequency(bUnderwater?950:15000);
  C->Play(B->bLoop?Random.FRand()*B->Durations[Slice]:0);
 }
 return B->Durations[Slice]/Pitch;
}
void UOceanSoundscapeComponent::Bed(int32 Index,FName Bank,float Target,float Delta,float LowPass)
{
 Gains[Index]=FMath::FInterpTo(Gains[Index],Target,Delta,2.5f);
 auto* C=Voices[Index].Get();
 if(Target>.001f && (!C || !C->IsPlaying())){Play(Index,Bank,0,.0001f,PreviousEye);C=Voices[Index];}
 if(C)
 {
  C->SetVolumeMultiplier(Gains[Index]*VoiceNormalization[Index]);C->SetLowPassFilterEnabled(true);C->SetLowPassFilterFrequency(LowPass);
  if(Target<.001f && Gains[Index]<.001f)C->Stop();
 }
}
void UOceanSoundscapeComponent::SurveyShore(const FVector& Eye)
{
 ShoreDistance=1e9f;
 for(const auto& P:ShorePoints)
 {const float D=FVector::Dist2D(Eye,P);if(D<ShoreDistance){ShoreDistance=D;Shore=P;}}
 // The detailed island coastline refines the coarse world bathymetry near its surf.
 const FVector& Island=NeonCityCoastalLayout::IslandOrigin;
 const double X=(Eye.X-Island.X)/100,Z=-(Eye.Y-Island.Y)/100;
 double D=0,B=0;NeonCityReferenceCoast::Query(X,Z,D,B);
 if(FMath::Abs(D)*100<ShoreDistance)
 {
  double DX=0,DZ=0,T=0;NeonCityReferenceCoast::Query(X+1,Z,DX,T);NeonCityReferenceCoast::Query(X,Z+1,DZ,T);
  const FVector N=FVector(DX-D,-(DZ-D),0).GetSafeNormal();
  Shore=Eye-N*D*100;Shore.Z=Ocean->GetField()->SeaLevel;ShoreDistance=FMath::Abs(D)*100;
 }
}
void UOceanSoundscapeComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 // Begin events only when the original bank clips are resident. Do not queue
 // stale crashes or water transitions while streaming, or block Tick loading
 // a first-use phrase.
 if(!bAudioReady)
 {
  if(!ResidentAudio.IsValid() || !ResidentAudio->HasLoadCompleted())return;
  for(FName Bank:{TEXT("surf_far"),TEXT("surf_crash"),TEXT("surf_backwash"),TEXT("under_reef"),TEXT("submerge"),TEXT("emerge"),TEXT("uw_swim")})
   for(const auto& Clip:Data->Find(Bank)->Clips)if(!Clip.Get()){UE_LOG(LogTemp,Warning,TEXT("Ocean audio source failed asynchronous loading: %s"),*Clip.ToString());SetComponentTickEnabled(false);return;}
  bAudioReady=true;
 }
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->IsLocalController() || !Ocean.IsValid() || !Data)return;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 FWaterSurfaceSample Water;
 const bool InWater=GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Eye,Water);
 const bool Under=InWater && Water.DepthAt(Eye)>4;
 const bool Continuous=bHadEye && FVector::DistSquared(PreviousEye,Eye)<FMath::Square(800.f);
 const float Speed=Continuous?FVector::Distance(Eye,PreviousEye)/FMath::Max(.01f,Delta):0;
 if(Under!=bUnderwater && Continuous)Play(5,Under?TEXT("submerge"):TEXT("emerge"),-1,.5f,Eye);
 bUnderwater=Under;PreviousEye=Eye;bHadEye=true;
 Survey-=Delta;if(Survey<=0){SurveyShore(Eye);Survey=.75f;}
 const float DistanceGain=1.f/(1.f+FMath::Square(ShoreDistance/3500.f));
 const float Strength=.6f+Ocean->GetStrength()*.4f;
 Bed(0,TEXT("surf_far"),DistanceGain*(Under?.025f:.4f)*Strength,Delta,Under?450:FMath::Lerp(12000.f,900.f,FMath::Clamp(ShoreDistance/22000,0.f,1.f)));
 FVector CreatureLocation;
 const bool HasCreature=GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->NearestCreaturePosition(Eye,2600,CreatureLocation);
 const float LifeGain=HasCreature?1.f/(1.f+FMath::Square(FVector::Distance(Eye,CreatureLocation)/650)):0;
 Bed(1,TEXT("under_reef"),Under?(.035f+LifeGain*.13f):0,Delta,1800);
 SurfWait-=Delta;
 if(SurfWait<=0 && ShoreDistance<10000)
 {
  const auto* B=Data->Find(TEXT("surf_crash"));if(B && B->Clips.Num())SurfSlice=(SurfSlice+Random.RandRange(1,FMath::Max(1,B->Clips.Num()-1)))%B->Clips.Num();
  Play(2,TEXT("surf_crash"),SurfSlice,DistanceGain*(Under?.08f:.9f)*Strength,Shore,Random.FRandRange(.96f,1.04f));
  ++ShoreEvents;WashWait=Random.FRandRange(1.5f,2.5f);
  SurfWait=Random.FRandRange(4.5f,8.f)/FMath::Max(.7f,Strength);
 }
 if(WashWait>0){WashWait-=Delta;if(WashWait<=0)Play(3,TEXT("surf_backwash"),-1,DistanceGain*(Under?.025f:.45f),Shore);}
 // Active crashes cross the water boundary with the listener, rather than
 // keeping their above-water brightness until the current clip finishes.
 for(int32 I=2;I<=3;++I)if(Voices[I] && Voices[I]->IsPlaying())
 {
  Voices[I]->SetVolumeMultiplier(DistanceGain*(I==2?(Under?.08f:.9f)*Strength:(Under?.025f:.45f))*VoiceNormalization[I]);
  Voices[I]->SetLowPassFilterFrequency(Under?650.f:FMath::Lerp(14000.f,1200.f,FMath::Clamp(ShoreDistance/18000,0.f,1.f)));
 }
 LifeWait-=Delta;
 if(LifeEnd>0){LifeEnd-=Delta;if(LifeEnd<=0 && Voices[4])Voices[4]->FadeOut(.45f,0);}
 if(!Under && Voices[4] && Voices[4]->IsPlaying() && LifeEnd>0){Voices[4]->FadeOut(.3f,0);LifeEnd=-1;}
 if(Under && HasCreature && LifeWait<=0)
 {
  // Short hydrophone passages belong to the occupied reef habitat, not fictional fish vocalizations.
  Play(4,TEXT("under_reef"),0,LifeGain*.18f,CreatureLocation,Random.FRandRange(.85f,1.08f));
  LifeEnd=Random.FRandRange(1.2f,2.8f);LifeWait=Random.FRandRange(5.f,11.f);++HabitatEvents;
 }
 MoveWait-=Delta;if(Under && Speed>25 && MoveWait<=0)
 {Play(5,TEXT("uw_swim"),-1,FMath::Clamp(Speed/300,0.f,.35f),Eye);MoveWait=Random.FRandRange(.65f,1.2f);}
}
void UOceanSoundscapeComponent::EndPlay(EEndPlayReason::Type Reason)
{for(auto& C:Voices)if(C){C->Stop();C->DestroyComponent();}Voices.Reset();if(ResidentAudio.IsValid()){ResidentAudio->CancelHandle();ResidentAudio.Reset();}bAudioReady=false;Super::EndPlay(Reason);}
int32 UOceanSoundscapeComponent::GetPlayingVoiceCount()const
{int32 Count=0;for(const auto& C:Voices)Count+=C && C->IsPlaying();return Count;}
