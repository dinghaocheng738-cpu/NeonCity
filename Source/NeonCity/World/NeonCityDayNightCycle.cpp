#include "World/NeonCityDayNightCycle.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"

ANeonCityDayNightCycle::ANeonCityDayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentRoot")));
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
 Moon=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
 Moon->SetupAttachment(RootComponent);Moon->SetMobility(EComponentMobility::Movable);
 Moon->SetAtmosphereSunLight(true);Moon->SetAtmosphereSunLightIndex(1);
 Moon->SetLightColor(FLinearColor(.72f,.82f,1.f));Moon->SetIntensity(0);
	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(RootComponent);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetRealTimeCaptureEnabled(true);
 Clouds=CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("OceanCloudLayer"));Clouds->SetupAttachment(RootComponent);
 Clouds->LayerBottomAltitude=1.8f;Clouds->LayerHeight=3.2f;Clouds->ViewSampleCountScale=.65f;
 Clouds->ReflectionViewSampleCountScaleValue=.3f;Clouds->ShadowViewSampleCountScale=.4f;
 Clouds->ShadowReflectionViewSampleCountScaleValue=.2f;Clouds->bVisibleInRealTimeSkyCaptures=true;
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"));
 Clouds->Material=CloudMaterial.Object;
 Sun->bCastCloudShadows=true;Sun->CloudShadowStrength=.6f;Sun->CloudShadowExtent=12;
 Sun->CloudShadowMapResolutionScale=.5f;
 Exposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("WorldExposureRange"));Exposure->SetupAttachment(RootComponent);
 Exposure->bUnbound=true;Exposure->Priority=-100;
 // A bounded world fallback. Authored interiors/urban volumes may override it.
 // EV100 -1.5 keeps moonlit gameplay dark and stays inside the renderer's cache range.
 Exposure->Settings.bOverride_AutoExposureMinBrightness=true;
 Exposure->Settings.AutoExposureMinBrightness=-1.5f;
 Exposure->Settings.bOverride_AutoExposureMaxBrightness=true;
 Exposure->Settings.AutoExposureMaxBrightness=16.f;
}

void ANeonCityDayNightCycle::SetTimeOfDay(float Hours)
{
	if (!FMath::IsFinite(Hours)) { return; }
	TimeOfDay = FMath::Fmod(FMath::Fmod(Hours, 24.f) + 24.f, 24.f);
	UpdateLighting();
}

void ANeonCityDayNightCycle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetTimeOfDay(TimeOfDay);
}

void ANeonCityDayNightCycle::BeginPlay()
{
	Super::BeginPlay();
 InitialTimeOfDay=TimeOfDay;
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
  for(APointLight* Light:NightLights)if(IsValid(Light))
   if(auto* Component=Cast<UPointLightComponent>(Light->GetLightComponent()))
   {
    Component->SetIntensityUnits(ELightUnits::Lumens);
    FNeonCityLocalLightPolicy Policy;Policy.Intensity=6000;Policy.ShadowDistance=2500;Policy.CullDistance=10000;
    Lighting->RegisterLight(Component,Policy);
   }
	SetTimeOfDay(TimeOfDay);
}

void ANeonCityDayNightCycle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCycleEnabled && FMath::IsFinite(DayLengthSeconds) && DayLengthSeconds >= 1.f)
	{
		const float Before=TimeOfDay;
		SetTimeOfDay(AdvanceClock(TimeOfDay,DeltaSeconds,DayLengthSeconds,DaylightShare));
  if(TimeOfDay<Before-12.f){++GameDay;OnNewDay.Broadcast(GameDay);}
	}
}

float ANeonCityDayNightCycle::AdvanceClock(float Hours,float Elapsed,float Cycle,float Share)
{
 if(!FMath::IsFinite(Hours) || !FMath::IsFinite(Elapsed) || !FMath::IsFinite(Cycle)
  || !FMath::IsFinite(Share) || Elapsed<0 || Cycle<1)return Hours;
 const double H=FMath::Fmod(FMath::Fmod(double(Hours),24.)+24.,24.)/24.;
 const double Bias=(FMath::Clamp(double(Share),.5,.8)-.5)*PI;
 auto Phase=[&](double X){return X-Bias*FMath::Sin(2*PI*X)/(2*PI);};
 const double Target=FMath::Fmod(Phase(H)+Elapsed/Cycle,1.);
 // The monotonic mapping integrates exactly over midnight and large deltas.
 // Its derivative stays positive, with no dawn/dusk clock-speed discontinuity.
 double Lo=0,Hi=1;
 for(int32 I=0;I<32;++I){const double Mid=(Lo+Hi)*.5;if(Phase(Mid)<Target)Lo=Mid;else Hi=Mid;}
 return float((Lo+Hi)*12.);
}

void ANeonCityDayNightCycle::UpdateLighting()
{
	const float Elevation = FMath::Sin((TimeOfDay - 6.f) * PI / 12.f);
	const float Day = FMath::Clamp(Elevation, 0.f, 1.f);
	const float Night = 1.f - FMath::SmoothStep(-0.08f, 0.12f, Elevation);
	Sun->SetRelativeRotation(FRotator(-(TimeOfDay - 6.f) * 15.f, SunAzimuthYaw, 0.f));
	Sun->SetIntensity(75000.f * Day);
 // A second, physically dim directional source makes unlit silhouettes readable.
 // It does not raise the sun at night or turn the entire scene into daylight.
 Moon->SetRelativeRotation(FRotator(-(TimeOfDay-6.f)*15.f+180.f,SunAzimuthYaw,0));
 Moon->SetIntensity(.25f*Night);
 // Water and translucency accept one forward directional light. Give the
 // brighter existing source an explicit priority at the day/night crossover.
 const bool SunDominant=Sun->Intensity>=Moon->Intensity;
 Sun->SetForwardShadingPriority(SunDominant?1:0);
 Moon->SetForwardShadingPriority(SunDominant?0:1);
	Sun->SetLightColor(FMath::Lerp(LowSunLightColor, FLinearColor(1.f, 0.96f, 0.88f), Day));
	Sky->SetIntensity(FMath::Lerp(0.25f, FMath::Clamp(DaySkyIntensity,0.25f,4.f), Day));
 if(GetWorld())if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->SetTimeOfDay(TimeOfDay);
 // Editor preview has no BeginPlay registration. Runtime lights have one owner.
 if(!HasActorBegunPlay())for (APointLight* Light : NightLights)
	{
		if (IsValid(Light))
		{
			UPointLightComponent* Component = Cast<UPointLightComponent>(Light->GetLightComponent());
			if (Component)
			{
				Component->SetIntensityUnits(ELightUnits::Lumens);
				Component->SetIntensity(6000.f * Night);
			}
		}
	}
}
