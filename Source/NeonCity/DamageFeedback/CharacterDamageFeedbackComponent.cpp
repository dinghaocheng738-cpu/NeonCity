#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackEffect.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "Particles/ParticleSystem.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
UCharacterDamageFeedbackComponent::UCharacterDamageFeedbackComponent() { PrimaryComponentTick.bCanEverTick=false; }
const UCharacterDamageFeedbackData& UCharacterDamageFeedbackComponent::GetProfile() const
{ return Profile && Profile->IsValidProfile() ? *Profile : NeonCityFeedback::DefaultProfile(); }
float UCharacterDamageFeedbackComponent::GetHealthFraction() const
{ return Health && Health->GetMaxHealth()>0 ? FMath::Clamp(Health->GetCurrentHealth()/Health->GetMaxHealth(),0.f,1.f) : 1.f; }
ENeonCityFeedbackTier UCharacterDamageFeedbackComponent::GetFeedbackTier() const { return Tier; }
bool UCharacterDamageFeedbackComponent::IsHitReactionActive() const
{ return GetWorld() && GetWorld()->GetTimeSeconds()<ReactionUntil; }
bool UCharacterDamageFeedbackComponent::IsDamageDirectionActive() const
{ return GetWorld() && GetWorld()->GetTimeSeconds()<DirectionUntil && !LastSample.DamageDirection.IsNearlyZero(); }
int32 UCharacterDamageFeedbackComponent::GetActiveEffectCount() const
{
 int32 Count=0; for(const auto& Effect:ActiveEffects) if(Effect.IsValid()) ++Count; return Count;
}
void UCharacterDamageFeedbackComponent::BeginPlay()
{
 Super::BeginPlay();
 Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>();
 if(Health)
 {
  Health->OnDamageTaken.AddDynamic(this,&UCharacterDamageFeedbackComponent::Damaged);
  Health->OnHealthChanged.AddDynamic(this,&UCharacterDamageFeedbackComponent::HealthChanged);
 }
 if(Body) Body->OnBodyStateChanged.AddDynamic(this,&UCharacterDamageFeedbackComponent::BodyStateChanged);
 const auto& Data=GetProfile();
 // Assets resolve once so a hit never hitches on a synchronous load.
 if(!Data.BloodHitEffect.IsNull()) BloodEffect=Data.BloodHitEffect.LoadSynchronous();
 if(!Data.BloodDecalMaterial.IsNull()) DecalMaterial=Data.BloodDecalMaterial.LoadSynchronous();
 FeedbackState=Body ? Body->GetBodyState() : ENeonCityBodyState::Normal; ApplyTier();
}
void UCharacterDamageFeedbackComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Health) { Health->OnDamageTaken.RemoveAll(this); Health->OnHealthChanged.RemoveAll(this); }
 if(Body) Body->OnBodyStateChanged.RemoveAll(this);
 for(auto& Effect:ActiveEffects) if(Effect.IsValid()) Effect->Destroy();
 ActiveEffects.Reset(); ActiveDecals.Reset();
 Super::EndPlay(Reason);
}
void UCharacterDamageFeedbackComponent::ResetFeedback()
{
 for(auto& Effect:ActiveEffects) if(Effect.IsValid()) Effect->Destroy();
 ActiveEffects.Reset(); ActiveDecals.Reset();
 LastSample=FNeonCityDamageFeedbackEvent(); ReactionUntil=-1; DirectionUntil=-1;
 Tier=ENeonCityFeedbackTier::Healthy; FeedbackState=ENeonCityBodyState::Normal;
 HitReactionCount=InjuredCount=DownedCount=DeathCount=ImpactEffectCount=RefusedEffectCount=DecalCount=0;
}
void UCharacterDamageFeedbackComponent::ApplyTier()
{
 const float Fraction=GetHealthFraction();
 const auto Previous=Tier;
 Tier=GetProfile().TierFor(Fraction,FeedbackState);
 if(Tier==Previous) return;
 if(Tier==ENeonCityFeedbackTier::Injured) { ++InjuredCount; OnInjuredFeedback(Fraction); }
 else if(Tier==ENeonCityFeedbackTier::Critical) { ++InjuredCount; OnCriticalFeedback(Fraction); }
 else if(Tier==ENeonCityFeedbackTier::Downed) { ++DownedCount; OnDownedFeedback(); }
 else if(Tier==ENeonCityFeedbackTier::Dead) { ++DeathCount; OnDeathFeedback(); }
}
void UCharacterDamageFeedbackComponent::HealthChanged(float Current,float Maximum) { ApplyTier(); }
void UCharacterDamageFeedbackComponent::BodyStateChanged(ENeonCityBodyState Previous,ENeonCityBodyState Current)
{ FeedbackState=Current; ApplyTier(); }
void UCharacterDamageFeedbackComponent::Damaged(const FNeonCityDamageEvent& Event)
{
 if(!GetWorld()) return;
 // A hit is still valid feedback when it carries no source actor; only the direction degrades.
 const auto& Data=GetProfile();
 if(!FMath::IsFinite(Event.DamageAmount) || Event.DamageAmount<=0 || Event.DamageAmount<Data.MinimumDamage) return;
 FeedbackState=Body ? Body->GetBodyState() : FeedbackState;
 const FVector Origin=GetOwner()->GetActorLocation();
 FVector Source=Event.HitLocation;
 if(Event.Instigator && Event.Instigator->GetPawn()) Source=Event.Instigator->GetPawn()->GetActorLocation();
 else if(Event.SourceActor) Source=Event.SourceActor->GetActorLocation();
 // The direction survives the hit so a HUD can point at the shooter after the reaction ends.
 FVector Direction=Source-Origin; Direction.Z=0;
 Direction=Direction.IsNearlyZero()?FVector::ZeroVector:Direction.GetSafeNormal();
 const float Now=GetWorld()->GetTimeSeconds();
 LastSample.Amount=Event.DamageAmount; LastSample.DamageType=Event.DamageType; LastSample.BodyState=FeedbackState;
 LastSample.Tier=Data.TierFor(GetHealthFraction(),FeedbackState);
 LastSample.HitLocation=Event.HitLocation.IsNearlyZero()?Origin:Event.HitLocation;
 LastSample.DamageDirection=Direction; LastSample.HealthFraction=GetHealthFraction();
 LastSample.Instigator=Event.Instigator; LastSample.Timestamp=Now;
 ++HitReactionCount; ReactionUntil=Now+Data.ReactionSecondsFor(FeedbackState);
 if(!Direction.IsNearlyZero()) DirectionUntil=Now+Data.DamageDirectionSeconds;
 EmitImpactEffect(LastSample.HitLocation);
 OnHitReactionFeedback(Direction,Event.DamageAmount);
 OnDamageFeedback.Broadcast(LastSample);
}
void UCharacterDamageFeedbackComponent::PruneEffects()
{
 ActiveEffects.RemoveAll([](const TWeakObjectPtr<AActor>& Effect){ return !Effect.IsValid(); });
 ActiveDecals.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& Decal){ return !Decal.IsValid(); });
}
void UCharacterDamageFeedbackComponent::EmitImpactEffect(FVector Location)
{
 PruneEffects();
 const auto& Data=GetProfile();
 if(ActiveEffects.Num()>=Data.MaximumConcurrentEffects) { ++RefusedEffectCount; return; }
 if(BloodEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),BloodEffect,Location,FVector::UpVector.Rotation());
 else if(Data.bPrototypeImpactEffect)
 {
  auto* Effect=ACharacterDamageFeedbackEffect::Spawn(GetWorld(),Location,Data.EffectRadius,Data.EffectLifetime,Data.BloodColor);
  if(!Effect) return; ActiveEffects.Add(Effect);
 }
 else return;
 ++ImpactEffectCount; EmitBloodDecal(Location);
}
void UCharacterDamageFeedbackComponent::EmitBloodDecal(FVector Location)
{
 const auto& Data=GetProfile();
 if(!DecalMaterial || Data.MaximumConcurrentDecals<=0) return;
 PruneEffects();
 // Marking decals expire on their own; nothing here is written to the save file.
 if(ActiveDecals.Num()>=Data.MaximumConcurrentDecals) { ++RefusedEffectCount; return; }
 auto* Decal=UGameplayStatics::SpawnDecalAtLocation(GetWorld(),DecalMaterial,FVector(44,44,14),Location,FRotator(-90,0,0),Data.DecalLifeSpan);
 if(!Decal) return; Decal->SetFadeScreenSize(0); ActiveDecals.Add(Decal); ++DecalCount;
}
