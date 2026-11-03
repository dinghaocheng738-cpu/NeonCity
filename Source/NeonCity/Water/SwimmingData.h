#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SwimmingData.generated.h"
UCLASS(BlueprintType)
class NEONCITY_API USwimmingData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float SwimSpeed=280;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float UnderwaterSpeed=220;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Acceleration=700;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float WaterDrag=1.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AscendSpeed=160;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DescendSpeed=140;
 bool IsValidProfile() const;
};
