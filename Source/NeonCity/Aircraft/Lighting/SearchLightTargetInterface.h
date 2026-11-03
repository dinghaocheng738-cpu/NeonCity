#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SearchLightTargetInterface.generated.h"
/** Future NPC/search consumers opt in; illumination is not police evidence by itself. */
UINTERFACE(BlueprintType)
class NEONCITY_API USearchLightTargetInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API ISearchLightTargetInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) void OnSearchLightExposure(AActor* Source,float Strength);
};
