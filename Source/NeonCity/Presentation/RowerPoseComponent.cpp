#include "Presentation/RowerPoseComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName Spine[]={TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05")};
const float SpineShare[]={.26f,.24f,.2f,.16f,.14f};
const TCHAR* Fingers[]={TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")};
const TCHAR* SideSuffix[]={TEXT("_l"),TEXT("_r")};
FName Bone(const TCHAR* Base,int32 Side) { return FName(*FString::Printf(TEXT("%s%s"),Base,SideSuffix[Side])); }
FName Finger(const TCHAR* Base,int32 Joint,int32 Side) { return FName(*FString::Printf(TEXT("%s_%02d%s"),Base,Joint,SideSuffix[Side])); }
float Smooth(float X) { X=FMath::Clamp(X,0.f,1.f); return X*X*(3-2*X); }
}

URowerPoseComponent::URowerPoseComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 SetCollisionEnabled(ECollisionEnabled::NoCollision);SetCanEverAffectNavigation(false);SetGenerateOverlapEvents(false);
 CanCharacterStepUpOn=ECB_No;CastShadow=true;bCastDynamicShadow=true;
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Sitting(TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Sitting_Idle_Loop.AN_NC205C_Sitting_Idle_Loop"));
 SeatedAnimation=Sitting.Object;
}

bool URowerPoseComponent::SetBody(USkeletalMesh* Mesh,const TArray<UMaterialInterface*>& Materials)
{
 if(!Mesh || !SeatedAnimation || !SeatedAnimation->GetSkeleton() || !SeatedAnimation->GetSkeleton()->IsCompatibleMesh(Mesh))return false;
 if(!Source)
 {
  Source=NewObject<USkeletalMeshComponent>(GetOwner(),MakeUniqueObjectName(GetOwner(),USkeletalMeshComponent::StaticClass(),TEXT("RowerPoseSource")));
  Source->SetupAttachment(this);Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);Source->SetCanEverAffectNavigation(false);
  Source->SetVisibility(false);Source->SetHiddenInGame(true);Source->SetCastShadow(false);
  // The hidden source only drives the base pose; it must keep animating while invisible.
  Source->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Source->RegisterComponent();
 }
 Source->SetSkeletalMeshAsset(Mesh);Source->PlayAnimation(SeatedAnimation,true);Source->SetComponentTickEnabled(true);
 Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
 SetSkinnedAssetAndUpdate(Mesh,true);EmptyOverrideMaterials();
 for(int32 I=0;I<Materials.Num();++I)SetMaterial(I,Materials[I]);
 SetHiddenInGame(false);SetVisibility(true);bHasBody=true;BlendTime=BlendDuration=1;bStanding=false;
 return true;
}

void URowerPoseComponent::ClearBody()
{
 bHasBody=false;SetHiddenInGame(true);SetVisibility(false);
 if(Source)Source->SetComponentTickEnabled(false);
}

void URowerPoseComponent::BeginSitFrom(USkeletalMeshComponent* Standing,float Duration)
{
 Snapshot.Reset();
 if(!Standing || Standing->GetSkeletalMeshAsset()!=GetSkinnedAsset())return;
 Snapshot=Standing->GetBoneSpaceTransforms();SnapshotWorld=Standing->GetComponentTransform();
 BlendTime=0;BlendDuration=FMath::Max(.05f,Duration);bStanding=false;
}

void URowerPoseComponent::BeginStandTo(const FTransform& StandingMeshWorld,float Duration)
{
 if(!Snapshot.Num())return;
 SnapshotWorld=StandingMeshWorld;BlendTime=0;BlendDuration=FMath::Max(.05f,Duration);bStanding=true;
}

FVector URowerPoseComponent::GetBoneWorld(FName Name)
{
 return GetBoneTransformByName(Name,EBoneSpaces::WorldSpace).GetLocation();
}

void URowerPoseComponent::Lean(const FVector& AxisCS,float Degrees)
{
 for(int32 I=0;I<UE_ARRAY_COUNT(Spine);++I)
 {
  if(GetBoneIndex(Spine[I])==INDEX_NONE)continue;
  FTransform T=GetBoneTransformByName(Spine[I],EBoneSpaces::ComponentSpace);
  T.SetRotation(FQuat(AxisCS,FMath::DegreesToRadians(Degrees*SpineShare[I]))*T.GetRotation());
  SetBoneTransformByName(Spine[I],T,EBoneSpaces::ComponentSpace);
 }
}

