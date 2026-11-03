#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Physics/PhysicalImpactEvent.h"
#include "PhysicalImpactComponent.generated.h"
class UPrimitiveComponent;
/** Vehicle contact adapter; snapshots velocity before physics, never owns health or damage equations. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UPhysicalImpactComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPhysicalImpactComponent();
 void Suppress(float Seconds=.75f);
 bool IsSuppressed() const;
 void SetKinematicVelocity(FVector Velocity) { SampleVelocity=Velocity.GetClampedToMaxSize(6000); SampleTime=GetWorldTime(); }
 FVector GetContactVelocity() const;
 float GetContactMass() const;
 void SubmitSweep(AActor* Other,const FHitResult& Hit);
 void Notify(const FPhysicalImpactEvent& Event);
 UPROPERTY(EditAnywhere) float KinematicMass=1450;
 /** Resting-contact exclusion remains 1000 for existing vehicles; aircraft opt into landing thresholds. */
 UPROPERTY(EditAnywhere) float MinimumFloorImpactSpeed=1000;
 UPROPERTY(BlueprintAssignable) FPhysicalImpactSignal OnImpactFeedback;
 UPROPERTY(BlueprintAssignable) FPhysicalImpactSignal OnCollisionAudioRequested;
 UPrimitiveComponent* GetBody() const { return Body; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 float GetWorldTime() const;
 void SetupBody();
 UFUNCTION() void Hit(UPrimitiveComponent* HitBody,AActor* Other,UPrimitiveComponent* OtherBody,FVector Impulse,const FHitResult& Contact);
 UPROPERTY() TObjectPtr<UPrimitiveComponent> Body;
 FVector SampleVelocity=FVector::ZeroVector;
 float SampleTime=-1,SuppressedUntil=0,StabilityUntil=0;
 FDelegateHandle PhysicsCreated;
};
