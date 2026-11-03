#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/LocalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"

bool FNeonCityLocalLightPolicy::IsValid() const
{
 return FMath::IsFinite(Intensity) && Intensity>=0 && FMath::IsFinite(DayMultiplier)
  && DayMultiplier>=0 && DayMultiplier<=1 && FMath::IsFinite(ShadowDistance) && ShadowDistance>=0
  && FMath::IsFinite(CullDistance) && CullDistance>0 && ShadowDistance<=CullDistance
  && FMath::IsFinite(VolumetricScattering) && VolumetricScattering>=0 && VolumetricScattering<=16
  && FMath::IsFinite(MidDistanceMultiplier) && MidDistanceMultiplier>=0 && MidDistanceMultiplier<=1;
}

void UWorldLightingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
 Super::OnWorldBeginPlay(InWorld);
 CityParameters=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/NeonCity/Art/WestLake/Materials/MPC_WL_City.MPC_WL_City"));
 ApplyCityParameters();
 InWorld.GetTimerManager().SetTimer(RefreshTimer,this,&UWorldLightingSubsystem::RefreshLighting,UpdateInterval,true);
}

void UWorldLightingSubsystem::Deinitialize()
{
 if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
 Lights.Reset();Materials.Reset();Super::Deinitialize();
}

bool UWorldLightingSubsystem::RegisterLight(ULocalLightComponent* Light,const FNeonCityLocalLightPolicy& Policy)
{
 if(!IsValid(Light) || Light->GetWorld()!=GetWorld() || !Policy.IsValid())return false;
 FLocalLight* Entry=Lights.FindByPredicate([&](const FLocalLight& L){return L.Component==Light;});
 if(!Entry){Entry=&Lights.AddDefaulted_GetRef();Entry->Component=Light;}
 Entry->Policy=Policy;
 Light->SetIntensity(Policy.Intensity*(Policy.bEnabled?FMath::Lerp(Policy.DayMultiplier,1.f,NightAmount):0.f));
 // Registration can precede possession. A missing viewer never enables every lamp.
 Light->SetVisibility(false);Light->SetCastShadows(false);return true;
}

void UWorldLightingSubsystem::UnregisterLight(ULocalLightComponent* Light)
{
 Lights.RemoveAll([&](const FLocalLight& L){return L.Component==Light || !L.Component.IsValid();});
 if(IsValid(Light)){Light->SetVisibility(false);Light->SetIntensity(0);}
}

void UWorldLightingSubsystem::SetLightEnabled(ULocalLightComponent* Light,bool bEnabled)
{
 if(auto* Entry=Lights.FindByPredicate([&](const FLocalLight& L){return L.Component==Light;}))
 {
  Entry->Policy.bEnabled=bEnabled;
  if(!bEnabled && IsValid(Light)){Light->SetVisibility(false);Light->SetIntensity(0);}
 }
}

void UWorldLightingSubsystem::RegisterNightMaterial(UMaterialInstanceDynamic* Material,FName Parameter)
{
 if(!IsValid(Material) || Parameter.IsNone())return;
 if(!Materials.ContainsByPredicate([&](const FNightMaterial& M){return M.Material==Material && M.Parameter==Parameter;}))
  Materials.Add({Material,Parameter});
 Material->SetScalarParameterValue(Parameter,NightAmount);
}

void UWorldLightingSubsystem::SetTimeOfDay(float Hours)
{
 if(!FMath::IsFinite(Hours))return;
 const float Wrapped=FMath::Fmod(FMath::Fmod(Hours,24.f)+24.f,24.f);
 const float Next=1.f-FMath::SmoothStep(-.08f,.12f,FMath::Sin((Wrapped-6.f)*PI/12.f));
 if(FMath::IsNearlyEqual(Next,NightAmount,1.e-5f))return;
 NightAmount=Next;ApplyClockState();RefreshLighting();
}

void UWorldLightingSubsystem::ApplyCityParameters()
{
 if(CityParameters && GetWorld() && GetWorld()->IsGameWorld())
  UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(),CityParameters,TEXT("Night"),NightAmount);
}

void UWorldLightingSubsystem::ApplyClockState()
{
 ApplyCityParameters();
 Materials.RemoveAll([](const FNightMaterial& M){return !M.Material.IsValid();});
 for(const auto& M:Materials)M.Material->SetScalarParameterValue(M.Parameter,NightAmount);
 for(const auto& L:Lights)if(auto* Light=L.Component.Get())
  Light->SetIntensity(L.Policy.Intensity*(L.Policy.bEnabled?FMath::Lerp(L.Policy.DayMultiplier,1.f,NightAmount):0.f));
}

void UWorldLightingSubsystem::RefreshLighting()
{
 Lights.RemoveAll([](const FLocalLight& L){return !L.Component.IsValid();});
 TArray<FVector,TInlineAllocator<4>> Eyes;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* PC=It->Get();PC && PC->IsLocalController())
  {FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);if(!Eye.ContainsNaN())Eyes.Add(Eye);}
 struct FCandidate {int32 Index,Priority;double DistanceSquared;};
 TArray<FCandidate> Candidates;Candidates.Reserve(Lights.Num());
 for(int32 I=0;I<Lights.Num();++I)
 {
  auto& L=Lights[I];auto* Light=L.Component.Get();double Distance=DBL_MAX;
  for(const FVector& Eye:Eyes)Distance=FMath::Min(Distance,FVector::DistSquared(Eye,Light->GetComponentLocation()));
  const float Intensity=L.Policy.Intensity*(L.Policy.bEnabled?FMath::Lerp(L.Policy.DayMultiplier,1.f,NightAmount):0.f);
  // Keep photometric values independent of visibility: far fixtures retain their
  // emissive meshes while their expensive local lighting is removed from rendering.
  Light->SetIntensity(Intensity*(Distance<FMath::Square(double(L.Policy.ShadowDistance))?1.f:L.Policy.MidDistanceMultiplier));
  if(Intensity>.001f && Distance<FMath::Square(double(L.Policy.CullDistance)))Candidates.Add({I,L.Policy.Priority,Distance});
  else {Light->SetVisibility(false);Light->SetCastShadows(false);Light->SetCastVolumetricShadow(false);}
 }
 Candidates.StableSort([](const FCandidate& A,const FCandidate& B)
 {return A.Priority!=B.Priority?A.Priority>B.Priority:A.DistanceSquared<B.DistanceSquared;});
 ActiveLightCount=0;ShadowLightCount=0;
 for(const auto& C:Candidates)
 {
  const auto& L=Lights[C.Index];auto* Light=L.Component.Get();
  const bool Active=ActiveLightCount<DynamicLightBudget;
  const bool Shadow=Active && L.Policy.bCastShadows && ShadowLightCount<ShadowLightBudget
   && C.DistanceSquared<FMath::Square(double(L.Policy.ShadowDistance));
  Light->SetVisibility(Active);Light->SetCastShadows(Shadow);Light->SetCastVolumetricShadow(Shadow);
  Light->SetVolumetricScatteringIntensity(Active?L.Policy.VolumetricScattering:0);
  ActiveLightCount+=Active?1:0;ShadowLightCount+=Shadow?1:0;
 }
}
