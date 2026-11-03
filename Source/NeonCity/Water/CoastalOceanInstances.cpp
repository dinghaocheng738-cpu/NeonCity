#include "Water/CoastalOceanInstances.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void FCoastalOceanInstances::Apply(UInstancedStaticMeshComponent& Surface,const TArray<FVector4f>& Nodes,const FVector& OceanOriginCm)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(CoastalOceanApplyInstances);
 ++Statistics.Selections;
 if(Nodes==PreviousSelection){++Statistics.IdenticalSelections;return;}
 Selected.Reset();Selected.Reserve(Nodes.Num());
 for(const FVector4f& Node:Nodes)Selected.Add(Node);
 TArray<FPrimitiveInstanceId,TInlineAllocator<32>> Removed;
 for(auto It=Instances.CreateIterator();It;++It)if(!Selected.Contains(It.Key()))
 {Removed.Add(It.Value());It.RemoveCurrent();}
 TArray<FVector4f,TInlineAllocator<32>> AddedNodes;
 TArray<FTransform,TInlineAllocator<32>> Transforms;
 for(const FVector4f& Node:Nodes)if(!Instances.Contains(Node))
 {
  AddedNodes.Add(Node);
  // Exact original source Y-up -> UE Z-up transform and scale.
  Transforms.Emplace(FQuat::Identity,OceanOriginCm+FVector(Node.X,-Node.Y,0)*100,FVector(Node.Z,Node.Z,1));
 }
 if(Removed.IsEmpty() && AddedNodes.IsEmpty())++Statistics.OrderOnlySelections;
 else
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(CoastalOceanIncrementalTopology);
  ++Statistics.TopologyUpdates;Statistics.Removed+=Removed.Num();Statistics.Added+=AddedNodes.Num();
  // By-ID removal handles the engine's swap compaction without invalidating
  // retained node identities. No ClearInstances/full render-state recreation.
  if(!Removed.IsEmpty())Surface.RemoveInstancesById(MakeArrayView(Removed),false);
  if(!AddedNodes.IsEmpty())
  {
   const auto Ids=Surface.AddInstancesById(MakeArrayView(Transforms),true,false);
   check(Ids.Num()==AddedNodes.Num());
   TArray<float,TInlineAllocator<128>> CustomData;CustomData.Reserve(AddedNodes.Num()*4);
   for(int32 I=0;I<AddedNodes.Num();++I)
   {
    const auto& Node=AddedNodes[I];Instances.Add(Node,Ids[I]);
    for(int32 J=0;J<4;++J)CustomData.Add(Node[J]);
   }
   Surface.SetCustomDataById(MakeArrayView(Ids),MakeArrayView(CustomData));
  }
 }
 PreviousSelection=Nodes;
}
