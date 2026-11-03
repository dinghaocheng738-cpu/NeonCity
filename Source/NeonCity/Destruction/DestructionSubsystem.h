#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DestructionSubsystem.generated.h"
class UDestructibleStateComponent;
/** Destruction budget and transient-damage register. Distance decides fragment fidelity, never correctness. */
UCLASS()
class NEONCITY_API UDestructionSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 void Register(UDestructibleStateComponent* Component);
 void Unregister(UDestructibleStateComponent* Component);
 /** Fragment simulation slot. Refusal degrades to logic-only damage, it never blocks the damage itself. */
 bool RequestFragmentSlot(FVector Location,float BudgetCost=1);
 bool ShouldSimulateFragments(FVector Location) const;
 /** 1 near the viewer, 0 beyond the logic-only radius. */
 float GetDistanceLODFraction(FVector Location) const;
 int32 GetActiveFragmentCount() const { return ActiveFragments; }
 int32 GetRegisteredSurfaceCount() const;
 int32 GetRefusedFragmentCount() const { return RefusedFragments; }
 int32 GetRestoreCount() const { return RestoreCount; }
 /** Load restores every transient surface; permanent game data is untouched by this call. */
 int32 RestoreTransientDestruction();
 void ResetForLoad();
 static constexpr int32 MaximumActiveFragments=24;
 static constexpr int32 MaximumRegisteredSurfaces=512;
 static constexpr float FullSimulationDistance=4000;
 static constexpr float LogicOnlyDistance=12000;
 static constexpr float FragmentHoldSeconds=6;
private:
 /** Const because every read-only query still expires its own slots first. */
 void ExpireSlots() const;
 TArray<TWeakObjectPtr<UDestructibleStateComponent>> Surfaces;
 mutable TArray<float> SlotExpiry;
 mutable int32 ActiveFragments=0;
 int32 RefusedFragments=0,RestoreCount=0;
};