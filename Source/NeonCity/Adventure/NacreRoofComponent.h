#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "NacreRoofComponent.generated.h"
class UStaticMeshComponent;
class UAudioComponent;
class UPhysicsConstraintComponent;
class UIslandSupercarPresentationData;
class UPrimitiveComponent;
enum class EComponentPhysicsStateChange : uint8;
struct FNacreRoofCollisionPair;

/** Two rigid telescoping panels retract into a physical bay behind the seats.
 * Progress is a continuous reversible kinematic coordinate, also saved on disk. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNacreRoofComponent : public USceneComponent
{
 GENERATED_BODY()
public:
 UNacreRoofComponent();
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
 void Configure(bool Enabled,UIslandSupercarPresentationData* Presentation=nullptr);
 bool RequestToggle();
 void RestoreState(float SavedProgress,bool SavedTargetOpen);
 float GetProgress() const { return Progress; }
 bool IsTargetOpen() const { return bTargetOpen; }
 bool IsEnabled() const { return bEnabled; }
 bool IsCycling() const { return bEnabled && !FMath::IsNearlyEqual(Progress,bTargetOpen?1.f:0.f,.00001f); }
 FText GetStatus() const;
 UStaticMeshComponent* GetFrontPanel() const { return FrontPanel; }
 UStaticMeshComponent* GetRearPanel() const { return RearPanel; }
 UStaticMeshComponent* GetBayCover() const { return BayCover; }
 const UIslandSupercarPresentationData* GetPresentationProfile() const { return PresentationProfile; }
 const TArray<TObjectPtr<UStaticMeshComponent>>& GetSwingArms() const { return SwingArms; }
 const TArray<TObjectPtr<UStaticMeshComponent>>& GetPhysicalCarriers() const { return PhysicalCarriers; }
 float GetCycleDuration() const;
 static constexpr float CycleSeconds=7.6f;
 static constexpr float MaximumOperationSpeedKmh=8.f;
protected:
 virtual void OnRegister() override;
 virtual void OnUnregister() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 void UpdateMotionInterlock();
 void ConfigureChassisContactFilter();
 void ClearChassisContactFilter();
 UFUNCTION() void HandleContactPhysicsStateChanged(UPrimitiveComponent* Component,EComponentPhysicsStateChange StateChange);
 float GetOperationSpeedKmh() const;
 void ApplyPose();
 void Notify(const FText& Message) const;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> FrontPanel;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RearPanel;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> BayCover;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftWindow;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RightWindow;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftLatch;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RightLatch;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftCarriage;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RightCarriage;
 UPROPERTY() TObjectPtr<UAudioComponent> Motor;
 UPROPERTY() TObjectPtr<UAudioComponent> LatchAudio;
 UPROPERTY(Transient) TObjectPtr<UPhysicsConstraintComponent> ChassisContactFilter;
 TSharedPtr<FNacreRoofCollisionPair,ESPMode::ThreadSafe> NativeChassisContactFilter;
 UPROPERTY() TObjectPtr<UIslandSupercarPresentationData> PresentationProfile;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SwingArms;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PhysicalCarriers;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> FixedGuides;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> RoofMounts;
 float Progress=0;
 bool bTargetOpen=false;
 bool bEnabled=false;
 bool bWasCycling=false;
 bool bUnsafeNotice=false;
 bool bContactFilterEndingPlay=false;
};
