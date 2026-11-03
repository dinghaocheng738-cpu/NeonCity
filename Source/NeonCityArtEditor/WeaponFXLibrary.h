#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponFXLibrary.generated.h"
class UNiagaraSystem;
class UMaterialInterface;

/** Editor-time authoring of the vehicle-weapon Niagara effects (lightweight / stateless emitters, one burst each),
 * driven by Tools/Art/Phase205cUpgrade/WeaponFX/WeaponFX2_UE.py. Kinds:
 *  0 NS_NC_TrailPuff    a few smoke puffs left behind a rocket / missile motor (spawned along the flight path)
 *  1 NS_NC_LaunchSmoke  the launch / backblast cloud at the rail or tube
 *  2 NS_NC_Fireball     the detonation's fireball (additive, short)
 *  3 NS_NC_BlastSmoke   the dark smoke column rising after a detonation
 *  4 NS_NC_Sparks       hot fragments and sparks thrown out by a detonation (velocity-aligned, gravity) */
UCLASS()
class NEONCITYARTEDITOR_API UWeaponFXLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|WeaponFX") static UNiagaraSystem* CreateWeaponBurst(UMaterialInterface* Material,int32 Kind);
};
