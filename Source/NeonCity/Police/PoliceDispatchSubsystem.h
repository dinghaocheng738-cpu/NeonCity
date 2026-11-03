#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Police/SuspectDescriptor.h"
#include "PoliceDispatchSubsystem.generated.h"
class APoliceResponseDirector;
class UPoliceResponseData;
class UPoliceUnitManager;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPoliceAlert,const FSuspectDescriptor&,Description);
UCLASS()
class NEONCITY_API UPoliceDispatchSubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void Tick(float Delta) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(NeonCityDispatch,STATGROUP_Tickables); }
 virtual bool IsTickable() const override;
 void RegisterStation(APoliceResponseDirector* Station);
 void UnregisterStation(APoliceResponseDirector* Station);
 void ResetForLoad();
 int32 GetActiveOfficers() const;
 int32 GetActiveVehicles() const;
 UPoliceUnitManager* GetUnits() const { return Units; }
 const UPoliceResponseData* GetProfile() const { return Profile; }
 UPROPERTY(BlueprintAssignable) FPoliceAlert OnPoliceAlert;
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
 UPROPERTY() TObjectPtr<UPoliceResponseData> Profile;
 UPROPERTY() TObjectPtr<UPoliceUnitManager> Units;
 TWeakObjectPtr<APoliceResponseDirector> Station;
 float Accumulator=0,NextSpawn=0,NextRoadblock=0;
 int32 LastLevel=0,NextUnitSlot=1;
};
