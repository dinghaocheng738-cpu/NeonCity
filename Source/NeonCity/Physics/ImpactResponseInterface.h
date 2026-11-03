#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Physics/PhysicalImpactEvent.h"
#include "ImpactResponseInterface.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API UImpactResponseInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IImpactResponseInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) void ReceivePhysicalImpact(const FPhysicalImpactEvent& Event);
};
