#include "Destruction/BreakableSurfaceData.h"
#include "UObject/StrongObjectPtr.h"
UBreakableSurfaceData::UBreakableSurfaceData() {}
bool UBreakableSurfaceData::IsValidProfile() const
{
 if(SurfaceID.IsNone() || uint8(Kind)>uint8(ENeonCitySurfaceKind::TrashCan)) return false;
 for(float V:{DamagedThreshold,BrokenThreshold,ShatterSpeed,MinimumImpactSpeed,FragmentBudget,MaximumSimulationDistance})
  if(!FMath::IsFinite(V) || V<0) return false;
 if(DamagedThreshold>1 || DamagedThreshold<.05f || BrokenThreshold<.01f || BrokenThreshold>=DamagedThreshold) return false;
 if(ShatterSpeed<0 || ShatterSpeed>6000 || MinimumImpactSpeed>ShatterSpeed) return false;
 return true;
}
EPhysicsPropState UBreakableSurfaceData::StateFor(float HealthFraction,float DamageAmount) const
{
 if(!FMath::IsFinite(HealthFraction) || !FMath::IsFinite(DamageAmount)) return EPhysicsPropState::Normal;
 if(HealthFraction<=BrokenThreshold) return EPhysicsPropState::Broken;
 return HealthFraction<=DamagedThreshold?EPhysicsPropState::Damaged:EPhysicsPropState::Normal;
}
ENeonCityGlassState UBreakableSurfaceData::GlassStateFor(EPhysicsPropState State) const
{
 if(!bGlass) return ENeonCityGlassState::Normal;
 return State==EPhysicsPropState::Broken?ENeonCityGlassState::Broken:State==EPhysicsPropState::Damaged?ENeonCityGlassState::Cracked:ENeonCityGlassState::Normal;
}
namespace NeonCitySurfaces
{
namespace
{
 TMap<ENeonCitySurfaceKind,TStrongObjectPtr<UBreakableSurfaceData>>& Registry()
 { static TMap<ENeonCitySurfaceKind,TStrongObjectPtr<UBreakableSurfaceData>> Map; return Map; }
 UBreakableSurfaceData* Build(ENeonCitySurfaceKind Kind)
 {
  auto* Data=NewObject<UBreakableSurfaceData>(GetTransientPackage(),FName(*FString::Printf(TEXT("NeonCity.Surface.%d"),int32(Kind))));
  Data->Kind=Kind;
  const TCHAR* Names[]={TEXT("Concrete"),TEXT("Glass"),TEXT("StorefrontGlass"),TEXT("Door"),TEXT("Sign"),TEXT("StreetLight"),TEXT("Billboard"),TEXT("Barrier"),TEXT("Fence"),TEXT("TrashCan")};
  Data->SurfaceID=FName(*FString::Printf(TEXT("NeonCity.Surface.%s"),Names[FMath::Clamp(int32(Kind),0,9)]));
  // Only glass disappears when broken; every released prop keeps its geometry and collision story.
  Data->bHiddenWhenBroken=Kind==ENeonCitySurfaceKind::Glass || Kind==ENeonCitySurfaceKind::StorefrontGlass;
  switch(Kind)
  {
  case ENeonCitySurfaceKind::Glass:
   Data->bGlass=true; Data->DamagedThreshold=.85f; Data->BrokenThreshold=.5f; Data->ShatterSpeed=900;
   Data->MinimumImpactSpeed=250; Data->FragmentBudget=1.5f; Data->MaximumSimulationDistance=9000; break;
  case ENeonCitySurfaceKind::StorefrontGlass:
   Data->bGlass=true; Data->DamagedThreshold=.8f; Data->BrokenThreshold=.45f; Data->ShatterSpeed=1000;
   Data->MinimumImpactSpeed=250; Data->FragmentBudget=2; Data->MaximumSimulationDistance=8000; break;
  case ENeonCitySurfaceKind::Door:
   Data->DamagedThreshold=.6f; Data->BrokenThreshold=.25f; Data->ShatterSpeed=2200; Data->FragmentBudget=.5f; break;
  case ENeonCitySurfaceKind::Sign:
   Data->DamagedThreshold=.6f; Data->BrokenThreshold=.3f; Data->ShatterSpeed=1800; Data->FragmentBudget=.5f; break;
  case ENeonCitySurfaceKind::StreetLight:
   Data->DamagedThreshold=.7f; Data->BrokenThreshold=.4f; Data->ShatterSpeed=2000; Data->FragmentBudget=1; break;
  case ENeonCitySurfaceKind::Billboard:
   Data->DamagedThreshold=.7f; Data->BrokenThreshold=.35f; Data->ShatterSpeed=2200; Data->FragmentBudget=1.5f; break;
  case ENeonCitySurfaceKind::Barrier:
   Data->DamagedThreshold=.6f; Data->BrokenThreshold=.3f; Data->ShatterSpeed=1400; Data->FragmentBudget=1; break;
  case ENeonCitySurfaceKind::Fence:
   Data->DamagedThreshold=.65f; Data->BrokenThreshold=.35f; Data->ShatterSpeed=1500; Data->FragmentBudget=1; break;
  case ENeonCitySurfaceKind::TrashCan:
   Data->DamagedThreshold=.5f; Data->BrokenThreshold=.25f; Data->ShatterSpeed=1200; Data->FragmentBudget=.5f; break;
  default:
   Data->DamagedThreshold=.6f; Data->BrokenThreshold=.25f; Data->ShatterSpeed=2400; Data->FragmentBudget=2;
   Data->MaximumSimulationDistance=5000; break;
  }
  return Data;
 }
}
TArray<ENeonCitySurfaceKind> AllKinds()
{ return {ENeonCitySurfaceKind::Concrete,ENeonCitySurfaceKind::Glass,ENeonCitySurfaceKind::StorefrontGlass,ENeonCitySurfaceKind::Door,ENeonCitySurfaceKind::Sign,ENeonCitySurfaceKind::StreetLight,ENeonCitySurfaceKind::Billboard,ENeonCitySurfaceKind::Barrier,ENeonCitySurfaceKind::Fence,ENeonCitySurfaceKind::TrashCan}; }
const UBreakableSurfaceData& Profile(ENeonCitySurfaceKind Kind)
{
 auto& Map=Registry();
 if(TStrongObjectPtr<UBreakableSurfaceData>* Found=Map.Find(Kind); Found && Found->IsValid()) return *Found->Get();
 TStrongObjectPtr<UBreakableSurfaceData> Built(Build(Kind)); Map.Add(Kind,Built);
 return *Map[Kind].Get();
}
}