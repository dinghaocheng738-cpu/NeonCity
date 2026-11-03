#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SystemInterface.generated.h"

/** Shared status contract. Engine subsystem callbacks own lifecycle, not callers. */
UINTERFACE(BlueprintType)
class NEONCITY_API USystemInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API ISystemInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|System")
	bool IsSystemReady() const;

	virtual bool IsSystemReady_Implementation() const;
};
