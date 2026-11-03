#pragma once
#include "CoreMinimal.h"
#include "Core/Subsystems/NeonCityGameInstanceSubsystem.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Property/PropertyData.h"
#include "NeonCitySaveSubsystem.generated.h"
class UNeonCitySaveGame;
class ANeonCityPlayerCharacter;
UENUM(BlueprintType)
enum class ENeonCitySaveDomain : uint8 { PlayerData, Vehicles, Properties, Missions };
UENUM(BlueprintType)
enum class ENeonCityAutosaveReason : uint8 { EnteredHome, TransportCompleted, SavePoint };
UENUM(BlueprintType)
enum class ENeonCitySaveResult : uint8 { None, Success, Deferred, NotReady, Busy, InvalidSlot, AlreadyExists, Missing, Corrupt, UnsupportedVersion, InvalidData, WrongWorld, IOError, UnsafeState };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCitySaveOperation,ENeonCitySaveResult,Result,const FString&,Detail);
/** Only disk boundary. Domains collect/normalize/apply their own value snapshots. */
UCLASS()
class NEONCITY_API UNeonCitySaveSubsystem : public UNeonCityGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Deinitialize() override;
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") bool IsPersistenceImplemented() const { return true; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") TArray<ENeonCitySaveDomain> GetPlannedDomains() const;
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") TArray<FNeonCityOwnedVehicle> BuildVehicleSnapshot();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") TArray<FNeonCityPropertyRecord> BuildPropertySnapshot();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool SaveGame();
 /** Missing slot creates design-default new game. Not-ready worlds use a bounded deferred load. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool LoadGame();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool DeleteSave();
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") bool DoesSaveExist() const;
 /** Refuses to overwrite existing generations. DeleteSave is the explicit reset boundary. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool CreateNewGameSave();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool RequestAutosave(ENeonCityAutosaveReason Reason);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Save") bool ConfigureSlot(const FString& Name,bool EnableAutosave=false);
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") FString GetSlotName() const { return SlotName; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") ENeonCitySaveResult GetLastResult() const { return LastResult; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") FString GetLastDetail() const { return LastDetail; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Save") bool IsOperationInProgress() const { return bBusy || PendingOperation!=0; }
 UPROPERTY(BlueprintAssignable) FNeonCitySaveOperation OnSaveOperation;
 void NotifyWorldReady(UWorld* World);
 /** Detached diagnostic model; never exposes mutable live registries. */
 UNeonCitySaveGame* ReadSaveForInspection();
#if WITH_DEV_AUTOMATION_TESTS
 // Fault injection restricted to active automation and NeonCity_Test_ slots.
 bool WriteTestFixture(UNeonCitySaveGame* Data,bool CorruptBytes=false);
#endif
private:
 ANeonCityPlayerCharacter* ReadyPlayer() const;
 UNeonCitySaveGame* BuildModel(bool Defaults);
 bool EnsureDefaults();
 bool PrepareLoad(UNeonCitySaveGame* Model,FTransform& PlayerDestination);
 bool ApplyModel(UNeonCitySaveGame* Model,const FTransform& Destination);
 UNeonCitySaveGame* ReadGeneration(const FString& Name,ENeonCitySaveResult& Result,FString& Detail) const;
 UNeonCitySaveGame* ReadBestGeneration();
 bool WriteGeneration(UNeonCitySaveGame* Model);
 bool Encode(UNeonCitySaveGame* Model,TArray<uint8>& Bytes) const;
 bool Finish(ENeonCitySaveResult Result,const FString& Detail);
 void DeferredLoad();
 void FlushAutosave();
 bool IsAutosaveSafe(bool AllowDoorTransition=false) const;
 FString SlotName=TEXT("NeonCity_Autosave_01");
 ENeonCitySaveResult LastResult=ENeonCitySaveResult::None;
 FString LastDetail;
 UPROPERTY() TObjectPtr<UNeonCitySaveGame> DesignDefaults;
 TWeakObjectPtr<UWorld> ActiveWorld;
 TArray<FString> RestoreWarnings;
 FTimerHandle ReadyTimer,AutosaveTimer;
 double ReadyDeadline=0,LastWriteTime=-1000;
 uint8 PendingOperation=0;
 bool bBusy=false,bAutosaveEnabled=false,bWriteProtected=false;
 /** The automatic load as the world opens: arrive at home (see NeonCityHomeArrival). */
 bool bSessionStartLoad=false;
};
