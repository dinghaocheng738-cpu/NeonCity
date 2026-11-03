#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HillsideLandscapeLibrary.generated.h"
class ALandscape;
class UMaterialInterface;

/** Editor-only creation of the reference hillside Landscape from a heightfile
 *  produced by Tools/Art/Phase205cUpgrade/HillsideLandscape (raw little-endian
 *  uint16, row-major, SizeX*SizeY samples, 32768 = actor Z). */
UCLASS()
class NEONCITYARTEDITOR_API UHillsideLandscapeLibrary:public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Spawns a new Landscape in the editor world's persistent level.
  *  SizeX/SizeY are vertex counts and must equal ComponentsX/Y*SectionQuads*Subsections+1. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|HillsideLandscape")
 static ALandscape* CreateLandscapeFromHeightfile(const FString& HeightfilePath,int32 SizeX,int32 SizeY,int32 SubsectionSizeQuads,int32 NumSubsections,
  FVector LocationCm,FVector ScaleCm,UMaterialInterface* Material,const FString& Label);

 /** Replaces all heights of an existing landscape created by the function above. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|HillsideLandscape")
 static bool ReplaceLandscapeHeights(ALandscape* Landscape,const FString& HeightfilePath,int32 SizeX,int32 SizeY);
};
