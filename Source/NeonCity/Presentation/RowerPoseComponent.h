#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "RowerPoseComponent.generated.h"
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;

/** Boat-local targets (cm, x = bow) for one seated person. */
USTRUCT()
struct NEONCITY_API FRowerTargets
{
 GENERATED_BODY()
 /** Pelvis on the seat. */
 FVector Hip=FVector::ZeroVector;
 /** Facing in degrees (0 = toward the bow). */
 float Yaw=0;
 /** Torso pitch, degrees, positive = leaning forward. Distributed over the spine. */
 float Lean=0;
 /** Torso yaw, degrees, positive = turning right. */
 float Twist=0;
 /** Grip points for the left / right hand. */
 FVector Hand[2]={FVector::ZeroVector,FVector::ZeroVector};
 /** Unit grip axis (along the held loom, pointing outboard). */
 FVector HandAxis[2]={FVector::ZeroVector,FVector::ZeroVector};
 /** Ankle targets (foot brace / floor). */
 FVector Foot[2]={FVector::ZeroVector,FVector::ZeroVector};
 /** Head pitch correction, degrees (keeps the gaze level while the torso leans). */
 float HeadPitch=0;
 /** False: hands stay where the seated loop puts them (passenger). */
 bool bHands=true;
 /** Per hand: false leaves that hand resting (e.g. its oar is shipped). */
 bool HandOn[2]={true,true};
 bool bFeet=true;
};

/** Seated rower / passenger presentation (no simulation). Each frame the authored seated loop
 * is the base pose; the pelvis is placed on the seat, the spine leans with the stroke and the
 * arms and legs reach their boat-local targets through two-bone IK. Boarding blends from a
 * captured standing pose into the seat; leaving blends back out to it. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API URowerPoseComponent : public UPoseableMeshComponent
{
 GENERATED_BODY()
public:
 URowerPoseComponent();
 /** Shows Mesh (must share the seated loop's skeleton) with the given materials. */
 bool SetBody(USkeletalMesh* Mesh,const TArray<UMaterialInterface*>& Materials);
 void ClearBody();
 bool HasBody() const { return bHasBody; }
 /** Poses the body for this frame. Call after the owner moved its rig. */
 void ApplyTargets(const FRowerTargets& Targets);
 /** Boarding: capture Standing's current pose and blend from it into the seat. */
 void BeginSitFrom(USkeletalMeshComponent* Standing,float Duration);
 /** Leaving: blend out of the seat to the captured standing pose placed at StandingMeshWorld. */
 void BeginStandTo(const FTransform& StandingMeshWorld,float Duration);
 bool IsBlending() const { return BlendTime<BlendDuration; }
 /** Abort a stand-up: back in the seat at once. */
 void SnapSeated() { bStanding=false; BlendTime=BlendDuration; }
 bool HasStandingPose() const { return Snapshot.Num()>0; }
 /** World position of a bone (diagnostics / tests). */
 FVector GetBoneWorld(FName Bone);
 UPROPERTY(EditAnywhere,Category="Rower") TObjectPtr<UAnimSequence> SeatedAnimation;
private:
 void Lean(const FVector& AxisCS,float Degrees);
 void SolveLimb(FName Root,FName Joint,FName End,const FVector& EffectorCS,const FVector& PoleCS,const FVector& EndAxisCS);
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> Source;
 TArray<FTransform> Snapshot;
 FTransform SnapshotWorld;
 float BlendTime=1,BlendDuration=1;
 bool bStanding=false,bHasBody=false;
 double LastTime=-1;
};
