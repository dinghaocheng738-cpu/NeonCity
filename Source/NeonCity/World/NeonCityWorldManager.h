#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/SystemInterface.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "NeonCityWorldManager.generated.h"
class APedestrianSpawnPoint;

UENUM(BlueprintType)
enum class ENeonCityWorldState : uint8 { Initializing, Active, ShuttingDown };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityRegionChanged, FName, PreviousRegion, FName, CurrentRegion);

/** World state and bounded ambient-marker scheduling; NPC controllers own behaviour. */
UCLASS()
class NEONCITY_API UNeonCityWorldManager : public UWorldSubsystem, public ISystemInterface
{
	GENERATED_BODY()
public:
 FNeonCityWorldSaveData BuildPersistenceSnapshot(bool Defaults=false) const;
 bool CanApplyPersistenceSnapshot(const FNeonCityWorldSaveData& Snapshot) const;
 void ApplyPersistenceSnapshot(const FNeonCityWorldSaveData& Snapshot);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool IsSystemReady_Implementation() const override;
	void RegisterPedestrian(APedestrianSpawnPoint* Marker);

	UFUNCTION(BlueprintPure, Category="NeonCity|World")
	FName GetCurrentRegion() const { return CurrentRegion; }

	UFUNCTION(BlueprintCallable, Category="NeonCity|World")
	void SetCurrentRegion(FName Region);

	UFUNCTION(BlueprintPure, Category="NeonCity|World")
	ENeonCityWorldState GetWorldState() const { return WorldState; }

	UPROPERTY(BlueprintAssignable, Category="NeonCity|World")
	FNeonCityRegionChanged OnRegionChanged;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void ReconcilePedestrians();
	TArray<TWeakObjectPtr<APedestrianSpawnPoint>> PedestrianMarkers;
	FTimerHandle PopulationTimer;
	bool bSystemReady = false;
	FName CurrentRegion = NAME_None;
	ENeonCityWorldState WorldState = ENeonCityWorldState::Initializing;
};
