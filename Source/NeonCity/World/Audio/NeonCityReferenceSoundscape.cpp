#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/SoundWave.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

const FNeonCityReferenceSoundBank* UNeonCityReferenceSoundscapeData::Find(FName Id) const
{return Banks.FindByPredicate([Id](const auto& B){return B.Id==Id;});}

bool UNeonCityReferenceSoundscapeData::IsValidLibrary() const
{
 if(ReferenceCommit!=TEXT("4811ba48d795197de5621985f404e765c0b7c0ef") || Banks.Num()!=43)return false;
 TSet<FName> Seen;
 for(const auto& B:Banks)
 {
  if(B.Id.IsNone() || Seen.Contains(B.Id) || B.Clips.IsEmpty() || B.Clips.Num()>32 || B.Clips.Num()!=B.LoudnessLUFS.Num()
   || B.Clips.Num()!=B.Durations.Num() || B.Channels<1 || B.Channels>2 || (B.bLoop && B.Clips.Num()!=1))return false;
  for(int32 I=0;I<B.Clips.Num();++I)
   if(B.Clips[I].IsNull() || !FMath::IsFinite(B.LoudnessLUFS[I]) || B.LoudnessLUFS[I]<-100 || B.LoudnessLUFS[I]>0
    || !FMath::IsFinite(B.Durations[I]) || B.Durations[I]<=0 || B.Durations[I]>180)return false;
  Seen.Add(B.Id);
 }
 return true;
}

UNeonCityReferenceSoundscapeComponent::UNeonCityReferenceSoundscapeComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;
 PrimaryComponentTick.TickInterval=1.f/30.f;
 SoundLibrary=FSoftObjectPath(TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape.DA_NCTW_Soundscape"));
}

bool UNeonCityReferenceSoundscapeComponent::InitializeSoundscape()
{
 if(bReady)return true;
 Vegetation=Cast<ANeonCityReferenceVegetation>(GetOwner());
 if(!Vegetation || !Vegetation->GrassData)return false; // Supplemental density layers never duplicate sound.
 Data=SoundLibrary.LoadSynchronous();
 if(!Data || !Data->IsValidLibrary())return false;
 // Prepare only banks this adapter plays. Retaining the handle keeps original
 // clips resident; first-use synchronous phrase loads cost5-7ms in trace784.
 TArray<FSoftObjectPath> Clips;
 for(const auto& Bank:Data->Banks)
  if(Bank.Id.ToString().StartsWith(TEXT("step_")) || Bank.Id==TEXT("bird_dove") || Bank.Id==TEXT("bird_forest")
   || Bank.Id==TEXT("wind") || Bank.Id==TEXT("palms") || Bank.Id==TEXT("crickets") || Bank.Id==TEXT("birds_dawn"))
   for(const auto& Clip:Bank.Clips)Clips.AddUnique(Clip.ToSoftObjectPath());
 ResidentAudio=UAssetManager::GetStreamableManager().RequestAsyncLoad(Clips);
 if(!ResidentAudio.IsValid())return false;
 Random.GenerateNewSeed();Beds.SetNum(4);Steps.SetNum(3);Birds.SetNum(4);
 bReady=true;SetComponentTickEnabled(true);return true;
}

bool UNeonCityReferenceSoundscapeComponent::IsAudioEnabled() const
{return GetWorld() && GetWorld()->AllowAudioPlayback() && GetWorld()->GetAudioDeviceRaw()!=nullptr;}

UAudioComponent* UNeonCityReferenceSoundscapeComponent::AllocateVoice(bool Spatial)
{
 if(!IsAudioEnabled())return nullptr;
 auto* C=NewObject<UAudioComponent>(GetOwner());C->bAutoActivate=false;C->bAutoDestroy=false;
 C->bAllowSpatialization=Spatial;C->bIsUISound=false;C->bShouldRemainActiveIfDropped=true;
 if(Spatial)
 {
  FSoundAttenuationSettings Settings;Settings.bAttenuate=false;Settings.bSpatialize=true;
  Settings.bEnableOcclusion=false;C->SetAttenuationOverrides(Settings);
 }
 GetOwner()->AddInstanceComponent(C);C->RegisterComponent();return C;
}

