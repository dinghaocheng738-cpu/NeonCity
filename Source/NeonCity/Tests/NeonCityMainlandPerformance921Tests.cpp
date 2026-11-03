#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Water/CoastalOceanInstances.h"
#include "Water/CoastalOceanSurface.h"
#include "NeonCityOceanCDLOD.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/Marine/NeonCityReferenceGrassInstances.h"
#include "Algo/Reverse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandOceanInstances921,
 "NeonCity.Phase205c.MainlandC921.OceanIncrementalInstances",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandOceanInstances921::RunTest(const FString&)
{
 // Real UE ISM IDs/custom-data arrays, driven by the production CDLOD selector.
 // No copied selector algorithm and no rendered-performance claim from this unit.
 TStrongObjectPtr<UInstancedStaticMeshComponent> Surface(NewObject<UInstancedStaticMeshComponent>());
 Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
 if(!TestNotNull(TEXT("Native ISM fixture mesh"),Surface->GetStaticMesh().Get()))return false;
 Surface->SetMobility(EComponentMobility::Movable);Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Surface->SetCanEverAffectNavigation(false);Surface->SetNumCustomDataFloats(4);
 FCoastalOceanInstances State;FNeonCityOceanCDLOD Selector;
 const FVector OceanOriginCm=NeonCityCoastalLayout::IslandOrigin;
 auto Verify=[&](const TArray<FVector4f>& Nodes)->bool
 {
  TSet<FVector4f> Expected;for(const auto& Node:Nodes)Expected.Add(Node);
  if(!TestEqual(TEXT("Selector output contains no duplicate world patches"),Expected.Num(),Nodes.Num()) ||
     !TestEqual(TEXT("Actual native ISM count exactly matches selected ocean coverage"),Surface->GetInstanceCount(),Nodes.Num()) ||
     !TestEqual(TEXT("All selected nodes have live owned IDs"),State.Num(),Nodes.Num()))return false;
  for(const auto& Node:Nodes)
  {
   const auto* Id=State.Find(Node);
   if(!Id || !Surface->IsValidId(*Id)){AddError(TEXT("A retained ocean node lost its native stable instance ID"));return false;}
   const int32 Index=Surface->GetInstanceIndexForId(*Id);FTransform Actual;
   const FTransform ExpectedTransform(FQuat::Identity,OceanOriginCm+FVector(Node.X,-Node.Y,0)*100,FVector(Node.Z,Node.Z,1));
   if(!Surface->GetInstanceTransform(Index,Actual,true) || !Actual.Equals(ExpectedTransform,.01))
   {AddError(TEXT("Ocean patch world transform changed relative to the original adapter"));return false;}
   for(int32 J=0;J<4;++J)if(!Surface->PerInstanceSMCustomData.IsValidIndex(Index*4+J) ||
      Surface->PerInstanceSMCustomData[Index*4+J]!=Node[J])
   {AddError(TEXT("Native instance custom-data/LOD values are not the selected patch values"));return false;}
  }
  return true;
 };
 auto ApplySelection=[&](const FVector3d& Camera)->bool
 {
  TStaticArray<FVector4d,6> Planes;
  Planes[0]=FVector4d(1,0,0,40000-Camera.X);Planes[1]=FVector4d(-1,0,0,40000+Camera.X);
  Planes[2]=FVector4d(0,0,1,40000-Camera.Z);Planes[3]=FVector4d(0,0,-1,40000+Camera.Z);
  Planes[4]=FVector4d(0,1,0,30000);Planes[5]=FVector4d(0,-1,0,30000);
  if(!Selector.Select(Camera,Planes)){AddError(TEXT("Production CDLOD exceeded its bounded node capacity"));return false;}
  const auto& Nodes=Selector.GetNodes();
  TMap<FVector4f,FPrimitiveInstanceId> Before;
  for(const auto& Node:Nodes)if(const auto* Id=State.Find(Node))Before.Add(Node,*Id);
  State.Apply(*Surface,Nodes,OceanOriginCm);
  if(!Verify(Nodes))return false;
  for(const auto& Pair:Before)if(!(*State.Find(Pair.Key)==Pair.Value))
  {AddError(TEXT("Topology update replaced an unchanged patch's stable native ID"));return false;}
  // Sorting is a scheduling detail, so reversing the same complete selection
  // must retain every real UE ID and emit zero native add/remove operations.
  const auto BeforeStats=State.GetStatistics();
  TArray<FVector4f> Reversed=Nodes;Algo::Reverse(Reversed);
  State.Apply(*Surface,Reversed,OceanOriginCm);
  if(!Verify(Nodes))return false;
  TestEqual(TEXT("Pure node order change adds no native instances"),State.GetStatistics().Added,BeforeStats.Added);
  TestEqual(TEXT("Pure node order change removes no native instances"),State.GetStatistics().Removed,BeforeStats.Removed);
  for(const auto& Node:Nodes)Before.Add(Node,*State.Find(Node));
  State.Apply(*Surface,Nodes,OceanOriginCm);
  for(const auto& Pair:Before)if(!(*State.Find(Pair.Key)==Pair.Value))
  {AddError(TEXT("Pure node reordering replaced an unchanged patch ID"));return false;}
  return true;
 };
 for(int32 I=0;I<120;++I)
 {
  const FVector3d Camera(-570.+I*1.1,2.7715+FMath::Max(0,I-40)*1.25,-1676.+FMath::Sin(I*.13)*35);
  if(!ApplySelection(Camera))return false;
 }
 for(const FVector3d& Camera:{FVector3d(16383.9,30,-16384.1),FVector3d(16384.1,30,-16383.9),FVector3d(-22000,300,18000),FVector3d(0,0,0)})
  if(!ApplySelection(Camera))return false;
 TArray<FVector4f> Empty;State.Apply(*Surface,Empty,OceanOriginCm);
 if(!Verify(Empty) || !ApplySelection(FVector3d(-570,2.7715,-1676)))return false;
 TestTrue(TEXT("Exercise genuine topology additions and removals"),State.GetStatistics().Added>500 && State.GetStatistics().Removed>500);
 TestTrue(TEXT("Exercise no-op sort-only updates"),State.GetStatistics().OrderOnlySelections>=200);
 AddInfo(FString::Printf(TEXT("Real ISM/CDLOD equivalence: 125 camera selections plus empty/refill, %llu added and %llu removed patches, %llu pure reorder selections; no visual quality/FFT/query change."),
  State.GetStatistics().Added,State.GetStatistics().Removed,State.GetStatistics().OrderOnlySelections));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandGrassInstances921,
 "NeonCity.Phase205c.MainlandC921.GrassIncrementalInstances",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandGrassInstances921::RunTest(const FString&)
{
 TestFalse(TEXT("Existing island/default grass data keeps its validated legacy adapter"),GetDefault<UNeonCityReferenceGrassData>()->bIncrementalInstances);
 TArray<FNeonCityReferenceGrassCell> Cells;
 for(int32 I=0;I<700;++I)
 {
  FNeonCityReferenceGrassCell Cell;Cell.Index=28000+I;
  Cell.MinimumHeightM=-1.3f+I*.011f;Cell.MaximumHeightM=Cell.MinimumHeightM+.2f+(I%11)*.07f;Cells.Add(Cell);
 }
 const int32 Budgets[]={48,160,420};
 for(int32 LOD=0;LOD<3;++LOD)
 {
  TStrongObjectPtr<UInstancedStaticMeshComponent> Patch(NewObject<UInstancedStaticMeshComponent>());
  Patch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
  if(!TestNotNull(TEXT("Native grass ISM fixture mesh"),Patch->GetStaticMesh().Get()))return false;
  Patch->SetMobility(EComponentMobility::Movable);Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Patch->SetCanEverAffectNavigation(false);Patch->SetNumCustomDataFloats(4);
  FNeonCityReferenceGrassInstances State;
  for(int32 Step=0;Step<25;++Step)
  {
   TArray<int32> Selected;
   if(Step!=12)for(int32 I=Step*7;I<Step*7+Budgets[LOD];++I)Selected.Add(I);
   TMap<int32,FPrimitiveInstanceId> Retained;
   for(int32 Index:Selected)if(const auto* Id=State.Find(Index))Retained.Add(Index,*Id);
   // Same key set in a different source selection order must not replace IDs.
   if(Step%2)Algo::Reverse(Selected);
   State.Apply(*Patch,Selected,Cells);
   if(!TestEqual(TEXT("Native grass patch exactly matches its selected LOD cell count"),Patch->GetInstanceCount(),Selected.Num()) ||
      !TestEqual(TEXT("Grass state has exactly one ID per selected cell"),State.Num(),Selected.Num()))return false;
   for(int32 Index:Selected)
   {
    const auto* Id=State.Find(Index);const auto& Cell=Cells[Index];
    if(!Id || !Patch->IsValidId(*Id)){AddError(TEXT("Selected grass cell has no valid native instance"));return false;}
    const auto* Before=Retained.Find(Index);
    if(Before && !(*Id==*Before)){AddError(TEXT("An unchanged grass cell lost its native ID/GPU history"));return false;}
    const int32 Slot=Patch->GetInstanceIndexForId(*Id);
    const float X=-1024+(Cell.Index%256)*8,Z=-1024+(Cell.Index/256)*8;
    const FTransform Expected(FQuat::Identity,FVector(X*100,-Z*100,(Cell.MinimumHeightM+Cell.MaximumHeightM)*50));
    FTransform Actual;
    if(!Patch->GetInstanceTransform(Slot,Actual,false) || !Actual.Equals(Expected,.01))
    {AddError(TEXT("Grass root/packed-cell midpoint height changed from the original native adapter"));return false;}
    const float Values[]={X,Z,0,0};
    for(int32 J=0;J<4;++J)if(!Patch->PerInstanceSMCustomData.IsValidIndex(Slot*4+J) || Patch->PerInstanceSMCustomData[Slot*4+J]!=Values[J])
    {AddError(TEXT("Grass source cell coordinates/wind data changed or were assigned to the wrong compacted slot"));return false;}
   }
  }
 }
 AddInfo(TEXT("All three real ISM grass LODs retain unchanged IDs and exact original local root/height/customdata across25 selections each, including reverse order, additions/removals, empty sets and refill. Frustum/LOD selection and island default path are unchanged."));
 return true;
}
#endif
