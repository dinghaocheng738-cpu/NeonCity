#include "Destruction/DestructionSubsystem.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Physics/DestructiblePropComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
void UDestructionSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDestructionSubsystem::Deinitialize()
{ Surfaces.Reset(); SlotExpiry.Reset(); ActiveFragments=0; Super::Deinitialize(); }
void UDestructionSubsystem::Register(UDestructibleStateComponent* Component)
{
 if(!IsValid(Component)) return;
 Surfaces.RemoveAll([](const auto& Entry){ return !Entry.IsValid(); });
 if(Surfaces.Contains(Component)) return;
 if(Surfaces.Num()>=MaximumRegisteredSurfaces) Surfaces.RemoveAt(0);
 Surfaces.Add(Component);
}
void UDestructionSubsystem::Unregister(UDestructibleStateComponent* Component)
{ Surfaces.RemoveAll([&](const auto& Entry){ return !Entry.IsValid() || Entry.Get()==Component; }); }
int32 UDestructionSubsystem::GetRegisteredSurfaceCount() const
{ int32 Count=0; for(const auto& Entry:Surfaces) if(Entry.IsValid()) ++Count; return Count; }
void UDestructionSubsystem::ExpireSlots() const
{
 const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
 SlotExpiry.RemoveAll([&](float Until){ return Until<=Now; });
 ActiveFragments=SlotExpiry.Num();
}
float UDestructionSubsystem::GetDistanceLODFraction(FVector Location) const
{
 if(Location.ContainsNaN() || !GetWorld()) return 0;
 const APlayerController* PC=GetWorld()->GetFirstPlayerController();
 const FVector Viewer=PC && PC->GetPawn()?PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
 const float Distance=FVector::Distance(Location,Viewer);
 if(Distance<=FullSimulationDistance) return 1;
 if(Distance>=LogicOnlyDistance) return 0;
 return FMath::Clamp(1-(Distance-FullSimulationDistance)/(LogicOnlyDistance-FullSimulationDistance),0.f,1.f);
}
bool UDestructionSubsystem::ShouldSimulateFragments(FVector Location) const
{
 ExpireSlots();
 return GetDistanceLODFraction(Location)>0;
}
bool UDestructionSubsystem::RequestFragmentSlot(FVector Location,float BudgetCost)
{
 ExpireSlots();
 if(Location.ContainsNaN() || !FMath::IsFinite(BudgetCost) || BudgetCost<=0 || GetDistanceLODFraction(Location)<=0
  || ActiveFragments+1>MaximumActiveFragments)
 { ++RefusedFragments; return false; }
 const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
 SlotExpiry.Add(Now+FragmentHoldSeconds*FMath::Clamp(BudgetCost,.5f,4.f));
 ActiveFragments=SlotExpiry.Num();
 return true;
}
int32 UDestructionSubsystem::RestoreTransientDestruction()
{
 int32 Restored=0;
 Surfaces.RemoveAll([](const auto& Entry){ return !Entry.IsValid(); });
 for(const auto& Entry:Surfaces)
 {
  UDestructibleStateComponent* Component=Entry.Get();
  if(!Component || !IsValid(Component->GetOwner()) || Component->IsPersistent()) continue;
  if(auto* Prop=Component->GetOwner()->FindComponentByClass<UDestructiblePropComponent>()) Prop->ResetForPool();
  if(Component->ResetDestruction()) ++Restored;
 }
 ++RestoreCount;
 return Restored;
}
void UDestructionSubsystem::ResetForLoad()
{ ExpireSlots(); SlotExpiry.Reset(); ActiveFragments=0; RefusedFragments=0; }