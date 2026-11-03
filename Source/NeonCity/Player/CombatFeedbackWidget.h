#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatFeedbackWidget.generated.h"
class UTextBlock;
UCLASS()
class NEONCITY_API UCombatFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()
public:
 void UpdateStatus(const FText& Text,bool Hit,bool Aim);
 /** Damage taken reads separately from "my shot hit": text plus the bearing the hit came from. */
 void UpdateDamage(const FText& Text,bool bActive);
 void UpdateVitals(const FText& Text,const FLinearColor& Color,bool bActive);
protected: virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Crosshair;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UTextBlock> Damage;
	UPROPERTY() TObjectPtr<UTextBlock> Vitals;
};
