#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingStation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequence.h"
#include "Adventure/IslandLandGrill.h"

namespace
{
float FishMeshLength(const UStaticMesh* Mesh)
{
 // Swimming meshes extend their visibility bounds for tail motion. That margin
 // is not anatomy: using it made an 18 cm sardine appear only a few cm long.
 const auto* Data=Mesh->GetRenderData();
 return FMath::Max(1.f,float((Data?Data->Bounds:Mesh->GetBounds()).BoxExtent.X*2));
}
}

void UFishingComponent::CreateVisuals()
{
 auto* P=ActivePlayer.Get();if(!P)return;
 const auto Part=[&](const TCHAR* Path)
 {
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,Path);if(!Mesh)return static_cast<UStaticMeshComponent*>(nullptr);
  auto* Component=NewObject<UStaticMeshComponent>(P);Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Mesh);
  Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetCanEverAffectNavigation(false);
  Component->SetupAttachment(P->GetRootComponent());Component->RegisterComponent();return Component;
 };
 CatchVisual=NewObject<UStaticMeshComponent>(P);CatchVisual->SetMobility(EComponentMobility::Movable);CatchVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CatchVisual->SetCanEverAffectNavigation(false);CatchVisual->SetupAttachment(P->GetRootComponent());CatchVisual->RegisterComponent();
 if(ActiveStation->Kind!=ENeonCityFishingStation::CastingPoint) { CatchVisual->SetVisibility(false);return; }
 Rod=Part(TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/SM_OceanRodBody.SM_OceanRodBody"));
 WindupMotion=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/Motion/A_FishingR2_Windup"));
 CastMotion=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/Motion/A_FishingR2_Cast"));
 if(WindupMotion)Session.WindupDuration=WindupMotion->GetPlayLength();
 Float=Part(TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/SM_OceanBobber.SM_OceanBobber"));
 RodPitch=24;RodYaw=0;RodBend=RodBendVelocity=ReelCrank=ReelRotor=ReelBail=ReelRate=ReelSpool=LastLineOut=0;
 RodLoad=.15f;ReelMeasuredRate=VisualStateAge=0;RodBendDirection=FVector(0,0,-1);bCastLaunched=false;
 if(Rod)
 {
  RodMaterial=UMaterialInstanceDynamic::Create(Rod->GetMaterial(0),this);
  for(int32 I=0;I<Rod->GetNumMaterials();++I)Rod->SetMaterial(I,RodMaterial);
  for(const TCHAR* Name:{TEXT("Rotor"),TEXT("Bail"),TEXT("Crank"),TEXT("Spool"),TEXT("Braid")})
  {
   const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/SM_OceanRod%s.SM_OceanRod%s"),Name,Name);
   auto* Assembly=Part(*Path);ReelParts.Add(Assembly);
   if(Assembly)Assembly->AttachToComponent(Rod,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
  }
 }
 SplashSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Fishing/Audio/SW_NC_FishingSplash.SW_NC_FishingSplash"));
 SplashAttenuation=NewObject<USoundAttenuation>(this);SplashAttenuation->Attenuation.bAttenuate=true;SplashAttenuation->Attenuation.bSpatialize=true;
 SplashAttenuation->Attenuation.AttenuationShapeExtents=FVector(150);SplashAttenuation->Attenuation.FalloffDistance=1200;
 auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Fishing/Audio/SW_NC_FishingReel.SW_NC_FishingReel"));
 if(Sound && Rod)
 {
  ReelAudio=NewObject<UAudioComponent>(P);ReelAudio->SetupAttachment(Rod);ReelAudio->bAutoActivate=false;ReelAudio->SetSound(Sound);
  ReelAudio->bOverrideAttenuation=true;ReelAudio->AttenuationOverrides.bAttenuate=true;ReelAudio->AttenuationOverrides.bSpatialize=true;
  ReelAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(80);ReelAudio->AttenuationOverrides.FalloffDistance=600;ReelAudio->RegisterComponent();
 }
 LastPresentedState=ENeonCityFishingState::Ready;
 auto* LineMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Fishing/Art/SM_NC_FishingLine.SM_NC_FishingLine"));
 auto* LineMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Fishing/Art/M_NC_FishingLine.M_NC_FishingLine"));
 if(LineMesh)for(int32 Index=0;Index<12;++Index)
 {
  auto* Segment=NewObject<USplineMeshComponent>(P);Segment->SetMobility(EComponentMobility::Movable);Segment->SetStaticMesh(LineMesh);Segment->SetForwardAxis(ESplineMeshAxis::X,false);
  if(LineMaterial)Segment->SetMaterial(0,LineMaterial);
  Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);Segment->SetCanEverAffectNavigation(false);
  Segment->SetupAttachment(P->GetRootComponent());Segment->RegisterComponent();Line.Add(Segment);
 }
 UpdateVisuals(0);
}
void UFishingComponent::DestroyVisuals()
{
 if(ReelAudio) { ReelAudio->Stop();ReelAudio->DestroyComponent(); }ReelAudio=nullptr;SplashSound=nullptr;SplashAttenuation=nullptr;
 for(auto* Part:{Rod.Get(),Float.Get(),CatchVisual.Get()})if(Part)Part->DestroyComponent();
 for(UStaticMeshComponent* Part:ReelParts)if(Part)Part->DestroyComponent();ReelParts.Reset();RodMaterial=nullptr;
 for(USplineMeshComponent* Segment:Line)if(Segment)Segment->DestroyComponent();
 Rod=nullptr;Float=nullptr;CatchVisual=nullptr;Line.Reset();
}
void UFishingComponent::UpdateVisuals(float Delta)
{
 if(!ActivePlayer.IsValid() || !ActiveStation.IsValid())return;
 if(ActiveStation->Kind==ENeonCityFishingStation::Grill && CatchVisual)
 {
  if(Cast<AIslandLandGrill>(ActiveStation.Get())){CatchVisual->SetVisibility(false);return;}
  auto* Inventory=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  const auto* Fish=Inventory->FindFish(CookingFish);const auto* Profile=Fish?Inventory->FindSpecies(Fish->SpeciesID):nullptr;
  CatchVisual->SetVisibility(Profile!=nullptr);
  if(Profile)
  {
   auto* Mesh=Profile->Mesh.LoadSynchronous();if(CatchVisual->GetStaticMesh()!=Mesh)CatchVisual->SetStaticMesh(Mesh);
   CatchVisual->SetWorldLocationAndRotation(ActiveStation->GetActorTransform().TransformPosition(FVector(0,0,89)),ActiveStation->GetActorRotation()+FRotator(0,90,90));
   if(Mesh)CatchVisual->SetWorldScale3D(FVector(Fish->SizeCm/FishMeshLength(Mesh)));
  }
  return;
 }
 if(!Rod)return;
 // Rod/reel motion adapted from Tidewater FishingRod.js (MIT), pinned 4811ba48.
 // All source metres are converted to UE centimetres; physics remains in the session.
 const float Dt=FMath::Clamp(Delta,0.f,.1f);
 const bool Windup=Session.State==ENeonCityFishingState::Windup;
 const bool Fight=Session.State==ENeonCityFishingState::Fighting;
 const bool Cast=Session.State==ENeonCityFishingState::Casting;
 const bool Caught=Session.State==ENeonCityFishingState::Caught;
 const bool InWater=Session.State==ENeonCityFishingState::Waiting || Session.State==ENeonCityFishingState::Bite || Fight;
 VisualStateAge=Session.State!=LastPresentedState?Session.Elapsed:VisualStateAge+Dt;
 const float LineDelta=Fight && LastPresentedState==Session.State?Session.RemainingLineM-LastLineOut:0;
 LastLineOut=Session.RemainingLineM;
 if(FMath::Abs(LineDelta)>.00001f)ReelMeasuredRate=FMath::Clamp(-LineDelta/.05f/.8f,0.f,1.6f);
 const float Rate=Fight && Session.Reeling?ReelMeasuredRate:(Caught && VisualStateAge<.5f?1.2f:0);
 // The rules advance at 20 Hz. Smooth the physical length delta for continuous cranking.
 ReelRate=FMath::FInterpTo(ReelRate,Rate,Dt,8);
 const float CrankStep=ReelRate*2*PI*Dt;
 ReelCrank=FMath::Fmod(ReelCrank+CrankStep,4*PI);
 ReelRotor=FMath::Fmod(ReelRotor+FMath::Min(CrankStep*5.2f,3.1f*2*PI*Dt),2*PI);
 if(Fight && LineDelta>0)ReelSpool=FMath::Fmod(ReelSpool-FMath::Min(LineDelta,.5f)/.023f,2*PI);
 ReelBail=FMath::FInterpConstantTo(ReelBail,Windup || Cast?1.f:0.f,Dt,Windup || Cast?6.f:16.f);
 const auto Around=[](FVector Centre,FQuat Rotation){return FTransform(Rotation,Centre-Rotation.RotateVector(Centre));};
 const FVector ReelCentre(0,0,-9.2),CrankCentre(32.7,0,-9.2),BailCentre(40.3,0,-9.2);
 const FTransform Rotor=Around(ReelCentre,FQuat(FVector::ForwardVector,-ReelRotor));
 const FTransform Crank=Around(CrankCentre,FQuat(FVector::RightVector,-ReelCrank));
 const FTransform Bail=Around(BailCentre,FQuat(FVector::RightVector,ReelBail*1.95f))*Rotor;
 FTransform Spool=Around(ReelCentre,FQuat(FVector::ForwardVector,-ReelSpool));Spool.AddToTranslation(FVector(FMath::Sin(ReelCrank*.5f)*.35f,0,0));
 const FTransform Assemblies[]={Rotor,Bail,Crank,Spool,Spool};
 for(int32 I=0;I<ReelParts.Num();++I)if(ReelParts[I])ReelParts[I]->SetRelativeTransform(Assemblies[I]);
 const bool Winding=ReelRate>.04f;
 if(ReelAudio)
 {
  if(Winding && !ReelAudio->IsPlaying())ReelAudio->Play();
  else if(!Winding && ReelAudio->IsPlaying())ReelAudio->Stop();
 }
 if(SplashSound && LastPresentedState!=Session.State && (Session.State==ENeonCityFishingState::Waiting || Session.State==ENeonCityFishingState::Bite))
  UGameplayStatics::PlaySoundAtLocation(this,SplashSound,WaterTarget,.6f,Session.State==ENeonCityFishingState::Bite?1.3f:1.f,0,SplashAttenuation);
 LastPresentedState=Session.State;
 const float PriorPitch=RodPitch;
 const float Elev=Windup || Cast?GetCastMotionCurve(TEXT("RodPitch")):Caught?49.f:Fight?52.f-Session.Surge*20+FMath::Sin(VisualClock*2.3f)*7:24.f;
 RodPitch=Windup || Cast?Elev:FMath::FInterpTo(RodPitch,Elev,Dt,5.f);
 RodYaw=Windup || Cast?GetCastMotionCurve(TEXT("RodYaw")):FMath::FInterpTo(RodYaw,Fight?FMath::Sin(VisualClock*1.1f+Session.Surge*2)*7.f:0.f,Dt,5.f);
 const FRotator Orientation(RodPitch,RodYaw,Windup || Cast?GetCastMotionCurve(TEXT("RodRoll")):0.f);
 const float Body=GetCastBodyMotion();
 const FVector Hand=Windup || Cast?FVector(GetCastMotionCurve(TEXT("SeatX")),GetCastMotionCurve(TEXT("SeatY")),GetCastMotionCurve(TEXT("SeatZ"))):FVector(37,18,37);
 // The seat stays in the palm; the butt moves under the forearm as the rod lifts.
 Rod->SetRelativeLocationAndRotation(Hand-Orientation.RotateVector(FVector(40.5,0,0)),Orientation);
 float BendTarget=InWater?.012f:0.f,LoadTarget=.15f;
 if(Windup || Cast){BendTarget=Dt>0?FMath::Clamp((RodPitch-PriorPitch)/Dt*.00048f,-.23f,.13f):0;LoadTarget=Cast?.55f:.25f;}
 if(Session.State==ENeonCityFishingState::Bite)BendTarget+=.07f*(.5f+.5f*FMath::Sin(VisualClock*20));
 if(Fight){BendTarget=.06f+.3f*FMath::Min(Session.Tension,1.1f)+.05f*Session.Surge;LoadTarget=FMath::Min(1.f,Session.Tension*1.1f);}
 if(Caught){BendTarget=.14f;LoadTarget=.5f;}
 // Fixed maximum integration step keeps the lightly damped blank stable through a hitch.
 for(float Remaining=Dt;Remaining>UE_SMALL_NUMBER;)
 {const float Step=FMath::Min(Remaining,1.f/120);RodBendVelocity+=((BendTarget-RodBend)*250-RodBendVelocity*8)*Step;RodBend+=RodBendVelocity*Step;Remaining-=Step;}
 RodLoad=FMath::FInterpTo(RodLoad,LoadTarget,Dt,6);
 FVector Direction(0,0,-1);
 if(InWater){Direction=Rod->GetComponentTransform().InverseTransformPosition(Fight?HookPosition:WaterTarget);Direction.X=0;Direction=Direction.GetSafeNormal();}
 RodBendDirection=FMath::Lerp(RodBendDirection,Direction,1-FMath::Exp(-Dt*10)).GetSafeNormal();
 const float Exponent=FMath::Lerp(3.4f,1.7f,RodLoad);
 if(RodMaterial)
 {
  RodMaterial->SetScalarParameterValue(TEXT("Bend"),RodBend);RodMaterial->SetScalarParameterValue(TEXT("BendExponent"),Exponent);
  RodMaterial->SetVectorParameterValue(TEXT("RodAxis"),FLinearColor(Rod->GetForwardVector()));
  RodMaterial->SetVectorParameterValue(TEXT("BendDirection"),FLinearColor(Rod->GetComponentTransform().TransformVectorNoScale(RodBendDirection)));
 }
 const auto RodPoint=[&](FVector Rest)
 {
  const float S=FMath::Clamp((float(Rest.X)-53.5f)/159.5f,0.f,1.f);
  const float Lat=RodBend*213.f*FMath::Pow(S,Exponent),Drop=.5f*Lat*Lat/(FMath::Max(float(Rest.X)-53.5f,0.f)+6.f);
  return Rod->GetComponentTransform().TransformPosition(Rest-FVector(Drop,0,0)+RodBendDirection*Lat);
 };
 const FVector Tip=RodPoint(FVector(213,0,0));
 FVector End=WaterTarget;
 if(!Cast && !InWater && !Caught)End=Tip-FVector(0,0,35);
 if(Cast)
 {
  if(!bCastLaunched && GetCastPoseIndex()==2 && GetCastMotionCurve(TEXT("LineReleased"))>=.99f)
  {
   bCastLaunched=true;CastOrigin=Tip-FVector(0,0,35);const float Flight=Session.CastFlightSeconds;
   CastVelocity=(WaterTarget-CastOrigin)/Flight+FVector(0,0,490*Flight);
  }
  const float Air=FMath::Clamp(Session.Elapsed+RuleAccumulator-Session.LaunchDelay,0.f,Session.CastFlightSeconds);
  End=bCastLaunched?CastOrigin+CastVelocity*Air-FVector(0,0,490*Air*Air):Tip-FVector(0,0,35);
 }
 if(Session.State==ENeonCityFishingState::Fighting)End=HookPosition;
 if(InWater)End.Z+=FMath::Sin(VisualClock*(Session.State==ENeonCityFishingState::Bite?24:3))*(Session.State==ENeonCityFishingState::Bite?6:1.5f);
 if(Caught)End=ActivePlayer->GetActorTransform().TransformPosition(FVector(105,25,78));
 const bool Show=!Session.IsTerminal() || Caught;
 if(Float) { Float->SetVisibility(Show && !Caught);Float->SetWorldLocationAndRotation(End,Cast && bCastLaunched?(CastVelocity-FVector(0,0,980*FMath::Max(0.f,Session.Elapsed-Session.LaunchDelay))).Rotation():FRotator(90,0,0)); }
 const FVector Guides[]={FVector(43,0,-9.2),FVector(86,0,-6.8),FVector(110,0,-4.6),FVector(131,0,-3.1),FVector(149,0,-2.3),FVector(165,0,-1.95),FVector(180,0,-1.65),FVector(193,0,-1.45),FVector(203.5,0,-1.25),FVector(213,0,0)};
 for(int32 Index=0;Index<Line.Num();++Index)
 {
  auto* Segment=Line[Index].Get();Segment->SetVisibility(Show);
  if(Index>=3)
  {
   const FVector A=RodPoint(Guides[Index-3]),B=RodPoint(Guides[Index-2]);const FTransform T=Segment->GetComponentTransform();
   Segment->SetStartAndEnd(T.InverseTransformPosition(A),T.InverseTransformVector(B-A),T.InverseTransformPosition(B),T.InverseTransformVector(B-A));continue;
  }
  const float Sag=Caught?0.f:12.f*(1-Session.Tension);
  const auto Curve=[&](float T){return FMath::Lerp(Tip,End,T)-FVector(0,0,FMath::Sin(T*PI)*Sag);};
  const FTransform Transform=Segment->GetComponentTransform();
  const float A=Index/3.f,B=(Index+1)/3.f;
  const FVector Tangent=(End-Tip)/3.f;
  Segment->SetStartAndEnd(Transform.InverseTransformPosition(Curve(A)),Transform.InverseTransformVector(Tangent),Transform.InverseTransformPosition(Curve(B)),Transform.InverseTransformVector(Tangent));
 }
 if(CatchVisual)
 {
  CatchVisual->SetVisibility(Caught);
  if(Caught)if(const auto* Profile=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->FindSpecies(PendingCatch.SpeciesID))
  {
   auto* Mesh=Profile->Mesh.LoadSynchronous();if(CatchVisual->GetStaticMesh()!=Mesh)CatchVisual->SetStaticMesh(Mesh);
   if(Mesh)CatchVisual->SetWorldScale3D(FVector(PendingCatch.SizeCm/FishMeshLength(Mesh)));
   CatchVisual->SetWorldLocationAndRotation(End,FRotator(-70,ActivePlayer->GetActorRotation().Yaw+FMath::Sin(VisualClock*8)*12,0));
  }
 }
}
bool UFishingComponent::GetHandTargets(FTransform& Right,FTransform& Left) const
{
 if(const auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get()))return Grill->GetCookingHandTargets(Right,Left);
 if(!IsActive() || !Rod || !Rod->DoesSocketExist(TEXT("RightPalm")) || !Rod->DoesSocketExist(TEXT("LeftPalm")))return false;
 Right=Rod->GetSocketTransform(TEXT("RightPalm"));Left=Rod->GetSocketTransform(TEXT("LeftPalm"));
 // Grip orientations are sampled from the authored ready clip at import. Follow
 // the actual rod pitch/yaw instead of inheriting the forearm's rotating IK frame.
 // The crank knob spins freely, so the palm orbits it without rotating with it.
 const FQuat GripFrame=Rod->GetComponentQuat()*FRotator(24,0,0).Quaternion().Inverse()
  *ActivePlayer->GetMesh()->GetRelativeRotation().Quaternion();
 Right.SetRotation(GripFrame*Rod->GetSocketTransform(TEXT("RightPalm"),RTS_Component).GetRotation());
 Left.SetRotation(GripFrame*Rod->GetSocketTransform(TEXT("LeftPalm"),RTS_Component).GetRotation());
 const FQuat Turn(FVector::RightVector,-ReelCrank);
 FVector LeftLocal=FVector(32.7,0,-9.2)+Turn.RotateVector(FVector(-5.2,-5.8,0))+FVector(0,-4,10);
 // During the wind-up both hands brace the grip; the left returns to the actual crank knob.
 const float Brace=GetCastMotionCurve(TEXT("Brace"));
 LeftLocal=FMath::Lerp(LeftLocal,FVector(9,-4,7),Brace);
 Left.SetLocation(Rod->GetComponentTransform().TransformPosition(LeftLocal));
 return true;
}
float UFishingComponent::GetCastBodyMotion() const
{
 return GetCastMotionCurve(TEXT("Body"));
}
int32 UFishingComponent::GetCastPoseIndex() const
{
 if(Session.State==ENeonCityFishingState::Windup)return 1;
 if(Session.State==ENeonCityFishingState::Casting)return Session.Elapsed+RuleAccumulator<Session.BackSwingRemaining?1:2;
 return Session.State==ENeonCityFishingState::Fighting || Session.State==ENeonCityFishingState::Caught?3:0;
}
float UFishingComponent::GetCastPoseTime() const
{
 const float Time=Session.Elapsed+RuleAccumulator;
 if(Session.State==ENeonCityFishingState::Windup)return FMath::Min(Time,Session.WindupDuration);
 if(Session.State==ENeonCityFishingState::Casting)
  return GetCastPoseIndex()==1?Session.CastPower*Session.WindupDuration+Time:FMath::Clamp(Time-Session.BackSwingRemaining,0.f,CastMotion?float(CastMotion->GetPlayLength()):1.3f);
 return Time;
}
float UFishingComponent::GetCastMotionCurve(FName Name) const
{
 const int32 Pose=GetCastPoseIndex();const UAnimSequence* Clip=Pose==1?WindupMotion.Get():Pose==2?CastMotion.Get():nullptr;
 return Clip?Clip->EvaluateCurveData(Name,FAnimExtractContext(FMath::Min(GetCastPoseTime(),float(Clip->GetPlayLength())))):0;
}
bool UFishingComponent::IsReelAudioPlaying() const { return ReelAudio && ReelAudio->IsPlaying(); }
