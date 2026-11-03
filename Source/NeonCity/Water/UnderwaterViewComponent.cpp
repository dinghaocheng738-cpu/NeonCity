#include "Water/UnderwaterViewComponent.h"
#include "Water/CoastalOceanSurface.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UUnderwaterViewComponent::UUnderwaterViewComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
void UUnderwaterViewComponent::BeginPlay()
{
 Super::BeginPlay();
 MarineSnow=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_MarineSnow"));
 auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 Camera=Player?Player->GetFollowCamera():nullptr;
 auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Marine/M_AdventureUnderwaterOptics"));
 if(Camera && Material)
 {
  WaterOptics=UMaterialInstanceDynamic::Create(Material,this);
  Camera->AddOrUpdateBlendable(WaterOptics,0);
  AddTickPrerequisiteComponent(Camera);
 }
 if(Camera)
 {
  OriginalMinEV=Camera->PostProcessSettings.AutoExposureMinBrightness;
  bOriginalMinEVOverride=Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness;
  if(auto* Lens=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/FX/M_OceanLensWater")))
  { LensOptics=UMaterialInstanceDynamic::Create(Lens,this);Camera->AddOrUpdateBlendable(LensOptics,0); }
 }
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){DayNight=*It;break;}
}
void UUnderwaterViewComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if(!Camera)return;
 auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 const auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 FWaterSurfaceSample Sample;
 const FVector Eye=Camera->GetComponentLocation();
 const bool Active=PC && PC->IsLocalController() && PC->GetViewTarget()==Player
  && GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Eye,Sample);
 // Blend over the actual top twelve centimetres, without delaying emergence.
 // Oxygen continues using its independent animated mouth sample and hysteresis.
 ImmersionWeight=Active?FMath::SmoothStep(0.f,12.f,Sample.DepthAt(Eye)):0;
 const bool Under=Active && Sample.DepthAt(Eye)>4;
 UpdateParticulates(Delta,Under && !Sample.ContainedWater,Eye,Active?Sample.SurfacePoint.Z:0);
 const bool Continuous=bEyeInitialized && FVector::DistSquared(Eye,PreviousEye)<FMath::Square(400.f);
 const bool ViewingPlayer=PC && PC->IsLocalController() && PC->GetViewTarget()==Player;
 if(!Continuous || !ViewingPlayer) { LensWetness=0;SubmergedSeconds=0;bWasUnderwater=false; }
 const float Step=FMath::Clamp(Delta,0.f,.2f);
 if(Under) { SubmergedSeconds+=Step;LensWetness=0;ExitAge=0; }
 else
 {
  if(bWasUnderwater && SubmergedSeconds>.35f && Continuous && ViewingPlayer)
  { LensWetness=FMath::Clamp(SubmergedSeconds*.35f,.35f,1.f);ExitAge=0;ExitSeed=FMath::FRandRange(1.f,4096.f); }
  SubmergedSeconds=0;ExitAge+=Step;LensWetness=FMath::Max(0.f,LensWetness-Step/9.f);
 }
 bWasUnderwater=Under;bEyeInitialized=true;PreviousEye=Eye;
 if(LensOptics)
 {
  LensOptics->SetScalarParameterValue(TEXT("Wetness"),LensWetness);
  LensOptics->SetScalarParameterValue(TEXT("ExitAge"),ExitAge);
  LensOptics->SetScalarParameterValue(TEXT("ExitSeed"),ExitSeed);
  Camera->AddOrUpdateBlendable(LensOptics,LensWetness>0?1:0);
 }
 // The material determines the medium at each near-plane pixel at the waterline.
 const bool OpticalActive=Active && !Sample.ContainedWater && Sample.DepthAt(Eye)>-GNearClippingPlane*2;
 if(WaterOptics)Camera->AddOrUpdateBlendable(WaterOptics,OpticalActive?1:0);
 if(WaterOptics && OpticalActive)
 {
  const float Day=DayNight.IsValid()?FMath::Clamp(FMath::Sin((DayNight->GetTimeOfDay()-6)*PI/12),0.f,1.f):1;
  WaterOptics->SetScalarParameterValue(TEXT("WaterSurfaceZ"),Sample.SurfacePoint.Z);
  WaterOptics->SetScalarParameterValue(TEXT("WaterDaylight"),Day);
  const auto* Sun=DayNight.IsValid()?DayNight->GetSun():nullptr;
  const float Lux=Sun?Sun->Intensity:75000.f*Day;
  const FVector SunDirection=Sun?-Sun->GetForwardVector():FVector::UpVector;
  WaterOptics->SetScalarParameterValue(TEXT("WaterSunLux"),Lux);
  if(!Ocean.IsValid())for(TActorIterator<ACoastalOceanSurface> It(GetWorld());It;++It){Ocean=*It;break;}
  const bool FocusReady=Ocean.IsValid() && Ocean->IsReady() && Ocean->GetFineCaustics() && Ocean->GetBroadCaustics();
  WaterOptics->SetScalarParameterValue(TEXT("WaterCausticsStrength"),FocusReady && SunDirection.Z>.035f?.65f:0.f);
  if(FocusReady)
  {
   WaterOptics->SetTextureParameterValue(TEXT("WaterCausticsFine"),Ocean->GetFineCaustics());
   WaterOptics->SetTextureParameterValue(TEXT("WaterCausticsBroad"),Ocean->GetBroadCaustics());
   const auto Sizes=Ocean->GetCausticTileSizes();WaterOptics->SetVectorParameterValue(TEXT("WaterCausticTiles"),FLinearColor(Sizes.X,Sizes.Y,0,0));
  }
  WaterOptics->SetScalarParameterValue(TEXT("WaterNearCm"),GNearClippingPlane);
  WaterOptics->SetVectorParameterValue(TEXT("WaterSunDirection"),FLinearColor(SunDirection));
  WaterOptics->SetVectorParameterValue(TEXT("WaterViewForward"),FLinearColor(Camera->GetForwardVector()));
  const auto* Lamp=Player?Player->FindComponentByClass<UNeonCityFlashlightComponent>():nullptr;
  const auto* Beam=Lamp && Lamp->IsEquipped() && Lamp->IsDiveMounted()?Lamp->GetBeam():nullptr;
  float TorchMeterEV=-1.5f;
  WaterOptics->SetVectorParameterValue(TEXT("WaterTorchPhotometry"),FLinearColor::Transparent);
  if(Beam && Beam->IsVisible())
  {
   const float Inner=FMath::Cos(FMath::DegreesToRadians(Beam->InnerConeAngle));
   const float Outer=FMath::Cos(FMath::DegreesToRadians(Beam->OuterConeAngle));
   // Lumens -> on-axis candela, integrated over the actual authored cone.
   const float Candela=Beam->Intensity/FMath::Max(.01f,2*PI*(1-Outer));
   // Meter a four-metre working distance, so a dark surrounding ocean does not
   // expose a near receiver or backscatter as a clipped white searchlight.
   TorchMeterEV=FMath::Log2(FMath::Max(1.f,Candela/16.f)*.125f)+.25f;
   WaterOptics->SetVectorParameterValue(TEXT("WaterTorchPosition"),FLinearColor(Beam->GetComponentLocation()));
   WaterOptics->SetVectorParameterValue(TEXT("WaterTorchDirection"),FLinearColor(Beam->GetForwardVector()));
   WaterOptics->SetVectorParameterValue(TEXT("WaterTorchColour"),Beam->GetLightColor());
   WaterOptics->SetVectorParameterValue(TEXT("WaterTorchPhotometry"),FLinearColor(Candela,Inner,Outer,Beam->AttenuationRadius*.01f));
  }
  WaterOptics->SetScalarParameterValue(TEXT("WaterFrameJitter"),float(OpticalFrame++%64));
  // Limit adaptation below the surface: depth darkness must not be metered back
  // into bright cyan. The ordinary land/night exposure returns after surfacing.
  // Preserve a darker daylight floor in the first eight metres. Allow only
  // limited deep adaptation; it can reveal nearby equipment without restoring
  // the distant contrast already removed by the optical path.
  const float DepthMetres=FMath::Max(0.f,Sample.DepthAt(Eye))*.01f;
  const float DeepAdaptation=FMath::Clamp((DepthMetres-8.f)*.15f,0.f,4.5f);
  WaterMinEV=FMath::Clamp(FMath::Max(TorchMeterEV,FMath::Log2(FMath::Max(1.f,Lux)*.125f)+.25f-DeepAdaptation),-1.5f,14.f);
 }
 ExposureWeight=FMath::FInterpTo(ExposureWeight,OpticalActive?ImmersionWeight:0.f,Step,3.f);
 if(ExposureWeight>.001f)
 {
  Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;
  Camera->PostProcessSettings.AutoExposureMinBrightness=FMath::Lerp(bOriginalMinEVOverride?OriginalMinEV:-1.5f,WaterMinEV,ExposureWeight);
 }
 else
 {
  Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness=bOriginalMinEVOverride;
  Camera->PostProcessSettings.AutoExposureMinBrightness=OriginalMinEV;
 }
}
void UUnderwaterViewComponent::UpdateParticulates(float Delta,bool Active,const FVector& Eye,float SurfaceZ)
{
 // Patches are fixed in the water, never attached to the camera. A bounded
 // world-space volume makes forward travel and turns produce real parallax.
 for(int32 I=SnowPatches.Num()-1;I>=0;--I)
 {
  auto* N=SnowPatches[I].Get();
  if(!IsValid(N) || !N->IsActive() || !Active || FVector::DistSquared(Eye,N->GetComponentLocation())>FMath::Square(1300.f))
  {if(IsValid(N))N->DestroyComponent();SnowPatches.RemoveAtSwap(I);}
  else N->SetVariableFloat(TEXT("User.SurfaceZ"),SurfaceZ);
 }
 if(!Active || !MarineSnow){SnowClock=6;return;}
 SnowClock+=FMath::Clamp(Delta,0.f,.2f);
 const bool Travel=FVector::DistSquared(Eye,LastSnowOrigin)>FMath::Square(500.f);
 if(SnowClock<6 && !(Travel && SnowClock>1.5f))return;
 if(SnowPatches.Num()>=4)return;
 if(auto* N=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),MarineSnow,Eye,FRotator::ZeroRotator,FVector::OneVector,true,false,ENCPoolMethod::None,true))
 {
  N->SetVariableFloat(TEXT("User.SurfaceZ"),SurfaceZ);N->Activate(true);SnowPatches.Add(N);
  SnowClock=0;LastSnowOrigin=Eye;
 }
}
void UUnderwaterViewComponent::EndPlay(EEndPlayReason::Type Reason)
{
 for(const auto& N:SnowPatches)if(IsValid(N))N->DestroyComponent();SnowPatches.Reset();MarineSnow=nullptr;
 if(Camera && WaterOptics)Camera->RemoveBlendable(WaterOptics);
 if(Camera && LensOptics)Camera->RemoveBlendable(LensOptics);
 if(Camera){Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness=bOriginalMinEVOverride;Camera->PostProcessSettings.AutoExposureMinBrightness=OriginalMinEV;}
 LensOptics=nullptr;
 WaterOptics=nullptr;Camera=nullptr;Super::EndPlay(Reason);
}