void UNeonCityReferenceSoundscapeComponent::UpdateBeds(float Delta,const NeonCityForestSound::FMix& Mix,bool Active)
{
 using namespace NeonCityForestSound;
 const FName Names[]={TEXT("wind"),TEXT("palms"),TEXT("crickets"),TEXT("birds_dawn")};
 const double Targets[]={Mix.Wind,Mix.Palms,Mix.Crickets,Mix.Dawn};
 const double Taus[]={.3,.5,1.,2.};
 for(int32 I=0;I<4;++I)
 {
  const auto* B=Data->Find(Names[I]);if(!B || !B->bLoop)continue;
  const double Target=Active?Targets[I]/Db(B->LoudnessLUFS[0])*(B->Channels==1?Db(-3):1)*.8:0;
  double Tau=Taus[I];auto* C=Beds[I].Get();
  if(Target>1e-4 && IsAudioEnabled() && (!C || !C->IsPlaying()))
  {
   if(!C){C=AllocateVoice(false);Beds[I]=C;}
   if(auto* Wave=B->Clips[0].Get();C && Wave)
   {
    C->SetSound(Wave);C->SetVolumeMultiplier(FMath::Max(LoopGain[I],.0001f));
    C->Play(Random.FRand()*B->Durations[0]);Tau=FMath::Max(Tau,.8);
   }
  }
  LoopGain[I]+=float((Target-LoopGain[I])*(1-FMath::Exp(-Delta/Tau)));
  if(C)
  {
   C->SetVolumeMultiplier(LoopGain[I]);
   if(I==0){C->SetLowPassFilterEnabled(true);C->SetLowPassFilterFrequency(float(Mix.WindLowPass));}
   if(!Active && LoopGain[I]<.00001f)C->Stop();
  }
 }
}

float UNeonCityReferenceSoundscapeComponent::PlayPhrase(FName Bank,int32 Pick,float TargetLufs,float Pitch,const FVector& At,bool Spatial)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(IslandSoundPhrase);
 const auto* B=Data->Find(Bank);if(!B || B->bLoop || !FMath::IsFinite(Pitch) || Pitch<=0)return 0;
 int32 Previous=LastSlice.FindRef(Bank);
 if(!B->Clips.IsValidIndex(Pick))
 {Pick=Random.RandRange(0,B->Clips.Num()-1);if(Pick==Previous && B->Clips.Num()>1)Pick=(Pick+1)%B->Clips.Num();}
 LastSlice.Add(Bank,Pick);
 const float Duration=B->Durations[Pick]/Pitch;
 if(!IsAudioEnabled())return Duration;
 auto* Wave=B->Clips[Pick].Get();if(!Wave)return Duration;
 auto& Voices=Spatial?Birds:Steps;int32& Next=Spatial?BirdSlot:StepSlot;
 const int32 Slot=Next++%Voices.Num();auto* Voice=Voices[Slot].Get();
 if(!Voice){Voice=AllocateVoice(Spatial);Voices[Slot]=Voice;}if(!Voice)return 0;
 const float Gain=float(NeonCityForestSound::SourceGain(TargetLufs,B->LoudnessLUFS[Pick],B->Channels)*.8);
 Voice->Stop();Voice->SetSound(Wave);Voice->SetPitchMultiplier(Pitch);Voice->SetWorldLocation(At);
 if(Spatial)
 {
  BirdGain[Slot]=Gain;BirdLocations[Slot]=At;
  const double Distance=FVector::Distance(Listener,At)/100;
  Voice->SetVolumeMultiplier(Gain*float(10/FMath::Max(10.,Distance)));
  Voice->SetLowPassFilterEnabled(true);Voice->SetLowPassFilterFrequency(float(FMath::Clamp(18000/(1+Distance/45),1500.,18000.)));
 }
 else Voice->SetVolumeMultiplier(Gain);
 Voice->Play();return Duration;
}

