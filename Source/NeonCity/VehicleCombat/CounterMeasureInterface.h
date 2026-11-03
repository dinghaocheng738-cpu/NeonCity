#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CounterMeasureInterface.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API UCounterMeasureInterface : public UInterface { GENERATED_BODY() };
/** Capability only. No flare/ECM emitter or automatic target spoofing exists in Phase 19. */
class NEONCITY_API ICounterMeasureInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool SupportsCounterMeasure(FName Type) const;
};
