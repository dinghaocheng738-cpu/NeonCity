#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandLandInteriorExposure.generated.h"
class UBoxComponent;
class UPostProcessComponent;
class ANeonCityPlayerCharacter;
class USkinnedMeshComponent;
UCLASS()
class NEONCITY_API AIslandLandInteriorExposure:public AActor
{
 GENERATED_BODY()
public:
 AIslandLandInteriorExposure();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UPostProcessComponent> Exposure;
 UPROPERTY(EditAnywhere,Category="Interior",meta=(ClampMin="100",ClampMax="300",Units="cm")) float InteriorArmLength=180;
 // Deep rooms need natural indoor metering; the default legacy rooms retain9.
 UPROPERTY(EditAnywhere,Category="Interior",meta=(ClampMin="2",ClampMax="15.8")) float DaylightEV100Floor=9;
 // Local highlight compression retains sky detail through large windows.
 // Default preserves existing rooms; authored overrides fade to the old night value.
 UPROPERTY(EditAnywhere,Category="Interior",meta=(ClampMin="0.3",ClampMax="1")) float DaylightHighlightContrast=.85f;
 /** Retain daytime metering, join it continuously to absolute night EV-1.5. */
 static float MinimumEV100(float SolarDay,float DaylightFloor);
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
 void RestoreCamera();
 TWeakObjectPtr<ANeonCityPlayerCharacter> CameraPlayer;
 float ExposureRefresh=0;
};