void UNeonCityReferenceSoundscapeComponent::UpdateFootsteps(ACharacter* Walker,float Delta,bool Active)
{
 StepTimer+=Delta;
 auto* M=Walker?Walker->GetCharacterMovement():nullptr;
 if(!Active || !M || !M->IsMovingOnGround())
 {PreviousWalker.Reset();PreviousBase.Reset();StepDistance=0;return;}
 const FVector Foot=Walker->GetActorLocation()-FVector(0,0,Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 auto* Base=Cast<UPrimitiveComponent>(Walker->GetMovementBaseObject());FVector Old=PreviousFoot;
 if(Base && Base==PreviousBase.Get())Old=Base->GetComponentTransform().TransformPosition(PreviousBaseTransform.InverseTransformPosition(Old));
 const double Moved=FVector::Dist2D(Old,Foot);
 const bool Same=PreviousWalker==Walker && PreviousBase==Base;
 PreviousWalker=Walker;PreviousBase=Base;PreviousFoot=Foot;
 PreviousBaseTransform=Base?Base->GetComponentTransform():FTransform::Identity;
 if(!Same || Moved>FMath::Max(100.,M->MaxWalkSpeed*Delta*2.5)) {StepDistance=0;return;}
 if(Moved<.5)return;
 StepDistance+=float(Moved);
 // Native 3P stride follows actual ground travel; subtract the moving base.
 // Sound does not change the template's animation or character movement.
 const float Stride=FMath::GetMappedRangeValueClamped(FVector2D(160,500),FVector2D(62,145),M->Velocity.Size2D());
 if(StepDistance<Stride || StepTimer<.2f)return;
 StepDistance=0;StepTimer=0;
 FName Surface=TEXT("grass");float Trim=2;
 const FHitResult& Ground=M->CurrentFloor.HitResult;
 if(Ground.Component.IsValid() && Ground.Component->ComponentHasTag(TEXT("NeonCity.Foliage.Wood")))
 {Surface=TEXT("wood");Trim=2;}
 else if(Ground.GetActor() && !Ground.GetActor()->IsA<ANeonCityReferenceVegetation>())
 {
  // The existing island's support geometry is the exact wooden village/pier;
  // terrain itself is a Landscape. Never classify arbitrary urban actors.
  const auto* Boat=Cast<ANeonCityMarineVehicleBase>(Ground.GetActor());
  if(Ground.GetActor()->IsA<ANeonCityReferenceVillage>() || (Boat && Boat->GetProfile()->bReferenceDisplacementHull))
  {Surface=TEXT("wood");Trim=2;}
  else
  {
   const double Height=(Foot.Z-Vegetation->GetActorLocation().Z)/100;
   Surface=Height<.6?TEXT("wetsand"):Height>3.6?TEXT("grass"):TEXT("sand");Trim=Surface==TEXT("grass")?2:0;
  }
 }
 LastStepSurface=Surface;++StepEvents;
 PlayPhrase(FName(*(TEXT("step_")+Surface.ToString())),-1,-56+Trim+(Random.FRand()-.5f)*3,.94f+Random.FRand()*.12f,Foot,false);
}

bool UNeonCityReferenceSoundscapeComponent::FindPerch(FVector& Position)
{
 // Distinct name: a local `Origin` trips C4459 against the anonymous-namespace
 // Origin in CoastalOceanSurface.cpp once both are visible to the same header set.
 const FVector VegetationOrigin=Vegetation->GetActorLocation();
 for(int32 I=0;I<8;++I)
 {
  const double Angle=Random.FRand()*UE_DOUBLE_PI*2,R=(10+Random.FRand()*70)*100;
  const FVector P=Listener+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);
  double Coast=0,Zone=0;
  if(!NeonCityReferenceCoast::Query((P.X-VegetationOrigin.X)/100,-(P.Y-VegetationOrigin.Y)/100,Coast,Zone) || -Coast<8
   || Random.FRand()>.25+.75*NeonCityReferenceCoast::Smooth(8,45,-Coast))continue;
  FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestBirdPerch),false);
  for(TActorIterator<ANeonCityReferenceVegetation> It(GetWorld());It;++It)Params.AddIgnoredActor(*It);
  // At most eight ground queries when starting a bout, never per tree/frame.
  if(!GetWorld()->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,VegetationOrigin.Z+40000),FVector(P.X,P.Y,VegetationOrigin.Z-10000),ECC_Visibility,Params))continue;
  Position=Hit.ImpactPoint+FVector(0,0,300+Random.FRand()*900);return true;
 }
 return false;
}

