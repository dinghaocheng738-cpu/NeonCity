#include "DamageFeedback/DamageFeedbackData.h"
#include "UObject/StrongObjectPtr.h"
UCharacterDamageFeedbackData::UCharacterDamageFeedbackData() { FeedbackID=TEXT("NeonCity.Feedback.Character"); }
bool UCharacterDamageFeedbackData::IsValidProfile() const
{
 return !FeedbackID.IsNone() && FMath::IsFinite(HitReactionSeconds) && HitReactionSeconds>0
  && FMath::IsFinite(MinimumDamage) && MinimumDamage>=0
  && MaximumConcurrentEffects>=1 && MaximumConcurrentEffects<=16
  && MaximumConcurrentDecals>=0 && MaximumConcurrentDecals<=16
  && FMath::IsFinite(EffectLifetime) && EffectLifetime>0
  && InjuredFraction>DownedFraction && CriticalFraction>DownedFraction && InjuredFraction<=1;
}
float UCharacterDamageFeedbackData::ReactionSecondsFor(ENeonCityBodyState State) const
{
 const float Scale=State==ENeonCityBodyState::Downed||State==ENeonCityBodyState::Dead?DownedReactionScale
  :State==ENeonCityBodyState::Injured?InjuredReactionScale:1;
 return FMath::Clamp(HitReactionSeconds*Scale,.05f,3.f);
}
ENeonCityFeedbackTier UCharacterDamageFeedbackData::TierFor(float HealthFraction,ENeonCityBodyState State) const
{
 if(State==ENeonCityBodyState::Dead) return ENeonCityFeedbackTier::Dead;
 if(State==ENeonCityBodyState::Downed) return ENeonCityFeedbackTier::Downed;
 const float Fraction=FMath::Clamp(HealthFraction,0.f,1.f);
 return Fraction<=CriticalFraction?ENeonCityFeedbackTier::Critical:Fraction<=InjuredFraction?ENeonCityFeedbackTier::Injured:ENeonCityFeedbackTier::Healthy;
}
namespace NeonCityFeedback
{
 const UCharacterDamageFeedbackData& DefaultProfile()
 {
  static TStrongObjectPtr<UCharacterDamageFeedbackData> Native;
  if(!Native.IsValid()) Native.Reset(NewObject<UCharacterDamageFeedbackData>(GetTransientPackage(),TEXT("NeonCityDamageFeedbackDefault")));
  return *Native.Get();
 }
}
