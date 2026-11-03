#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoastalOceanLibrary.generated.h"
class UStaticMesh;
class UCoastalWaveData;
class UTexture2D;
class UTexture2DArray;
/** Native asset bridge for Tidewater CDLOD and game-world bathymetry. */
UCLASS()
class NEONCITYARTEDITOR_API UCoastalOceanLibrary:public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UStaticMesh* CreateOceanGrid(const FString& PackagePath);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UTexture2D* BakeBathymetry(UCoastalWaveData* Field,const FString& PackagePath);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UTexture2DArray* CreateOceanPlaceholder(const FString& PackagePath);
 /** Re-trace the field's depth in place (CPU array and its depth texture) for the grid cells inside a
  * world rectangle (cm), against the editor world's native ground as BakeBathymetry does. Every cell
  * outside it - including the reviewed marina / harbour patches - keeps its bytes. Used after the
  * offshore island moved. Returns the number of cells whose depth changed, or -1. Does not save. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static int32 RetraceBathymetryRegion(UCoastalWaveData* Field,FVector2D RegionMinCm,FVector2D RegionMaxCm);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UTexture2D* ImportOceanFloatField(const FString& PackagePath,const FString& RawFile,int32 Resolution,int32 Channels,int32 Height=0);
 /** Only two exact C923 Bathymetry19 owned duplicates. Apply the frozen288
  * native-lattice half-depth replacements, preserving every other source byte
  * and all original field/profile/island values. Does not save or bind a map. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static FString PatchC923MarinaBathymetry(UCoastalWaveData* Candidate,UTexture2D* CandidateTexture,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits);
 /** Read-only native source/CPU verification of the exact Bathymetry19 patch. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static FString ValidateC923MarinaBathymetry(UCoastalWaveData* Candidate,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits);
 /** Exact owned Harbor26 duplicates of the already validated Marina19 field.
  * Applies only the reviewed105 half-depth replacements; preserves all other
  * source bytes, policies, profiles and island references. No save/map binding. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static FString PatchC923HarborBathymetry(UCoastalWaveData* Candidate,UTexture2D* CandidateTexture,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits);
 /** Read-only native source/CPU verification of the exact Harbor26 patch. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static FString ValidateC923HarborBathymetry(UCoastalWaveData* Candidate,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits);
};
