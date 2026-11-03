#pragma once
#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"

/** Runtime-only CDLOD instance ownership. Distance-sort changes do not change
 * geometry; stable ISM IDs keep unchanged patches and their GPU history alive.
 * This state owns the whole component and must not be used with HISM/editor
 * instance manipulation, which can invalidate the engine's preliminary IDs. */
class FCoastalOceanInstances
{
public:
 struct FStatistics
 {
  uint64 Selections=0,IdenticalSelections=0,OrderOnlySelections=0,TopologyUpdates=0,Added=0,Removed=0;
 };
 void Apply(UInstancedStaticMeshComponent& Surface,const TArray<FVector4f>& Nodes,const FVector& OceanOriginCm);
 int32 Num()const{return Instances.Num();}
 const FStatistics& GetStatistics()const{return Statistics;}
 const FPrimitiveInstanceId* Find(const FVector4f& Node)const{return Instances.Find(Node);}
private:
 TMap<FVector4f,FPrimitiveInstanceId> Instances;
 TArray<FVector4f> PreviousSelection;
 TSet<FVector4f> Selected;
 FStatistics Statistics;
};
