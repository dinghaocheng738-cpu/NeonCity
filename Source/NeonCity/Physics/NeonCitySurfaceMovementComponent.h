#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Physics/DynamicSurfaceMotionFilter.h"
#include "NeonCitySurfaceMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDynamicSurfaceLanding,AActor*,SurfaceActor,FVector,RelativeVelocity);

/** Narrow extension hooks only; walking, falling, jumping and based movement use Engine Super. */
UCLASS()
class NEONCITY_API UNeonCitySurfaceMovementComponent : public UCharacterMovementComponent
{
 GENERATED_BODY()
public:
 virtual bool IsWalkable(const FHitResult& Hit) const override;
 virtual bool IsValidLandingSpot(const FVector& CapsuleLocation,const FHitResult& Hit) const override;
 virtual FVector GetImpartedMovementBaseVelocity() const override;
 virtual bool DoJump(bool bReplayingMoves,float DeltaTime) override;
 bool BeginIslandGlide(const FVector& Heading);
 bool IsIslandGliding()const{return MovementMode==MOVE_Custom && CustomMovementMode==42;}
 void SetIslandFlightControls(float Turn,float Dive,bool Canopy);
 UPROPERTY(BlueprintAssignable) FDynamicSurfaceLanding OnDynamicSurfaceLanded;
 int32 GetDynamicLandingCount() const { return LandingCount; }
 int32 GetSurfaceReleaseCount() const { return ReleaseCount; }
 const FString& GetLastSurfaceReleaseDetails() const { return LastSurfaceReleaseDetails; }
protected:
 virtual void BeginPlay() override;
 virtual void PhysCustom(float DeltaTime,int32 Iterations)override;
 virtual void PhysicsRotation(float DeltaTime) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 UFUNCTION() void ResetForLoad();
 virtual void SetPostLandedPhysics(const FHitResult& Hit) override;
 virtual void UpdateBasedMovement(float DeltaSeconds) override;
 virtual void ApplyImpactPhysicsForces(const FHitResult& Hit,const FVector& ImpactAcceleration,const FVector& ImpactVelocity) override;
 virtual void ApplyDownwardForce(float DeltaSeconds) override;
 virtual void ApplyRepulsionForce(float DeltaSeconds) override;
 virtual void CapsuleTouched(UPrimitiveComponent* OverlappedComp,AActor* Other,UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult) override;
private:
 TWeakObjectPtr<AActor> SampleBase,ReleasedBase;
 FVector SamplePointVelocity=FVector::ZeroVector;
 FDynamicSurfaceMotionFilter MotionFilter;
 double SampleTime=-1,ReleaseUntil=0;
 int32 LandingCount=0,ReleaseCount=0;
 FString LastSurfaceReleaseDetails;
 float IslandTurn=0,IslandDive=0;
 bool bIslandCanopy=false;
};
