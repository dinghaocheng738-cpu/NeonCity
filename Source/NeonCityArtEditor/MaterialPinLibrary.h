#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MaterialPinLibrary.generated.h"
class UMaterial;
class UMaterialExpression;

/** Editor-time material authoring for the pins Python cannot name: EMaterialProperty's CustomData0 / CustomData1
 * (Clear Coat and Clear Coat Roughness on a clear-coat material). Used by LandRebuild/ImportWaterway_WL.py. */
UCLASS()
class NEONCITYARTEDITOR_API UMaterialPinLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Connects an expression's first output to Clear Coat (Slot 0) or Clear Coat Roughness (Slot 1). */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Materials") static bool ConnectClearCoat(UMaterial* Material,UMaterialExpression* Expression,int32 Slot);
};
