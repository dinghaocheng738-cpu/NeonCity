#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "PhoneSubsystem.generated.h"
class APlayerController;
DECLARE_DELEGATE_RetVal_OneParam(bool,FNeonCityPhoneAction,APlayerController*);

/** Routes phone actions to their weak gameplay owner; contains no transport or economy logic. */
UCLASS()
class NEONCITY_API UPhoneSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
 UObject* GetAppObject(ENeonCityPhoneApp ID) const { const auto* App=Apps.Find(ID);return App?App->GetObject():nullptr; }
 FNeonCityPhoneSaveData BuildPersistenceSnapshot(bool Defaults=false) const;
 void NormalizePersistenceSnapshot(FNeonCityPhoneSaveData& Snapshot,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const FNeonCityPhoneSaveData& Snapshot);

	bool RegisterAction(FName Id, FNeonCityPhoneAction Handler);
	void UnregisterAction(FName Id, const UObject* Owner);
	bool ExecuteAction(FName Id, APlayerController* Player);
	bool HasAction(FName Id) const;
	void UnregisterApp(ENeonCityPhoneApp Id, const UObject* Owner);
	UFUNCTION(BlueprintCallable, Category="NeonCity|Phone") bool RegisterApp(TScriptInterface<IPhoneAppInterface> App);
	UFUNCTION(BlueprintCallable, Category="NeonCity|Phone") bool OpenApp(ENeonCityPhoneApp Id, APlayerController* Player);
private:
	TMap<FName,FNeonCityPhoneAction> Actions;
	UPROPERTY() TMap<ENeonCityPhoneApp,TScriptInterface<IPhoneAppInterface>> Apps;
};
