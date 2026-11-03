#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WantedStatusWidget.generated.h"
class UTextBlock;
/** Minimal response feedback; never owns evidence, inputs or pause state. */
UCLASS()
class NEONCITY_API UWantedStatusWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void UpdateStatus(int32 Level,bool bVisibleSuspect);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 UPROPERTY() TObjectPtr<UTextBlock> Status;
};
