#pragma once
#include "CoreMinimal.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCityVehicleWheel.h"
#include "IslandSupercar.generated.h"
class UAudioComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UNacreRoofComponent;
class UIslandSupercarPresentationData;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class NEONCITY_API UIslandSupercarWheel : public UNeonCityVehicleWheel
{
 GENERATED_BODY()
public: UIslandSupercarWheel();
};
UCLASS()
class NEONCITY_API UIslandSupercarFrontWheel : public UIslandSupercarWheel
{
 GENERATED_BODY()
public: UIslandSupercarFrontWheel();
};
UCLASS()
class NEONCITY_API UIslandRoadsterWheel : public UIslandSupercarWheel
{
 GENERATED_BODY()
public: UIslandRoadsterWheel();
};
UCLASS()
class NEONCITY_API UIslandRoadsterFrontWheel : public UIslandRoadsterWheel
{
 GENERATED_BODY()
public: UIslandRoadsterFrontWheel();
};
UCLASS()
class NEONCITY_API UIslandReplicaRearWheel : public UIslandRoadsterWheel
{
 GENERATED_BODY()
public: UIslandReplicaRearWheel();
};
UCLASS()
class NEONCITY_API UIslandReplicaFrontWheel : public UIslandRoadsterFrontWheel
{
 GENERATED_BODY()
public: UIslandReplicaFrontWheel();
};

/** Stable Aster/Kestrel identities and the original retractable-roof Nacre. */
UCLASS()
class NEONCITY_API AIslandSupercar : public ANeonCityStarterSedan
{
 GENERATED_BODY()
public:
 AIslandSupercar();
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere,Category="Hero Car") bool bRoadster=false;
 UPROPERTY(EditAnywhere,Category="Hero Car") bool bRetractable=false;
 UNacreRoofComponent* GetRoofMechanism() const { return RoofMechanism; }
 const UIslandSupercarPresentationData* GetPresentationProfile() const { return PresentationProfile; }
 /** Audio perspective is independent of engine state; ready for a cockpit camera. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hero Car|Sound",meta=(ClampMin="0",ClampMax="1")) float InteriorMix=0;
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
 virtual void UnPossessed() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void OnCockpitViewChanged(bool bEnabled) override { InteriorMix=bEnabled?1.f:0.f; }   // cabin sound in the driver's view
private:
 void ConfigureChassis();
 void ConfigureReplicaRuntimePhysics();
 void ApplyHeroBody();
 void ConfigureSound();
 void ToggleRoof();
 void RemoveRoofMapping();
 UPROPERTY(VisibleAnywhere,Category="Hero Car|Roof") TObjectPtr<UNacreRoofComponent> RoofMechanism;
 UPROPERTY() TObjectPtr<UIslandSupercarPresentationData> PresentationProfile;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RoofMapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> RoofAction;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RoofInputSubsystem;
 UPROPERTY() TArray<TObjectPtr<UAudioComponent>> EngineLayers;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BrakeCalipers;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeroWheels;
 UPROPERTY() TObjectPtr<UAudioComponent> Overrun;
 UPROPERTY() TObjectPtr<UAudioComponent> Shift;
 UPROPERTY() TObjectPtr<USpotLightComponent> LeftLamp;
 UPROPERTY() TObjectPtr<USpotLightComponent> RightLamp;
 float SmoothedRPM=900;
 float PreviousThrottle=0;
 float ShiftEnvelope=0;
 float PopCooldown=0;
 int32 PreviousGear=0;
};
