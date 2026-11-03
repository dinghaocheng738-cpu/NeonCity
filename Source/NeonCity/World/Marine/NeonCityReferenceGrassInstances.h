#pragma once
#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
struct FNeonCityReferenceGrassCell;

namespace NeonCityReferenceGrassLayout
{
 constexpr int32 Side=256;
 constexpr float CellM=8,OriginM=-1024;
}

/** One runtime grass LOD patch. Keys are immutable GrassData cell indices;
 * unlike ocean patches, cell transforms include per-cell height and remain
 * local to the vegetation actor. It owns all instances in this one ISM. */
class FNeonCityReferenceGrassInstances
{
public:
 void Apply(UInstancedStaticMeshComponent& Patch,const TArray<int32>& Selected,const TArray<FNeonCityReferenceGrassCell>& Cells);
 void Reset(){Instances.Reset();}
 const FPrimitiveInstanceId* Find(int32 CellIndex)const{return Instances.Find(CellIndex);}
 int32 Num()const{return Instances.Num();}
private:
 TMap<int32,FPrimitiveInstanceId> Instances;
};
