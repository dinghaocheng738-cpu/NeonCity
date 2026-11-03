#include "Adventure/IslandAdventureLighting.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
namespace { constexpr float MountainDayEV100Floor=9.f; }
AIslandAdventureLighting::AIslandAdventureLighting()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=1;
 Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("MountainExposureBounds"));SetRootComponent(Bounds);
 Bounds->SetBoxExtent(FVector(35000,32000,40000));
 // UE 5.8 uses the shape's Chaos object for post-process distance queries.
 // Keep a query body, but ignore every gameplay collision channel.
 Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
 Bounds->SetGenerateOverlapEvents(false);
 Exposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("MountainExposure"));Exposure->SetupAttachment(Bounds);
 Exposure->bUnbound=false;Exposure->Priority=-5;Exposure->BlendRadius=4000;
 auto& S=Exposure->Settings;
 S.bOverride_AutoExposureMinBrightness=S.bOverride_AutoExposureMaxBrightness=S.bOverride_AutoExposureBias=true;
 // Allow shaded road cuts to adapt while retaining the bright-sky ceiling.
 // The previous EV13.6 floor made the normal daytime canyon almost black.
 S.AutoExposureMinBrightness=MountainDayEV100Floor;S.AutoExposureMaxBrightness=16;S.AutoExposureBias=0;
 S.bOverride_LocalExposureHighlightContrastScale=true;S.LocalExposureHighlightContrastScale=1;
 S.bOverride_LocalExposureShadowContrastScale=true;S.LocalExposureShadowContrastScale=1;
}
void AIslandAdventureLighting::BeginPlay(){Super::BeginPlay();UpdateExposure();}
void AIslandAdventureLighting::Tick(float Delta){Super::Tick(Delta);UpdateExposure();}
void AIslandAdventureLighting::UpdateExposure()
{
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It)
 {
  const float Day=FMath::Clamp(FMath::Sin((It->GetTimeOfDay()-6)*PI/12),0.f,1.f);
  Exposure->Settings.AutoExposureMinBrightness=Day>.01f?MountainDayEV100Floor+FMath::Log2(Day):-1.5f;
  break;
 }
}
