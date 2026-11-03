#include "Presentation/CoastalAmbientAnimInstance.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "AI/CoastalAmbientSeatComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UCoastalAmbientAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwningActor());
 const auto* Marker=NPC?NPC->GetSpawnPoint():nullptr;
 const auto* Seat=NPC?NPC->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;
 AmbientSeatAlpha=Seat?Seat->GetSeatAlpha():0;
 if(!bPhaseAssigned && NPC)
 {
  FRandomStream Phase(NPC->GetBehaviorSeed());AmbientIdleOffset=Phase.FRandRange(0,8);
  AmbientLookOffset=Phase.FRandRange(0,3);AmbientTalkOffset=Phase.FRandRange(0,20);bPhaseAssigned=true;
 }
 bool Active=false,HardInterrupt=false;
 if(NPC && Marker && Marker->AmbientActivity!=ECoastalAmbientActivity::Legacy)
 {
  const auto* Body=NPC->FindComponentByClass<UDamageableCharacterComponent>();
  const auto* Impact=NPC->FindComponentByClass<UCharacterImpactComponent>();
  HardInterrupt=(Body && !Body->CanAct()) || (Impact && Impact->IsPhysicsFeedbackActive());
  const auto State=NPC->GetNPCState()->GetState();
  Active=!HardInterrupt && !NPC->GetThreatComponent()->HasActiveThreat() && !NPC->GetCombatAI()->ControlsLocomotion()
   && NPC->GetVelocity().Size2D()<8 && (State==ENeonCityNPCState::Idle || State==ENeonCityNPCState::Social || (Seat && Seat->IsSeatedPresentationActive()));
  AmbientActivityIndex=(Marker->AmbientActivity==ECoastalAmbientActivity::CafeConversation && AmbientSeatAlpha>.9f)?2:
   (Marker->AmbientActivity==ECoastalAmbientActivity::Viewpoint || Marker->AmbientActivity==ECoastalAmbientActivity::HarborStaff)?1:0;
 }
 AmbientAlpha=HardInterrupt?0:FMath::FInterpConstantTo(AmbientAlpha,Active?1.f:0.f,FMath::Clamp(DeltaSeconds,0.f,.1f),4.f);
 const bool CupActive=Active && Seat && Seat->HasCup() && AmbientSeatAlpha>.95f;
 AmbientCupAlpha=FMath::FInterpConstantTo(AmbientCupAlpha,CupActive?1.f:0.f,FMath::Clamp(DeltaSeconds,0.f,.1f),6.f);
 if(CupActive && GetSkelMeshComponent())
 {
  const auto* Mesh=GetSkelMeshComponent();const FTransform Frame=Mesh->GetComponentTransform();
  // Genuine finite hand IK: periodically lift the attached cup toward the
  // mouth, hold briefly and lower it; the licensed left-arm conversation lives.
  const float Phase=FMath::Fmod(Seat->GetSeatedSeconds()+AmbientTalkOffset,CupCycleSeconds);
  const float Raise=FMath::SmoothStep(5.f,7.f,Phase)*(1-FMath::SmoothStep(9.f,11.f,Phase));
  const FVector Forward=NPC->GetActorForwardVector(),Right=NPC->GetActorRightVector();
  const FVector Hip=Mesh->GetSocketLocation(TEXT("pelvis"));
  const FVector Rest=Hip+Forward*25+Right*18+FVector(0,0,18);
  const FQuat CupWorld=NPC->GetActorQuat()*FRotator(-12*Raise,0,0).Quaternion();
  const FQuat WristWorld=CupWorld*Marker->AmbientCupGripLocal.GetRotation().Inverse();
  const FVector Mouth=Mesh->GetSocketTransform(TEXT("head"),RTS_World).TransformPosition(Marker->AmbientMouthHeadLocal);
  const FVector Sip=Mouth-CupWorld.RotateVector(Marker->AmbientCupRimLocal)-WristWorld.RotateVector(Marker->AmbientCupGripLocal.GetLocation());
  AmbientCupHand=Frame.InverseTransformPosition(FMath::Lerp(Rest,Sip,Raise));
  AmbientCupElbow=Frame.InverseTransformPosition(Hip+Forward*10+Right*48+FVector(0,0,35));
  AmbientCupWrist=(Frame.GetRotation().Inverse()*WristWorld).Rotator();
 }
 UpdateGroundedCafeFeet(DeltaSeconds,Active && !HardInterrupt && AmbientSeatAlpha>.95f);
}

