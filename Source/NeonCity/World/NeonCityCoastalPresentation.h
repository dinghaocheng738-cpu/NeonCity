#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PostProcessComponent.h"
#include "NeonCityCoastalPresentation.generated.h"

class UBoxComponent;

/** Explicit opt-in for a nonphysical box region. UE5.8 can expose an empty
 * physics object for NoCollision shapes and reject its postprocess distance. */
UCLASS()
class NEONCITY_API UNeonCityCoastalPostProcessComponent : public UPostProcessComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, Category="Coastal Presentation")
 bool bUseAnalyticBoxBounds = false;

 virtual bool EncompassesPoint(FVector Point, float SphereRadius=0.f, float* OutDistanceToPoint=nullptr) override;
 virtual FPostProcessVolumeProperties GetProperties() const override;
protected:
 // UPostProcessComponent is MinimalAPI: these nonexported hooks cannot be
 // inherited across Engine's DLL boundary. Keep its registration/load policy.
 virtual void OnRegister() override;
 virtual void OnUnregister() override;
 virtual void Serialize(FArchive& Ar) override;
 virtual void PostInitProperties() override;
};

/** Opt-in, bounded presentation for the authored coastal sample.
 * Defaults supply colour only. Explicit actor instances may instead author a
 * local exposure calibration; lighting, quality and gameplay remain separate. */
UCLASS()
class NEONCITY_API ANeonCityCoastalPresentation : public AActor
{
 GENERATED_BODY()

public:
 ANeonCityCoastalPresentation(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());

 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Coastal Presentation")
 TObjectPtr<UBoxComponent> Region;

 /** Disabled by default. Only the explicitly authored mainland actor enables it. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Coastal Presentation")
 TObjectPtr<UPostProcessComponent> ColourTreatment;
};

/** Owned C923 calibration only; existing coastal colour actors keep the
 * original component class and distance policy. */
UCLASS()
class NEONCITY_API ANeonCityCoastalExposureRegion : public ANeonCityCoastalPresentation
{
 GENERATED_BODY()
public:
 ANeonCityCoastalExposureRegion(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
};
