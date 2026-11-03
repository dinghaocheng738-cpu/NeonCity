#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SwimmingAnimInstance.generated.h"

/** Presentation only. The linked Epic graph continues to own land locomotion.
 * Swimming is in-place skeletal animation; all displacement remains with CMC. */
UCLASS(Transient, Blueprintable)
class NEONCITY_API USwimmingAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float WaterAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float MovingAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float UnderwaterAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float AscendAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float DescendAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float SwimPlayRate=1;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float DiveAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") bool bNativeSwimming=false;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") FName VisibleSwimState=NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float EquipmentWaterAlpha=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") int32 OceanPose=0;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float OceanPoseRate=1;
    UPROPERTY(BlueprintReadOnly, Category="Swimming") float OceanActionTime=0;
private:
    float PreviousYaw=0,SmoothedTurn=0;
    double PoseStarted=0;
    int32 PreviousOceanPose=INDEX_NONE;
    float SurfaceDrive=0;
    bool bSurfaceTravel=false;
};
