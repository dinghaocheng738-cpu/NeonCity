#include "Adventure/IslandLandLight.h"
#include "Components/PointLightComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Engine/World.h"
AIslandLandLight::AIslandLandLight()
{
 // This actor exposes Lumens and is continuously driven by the shared clock
 // and light budget. APointLight's stationary/unitless defaults do not match
 // that contract: 2200 unitless is only about44 lumens for a point source.
 auto* Point=CastChecked<UPointLightComponent>(GetLightComponent());
 Point->SetMobility(EComponentMobility::Movable);
 Point->SetIntensityUnits(ELightUnits::Lumens);
}
void AIslandLandLight::BeginPlay()
{
 Super::BeginPlay();
 if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  FNeonCityLocalLightPolicy P;P.Intensity=Lumens;P.DayMultiplier=DayMultiplier;P.bCastShadows=bBudgetShadows;
  P.CullDistance=FMath::Max(1.f,CullDistance);P.ShadowDistance=FMath::Clamp(ShadowDistance,0.f,P.CullDistance);P.VolumetricScattering=.1f;S->RegisterLight(Cast<UPointLightComponent>(GetLightComponent()),P);
 }
}
void AIslandLandLight::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())S->UnregisterLight(Cast<UPointLightComponent>(GetLightComponent()));
 Super::EndPlay(Reason);
}
