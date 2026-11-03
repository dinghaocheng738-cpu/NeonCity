#include "World/Marine/NeonCityReferenceGrassInstances.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void FNeonCityReferenceGrassInstances::Apply(UInstancedStaticMeshComponent& Patch,const TArray<int32>& Selected,const TArray<FNeonCityReferenceGrassCell>& Cells)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(IslandGrassIncrementalCells);
 using namespace NeonCityReferenceGrassLayout;
 TSet<int32> Wanted;Wanted.Reserve(Selected.Num());for(int32 Index:Selected)Wanted.Add(Index);
 TArray<FPrimitiveInstanceId,TInlineAllocator<32>> Removed;
 for(auto It=Instances.CreateIterator();It;++It)if(!Wanted.Contains(It.Key()))
 {Removed.Add(It.Value());It.RemoveCurrent();}
 TArray<int32,TInlineAllocator<32>> AddedCells;TArray<FTransform,TInlineAllocator<32>> Transforms;
 TArray<float,TInlineAllocator<128>> CustomData;
 for(int32 Index:Selected)if(!Instances.Contains(Index))
 {
  const auto& Cell=Cells[Index];const float X=OriginM+(Cell.Index%Side)*CellM,Z=OriginM+(Cell.Index/Side)*CellM;
  AddedCells.Add(Index);Transforms.Emplace(FQuat::Identity,FVector(X*100,-Z*100,(Cell.MinimumHeightM+Cell.MaximumHeightM)*50));
  CustomData.Append({X,Z,0,0});
 }
 if(!Removed.IsEmpty())Patch.RemoveInstancesById(MakeArrayView(Removed),false);
 if(!AddedCells.IsEmpty())
 {
  const auto Ids=Patch.AddInstancesById(MakeArrayView(Transforms),false,false);
  check(Ids.Num()==AddedCells.Num());
  for(int32 I=0;I<AddedCells.Num();++I)Instances.Add(AddedCells[I],Ids[I]);
  Patch.SetCustomDataById(MakeArrayView(Ids),MakeArrayView(CustomData));
 }
}
