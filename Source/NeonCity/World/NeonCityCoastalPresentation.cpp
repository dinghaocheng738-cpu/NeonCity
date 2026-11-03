#include "World/NeonCityCoastalPresentation.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"

void UNeonCityCoastalPostProcessComponent::OnRegister()
{
 USceneComponent::OnRegister();GetWorld()->AddPostProcessVolume(this);
}
void UNeonCityCoastalPostProcessComponent::OnUnregister()
{
 USceneComponent::OnUnregister();GetWorld()->RemovePostProcessVolume(this);
}
void UNeonCityCoastalPostProcessComponent::Serialize(FArchive& Ar)
{
 USceneComponent::Serialize(Ar);
#if WITH_EDITORONLY_DATA
 if(Ar.IsLoading())Settings.OnAfterLoad();
#endif
}
void UNeonCityCoastalPostProcessComponent::PostInitProperties()
{
 USceneComponent::PostInitProperties();
#if WITH_EDITOR
 if(!IsTemplate() && !VolumeGuid.IsValid())VolumeGuid=FGuid::NewDeterministicGuid(GetPathName());
#endif
}
FPostProcessVolumeProperties UNeonCityCoastalPostProcessComponent::GetProperties() const
{
 const auto* Box=Cast<UBoxComponent>(GetAttachParent());FPostProcessVolumeProperties Result;
 Result.Settings=&Settings;Result.Priority=Priority;Result.BlendRadius=BlendRadius;Result.BlendWeight=BlendWeight;
 Result.bIsEnabled=bEnabled && ShouldRender() && (bUnbound || Box);Result.bIsUnbound=bUnbound;
 Result.Size=!bUnbound && Box?Box->GetBounds().GetBox().GetVolume():DBL_MAX;Result.VolumeGuid=VolumeGuid;
 return Result;
}

bool UNeonCityCoastalPostProcessComponent::EncompassesPoint(FVector Point, float SphereRadius, float* OutDistanceToPoint)
{
 const auto* Box=Cast<UBoxComponent>(GetAttachParent());
 if(!bUseAnalyticBoxBounds || bUnbound || !Box)return Super::EncompassesPoint(Point,SphereRadius,OutDistanceToPoint);
 const FTransform& Transform=Box->GetComponentTransform();
 const FVector Local=Transform.InverseTransformPosition(Point),Extent=Box->GetUnscaledBoxExtent();
 const FVector Closest(FMath::Clamp(Local.X,-Extent.X,Extent.X),FMath::Clamp(Local.Y,-Extent.Y,Extent.Y),FMath::Clamp(Local.Z,-Extent.Z,Extent.Z));
 // Inverse/forward floating-point round trips can leave a tiny positive
 // residual for an interior point. Its geometric distance is exactly zero.
 const bool Inside=FMath::Abs(Local.X)<=Extent.X && FMath::Abs(Local.Y)<=Extent.Y && FMath::Abs(Local.Z)<=Extent.Z;
 const float Distance=Inside?0.f:(Point-Transform.TransformPosition(Closest)).Size();
 if(OutDistanceToPoint)*OutDistanceToPoint=Distance;
 return Distance<=FMath::Max(0.f,SphereRadius);
}

ANeonCityCoastalPresentation::ANeonCityCoastalPresentation(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 PrimaryActorTick.bCanEverTick = false;
 Region = CreateDefaultSubobject<UBoxComponent>(TEXT("CoastalPresentationRegion"));
 SetRootComponent(Region);
 Region->InitBoxExtent(FVector(18000, 9500, 3500));
 Region->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Region->SetGenerateOverlapEvents(false);
 Region->SetCanEverAffectNavigation(false);
 Region->SetHiddenInGame(true);

 ColourTreatment = CreateDefaultSubobject<UPostProcessComponent>(TEXT("CoastalColourTreatment"));
 ColourTreatment->SetupAttachment(Region);
 ColourTreatment->bEnabled = false;
 ColourTreatment->bUnbound = false;
 // World fallback is -100. Interior/authored facility volumes retain precedence.
 ColourTreatment->Priority = -10;
 ColourTreatment->BlendRadius = 1500;
 ColourTreatment->BlendWeight = .75f;
 FPostProcessSettings& Settings = ColourTreatment->Settings;
 Settings.bOverride_ColorSaturation = true;
 Settings.ColorSaturation = FVector4(1, 1, 1, 1.015f);
 Settings.bOverride_ColorContrastMidtones = true;
 Settings.ColorContrastMidtones = FVector4(1, 1, 1, 1.055f);
 Settings.bOverride_ColorGainShadows = true;
 Settings.ColorGainShadows = FVector4(.985f, 1, 1.012f, 1);
 Settings.bOverride_ColorGainHighlights = true;
 Settings.ColorGainHighlights = FVector4(1.010f, 1.004f, .986f, 1);
 Settings.bOverride_ColorSaturationHighlights = true;
 Settings.ColorSaturationHighlights = FVector4(1, 1, 1, .97f);
 // Exposure, white balance, bloom, LUTs and rendering quality are inherited.
}

ANeonCityCoastalExposureRegion::ANeonCityCoastalExposureRegion(const FObjectInitializer& ObjectInitializer)
 :Super(ObjectInitializer.SetDefaultSubobjectClass<UNeonCityCoastalPostProcessComponent>(TEXT("CoastalColourTreatment")))
{
 CastChecked<UNeonCityCoastalPostProcessComponent>(ColourTreatment)->bUseAnalyticBoxBounds=true;
}
