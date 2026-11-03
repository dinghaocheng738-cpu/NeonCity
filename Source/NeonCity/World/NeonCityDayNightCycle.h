#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityDayNightCycle.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class APointLight;
class UPostProcessComponent;
class UVolumetricCloudComponent;

/** One local prototype clock with sun/sky and explicit city-light references. */
UCLASS()
class NEONCITY_API ANeonCityDayNightCycle : public AActor
{
	GENERATED_BODY()
public:
	ANeonCityDayNightCycle();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category="NeonCity|Time")
	float GetTimeOfDay() const { return TimeOfDay; }
 float GetInitialTimeOfDay() const { return InitialTimeOfDay; }

	UFUNCTION(BlueprintCallable, Category="NeonCity|Time")
	void SetTimeOfDay(float Hours);

	UFUNCTION(BlueprintCallable, Category="NeonCity|Time")
	void SetCycleEnabled(bool bEnabled) { bCycleEnabled = bEnabled; }

 /** Days since the save began: the clock running past midnight starts the next one (business income, weather). */
 UFUNCTION(BlueprintPure,Category="NeonCity|Time") int32 GetGameDay() const { return GameDay; }
 void SetGameDay(int32 Day) { GameDay=FMath::Max(0,Day); }
 DECLARE_MULTICAST_DELEGATE_OneParam(FNeonCityNewDay,int32);
 /** Broadcast when the running clock passes midnight, with the new day. */
 FNeonCityNewDay OnNewDay;

	UFUNCTION(BlueprintPure, Category="NeonCity|Time")
	UDirectionalLightComponent* GetSun() const { return Sun; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NeonCity|Time", meta=(ClampMin="1.0", Units="s"))
	float DayLengthSeconds = 3600.f;

 /** Fraction of real cycle spent between 06:00 and 18:00; speed is continuous. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NeonCity|Time",meta=(ClampMin="0.5",ClampMax="0.8"))
 float DaylightShare=.75f;
 static float AdvanceClock(float Hours,float ElapsedSeconds,float CycleSeconds,float DayShare);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NeonCity|Time")
	bool bCycleEnabled = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="NeonCity|Lighting")
	TArray<TObjectPtr<APointLight>> NightLights;

 /** Low-sun source tint, before SkyAtmosphere's own wavelength scattering. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NeonCity|Lighting")
 FLinearColor LowSunLightColor=FLinearColor(1.f,.48f,.25f);

 /** Compass bearing (yaw, degrees) of the plane the sun and moon travel in.
  * -35 is the original arc; a map can turn it so its key views are front-lit. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NeonCity|Lighting",meta=(ClampMin="-180.0",ClampMax="180.0"))
 float SunAzimuthYaw=-35.f;

 /** Daytime sky level; nighttime retains the existing moonlit baseline. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="NeonCity|Lighting",meta=(ClampMin="0.25",ClampMax="4.0"))
 float DaySkyIntensity=1.f;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateLighting();
 float InitialTimeOfDay=10;
 int32 GameDay=0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NeonCity|Time", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="24.0"))
	float TimeOfDay = 10.f;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UDirectionalLightComponent> Sun;

 UPROPERTY(VisibleAnywhere, Category="Components")
 TObjectPtr<UDirectionalLightComponent> Moon;

 UPROPERTY(VisibleAnywhere, Category="Components")
 TObjectPtr<UPostProcessComponent> Exposure;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkyLightComponent> Sky;
 UPROPERTY(VisibleAnywhere,Category="Components") TObjectPtr<UVolumetricCloudComponent> Clouds;
};
