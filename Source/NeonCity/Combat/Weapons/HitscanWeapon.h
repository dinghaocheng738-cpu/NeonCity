#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/WeaponBase.h"
#include "HitscanWeapon.generated.h"
UCLASS(Abstract)
class NEONCITY_API AHitscanWeapon : public AWeaponBase
{
	GENERATED_BODY()
protected:
	virtual bool FireShot(const FCombatAttackRequest& Request) override;
};
