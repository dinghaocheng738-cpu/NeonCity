#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OceanLiveReviewLibrary.generated.h"

/** Passive recording of an existing interactive PIE. Never drives the pawn. */
UCLASS()
class NEONCITYARTEDITOR_API UOceanLiveReviewLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|Review") static bool StartRecording(const FString& Label);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Review") static void StopRecording();
 /** Explicit finite game input for a reviewer whose desktop controller cannot hold keys.
  * Uses the normal player input path; never sets velocity, state or animation. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Review") static bool HoldGameKey(FName KeyName,float Seconds);
};
