#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferenceBoatLibrary.generated.h"

class UMaterialExpression;

/** Bounded editor authoring helpers for the licensed reference boat. */
UCLASS()
class NEONCITYARTEDITOR_API UReferenceBoatLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Clear-coat material properties are hidden from the Python enum in UE 5.8. */
 UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
 static bool ConnectReferenceClearCoat(UMaterialExpression* Expression);
};
