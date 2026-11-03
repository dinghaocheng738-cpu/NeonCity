#include "VehicleCombat/LockOnSystem.h"
void ULockOnSystem::Reset() { Target.Reset(); Elapsed=0; State=EVehicleLockState::Searching; }
void ULockOnSystem::Update(const FVehicleTargetInfo& Info,float Delta)
{
 if(!Info.Actor.IsValid() || !Info.Visible)
 { const bool HadTarget=Target.IsValid() || State==EVehicleLockState::Locked || State==EVehicleLockState::Locking; Target.Reset(); Elapsed=0; if(HadTarget) State=EVehicleLockState::Lost; return; }
 if(Target!=Info.Actor) { Target=Info.Actor; Elapsed=0; State=EVehicleLockState::Locking; return; }
 if(!FMath::IsFinite(Delta) || Delta<=0) return;
 Elapsed+=FMath::Min(Delta,.2f); State=Elapsed>=FMath::Max(.1f,LockTime)?EVehicleLockState::Locked:EVehicleLockState::Locking;
}
