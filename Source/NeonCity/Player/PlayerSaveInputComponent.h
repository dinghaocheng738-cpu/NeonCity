#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "PlayerSaveInputComponent.generated.h"
class UInputMappingContext;
class UInputAction;
class UInputComponent;
class UTextBlock;
UCLASS()
class NEONCITY_API UPlayerSaveNotice : public UUserWidget { GENERATED_BODY() };

/** Player-facing quick save/load; all validation and disk writes stay in SaveSubsystem. */
UCLASS()
class NEONCITY_API UPlayerSaveInputComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 void SetupInput(UInputComponent* Input);
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 void Save();void Load();void Notify(const FString& Message);void Hide();
 bool CanOperate(bool AllowGrillSave=false) const;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> SaveAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> LoadAction;
 UPROPERTY(Transient) TObjectPtr<UPlayerSaveNotice> Notice;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
 FTimerHandle HideTimer;
};