void UCoastalAmbientAnimInstance::UpdateGroundedCafeFeet(float DeltaSeconds,bool Active)
{
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwningActor());
 const auto* Marker=NPC?NPC->GetSpawnPoint():nullptr;
 auto* Mesh=GetSkelMeshComponent();
 // Default-off people perform no extra traces and keep their existing graph.
 if(!Marker || !Marker->bAmbientGroundFeet || !Active || !Mesh || !GetWorld()
  || Marker->AmbientActivity!=ECoastalAmbientActivity::CafeConversation)
 {
  AmbientFeetAlpha=0;bAmbientFeetGrounded=false;bFeetFloorCached=false;FeetTraceCountdown=0;return;
 }
 const FVector Soles[]={Marker->AmbientLeftShoeSoleFootLocal,Marker->AmbientRightShoeSoleFootLocal};
 const FName Feet[]={FName(TEXT("foot_l")),FName(TEXT("foot_r"))};
 const FName Knees[]={FName(TEXT("calf_l")),FName(TEXT("calf_r"))};
 const FName Thighs[]={FName(TEXT("thigh_l")),FName(TEXT("thigh_r"))};
 FTransform FeetWorld[2];FVector SoleWorld[2];
 for(int32 I=0;I<2;++I)
 {
  if(Soles[I].ContainsNaN() || Soles[I].SizeSquared()<1. || Soles[I].SizeSquared()>FMath::Square(40.)
   || Mesh->GetBoneIndex(Feet[I])==INDEX_NONE || Mesh->GetBoneIndex(Knees[I])==INDEX_NONE || Mesh->GetBoneIndex(Thighs[I])==INDEX_NONE)
  {AmbientFeetAlpha=0;bAmbientFeetGrounded=false;bFeetFloorCached=false;return;}
  FeetWorld[I]=Mesh->GetSocketTransform(Feet[I],RTS_World);
  SoleWorld[I]=FeetWorld[I].TransformPosition(Soles[I]);
  if(FeetWorld[I].ContainsNaN() || SoleWorld[I].ContainsNaN())
  {AmbientFeetAlpha=0;bAmbientFeetGrounded=false;bFeetFloorCached=false;return;}
 }
 constexpr float FloorRefreshSeconds=.1f;
 constexpr double SoleClearanceCm=.35,MaximumGroundCorrectionCm=25.;
 FeetTraceCountdown-=FMath::Clamp(DeltaSeconds,0.f,.1f);
 if(!bFeetFloorCached || FeetTraceCountdown<=0)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalCafeShoeGround),false,NPC);
  Q.AddIgnoredActor(Marker->AmbientSeatSupport.Get());
  FVector Floors[2];bool Valid=true;
  for(int32 I=0;I<2;++I)
  {
   FHitResult Hit;++AmbientFootFloorQueries;
   const bool Found=GetWorld()->LineTraceSingleByChannel(Hit,SoleWorld[I]+FVector(0,0,60),SoleWorld[I]-FVector(0,0,160),ECC_Pawn,Q);
   if(!Found || Hit.bStartPenetrating || Hit.ImpactNormal.Z<.94f || Hit.ImpactPoint.ContainsNaN()) {Valid=false;break;}
   Floors[I]=Hit.ImpactPoint;
  }
  bFeetFloorCached=Valid;FeetTraceCountdown=FloorRefreshSeconds;
  if(Valid){AmbientLeftGroundWorld=Floors[0];AmbientRightGroundWorld=Floors[1];}
 }
 if(!bFeetFloorCached){AmbientFeetAlpha=0;bAmbientFeetGrounded=false;return;}
 const FTransform Frame=Mesh->GetComponentTransform();
 FVector Targets[2],JointTargets[2];
 const FVector Floors[]={AmbientLeftGroundWorld,AmbientRightGroundWorld};
 for(int32 I=0;I<2;++I)
 {
  const double Correction=Floors[I].Z+SoleClearanceCm-SoleWorld[I].Z;
  if(!FMath::IsFinite(Correction) || FMath::Abs(Correction)>MaximumGroundCorrectionCm)
  {AmbientFeetAlpha=0;bAmbientFeetGrounded=false;bFeetFloorCached=false;return;}
  Targets[I]=Frame.InverseTransformPosition(FeetWorld[I].GetLocation()+FVector(0,0,Correction));
  // The real animated knee determines its bend plane. Extending that plane
  // supplies a stable IK pole without inventing a new knee or body pose.
  const FVector Hip=Mesh->GetSocketLocation(Thighs[I]);
  const FVector Knee=Mesh->GetSocketLocation(Knees[I]);
  const FVector Axis=FeetWorld[I].GetLocation()-Hip;
  const FVector Projection=Hip+Axis*FVector::DotProduct(Knee-Hip,Axis)/FMath::Max(Axis.SizeSquared(),1.);
  const FVector Bend=(Knee-Projection).GetSafeNormal();
  if(Hip.ContainsNaN() || Knee.ContainsNaN() || Bend.ContainsNaN() || Bend.IsNearlyZero())
  {AmbientFeetAlpha=0;bAmbientFeetGrounded=false;return;}
  JointTargets[I]=Frame.InverseTransformPosition(Knee+Bend*20.);
  if(Targets[I].ContainsNaN() || JointTargets[I].ContainsNaN())
  {AmbientFeetAlpha=0;bAmbientFeetGrounded=false;return;}
 }
 AmbientLeftFoot=Targets[0];AmbientRightFoot=Targets[1];AmbientLeftKnee=JointTargets[0];AmbientRightKnee=JointTargets[1];
 bAmbientFeetGrounded=true;
 AmbientFeetAlpha=FMath::FInterpConstantTo(AmbientFeetAlpha,1.f,FMath::Clamp(DeltaSeconds,0.f,.1f),6.f);
}