void UNeonCityReferenceSoundscapeComponent::UpdateBirds(float Delta,const NeonCityForestSound::FMix& Mix,bool Active)
{
 if(!Active || Mix.BirdActivity<.01)
 {Singers.Reset();for(auto C:Birds)if(C)C->FadeOut(.15f,0);return;}
 BirdTimer-=Delta;const double Rate=Mix.BirdActivity*Mix.Habitat;
 if(BirdTimer<=0)
 {
  BirdTimer=Rate>.02?float((4+Random.FRand()*20)/Rate):3;
  if(Rate>.02 && Singers.Num()<3)
  {
   FVector At;if(FindPerch(At))
   {
    const bool Dove=Random.FRand()<.15+.25*Mix.Lull;FSinger S;
    S.Location=At;S.Bank=Dove?TEXT("bird_dove"):TEXT("bird_forest");S.Group=Random.RandRange(0,1);
    S.Remaining=1+Random.RandRange(0,Dove?1:3);S.Wait=Random.FRand()*.5f;
    S.Pitch=.93f+Random.FRand()*.14f;S.TargetLufs=(Dove?-34:-31)+(Random.FRand()-.5f)*6;Singers.Add(S);
   }
  }
 }
 for(int32 I=Singers.Num()-1;I>=0;--I)
 {
  auto& S=Singers[I];S.Wait-=Delta;if(S.Wait>0)continue;
  if(S.Remaining<=0 || FVector::DistSquared(Listener,S.Location)>FMath::Square(15000.)) {Singers.RemoveAt(I);continue;}
  int32 Pick=-1;
  if(S.Bank==TEXT("bird_forest"))
  {const int32 Start=S.Group?7:0,Count=S.Group?6:7;Pick=Start+Random.RandRange(0,Count-1);if(Pick==LastSlice.FindRef(S.Bank))Pick=Start+(Pick-Start+1)%Count;}
  const float Duration=PlayPhrase(S.Bank,Pick,S.TargetLufs+(Random.FRand()-.5f)*2,S.Pitch*(.98f+Random.FRand()*.04f),S.Location,true);
  ++BirdEvents;--S.Remaining;S.Wait=Duration+.8f+Random.FRand()*4.5f;
 }
 for(int32 I=0;I<Birds.Num();++I)if(auto* C=Birds[I].Get();C && C->IsPlaying())
  C->SetVolumeMultiplier(BirdGain[I]*float(1000/FMath::Max(1000.,FVector::Distance(Listener,BirdLocations[I]))));
}

void UNeonCityReferenceSoundscapeComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);if(!bReady || !ResidentAudio.IsValid() || !ResidentAudio->HasLoadCompleted() || !FMath::IsFinite(Delta) || Delta<=0)return;
 Delta=FMath::Min(Delta,.25f);ClockTimer+=Delta;
 if(ClockTimer>=1)
 {
  ClockTimer=0;if(!Clock.IsValid())for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){Clock=*It;break;}
 }
 APlayerController* PC=nullptr;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto* P=It->Get();P && P->IsLocalController()){PC=P;break;}
 bool Active=false;ACharacter* Walker=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(PC && PC->GetPawn() && Vegetation)
 {
  FVector Front,Right;PC->GetAudioListenerPosition(Listener,Front,Right);
  const FVector P=(Listener-Vegetation->GetActorLocation())/100;
  double D=0,BZ=0;
  Active=!P.ContainsNaN() && FMath::Abs(P.X)<1024 && FMath::Abs(P.Y)<1024 && P.Z>-.1 && P.Z<400
   && NeonCityReferenceCoast::Query(P.X,-P.Y,D,BZ);
  Environment.Hour=Clock.IsValid()?Clock->GetTimeOfDay():12;
  Environment.Day=FMath::Max(0.,FMath::Sin((Environment.Hour-6)*UE_DOUBLE_PI/12));
  Environment.bOnLand=D<0;Environment.DistanceInland=FMath::Abs(D);
  Environment.Wind=FMath::IsFinite(Vegetation->WindSpeed)?FMath::Clamp(double(Vegetation->WindSpeed),0.,25.):0;
 }
 GustTimer-=Delta;
 if(GustTimer<=0){GustTarget=Random.FRand()<.35f?.03f+Random.FRand()*.12f:.3f+Random.FRand()*.7f;GustTimer=2+Random.FRand()*6;}
 Gust=float(NeonCityForestSound::AdvanceGust(Gust,GustTarget,Delta));
 const auto Mix=NeonCityForestSound::Evaluate(Environment,Gust);
 bActiveNow=Active;
 UpdateBeds(Delta,Mix,Active);UpdateFootsteps(Walker,Delta,Active);UpdateBirds(Delta,Mix,Active);
}

int32 UNeonCityReferenceSoundscapeComponent::GetVoiceAllocationCount() const
{
 int32 Count=0;for(const auto* Voices:{&Beds,&Steps,&Birds})for(auto C:*Voices)Count+=IsValid(C);return Count;
}
int32 UNeonCityReferenceSoundscapeComponent::GetPlayingVoiceCount() const
{
 int32 Count=0;for(const auto* Voices:{&Beds,&Steps,&Birds})for(auto C:*Voices)Count+=C && C->IsPlaying();return Count;
}
void UNeonCityReferenceSoundscapeComponent::EndPlay(EEndPlayReason::Type Reason)
{
 bReady=false;SetComponentTickEnabled(false);
 for(auto* Voices:{&Beds,&Steps,&Birds}){for(auto C:*Voices)if(C){C->Stop();C->DestroyComponent();}Voices->Reset();}
 Singers.Reset();ResidentAudio.Reset();Super::EndPlay(Reason);
}
