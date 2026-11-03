#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionEvent.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Explosion/ExplosionImpulseUtility.h"
#include "Explosion/ExplosionFutureInterfaces.h"
#include "Destruction/BreakableSurfaceData.h"
#include "Destruction/DestructionTypes.h"
#include "Destruction/DestructionSubsystem.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Destruction/DestructibleStructure.h"
#include "Destruction/DestructionTestArea.h"
#include "DamageFeedback/DamageFeedbackData.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackEffect.h"
#include "World/WorldEventSubsystem.h"
#include "World/WorldEventTypes.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Physics/DestructiblePropComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "VehicleCombat/ExplosionDamageComponent.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "AI/Police/PoliceCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplosionProfiles,"NeonCity.Explosion.Profiles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExplosionProfiles::RunTest(const FString&)
{
 const TCHAR* IDs[]={TEXT("NeonCity.SmallExplosion"),TEXT("NeonCity.VehicleExplosion"),TEXT("NeonCity.RocketExplosion"),TEXT("NeonCity.MissileExplosion")};
 for(const TCHAR* ID:IDs)
 {
  const FName Key(ID); const UNeonCityExplosionData& Data=NeonCityExplosions::Profile(Key);
  TestTrue(TEXT("Four native blast profiles are authored and valid"),NeonCityExplosions::IsNativeID(Key) && Data.IsValidProfile() && Data.ExplosionID==Key);
  TestTrue(TEXT("Inner radius stays inside the outer radius"),Data.InnerRadius<Data.OuterRadius);
  TestEqual(TEXT("Lethal core keeps full damage"),Data.DamageAt(0.f),Data.Damage);
  TestEqual(TEXT("Blast edge delivers no damage"),Data.DamageAt(Data.OuterRadius),0.f);
  TestTrue(TEXT("Damage attenuates with distance"),Data.DamageAt(Data.InnerRadius+1)<Data.Damage && Data.DamageAt(Data.OuterRadius-1)>0);
  TestTrue(TEXT("Authored damage multiplier per target kind"),Data.DamageMultiplier(EImpactTargetKind::Vehicle)>0 && Data.DamageMultiplier(EImpactTargetKind::Vehicle)<=4
   && Data.DamageMultiplier(EImpactTargetKind::Character)>0 && Data.DamageMultiplier(EImpactTargetKind::Character)<=4);
  TestTrue(TEXT("Wall occlusion never keeps full damage"),Data.bRequiresLineOfSight || Data.OccludedDamageFraction<1);
  TestTrue(TEXT("Occluded damage is strictly reduced"),Data.OccludedDamageFraction<1 && Data.OccludedImpulseFraction<1);
  TestTrue(TEXT("Characters are never launched by a blast"),Data.ImpulseDeltaVelocityLimit(EImpactTargetKind::Character)==0);
  TestTrue(TEXT("Vehicles keep a bounded impulse limit"),Data.ImpulseDeltaVelocityLimit(EImpactTargetKind::Vehicle)>0 && Data.ImpulseDeltaVelocityLimit(EImpactTargetKind::Vehicle)<=3000);
  TestTrue(TEXT("Impulse is clamped by an authored maximum"),Data.Impulse.MaximumImpulse>0 && Data.Impulse.MaximumImpulse<=UNeonCityExplosionData::MaximumAuthoredImpulse);
  TestTrue(TEXT("Blast cooldown prevents refire spam"),Data.CooldownSeconds>=.2f);
  TestTrue(TEXT("Target cap bounds one detonation"),Data.MaximumTargets>=1 && Data.MaximumTargets<=64);
 }
 TestEqual(TEXT("Unknown blast id degrades to small explosion"),NeonCityExplosions::Profile(TEXT("NeonCity.PIEInjected")).ExplosionID,FName(TEXT("NeonCity.SmallExplosion")));
 TestFalse(TEXT("Injected id is not native"),NeonCityExplosions::IsNativeID(TEXT("NeonCity.PIEInjected")));
 TestEqual(TEXT("Vehicle-source native id"),NeonCityExplosions::NativeID(ENeonCityExplosionSourceType::Vehicle),FName(TEXT("NeonCity.VehicleExplosion")));
 TestEqual(TEXT("Rocket/missile source keep their own native ids"),NeonCityExplosions::NativeID(ENeonCityExplosionSourceType::Rocket),FName(TEXT("NeonCity.RocketExplosion")));
 TestEqual(TEXT("Missile source native id"),NeonCityExplosions::NativeID(ENeonCityExplosionSourceType::Missile),FName(TEXT("NeonCity.MissileExplosion")));
 TestEqual(TEXT("Legacy adapter keeps released center falloff"),UExplosionDamageComponent::Falloff(0,500),1.f);
 TestEqual(TEXT("Legacy adapter keeps released zero edge"),UExplosionDamageComponent::Falloff(500,500),0.f);
 TestTrue(TEXT("Legacy adapter attenuation stays monotonic"),UExplosionDamageComponent::Falloff(100,500)>UExplosionDamageComponent::Falloff(400,500));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplosionImpulse,"NeonCity.Explosion.Impulse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExplosionImpulse::RunTest(const FString&)
{
 FNeonCityImpulsePolicy Policy; FNeonCityImpulsePolicy Zero; Zero.CharacterDeltaVelocity=0; Zero.WorldDeltaVelocity=0;
 TestTrue(TEXT("Vehicles receive at most their authored delta velocity"),FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind::Vehicle,1,4,1,1,Policy)<=Policy.VehicleDeltaVelocity+SMALL_NUMBER);
 TestTrue(TEXT("A blast never accelerates farther than the runaway limit"),Policy.MaximumDeltaVelocity<=FExplosionImpulseUtility::RunawaySpeed);
 TestTrue(TEXT("Zero occlusion fraction strips the impulse"),FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind::Prop,1,1,0,1,Policy)==0);
 TestTrue(TEXT("Zero falloff strips the impulse"),FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind::Prop,0,1,1,1,Policy)==0);
 TestTrue(TEXT("Characters and unclassified world keep a zero limit"),FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind::Character,1,4,1,1,Zero)==0
  && FExplosionImpulseUtility::VelocityDelta(EImpactTargetKind::World,1,4,1,1,Zero)==0);
 TestTrue(TEXT("Mass scaling is clamped by the maximum impulse"),FExplosionImpulseUtility::ImpulseMagnitude(1e9f,2000,Policy)<=Policy.MaximumImpulse);
 TestTrue(TEXT("Zero mass produces no impulse"),FExplosionImpulseUtility::ImpulseMagnitude(0,500,Policy)==0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplosionSurfaces,"NeonCity.Explosion.Surfaces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExplosionSurfaces::RunTest(const FString&)
{
 const auto Kinds=NeonCitySurfaces::AllKinds();
 TestEqual(TEXT("Ten authored surface classes"),Kinds.Num(),10);
 for(ENeonCitySurfaceKind Kind:Kinds)
 {
  const UBreakableSurfaceData& Data=NeonCitySurfaces::Profile(Kind);
  TestTrue(TEXT("Surface profile valid and identified"),Data.IsValidProfile() && Data.Kind==Kind && Data.SurfaceID!=NAME_None);
  TestTrue(TEXT("Damaged threshold sits above the broken threshold"),Data.BrokenThreshold<Data.DamagedThreshold);
  TestEqual(TEXT("Full health is Normal"),Data.StateFor(1,0),EPhysicsPropState::Normal);
  TestEqual(TEXT("Health at the broken threshold is Broken"),Data.StateFor(Data.BrokenThreshold,0),EPhysicsPropState::Broken);
  TestEqual(TEXT("Zero health is Broken"),Data.StateFor(0,0),EPhysicsPropState::Broken);
  TestTrue(TEXT("Fragment budget is authored and bounded"),Data.FragmentBudget>0 && Data.FragmentBudget<=4);
  TestTrue(TEXT("Simulation distance is bounded"),Data.MaximumSimulationDistance>0 && Data.MaximumSimulationDistance<=12000);
  TestFalse(TEXT("Released city surfaces stay transient"),Data.bPersistent);
  TestTrue(TEXT("Shatter speed is above the minimum impact speed"),Data.ShatterSpeed>=Data.MinimumImpactSpeed);
  const bool bGlass=Kind==ENeonCitySurfaceKind::Glass || Kind==ENeonCitySurfaceKind::StorefrontGlass;
  TestTrue(TEXT("Glass keeps a cracked stage and hides only when broken"),Data.bGlass==bGlass && Data.bHiddenWhenCracked==false && Data.bHiddenWhenBroken==bGlass);
  TestEqual(TEXT("Glass state maps the cracked stage"),Data.GlassStateFor(EPhysicsPropState::Damaged),bGlass?ENeonCityGlassState::Cracked:ENeonCityGlassState::Normal);
  TestEqual(TEXT("Glass state maps the broken stage"),Data.GlassStateFor(EPhysicsPropState::Broken),bGlass?ENeonCityGlassState::Broken:ENeonCityGlassState::Normal);
 }
 auto* Bad=NewObject<UBreakableSurfaceData>(); Bad->SurfaceID=NAME_None; TestFalse(TEXT("Unnamed surface refused"),Bad->IsValidProfile());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplosionFeedbackData,"NeonCity.Explosion.FeedbackData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExplosionFeedbackData::RunTest(const FString&)
{
 const UCharacterDamageFeedbackData& Data=NeonCityFeedback::DefaultProfile();
 TestTrue(TEXT("Native feedback fallback is valid"),Data.IsValidProfile());
 TestTrue(TEXT("Damaged reactions last longer than healthy ones"),Data.ReactionSecondsFor(ENeonCityBodyState::Downed)>Data.ReactionSecondsFor(ENeonCityBodyState::Normal));
 TestEqual(TEXT("Full health reads Healthy"),Data.TierFor(1,ENeonCityBodyState::Normal),ENeonCityFeedbackTier::Healthy);
 TestEqual(TEXT("Half health reads Injured"),Data.TierFor(Data.InjuredFraction,ENeonCityBodyState::Normal),ENeonCityFeedbackTier::Injured);
 TestEqual(TEXT("Low health reads Critical"),Data.TierFor(Data.CriticalFraction,ENeonCityBodyState::Injured),ENeonCityFeedbackTier::Critical);
 TestEqual(TEXT("Downed state wins over the health fraction"),Data.TierFor(1,ENeonCityBodyState::Downed),ENeonCityFeedbackTier::Downed);
 TestEqual(TEXT("Dead state wins over the health fraction"),Data.TierFor(1,ENeonCityBodyState::Dead),ENeonCityFeedbackTier::Dead);
 TestTrue(TEXT("Effects are bounded and self-cleaning"),Data.MaximumConcurrentEffects<=16 && Data.MaximumConcurrentDecals<=16 && Data.EffectLifetime>0);
 TestTrue(TEXT("Damage direction outlives the reaction"),Data.DamageDirectionSeconds>=Data.HitReactionSeconds);
 auto* Bad=NewObject<UCharacterDamageFeedbackData>(); Bad->EffectLifetime=0; TestFalse(TEXT("Unbounded effect lifetime refused"),Bad->IsValidProfile());
 auto* BadTiers=NewObject<UCharacterDamageFeedbackData>(); BadTiers->InjuredFraction=.2f; BadTiers->DownedFraction=.3f; TestFalse(TEXT("Inverted severity thresholds refused"),BadTiers->IsValidProfile());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExplosionEvents,"NeonCity.Explosion.Events",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExplosionEvents::RunTest(const FString&)
{
 const FNeonCityExplosionEvent Empty;
 TestFalse(TEXT("Default blast event is not valid"),Empty.IsValidEvent());
 FNeonCityExplosionEvent Event; Event.ExplosionId=FGuid::NewGuid(); Event.Center=FVector(10,20,30); Event.Damage=40; Event.OuterRadius=300;
 TestTrue(TEXT("Authored blast event is valid"),Event.IsValidEvent());
 Event.Damage=0; TestFalse(TEXT("Damageless blast event refused"),Event.IsValidEvent());
 const FNeonCityExplosionResult Result;
 TestTrue(TEXT("Result is explicit about refusal"),!Result.bApplied && Result.DamageTargets==0 && Result.RefusedReason.IsEmpty());
 FNeonCityWeatherModifier Weather; TestTrue(TEXT("Default weather modifier is valid"),Weather.IsValid());
 Weather.FireScale=10; TestFalse(TEXT("Absurd weather scaling refused"),Weather.IsValid());
 FNeonCityWaterExplosionRequest Water; Water.SurfacePoint=FVector(10,0,0); Water.Depth=-40; Water.WaveRadius=250;
 TestTrue(TEXT("Reserved water request carries a surface point and depth"),!Water.SurfacePoint.IsZero() && Water.Depth<0);
 TestTrue(TEXT("Reserved mediums cover surface and underwater blasts"),uint8(ENeonCityExplosionMedium::Underwater)>uint8(ENeonCityExplosionMedium::WaterSurface));
 TestTrue(TEXT("World event taxonomy reserves the vehicle blast signals"),
  uint8(ENeonCityWorldEventType::VehicleExplosion)>uint8(ENeonCityWorldEventType::Explosion) && uint8(ENeonCityWorldEventType::VehicleFire)>0 && uint8(ENeonCityWorldEventType::MajorCollision)>0);
 return true;
}

#endif