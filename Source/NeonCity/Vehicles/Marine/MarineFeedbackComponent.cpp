#include "Vehicles/Marine/MarineFeedbackComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "Sound/SoundWave.h"

UMarineFeedbackComponent::UMarineFeedbackComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 EngineLoop=TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/VerticalSlice/Audio/SW_NC205C_BoatEngineLoop.SW_NC205C_BoatEngineLoop")));
}
void UMarineFeedbackComponent::BeginPlay()
{
 Super::BeginPlay();
 if(GetWorld()->GetNetMode()==NM_DedicatedServer || !GetOwner()->GetRootComponent())return;
 USoundBase* Sound=EngineLoop.LoadSynchronous();
 EngineAudio=NewObject<UAudioComponent>(GetOwner(),TEXT("MarineEngineAudio"));
 EngineAudio->bAutoActivate=false;EngineAudio->bAutoDestroy=false;
 EngineAudio->bAllowSpatialization=true;EngineAudio->bOverrideAttenuation=true;
 EngineAudio->AttenuationOverrides.bAttenuate=true;EngineAudio->AttenuationOverrides.bSpatialize=true;
 EngineAudio->AttenuationOverrides.AttenuationShape=EAttenuationShape::Sphere;
 EngineAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(450);
 EngineAudio->AttenuationOverrides.FalloffDistance=5500;
 GetOwner()->AddInstanceComponent(EngineAudio);
 EngineAudio->SetupAttachment(GetOwner()->GetRootComponent());EngineAudio->SetRelativeLocation(FVector(-180,0,0));
 EngineAudio->SetSound(Sound);EngineAudio->RegisterComponent();
 const auto* Library=LoadObject<UNeonCityReferenceSoundscapeData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape"));
 if(!Library)return;
 // Recorded water layers remain driven by hull speed when the engine is stopped.
 const FName Banks[]={TEXT("boat_rush"),TEXT("boat_lap"),TEXT("boat_rush")};
 const FVector Points[]={FVector(180,0,-20),FVector(-210,0,-30),FVector(100,65,10)};
 WaterAudio.SetNum(3);
 for(int32 I=0;I<3;++I)
 {
  const auto* Bank=Library->Find(Banks[I]);if(!Bank || Bank->Clips.IsEmpty())continue;
  auto* Wave=Bank->Clips[0].LoadSynchronous();if(!Wave)continue;
  auto* C=NewObject<UAudioComponent>(GetOwner());C->bAutoActivate=false;C->bAutoDestroy=false;
  C->bAllowSpatialization=true;C->bOverrideAttenuation=true;C->AttenuationOverrides=EngineAudio->AttenuationOverrides;
  C->AttenuationOverrides.FalloffDistance=I==2?1800:3200;
  GetOwner()->AddInstanceComponent(C);C->SetupAttachment(GetOwner()->GetRootComponent());C->SetRelativeLocation(Points[I]);
  C->SetSound(Wave);C->SetVolumeMultiplier(0);C->SetLowPassFilterEnabled(true);
  C->SetLowPassFilterFrequency(I==1?1200:14000);C->RegisterComponent();WaterAudio[I]=C;
 }
}
void UMarineFeedbackComponent::UpdateFeedback(const FMarineDriveTelemetry& State,float MaxSpeed)
{
 const float Speed=FMath::Clamp(State.SpeedKmh/FMath::Max(1.f,MaxSpeed),0.f,1.f);
 const float SpeedKmh=FMath::Abs(State.SpeedKmh);
 const float Targets[]={State.Afloat?FMath::Clamp(SpeedKmh/40,0.f,1.f)*.6f:0,
  State.Afloat?.09f+Speed*.28f:0,State.Afloat?FMath::Square(FMath::Clamp((SpeedKmh-16)/50,0.f,1.f))*.42f:0};
 for(int32 I=0;I<WaterAudio.Num();++I)
 {
  auto* C=WaterAudio[I].Get();if(!C)continue;
  WaterGains[I]=FMath::FInterpTo(WaterGains[I],Targets[I],GetWorld()->GetDeltaSeconds(),3);
  if(Targets[I]>.001f && !C->IsPlaying())C->Play(I==2?3.4f:0);
  C->SetVolumeMultiplier(WaterGains[I]);C->SetPitchMultiplier(I==1?.8f+Speed*.15f: .8f+Speed*.6f);
  if(Targets[I]<=.001f && WaterGains[I]<.001f)C->Stop();
 }
 if(!EngineAudio)return;
 if(!EngineAudio->GetSound())return;
 if(State.EngineRunning!=bPlaybackRequested)
 {
  bPlaybackRequested=State.EngineRunning;
  if(bPlaybackRequested)EngineAudio->FadeIn(.2f);
  else EngineAudio->FadeOut(.25f,0);
 }
 if(!bPlaybackRequested)return;
 const float Load=FMath::Clamp(State.Throttle,0.f,1.f);
 EngineAudio->SetPitchMultiplier(.72f+.5f*Load+.28f*Speed);
 EngineAudio->SetVolumeMultiplier(.18f+.42f*Load+.10f*Speed);
}
void UMarineFeedbackComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(EngineAudio) { EngineAudio->Stop();EngineAudio->DestroyComponent();EngineAudio=nullptr; }
 for(auto& C:WaterAudio)if(C){C->Stop();C->DestroyComponent();}WaterAudio.Reset();
 bPlaybackRequested=false;Super::EndPlay(Reason);
}