void URowerPoseComponent::SolveLimb(FName RootName,FName JointName,FName EndName,const FVector& Effector,const FVector& Pole,const FVector& EndAxis)
{
 if(GetBoneIndex(RootName)==INDEX_NONE || GetBoneIndex(JointName)==INDEX_NONE || GetBoneIndex(EndName)==INDEX_NONE)return;
 FTransform Root=GetBoneTransformByName(RootName,EBoneSpaces::ComponentSpace);
 FTransform Joint=GetBoneTransformByName(JointName,EBoneSpaces::ComponentSpace);
 FTransform End=GetBoneTransformByName(EndName,EBoneSpaces::ComponentSpace);
 const FQuat JointBefore=Joint.GetRotation(),EndBefore=End.GetRotation();
 AnimationCore::SolveTwoBoneIK(Root,Joint,End,Pole,Effector,false,1.,1.);
 // The solver aims root and joint; carry the joint's swing on to the end bone so the wrist / ankle
 // stays in line with the forearm / shin instead of keeping its old orientation.
 End.SetRotation((Joint.GetRotation()*JointBefore.Inverse())*EndBefore);
 SetBoneTransformByName(RootName,Root,EBoneSpaces::ComponentSpace);
 SetBoneTransformByName(JointName,Joint,EBoneSpaces::ComponentSpace);
 SetBoneTransformByName(EndName,End,EBoneSpaces::ComponentSpace);
 (void)EndAxis;
}

