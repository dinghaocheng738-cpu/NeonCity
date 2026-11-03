#include "World/Foliage/NeonCityFoliageContactAudioComponent.h"
#include "World/Foliage/NeonCityFoliageInteractionComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UNeonCityFoliageContactAudioComponent::UNeonCityFoliageContactAudioComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 BrushSound=FSoftObjectPath(TEXT("/Game/NeonCity/World/Foliage/Audio/SW_NC_FoliageBrush.SW_NC_FoliageBrush"));
}
void UNeonCityFoliageContactAudioComponent::BeginPlay()
{
 Super::BeginPlay();Voices.SetNum(VoiceBudget);Random.GenerateNewSeed();
 Interaction=GetOwner()->FindComponentByClass<UNeonCityFoliageInteractionComponent>();
 if(Interaction.IsValid())ContactHandle=Interaction->OnSoftContact.AddUObject(this,&UNeonCityFoliageContactAudioComponent::OnSoftContact);
 if(GetWorld()->AllowAudioPlayback() && GetWorld()->GetAudioDeviceRaw())
 {
  Wave=BrushSound.LoadSynchronous();
  if(!Wave)UE_LOG(LogTemp,Warning,TEXT("Foliage brush sound unavailable; physical contact remains active"));
 }
}
void UNeonCityFoliageContactAudioComponent::OnSoftContact(const FVector& Position,float Speed,float Strength)
{
 auto* W=GetWorld();if(!W || Position.ContainsNaN() || !FMath::IsFinite(Speed) || !FMath::IsFinite(Strength)
  || Speed<35 || Strength<4)return;
 auto* PC=W->GetFirstPlayerController();if(!PC || !PC->IsLocalController() || !PC->GetPawn())return;
 const float Now=W->GetTimeSeconds();if(Now<NextContact)return;
 FVector Listener,Front,Right;PC->GetAudioListenerPosition(Listener,Front,Right);
 if(FVector::DistSquared(Listener,Position)>FMath::Square(1200.))return;
 int32 Slot=INDEX_NONE;
 for(int32 I=0;I<VoiceBudget;++I)
  if(Now>=AvailableAt[I] && (!Voices[I] || !Voices[I]->IsPlaying())){Slot=I;break;}
 if(Slot==INDEX_NONE)return; // Never abruptly steal an active rustle or grow the pool.
 const float Pitch=Random.FRandRange(.95f,1.05f);
 const float Duration=(Wave?Wave->Duration:1.1145805f)/Pitch;
 NextContact=Now+.3f;AvailableAt[Slot]=Now+Duration;++Events;
 if(!Wave || !W->AllowAudioPlayback() || !W->GetAudioDeviceRaw())return;
 auto* Voice=Voices[Slot].Get();
 if(!Voice)
 {
  Voice=NewObject<UAudioComponent>(GetOwner());Voice->bAutoActivate=false;Voice->bAutoDestroy=false;
  Voice->bAllowSpatialization=true;Voice->bIsUISound=false;
  FSoundAttenuationSettings Settings;Settings.bAttenuate=true;Settings.bSpatialize=true;
  Settings.AttenuationShapeExtents=FVector(300,0,0);Settings.FalloffDistance=900;Settings.bEnableOcclusion=false;
  Voice->SetAttenuationOverrides(Settings);GetOwner()->AddInstanceComponent(Voice);Voice->RegisterComponent();Voices[Slot]=Voice;
 }
 Voice->SetSound(Wave);Voice->SetWorldLocation(Position);Voice->SetPitchMultiplier(Pitch);
 const float Motion=FMath::GetMappedRangeValueClamped(FVector2D(35,550),FVector2D(.12f,.42f),Speed);
 Voice->SetVolumeMultiplier(Motion*FMath::Clamp(Strength/24.f,.35f,1.f));Voice->Play();
}
int32 UNeonCityFoliageContactAudioComponent::GetVoiceAllocationCount() const
{int32 N=0;for(auto C:Voices)N+=IsValid(C);return N;}
int32 UNeonCityFoliageContactAudioComponent::GetPlayingVoiceCount() const
{int32 N=0;for(auto C:Voices)N+=C && C->IsPlaying();return N;}
void UNeonCityFoliageContactAudioComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Interaction.IsValid())Interaction->OnSoftContact.Remove(ContactHandle);
 for(auto C:Voices)if(C){C->Stop();C->DestroyComponent();}Voices.Reset();Wave=nullptr;
 Super::EndPlay(Reason);
}
