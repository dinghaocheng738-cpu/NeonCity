#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferenceVillageLibrary.generated.h"

class UTextureRenderTarget2D;
class ANeonCityReferenceVillage;

/** Offline native bake readback; never part of game runtime. */
UCLASS()
class NEONCITYARTEDITOR_API UReferenceVillageLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Writes untonemapped linear RGBA float32 pixels under Saved/VerticalSlice only. */
 UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
 static bool WriteReferenceBakePixels(UTextureRenderTarget2D* Target,const FString& Filename);
 /** Checks authored walkable supports against real native collision; no actor movement. */
 UFUNCTION(BlueprintCallable, Category="NeonCity|Art")
 static TArray<FString> CheckReferenceVillageSupports(ANeonCityReferenceVillage* Village);
};
