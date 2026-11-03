#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityWorldRegion.generated.h"

/** Map-level metadata for this single-region prototype, not a streaming volume. */
UCLASS()
class NEONCITY_API ANeonCityWorldRegion : public AActor
{
	GENERATED_BODY()
public:
	ANeonCityWorldRegion();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NeonCity|World")
	FName RegionId = NAME_None;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
