#include "Adventure/IslandLandInteriorExposure.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
AIslandLandInteriorExposure::AIslandLandInteriorExposure()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("InteriorBounds"));SetRootComponent(Bounds);
 Bounds->SetBoxExtent(FVector(1500,900,650));Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);Bounds->SetGenerateOverlapEvents(false);
 Exposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("InteriorAdaptation"));Exposure->SetupAttachment(Bounds);
 Exposure->bUnbound=false;Exposure->Priority=5;Exposure->BlendRadius=80;
 auto& S=Exposure->Settings;
 S.bOverride_AutoExposureMinBrightness=S.bOverride_AutoExposureMaxBrightness=S.bOverride_AutoExposureBias=true;
 S.AutoExposureMinBrightness=9;S.AutoExposureMaxBrightness=15.8f;S.AutoExposureBias=0;
 S.bOverride_LocalExposureShadowContrastScale=true;S.LocalExposureShadowContrastScale=.65f;
 S.bOverride_LocalExposureHighlightContrastScale=true;S.LocalExposureHighlightContrastScale=.85f;
}
void AIslandLandInteriorExposure::BeginPlay()
{
 Super::BeginPlay();SetActorTickInterval(.1f);
}
float AIslandLandInteriorExposure::MinimumEV100(float SolarDay,float DaylightFloor)
{
 const float Day=FMath::Clamp(SolarDay,0.f,1.f);
 if(Day<=.01f)return -1.5f;
 const float DayFloor=DaylightFloor+FMath::Log2(Day);
 if(Day>=.15f)return DayFloor;
 // A minimum is a safety bound, not the camera's forced exposure. Preserve
 // physical sunlight and every lamp. Only the last low-sun interval releases
 // the legacy outdoor-biased floor smoothly towards the original night bound.
 const float Weight=FMath::SmoothStep(.01f,.15f,Day);
 return FMath::Lerp(-1.5f,FMath::Max(-1.5f,DayFloor),Weight);
}
void AIslandLandInteriorExposure::RestoreCamera()
{
 if(auto* P=CameraPlayer.Get())
 {
  // Aim and fishing retain their own saved camera state. Defer the room
  // restoration until they have restored Default rather than overwriting it.
  auto* Extension=P->GetCameraExtensionComponent();
  if(Extension && !Extension->ReleaseInteriorCamera(this))return;
 }
 CameraPlayer.Reset();
}
void AIslandLandInteriorExposure::Tick(float Delta)
{
 Super::Tick(Delta);
 ExposureRefresh-=Delta;
 if(ExposureRefresh<=0)for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It)
 {
  const float Day=FMath::Clamp(FMath::Sin((It->GetTimeOfDay()-6)*PI/12),0.f,1.f);
  Exposure->Settings.AutoExposureMinBrightness=MinimumEV100(Day,DaylightEV100Floor);
  Exposure->Settings.LocalExposureHighlightContrastScale=FMath::Lerp(.85f,DaylightHighlightContrast,FMath::SmoothStep(.01f,.15f,Day));
  ExposureRefresh=1;break;
 }
 const auto* PC=GetWorld()->GetFirstPlayerController();
 auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
 const FVector Extent=Bounds->GetUnscaledBoxExtent();
 const FVector Local=Player?Bounds->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation()):FVector(FLT_MAX);
 const bool Inside=Player && Player->IsLocallyControlled() && FMath::Abs(Local.X)<Extent.X && FMath::Abs(Local.Y)<Extent.Y && FMath::Abs(Local.Z)<Extent.Z;
 SetActorTickInterval(Player && (Inside || CameraPlayer.IsValid() || Local.SizeSquared()<(Extent+FVector(500)).SizeSquared())?.033f:.5f);
 if(CameraPlayer.IsValid() && (!Inside || CameraPlayer.Get()!=Player))RestoreCamera();
 if(!Inside)return;
 auto* Extension=Player->GetCameraExtensionComponent();
 if(Extension && Extension->GetCameraContext()!=ENeonCityCameraContext::Default)return;
 if(!CameraPlayer.IsValid())
 {
  if(!Player->GetCharacterMovement()->IsMovingOnGround())return;
  CameraPlayer=Player;
 }
 if(Extension)Extension->ApplyInteriorCamera(this,Player,InteriorArmLength,Delta);
}
void AIslandLandInteriorExposure::EndPlay(const EEndPlayReason::Type Reason)
{
 RestoreCamera();Super::EndPlay(Reason);
}
