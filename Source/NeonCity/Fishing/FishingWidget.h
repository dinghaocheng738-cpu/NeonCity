#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FishingWidget.generated.h"
class UTextBlock;
class UProgressBar;
class USizeBox;
class UCanvasPanelSlot;
UCLASS()
class NEONCITY_API UFishingWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void ShowState(const FText& Text,bool Fighting,float Tension,float Progress,bool Charging=false,float Power=0);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 UPROPERTY() TObjectPtr<UTextBlock> Body;
 UPROPERTY() TObjectPtr<UTextBlock> TensionLabel;
 UPROPERTY() TObjectPtr<UTextBlock> ProgressLabel;
 UPROPERTY() TObjectPtr<USizeBox> TensionMeter;
 UPROPERTY() TObjectPtr<UCanvasPanelSlot> TensionCursor;
 UPROPERTY() TObjectPtr<UProgressBar> ProgressBar;
 UPROPERTY() TObjectPtr<USizeBox> CastMeter;
 UPROPERTY() TObjectPtr<UCanvasPanelSlot> CastCursor;
};
