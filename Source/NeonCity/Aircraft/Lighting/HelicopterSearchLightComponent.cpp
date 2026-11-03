#include "Aircraft/Lighting/HelicopterSearchLightComponent.h"
#include "Aircraft/Lighting/SearchLightData.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
UHelicopterSearchLightComponent::UHelicopterSearchLightComponent()
{
 PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/NeonCity/Art/VerticalSlice/AircraftLighting/SM_NC205C_AircraftSearchlight"));
 HousingMesh=Mesh.Object;
}
const USearchLightData* UHelicopterSearchLightComponent::GetProfile() const
{ return SearchLightData && SearchLightData->IsValidProfile()?SearchLightData.Get():GetDefault<USearchLightData>(); }
void UHelicopterSearchLightComponent::BeginPlay()
{
 Super::BeginPlay();
 USceneComponent* Mount=GetOwner()->GetRootComponent();FName MountSocket=NAME_None;
 for(auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(GetOwner()))
  if(Mesh->DoesSocketExist(TEXT("SearchlightMount"))) { Mount=Mesh;MountSocket=TEXT("SearchlightMount");break; }
 Housing=NewObject<UStaticMeshComponent>(GetOwner(),TEXT("SearchlightOptics"));
 Housing->SetupAttachment(Mount,MountSocket);Housing->SetStaticMesh(HousingMesh);
 // The root-relative fallback serves older aircraft art only. It never changes physics.
 if(MountSocket.IsNone())Housing->SetRelativeLocation(FVector(184,0,-53));
 Housing->SetRelativeRotation(FRotator(-55,0,0));Housing->SetMobility(EComponentMobility::Movable);
 Housing->SetCollisionEnabled(ECollisionEnabled::NoCollision);Housing->SetGenerateOverlapEvents(false);
 Housing->SetCanEverAffectNavigation(false);Housing->CanCharacterStepUpOn=ECB_No;Housing->SetCastShadow(false);
 Housing->RegisterComponent();
 const int32 LensIndex=Housing->GetMaterialIndex(TEXT("M_NC205B_WhiteLight"));
 if(LensIndex!=INDEX_NONE)Lens=Housing->CreateDynamicMaterialInstance(LensIndex);
 Light=NewObject<USpotLightComponent>(GetOwner(),TEXT("HelicopterSearchSpot"));
 Light->SetupAttachment(Housing,TEXT("LightEmission"));Light->SetMobility(EComponentMobility::Movable);
 Light->SetIntensityUnits(ELightUnits::Candelas); Light->SetLightColor(FLinearColor(.88f,.94f,1));
 Light->SetVisibility(false); Light->RegisterComponent();
 const auto* D=GetProfile();
 Light->SetAttenuationRadius(D->Range);Light->SetInnerConeAngle(D->InnerCone);Light->SetOuterConeAngle(D->OuterCone);
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  FNeonCityLocalLightPolicy Policy;Policy.Intensity=D->Intensity;Policy.DayMultiplier=D->DayMultiplier;
  Policy.ShadowDistance=D->ShadowDistance;Policy.CullDistance=D->CullDistance;Policy.VolumetricScattering=D->VolumetricScattering;
  Policy.Priority=10;Policy.MidDistanceMultiplier=.25f;Policy.bEnabled=false;
  Lighting->RegisterLight(Light,Policy);
 }
}
void UHelicopterSearchLightComponent::SetEnabled(bool Enabled)
{
 if(const auto* H=GetOwner()->FindComponentByClass<UHealthComponent>(); H && H->IsDead()) Enabled=false;
 bEnabled=Enabled;
 if(Light)
 {
  if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
  {Lighting->SetLightEnabled(Light,bEnabled);Lighting->RefreshLighting();}
  UpdateLight(0);
 }
}
void UHelicopterSearchLightComponent::SetAimOffset(FRotator Offset)
{
 if(Offset.ContainsNaN()) return;
 AimOffset=FRotator(FMath::Clamp(FRotator::NormalizeAxis(Offset.Pitch),-20.f,20.f),FMath::Clamp(FRotator::NormalizeAxis(Offset.Yaw),-25.f,25.f),0);
}
void UHelicopterSearchLightComponent::ResetForLoad()
{
 AimOffset=FRotator::ZeroRotator; Elapsed=0; SetEnabled(false);
 if(Housing) Housing->SetRelativeRotation(FRotator(-55,0,0));
}
FRotator UHelicopterSearchLightComponent::GetAimRotation() const
{ return Housing?Housing->GetRelativeRotation():FRotator(-55,0,0); }
void UHelicopterSearchLightComponent::RotateOptics(float Delta)
{ if(Housing)Housing->SetRelativeRotation(FMath::RInterpConstantTo(Housing->GetRelativeRotation(),FRotator(-55,0,0)+AimOffset,Delta,GetProfile()->RotationSpeed)); }
void UHelicopterSearchLightComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* Pawn=Cast<APawn>(GetOwner()); auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 const bool Local=PC && PC->IsLocalController() && PC->GetPawn()==Pawn;
 if(Local)
 {
  if(PC->WasInputKeyJustPressed(EKeys::L)) ToggleSearchLight();
  const FRotator Relative=(PC->GetControlRotation()-Pawn->GetActorRotation()).GetNormalized();
  SetAimOffset(FRotator(Relative.Pitch*.35f,Relative.Yaw*.35f,0));
 }
 if(!bEnabled) return;
 if(const auto* H=GetOwner()->FindComponentByClass<UHealthComponent>(); H && H->IsDead()) { SetEnabled(false); return; }
 // Keep local input and aiming smooth, but do not dirty rendering properties every frame.
 if(Local)RotateOptics(Delta);
 Elapsed+=Delta;
 if(Elapsed>=UpdateInterval) { UpdateLight(Local?0:Elapsed); Elapsed=0; }
}
void UHelicopterSearchLightComponent::UpdateLight(float Delta)
{
 if(!Light) return;
 const auto* D=GetProfile(); auto* Viewer=GetWorld()->GetFirstPlayerController();
 FVector Eye=FVector::ZeroVector; FRotator View; if(Viewer) Viewer->GetPlayerViewPoint(Eye,View);
 const float Distance=Viewer?FVector::Dist(Eye,GetOwner()->GetActorLocation()):TNumericLimits<float>::Max();
 bHighQuality=bEnabled && Distance<D->ShadowDistance; UpdateInterval=bHighQuality?.1f:1.f;
 if(Lens)Lens->SetScalarParameterValue(TEXT("LightEnabled"),Light->IsVisible()?1.f:0.f);
 // Optical aiming remains local; photometry, dusk/dawn and shadow budgets have
 // one world owner. This component no longer searches for or polls the clock.
 RotateOptics(Delta);
}
void UHelicopterSearchLightComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->UnregisterLight(Light);
 if(IsValid(Light))Light->DestroyComponent();if(IsValid(Housing))Housing->DestroyComponent();
 Lens=nullptr;Super::EndPlay(Reason);
}
