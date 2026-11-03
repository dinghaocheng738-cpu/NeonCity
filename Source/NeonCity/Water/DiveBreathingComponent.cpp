#include "Water/DiveBreathingComponent.h"
#include "Adventure/DivingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Camera/CameraComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"

UDiveBreathingComponent::UDiveBreathingComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork; }
void UDiveBreathingComponent::BeginPlay()
{
 Super::BeginPlay();if(GetWorld()->GetNetMode()==NM_DedicatedServer){SetComponentTickEnabled(false);return;}
 for(int32 I=1;I<=3;++I)
  if(auto* S=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Audio/SW_DiveBreath_%02d"),I)))Cycles.Add(S);
 Bubbles=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_RegulatorExhaust"));
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P)return;
 Breath=NewObject<UAudioComponent>(P,TEXT("DemandRegulatorBreathing"));P->AddInstanceComponent(Breath);
 Breath->bAutoActivate=false;Breath->bAutoDestroy=false;Breath->bAllowSpatialization=false;
 Breath->SetupAttachment(P->GetMesh(),TEXT("DiveExhaust"));Breath->RegisterComponent();
 AddTickPrerequisiteComponent(P->GetMesh());
}
FVector UDiveBreathingComponent::GetRegulatorLocation() const
{
 const auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());
 return P && P->GetMesh()->DoesSocketExist(TEXT("DiveExhaust"))?P->GetMesh()->GetSocketLocation(TEXT("DiveExhaust")):FVector::ZeroVector;
}
void UDiveBreathingComponent::StopBreathing()
{
 if(bBreathing && Breath)Breath->FadeOut(.12f,0);
 bBreathing=false;CycleAge=CycleDuration=ExhaustClock=0;
}
FString UDiveBreathingComponent::DescribeExhaust() const
{
 int32 Count=0,Components=0;FString Detail;
 for(const auto& N:Plumes)if(IsValid(N))
 {
  ++Components;
  const auto Controller=N->GetSystemInstanceController();
  if(const auto* Instance=Controller?Controller->GetSystemInstance_Unsafe():nullptr)
   for(const auto& E:Instance->GetEmitters())
   {
    Count+=E->GetNumParticles();
    if(Detail.IsEmpty())Detail=FString::Printf(TEXT("state=%d particles=%d bound=%s at=%s"),int32(E->GetExecutionState()),E->GetNumParticles(),*E->GetBounds().ToString(),*N->GetComponentLocation().ToString());
   }
 }
 return FString::Printf(TEXT("exhaust=%d components=%d live=%d age=%.2f %s"),ExhaustCount,Components,Count,CycleAge,*Detail);
}
void UDiveBreathingComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P || !Breath)return;
 const auto* Kit=P->FindComponentByClass<UDivingComponent>();
 const auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
 const auto* Air=P->FindComponentByClass<UOxygenComponent>();
 const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();
 Plumes.RemoveAllSwap([](const auto& N){return !IsValid(N) || !N->IsActive();});
 const bool Active=P->IsLocallyControlled() && Kit && Kit->IsEquipped() && Water && Water->IsFullySubmerged()
  && Air && Air->CurrentOxygen>0 && Body && Body->CanAct() && !P->IsHidden();
 if(!Active){StopBreathing();return;}
 const float Step=FMath::Clamp(Delta,0.f,.2f);CycleAge+=Step;
 // Exertion decays after a sprint/DPV run, instead of instantly becoming a
 // resting breath when the player releases forward input.
 const float Effort=FMath::Clamp(P->GetVelocity().Size()/320.f,0.f,1.f);
 Exertion=FMath::FInterpTo(Exertion,Effort,Step,Effort>Exertion?1.6f:.22f);
 FirstPersonMix=FMath::FInterpTo(FirstPersonMix,Kit->IsFirstPerson()?1.f:0.f,Step,7.f);
 Breath->SetVolumeMultiplier(FMath::Lerp(.48f,.94f,FirstPersonMix));
 Breath->SetLowPassFilterEnabled(true);
 Breath->SetLowPassFilterFrequency(FMath::Lerp(2500.f,3900.f,FirstPersonMix));
 if(!bBreathing || CycleAge>=CycleDuration)
 {
  bBreathing=true;CycleAge=0;ExhaustClock=0;
  PlaybackRate=FMath::Lerp(.94f,1.22f,Exertion);
  if(Cycles.Num())
  {
   auto* Clip=Cycles[CycleIndex++%Cycles.Num()].Get();Breath->SetSound(Clip);Breath->SetPitchMultiplier(PlaybackRate);
   Breath->Play();CycleDuration=Clip->GetDuration()/PlaybackRate+FMath::Lerp(.45f,.05f,Exertion);
  }
  else CycleDuration=4.2f;
 }
 // Source cycle cuts begin at inhalation; the bubble-rich exhalation follows it.
 // No exhaust is emitted without the mesh-authored regulator socket.
 if(Bubbles && CycleAge*PlaybackRate>=1.45f && CycleAge*PlaybackRate<3.55f && P->GetMesh()->DoesSocketExist(TEXT("DiveExhaust")))
 {
  ExhaustClock+=Step;
  if(ExhaustClock>=FMath::Lerp(.21f,.14f,Exertion) && Plumes.Num()<24)
  {
   ExhaustClock=0;const FVector At=GetRegulatorLocation();FWaterSurfaceSample Sample;
   if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(At,Sample) && Sample.DepthAt(At)>6)
    if(auto* Plume=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Bubbles,At,FRotator::ZeroRotator,FVector::OneVector,true,false,ENCPoolMethod::None,true))
    {
     Plume->SetVariableFloat(TEXT("User.SurfaceZ"),Sample.SurfacePoint.Z);
     // World-space particles retain release momentum while the diver moves
     // away. The burst cannot stay welded to the body like a fountain.
     FVector Release=P->GetVelocity()*.32f+FVector(0,0,55);
     Release.Z=FMath::Max(32.,Release.Z);
     Plume->SetVariableVec3(TEXT("User.ReleaseDirection"),Release.GetSafeNormal());
     Plume->SetVariableFloat(TEXT("User.ReleaseSpeed"),Release.Size());
     Plume->Activate(true);
     Plumes.Add(Plume);++ExhaustCount;
    }
  }
 }
}
void UDiveBreathingComponent::EndPlay(EEndPlayReason::Type Reason)
{
 StopBreathing();if(Breath){Breath->Stop();Breath->DestroyComponent();Breath=nullptr;}
 for(const auto& N:Plumes)if(IsValid(N))N->DestroyComponent();Plumes.Reset();
 Super::EndPlay(Reason);
}
