#pragma once
#include "CoreMinimal.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "MissileProjectile.generated.h"
UCLASS()
class NEONCITY_API AMissileProjectile : public ARocketProjectile
{
 GENERATED_BODY()
public:
 AMissileProjectile();
 virtual bool HasGuidance() const override { return !bLost && Target.IsValid(); }
protected:
 virtual void Tick(float Delta) override;
private:
 bool bLost=false;
 float SightBlocked=0;          // s the missile has not seen a vehicle / person target
};