void URowerPoseComponent::ApplyTargets(const FRowerTargets& Target)
{
 if(!bHasBody || !Source || !GetSkinnedAsset())return;
 const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
 const float Delta=LastTime<0?0.f:FMath::Clamp(float(Now-LastTime),0.f,.1f);LastTime=Now;
 BlendTime=FMath::Min(BlendDuration,BlendTime+Delta);
 // Mannequin meshes face +Y; the seat faces Target.Yaw in boat space.
 const FRotator Facing(0,Target.Yaw-90,0);
 SetRelativeRotation(Facing);
 CopyPoseFromSkeletalComponent(Source);
 // Pelvis on the seat: shift the whole mesh, keep the authored pelvis offset from its root.
 if(GetBoneIndex(TEXT("pelvis"))!=INDEX_NONE)
 {
  const FVector PelvisCS=GetBoneTransformByName(TEXT("pelvis"),EBoneSpaces::ComponentSpace).GetLocation();
  SetRelativeLocation(Target.Hip-Facing.RotateVector(PelvisCS));
 }
 const FTransform Rel=GetRelativeTransform();
 auto CS=[&](const FVector& BoatPoint){return Rel.InverseTransformPosition(BoatPoint);};
 auto CSDir=[&](const FVector& BoatDir){return Rel.InverseTransformVectorNoScale(BoatDir);};
 const FVector Fwd(FMath::Cos(FMath::DegreesToRadians(Target.Yaw)),FMath::Sin(FMath::DegreesToRadians(Target.Yaw)),0);
 const FVector Right(-Fwd.Y,Fwd.X,0),Up(0,0,1);
 // Torso: lean about the body's right axis (positive pitches the head toward the facing),
 // twist about up. The neck / head take back most of the lean so the gaze stays level.
 Lean(CSDir(Right),Target.Lean);
 if(!FMath::IsNearlyZero(Target.Twist))
  for(int32 I=2;I<UE_ARRAY_COUNT(Spine);++I) if(GetBoneIndex(Spine[I])!=INDEX_NONE)
  {
   FTransform T=GetBoneTransformByName(Spine[I],EBoneSpaces::ComponentSpace);
   T.SetRotation(FQuat(CSDir(Up),FMath::DegreesToRadians(Target.Twist/3))*T.GetRotation());
   SetBoneTransformByName(Spine[I],T,EBoneSpaces::ComponentSpace);
  }
 for(FName Name:{FName(TEXT("neck_01")),FName(TEXT("head"))}) if(GetBoneIndex(Name)!=INDEX_NONE)
 {
  FTransform T=GetBoneTransformByName(Name,EBoneSpaces::ComponentSpace);
  T.SetRotation(FQuat(CSDir(Right),FMath::DegreesToRadians(Target.HeadPitch*.5f))*T.GetRotation());
  SetBoneTransformByName(Name,T,EBoneSpaces::ComponentSpace);
 }
 if(Target.bHands) for(int32 S=0;S<2;++S) if(Target.HandOn[S])
 {
  const float Out=S==0?-1.f:1.f;
  const FVector Shoulder=GetBoneTransformByName(Bone(TEXT("upperarm"),S),EBoneSpaces::ComponentSpace).GetLocation();
  const FVector Grip=CS(Target.Hand[S]);
  // The wrist sits a hand-width behind the grip, toward the shoulder and a little above the loom.
  const FVector Wrist=Grip+(Shoulder-Grip).GetSafeNormal()*7.5f+CSDir(Up)*2.5f;
  const FVector Pole=Shoulder+CSDir(Right)*Out*45-CSDir(Up)*35-CSDir(Fwd)*20;
  SolveLimb(Bone(TEXT("upperarm"),S),Bone(TEXT("lowerarm"),S),Bone(TEXT("hand"),S),Wrist,Pole,CSDir(Target.HandAxis[S]));
  // Roll the hand about the forearm so the thumb lies along the loom toward the boat's centre.
  const FName Hand=Bone(TEXT("hand"),S),Thumb=Finger(TEXT("thumb"),1,S);
  if(GetBoneIndex(Thumb)!=INDEX_NONE)
  {
   FTransform H=GetBoneTransformByName(Hand,EBoneSpaces::ComponentSpace);
   const FVector Forearm=(H.GetLocation()-GetBoneTransformByName(Bone(TEXT("lowerarm"),S),EBoneSpaces::ComponentSpace).GetLocation()).GetSafeNormal();
   const FVector Current=FVector::VectorPlaneProject(GetBoneTransformByName(Thumb,EBoneSpaces::ComponentSpace).GetLocation()-H.GetLocation(),Forearm).GetSafeNormal();
   const FVector Desired=FVector::VectorPlaneProject(-CSDir(Target.HandAxis[S]),Forearm).GetSafeNormal();
   if(!Current.IsNearlyZero() && !Desired.IsNearlyZero())
   {
    const float Angle=FMath::Clamp(FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Current,Desired),Forearm),FVector::DotProduct(Current,Desired)),-1.4f,1.4f);
    H.SetRotation(FQuat(Forearm,Angle)*H.GetRotation());SetBoneTransformByName(Hand,H,EBoneSpaces::ComponentSpace);
   }
  }
  // Close the fingers around the loom: each joint turns about the grip axis toward the loom.
  const FVector Axis=CSDir(Target.HandAxis[S]).GetSafeNormal();
  for(const TCHAR* F:Fingers) for(int32 J=1;J<=3;++J)
  {
   const FName Name=Finger(F,J,S);if(GetBoneIndex(Name)==INDEX_NONE || Axis.IsNearlyZero())continue;
   FTransform T=GetBoneTransformByName(Name,EBoneSpaces::ComponentSpace);
   const FName TipName=Finger(F,FMath::Min(J+1,3),S);
   const FVector Tip=GetBoneIndex(TipName)!=INDEX_NONE && J<3?GetBoneTransformByName(TipName,EBoneSpaces::ComponentSpace).GetLocation():T.GetLocation()+T.GetRotation().GetAxisX()*2.5f*(S==0?-1.f:1.f);
   const FVector Arm=Tip-T.GetLocation();
   // Pick the turning sense that brings the fingertip toward the loom line through the grip.
   auto Dist=[&](float A){const FVector P=T.GetLocation()+FQuat(Axis,A).RotateVector(Arm);return FVector::CrossProduct(P-Grip,Axis).Size();};
   const float Sign=Dist(.1f)<Dist(-.1f)?1.f:-1.f;
   const float Curl=FMath::DegreesToRadians(J==1?55.f:J==2?70.f:45.f);
   T.SetRotation(FQuat(Axis,Sign*Curl)*T.GetRotation());SetBoneTransformByName(Name,T,EBoneSpaces::ComponentSpace);
  }
 }
 if(Target.bFeet) for(int32 S=0;S<2;++S)
 {
  const float Out=S==0?-1.f:1.f;
  const FVector Hip=GetBoneTransformByName(Bone(TEXT("thigh"),S),EBoneSpaces::ComponentSpace).GetLocation();
  const FVector Pole=Hip+CSDir(Fwd)*80+CSDir(Up)*45+CSDir(Right)*Out*12;
  SolveLimb(Bone(TEXT("thigh"),S),Bone(TEXT("calf"),S),Bone(TEXT("foot"),S),CS(Target.Foot[S]),Pole,FVector::ZeroVector);
 }
 // Boarding / leaving: blend between the captured standing pose and this seated pose.
 if(Snapshot.Num()==BoneSpaceTransforms.Num() && (BlendTime<BlendDuration || bStanding))
 {
  const float A=Smooth(BlendTime/BlendDuration);const float Seated=bStanding?1-A:A;
  if(Seated<1)
  {
   TArray<FTransform> Out=BoneSpaceTransforms;
   for(int32 I=0;I<Out.Num();++I)
   {
    FTransform From=Snapshot[I];
    if(I==0)From=Snapshot[0]*SnapshotWorld*GetComponentTransform().Inverse();   // root carries the body's placement
    Out[I].SetRotation(FQuat::Slerp(From.GetRotation(),BoneSpaceTransforms[I].GetRotation(),Seated).GetNormalized());
    Out[I].SetTranslation(FMath::Lerp(From.GetTranslation(),BoneSpaceTransforms[I].GetTranslation(),Seated));
    Out[I].SetScale3D(BoneSpaceTransforms[I].GetScale3D());
   }
   BoneSpaceTransforms=MoveTemp(Out);
  }
 }
 RefreshBoneTransforms();
}
