#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SwimmingComponent.generated.h"
class USwimmingData;
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API USwimmingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USwimmingComponent();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<USwimmingData> SwimmingData;
 const USwimmingData* GetProfile() const;
 void ResetForLoad();
 // Explicit dive intent outlives a released key. Animated mouth depth must not
 // make the surface assist pull a diver straight back into the air.
 bool HasDiveIntent() const { return bDiveIntent; }
 bool IsFollowingSurface() const { return bSurfaceRecovery && !bDiveIntent; }
 bool IsSeafloorStanding() const { return bSeafloorStanding; }
 bool IsSeafloorPushOff() const { return bSeafloorPushOff; }
 float GetPushOffPoseWeight() const;
 bool PushOffSeafloor();
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 void RestoreLandSettings();
 void RestoreSeafloorSettings();
 bool bSeafloorStanding=false,bFloorSettings=false;
 bool bSeafloorPushOff=false;
 float LandWalkSpeed=0,LandGroundFriction=0,LandWalkBraking=0,FloorAcceleration=0,LandStepHeight=0;
 double SeafloorReleaseUntil=0;
 double PushOffStarted=0;
 float PushOffFloorZ=0;
 bool bApplied=false;
 bool bDiveIntent=false;
 // Once acquired, a surface swimmer follows rising waves until choosing to dive.
 bool bSurfaceRecovery=false;
 float LandAcceleration=0,LandBraking=0,LandBuoyancy=0,LandSwimSpeed=0;
};
