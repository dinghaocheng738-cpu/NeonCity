#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Destruction/BreakableSurfaceData.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "DestructionTestArea.generated.h"
class UStaticMeshComponent;
class ANeonCityBaseVehicle;
class APoliceCharacter;
class ADestructibleStructure;
class ACharacter;
/** Opt-in Phase 20 proving ground: wrecks, blast targets, breakable surfaces and bystanders on one pad. */
UCLASS()
class NEONCITY_API ADestructionTestArea : public AActor
{
 GENERATED_BODY()
public:
 ADestructionTestArea();
 /** World-space centre between the two exploding wrecks; every blast test aims here. */
 FVector GetVehicleBlastCenter() const { return GetActorLocation()+FVector(-1150,-1200,60); }
 ANeonCityBaseVehicle* GetVehicle(int32 Index) const { return Vehicles.IsValidIndex(Index)?Vehicles[Index]:nullptr; }
 int32 GetVehicleCount() const { return Vehicles.Num(); }
 /** First fixture of a kind, in spawn order. */
 ANeonCityPhysicsProp* GetProp(ECityPhysicsProp Type,int32 Index=0) const;
 const TArray<TObjectPtr<ANeonCityPhysicsProp>>& GetProps() const { return Props; }
 ADestructibleStructure* GetStructure() const { return Structure; }
 ACharacter* GetPedestrian(int32 Index) const { return Pedestrians.IsValidIndex(Index)?Pedestrians[Index]:nullptr; }
 int32 GetPedestrianCount() const { return Pedestrians.Num(); }
 APoliceCharacter* GetOfficer() const { return Officer; }
 static constexpr float PadExtentX=8000;
 static constexpr float PadExtentY=6000;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 ANeonCityPhysicsProp* AddProp(ECityPhysicsProp Type,FVector Location,FRotator Rotation=FRotator::ZeroRotator);
 ANeonCityBaseVehicle* AddVehicle(FVector Location,const TCHAR* DataPath);
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Pad;
 UPROPERTY() TArray<TObjectPtr<ANeonCityPhysicsProp>> Props;
 UPROPERTY() TArray<TObjectPtr<ANeonCityBaseVehicle>> Vehicles;
 UPROPERTY() TArray<TObjectPtr<ACharacter>> Pedestrians;
 UPROPERTY() TObjectPtr<ADestructibleStructure> Structure;
 UPROPERTY() TObjectPtr<APoliceCharacter> Officer;
};
