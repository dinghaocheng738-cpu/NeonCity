#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "Engine/World.h"
ABulletProjectile* UVehicleProjectilePool::Acquire(TSubclassOf<ABulletProjectile> Type,const FTransform& Transform,AActor* Owner)
{
 Active.RemoveAll([](const auto& P){return !P.IsValid() || P->IsActorBeingDestroyed();});
 if(GetWorld()->GetNetMode()==NM_Client || Active.Num()>=MaxActive || !Type || !IsValid(Owner) || Transform.ContainsNaN()) return nullptr;
 FActorSpawnParameters Params; Params.Owner=Owner; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* P=GetWorld()->SpawnActor<ABulletProjectile>(Type,Transform,Params); if(P) { P->SetLifeSpan(.5f); Active.Add(P); } return P;
}
void UVehicleProjectilePool::Release(ABulletProjectile* P) { if(IsValid(P) && P->GetWorld()==GetWorld()) P->Destroy(); }
int32 UVehicleProjectilePool::GetActiveCount() const
{ int32 Count=0; for(const auto& P:Active) if(P.IsValid() && !P->IsActorBeingDestroyed()) ++Count; return Count; }
void UVehicleProjectilePool::ResetForLoad() { for(const auto& P:Active) if(P.IsValid()) P->Destroy(); Active.Reset(); }
