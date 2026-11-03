#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VehicleStatusWidget.generated.h"
class UTextBlock;
UCLASS()
class NEONCITY_API UVehicleStatusWidget : public UUserWidget
{
 GENERATED_BODY()
public: void UpdateStatus(const FText& Text,bool bDamage);
protected: virtual TSharedRef<SWidget> RebuildWidget() override;
private: UPROPERTY() TObjectPtr<UTextBlock> Status;
};
