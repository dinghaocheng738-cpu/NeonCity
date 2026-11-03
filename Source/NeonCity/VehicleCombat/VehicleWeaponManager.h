#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleCombat/VehicleCombatSaveTypes.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "VehicleWeaponManager.generated.h"
class UWeaponHardpointComponent;
class UCombatComponent;
class AVehicleMountedWeapon;
USTRUCT()
struct FVehicleWeaponMount
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UWeaponHardpointComponent> Point;
 UPROPERTY() TObjectPtr<UCombatComponent> Combat;
 UPROPERTY() FName DefaultWeaponID;
};
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UVehicleWeaponManager : public UActorComponent
{
 GENERATED_BODY()
public:
 UVehicleWeaponManager() { PrimaryComponentTick.bCanEverTick=false; }
 UWeaponHardpointComponent* AddHardpoint(FName ID,const FTransform& Local,const TArray<EVehicleWeaponType>& Allowed,bool FixedForward=false);
 bool Equip(FName Hardpoint,FName WeaponID);
 bool Select(FName Hardpoint);
 void NextWeapon();
 AVehicleMountedWeapon* GetWeapon() const;
 UWeaponHardpointComponent* GetPoint() const;
 FName GetVehicleID() const;
 TArray<FVehicleWeaponSaveData> BuildSnapshot(bool Defaults=false) const;
 void ApplySnapshot(const TArray<FVehicleWeaponSaveData>& Records);
 bool IsValidRecord(const FVehicleWeaponSaveData& Record) const;
 static TSubclassOf<AVehicleMountedWeapon> ResolveWeapon(FName ID);
 static const UVehicleWeaponData* Profile(FName ID);
 UCombatComponent* GetCombat() const;
 const TArray<FVehicleWeaponMount>& GetMounts() const { return Mounts; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<FVehicleWeaponMount> Mounts;
 int32 Selected=0;
};
UCLASS()
class NEONCITY_API UVehicleWeaponRegistry : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool Register(UVehicleWeaponManager* Manager);
 TArray<FVehicleWeaponSaveData> BuildSnapshot(bool Defaults=false) const;
 void Normalize(TArray<FVehicleWeaponSaveData>& Records,TArray<FString>& Warnings) const;
 void ApplySnapshot(const TArray<FVehicleWeaponSaveData>& Records);
private:
 TMap<FName,TWeakObjectPtr<UVehicleWeaponManager>> Managers;
};
