#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Adventure/DivingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Combat/CombatComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Fishing/FishingComponent.h"
#include "Adventure/IslandLandGrill.h"
#include "Adventure/IslandEstateSport.h"
#include "Adventure/IslandEstatePrecisionSport.h"
#include "GameFramework/Controller.h"

void UCharacterPresentationAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);
 const auto* Character=Cast<ACharacter>(TryGetPawnOwner());
 const auto* World=GetWorld(); if(!Character || !World) return;
 const auto* Combat=Character->FindComponentByClass<UCombatComponent>();
 const auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>();
 const auto* Feedback=Character->FindComponentByClass<UCharacterDamageFeedbackComponent>();
 const auto* Impact=Character->FindComponentByClass<UCharacterImpactComponent>();
 AWeaponBase* Weapon=Combat?Combat->GetWeapon():nullptr;
 const auto State=Body?Body->GetBodyState():ENeonCityBodyState::Normal;
 const bool bIncapacitated=State==ENeonCityBodyState::Downed || State==ENeonCityBodyState::Dead;
 const auto* Water=Character->FindComponentByClass<UWaterInteractionComponent>();
 // Seafloor support uses MOVE_Walking, but still owns an ocean upper body.
 const bool bOceanBody=bNativeSwimming || (Water && Water->IsFullySubmerged()) || EquipmentWaterAlpha>.01f;
 const bool bArmed=Weapon && Weapon->IsEquipped() && !bIncapacitated && !bOceanBody;
 const double Now=World->GetTimeSeconds(); const float Delta=FMath::Clamp(DeltaSeconds,0.f,.1f);
 const auto* Diving=Character->FindComponentByClass<UDivingComponent>();
 const bool Changing=Diving && Diving->IsChangingGear() && !bIncapacitated;
 if(Changing)GearChangeTime=Diving->GetGearChangeTime();
 GearChangeAlpha=FMath::FInterpConstantTo(GearChangeAlpha,Changing?1.f:0.f,Delta,7.f);
 const auto* AI=Character->FindComponentByClass<UNeonCityCombatAIComponent>();
 const bool bAIEvidence=AI && AI->ControlsLocomotion() && AI->GetPerception().HasVisualContact;
 const bool bAiming=(Combat && Combat->GetCombatState()==ENeonCityCombatState::Aiming) || bAIEvidence;
 ArmedAlpha=FMath::FInterpConstantTo(ArmedAlpha,bArmed?1.f:0.f,Delta,7.f);
 AimAlpha=FMath::FInterpTo(AimAlpha,bArmed && bAiming?1.f:0.f,Delta,12.f);
 const float Pitch=FRotator::NormalizeAxis(bAIEvidence
  ?(AI->GetPerception().LastKnownAimPoint-Character->GetPawnViewLocation()).Rotation().Pitch
  :Character->GetBaseAimRotation().Pitch);
 AimUpAlpha=FMath::Clamp(Pitch/60.f,0.f,1.f)*AimAlpha;
 AimDownAlpha=FMath::Clamp(-Pitch/60.f,0.f,1.f)*AimAlpha;
 if(LastWeapon.Get()!=Weapon)
 {
  LastWeapon=Weapon; LastShotCount=Weapon?Weapon->GetShotCount():0;
  ShotAt=-100; bWasReloading=false;
  WeaponVisual.Reset();
  if(Weapon)
  {
   TInlineComponentArray<UStaticMeshComponent*> Parts(Weapon);
   for(auto* Part:Parts) if(Part->DoesSocketExist(TEXT("SupportHand"))) { WeaponVisual=Part;break; }
  }
 }
 if(Weapon && Weapon->GetShotCount()!=LastShotCount)
 { LastShotCount=Weapon->GetShotCount(); ShotAt=Now; }
 ShotTime=FMath::Clamp(float((Now-ShotAt)/.24)*.633333f,0.f,.633333f);
 ShotAlpha=bArmed?FMath::Clamp(1.f-float(Now-ShotAt)/.24f,0.f,1.f):0;
 const bool bReloading=bArmed && Weapon->IsReloading();
 if(bReloading && !bWasReloading) ReloadAt=Now;
 bWasReloading=bReloading;
 ReloadAlpha=FMath::FInterpConstantTo(ReloadAlpha,bReloading?1.f:0.f,Delta,12.f);
 ReloadTime=bReloading?FMath::Clamp(float(Now-ReloadAt)/FMath::Max(.1f,Weapon->GetData()->ReloadSeconds),0.f,1.f)*1.666667f:ReloadTime;
 const bool bPhysical=Impact && Impact->IsPhysicsFeedbackActive();
 const float HitAge=Feedback?float(Now-Feedback->GetLastFeedback().Timestamp):100;
 HitTime=FMath::Clamp(HitAge,0.f,.333333f);
 HitAlpha=Feedback && Feedback->GetHitReactionCount()>0 && !bIncapacitated && !bPhysical && !bOceanBody
  ? FMath::Clamp(1.f-HitAge/.4f,0.f,1.f):0;
 InjuryAlpha=FMath::FInterpTo(InjuryAlpha,State==ENeonCityBodyState::Injured && !bArmed && !bOceanBody?.22f:0.f,Delta,4.f);
 if(bIncapacitated && !bWasIncapacitated) CollapseAt=Now;
 bWasIncapacitated=bIncapacitated;
 // Keep the final collapse frame while blending out, avoiding a one-frame snap
 // back to the clip's standing first frame when health is restored.
 if(bIncapacitated) CollapseTime=FMath::Clamp(float(Now-CollapseAt),0.f,2.4f);
 IncapacitatedAlpha=FMath::FInterpConstantTo(IncapacitatedAlpha,bIncapacitated?1.f:0.f,Delta,bIncapacitated?6.f:1.8f);
 const float Speed=Character->GetVelocity().Size2D();
 WalkAlpha=FMath::FInterpTo(WalkAlpha,Speed>8.f?1.f:0.f,Delta,8.f);
 WalkRate=FMath::Clamp(Speed/150.f,.4f,2.f);
 // Only character-mounted artwork with an authored support socket opts in.
 // Express the target in the current right-hand bone's space. The graph then
 // follows that frame without reading last frame's hand world transform.
 bool bSupport=false;
 if(bArmed && WeaponVisual.IsValid())
 {
  const auto* Mount=Weapon->GetRootComponent()->GetAttachParent();
  if(Mount && Mount->GetAttachParent()==Character->GetMesh() && Mount->GetAttachSocketName()==TEXT("hand_r"))
  {
   const FTransform ToHand=WeaponVisual->GetRelativeTransform()*Weapon->GetRootComponent()->GetRelativeTransform()*Mount->GetRelativeTransform();
   SupportHandRightSpace=ToHand.TransformPosition(WeaponVisual->GetSocketTransform(TEXT("SupportHand"),RTS_Component).GetLocation());
   SupportElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8,-65,0));
   bSupport=true;
  }
 }
 const float SupportWeight=bSupport?ArmedAlpha*(1-ReloadAlpha)*(1-IncapacitatedAlpha)*(1-HitAlpha):0;
 SupportHandAlpha=FMath::FInterpConstantTo(SupportHandAlpha,SupportWeight,Delta,10.f);
 const auto* Fishing=Character->GetController()?Character->GetController()->FindComponentByClass<UFishingComponent>():nullptr;
 FTransform Right,Left;
 const bool bFishing=Fishing && !bIncapacitated && !bPhysical && !bArmed && !bOceanBody && Fishing->GetHandTargets(Right,Left);
 const auto* Precision=Character->FindComponentByClass<UIslandPrecisionParticipant>();
 const bool bPrecision=Precision && !bFishing && !bIncapacitated && !bPhysical && !bArmed && !bOceanBody && Precision->GetHandTargets(Right,Left);
 const float PrecisionLean=bPrecision?Precision->GetBodyLeanDegrees():0.f;
 // A counter-height task needs a small articulated torso lean. This precedes
 // the existing finite-length hand IK; it never scales limbs or moves feet.
 const auto* Grill=bFishing?Cast<AIslandLandGrill>(Fishing->GetActiveStation()):nullptr;
 const bool bCooking=Grill!=nullptr;
 const float CookLean=Grill?Grill->GetCookingBodyLeanDegrees():25.f;
 CookingLeanDegrees=FMath::FInterpTo(CookingLeanDegrees,bCooking?CookLean:0.f,Delta,8.f);
 CookingBodyAlpha=FMath::FInterpConstantTo(CookingBodyAlpha,(bCooking || PrecisionLean>0)?1.f:0.f,Delta,4.f);
 if(bCooking || PrecisionLean>0)
 {
  const FVector Forward=Character->GetActorForwardVector();const float Angle=FMath::DegreesToRadians(bCooking?CookingLeanDegrees:PrecisionLean);
  const FQuat Lean=FQuat::FindBetweenNormals(Forward,Forward*FMath::Cos(Angle)-FVector::UpVector*FMath::Sin(Angle));
  const FQuat MeshRotation=Character->GetMesh()->GetComponentQuat();
  CookingSpineRotation=(MeshRotation.Inverse()*Lean*MeshRotation).Rotator();
 }
 if(bFishing)
 {
  const auto& Session=Fishing->GetSession();
  FishingPose=Fishing->GetCastPoseIndex();
  FishingPoseTime=Fishing->GetCastPoseTime();
  const auto MeshWorld=Character->GetMesh()->GetComponentTransform();
  FishingRightHand=MeshWorld.InverseTransformPosition(Right.GetLocation());
  FishingLeftHand=MeshWorld.InverseTransformPosition(Left.GetLocation());
  FishingRightWrist=(MeshWorld.GetRotation().Inverse()*Right.GetRotation()).Rotator();
  FishingLeftWrist=(MeshWorld.GetRotation().Inverse()*Left.GetRotation()).Rotator();
  const float Load=FMath::Max(Fishing->GetCastBodyMotion(),0.f);
  FishingRightElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8-Load*5,33+Load*4,29+Load*15));
  FishingLeftElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8-Load*5,-33-Load*4,29+Load*15));
 }
 const auto* Tennis=Character->FindComponentByClass<UIslandTennisParticipant>();
 const bool bTennis=Tennis && !bIncapacitated && !bPhysical && !bArmed && !bOceanBody && Tennis->GetHandTargets(Right,Left);
 if(bTennis)
 {
  const auto Frame=Character->GetMesh()->GetComponentTransform();
  FishingRightHand=Frame.InverseTransformPosition(Right.GetLocation());FishingLeftHand=Frame.InverseTransformPosition(Left.GetLocation());
  FishingRightWrist=(Frame.GetRotation().Inverse()*Right.GetRotation()).Rotator();FishingLeftWrist=(Frame.GetRotation().Inverse()*Left.GetRotation()).Rotator();
  FishingRightElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8,43,35));
  FishingLeftElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8,-43,35));
 }
 if(bPrecision)
 {
  const auto Frame=Character->GetMesh()->GetComponentTransform();
  FishingRightHand=Frame.InverseTransformPosition(Right.GetLocation());FishingLeftHand=Frame.InverseTransformPosition(Left.GetLocation());
  FishingRightWrist=(Frame.GetRotation().Inverse()*Right.GetRotation()).Rotator();FishingLeftWrist=(Frame.GetRotation().Inverse()*Left.GetRotation()).Rotator();
  FishingRightElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8,43,25));
  FishingLeftElbow=Character->GetMesh()->GetRelativeTransform().InverseTransformPosition(FVector(8,-43,25));
 }
 FishingBodyAlpha=FMath::FInterpConstantTo(FishingBodyAlpha,bFishing?1.f:0.f,Delta,8.f);
 FishingAlpha=FMath::FInterpConstantTo(FishingAlpha,(bFishing || bTennis || bPrecision)?1.f:0.f,Delta,8.f);
 const auto* Light=Character->FindComponentByClass<UNeonCityFlashlightComponent>();
 const bool bHeldLight=Light && Light->IsEquipped() && !Light->IsDiveMounted() && !bArmed && !bFishing && !bTennis && !bPrecision && !bIncapacitated && !bPhysical && !bOceanBody;
 FlashlightAlpha=FMath::FInterpConstantTo(FlashlightAlpha,bHeldLight?1.f-HitAlpha:0.f,Delta,8.f);
}
