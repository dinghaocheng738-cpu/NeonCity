// Source selection/patch semantics adapted from Tidewater GrassField.js (MIT).
// Copyright (c) 2026 DRG Software Solutions LLC. See Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "ConvexVolume.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
 using namespace NeonCityReferenceGrassLayout;
 constexpr int32 MaximumCells[]={48,160,420};
 constexpr float NearM=18,MidM=46,FarM=88;
}

UNeonCityReferenceGrassComponent::UNeonCityReferenceGrassComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
}

bool UNeonCityReferenceGrassComponent::InitializeGrass(UNeonCityReferenceGrassData* InData)
{
 auto* Owner=GetOwner();
 if(!Owner || !Owner->GetRootComponent() || !Owner->GetActorRotation().IsNearlyZero()
  || !Owner->GetActorScale3D().Equals(FVector::OneVector) || !InData
  || InData->ReferenceCommit!=TEXT("4811ba48d795197de5621985f404e765c0b7c0ef")
  || InData->PatchMeshes.Num()!=3 || InData->Cells.IsEmpty()
  || !FMath::IsFinite(InData->MaximumPlantRadiusM) || InData->MaximumPlantRadiusM<=0
  || InData->MaximumPlantRadiusM>10)return false;
 TArray<UStaticMesh*> Meshes;TArray<int32> Lookup;Lookup.Init(INDEX_NONE,Side*Side);
 for(const auto& Ref:InData->PatchMeshes)
 {
  auto* Mesh=Ref.LoadSynchronous();if(!Mesh || !Mesh->GetMaterial(0))return false;
  if(!Meshes.IsEmpty() && Mesh->GetMaterial(0)!=Meshes[0]->GetMaterial(0))return false;
  Meshes.Add(Mesh);
 }
 for(int32 I=0;I<InData->Cells.Num();++I)
 {
  const auto& C=InData->Cells[I];
  if(!Lookup.IsValidIndex(C.Index) || Lookup[C.Index]!=INDEX_NONE
   || !FMath::IsFinite(C.MinimumHeightM) || !FMath::IsFinite(C.MaximumHeightM)
   || C.MinimumHeightM>C.MaximumHeightM)return false;
  Lookup[C.Index]=I;
 }
 auto* NewMaterial=UMaterialInstanceDynamic::Create(Meshes[0]->GetMaterial(0),this);
 if(!NewMaterial)return false;
 // Validation completes before existing components are replaced.
 for(auto Patch:Patches)if(IsValid(Patch))Patch->DestroyComponent();
 for(auto& State:CellInstances)State.Reset();
 Patches.Reset();for(auto& List:ActiveCells)List.Reset();bHasView=false;
 Data=InData;CellLookup=MoveTemp(Lookup);
 bUseIncrementalInstances=InData->bIncrementalInstances;
 Material=NewMaterial;
 for(auto* Mesh:Meshes)
 {
  auto* Patch=NewObject<UInstancedStaticMeshComponent>(Owner,NAME_None,RF_Transient);
  Owner->AddOwnedComponent(Patch);Patch->SetupAttachment(Owner->GetRootComponent());
  Patch->SetMobility(EComponentMobility::Movable);Patch->SetStaticMesh(Mesh);Patch->SetMaterial(0,Material);
  Patch->SetNumCustomDataFloats(4);Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Patch->SetGenerateOverlapEvents(false);Patch->SetCanEverAffectNavigation(false);Patch->SetCastShadow(false);
  // The source ground flora receives lighting but casts no shadow. Its shader
  // reconstructs roots/blades from packed data; undeformed CPU triangles must
  // not enter ray tracing or consume Lumen surface-cache cards.
  Patch->SetVisibleInRayTracing(false);Patch->SetAffectDistanceFieldLighting(false);Patch->SetAffectDynamicIndirectLighting(false);
  Patch->RegisterComponent();Patches.Add(Patch);
 }
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->RegisterNightMaterial(Material);
 return true;
}

void UNeonCityReferenceGrassComponent::SetWind(FVector2D Direction,float Speed,FVector2D GustOffset)
{
 if(!Material)return;
 const FVector Origin=GetOwner()->GetActorLocation();
 Direction=Direction.ContainsNaN()?FVector2D::ZeroVector:Direction.GetSafeNormal();
 if(GustOffset.ContainsNaN())GustOffset=FVector2D::ZeroVector;
 Material->SetVectorParameterValue(TEXT("IslandOriginCm"),FLinearColor(Origin.X,Origin.Y,Origin.Z,0));
 Material->SetVectorParameterValue(TEXT("WindDirection"),FLinearColor(Direction.X,Direction.Y,0,0));
 Material->SetVectorParameterValue(TEXT("GustOffset"),FLinearColor(GustOffset.X,GustOffset.Y,0,0));
 Material->SetScalarParameterValue(TEXT("WindSpeed"),FMath::IsFinite(Speed)?FMath::Clamp(Speed,0.f,25.f):0);
}

void UNeonCityReferenceGrassComponent::UpdateObserver(const FVector& Eye,APlayerController* Controller)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(IslandGrassObserver);
 if(!Data || !Material || !Controller || Eye.ContainsNaN())return;
 Material->SetVectorParameterValue(TEXT("VegetationViewerCm"),FLinearColor(Eye.X,Eye.Y,Eye.Z,0));
 auto* Local=Controller->GetLocalPlayer();
 if(!Local || !Local->ViewportClient || !Local->ViewportClient->Viewport)return;
 FSceneViewProjectionData Projection;
 if(!Local->GetProjectionData(Local->ViewportClient->Viewport,Projection))return;
 const FMatrix VP=Projection.ComputeViewProjectionMatrix();
 if(bHasView && Eye.Equals(LastEye,5) && VP.Equals(LastProjection,1e-7))return;
 LastEye=Eye;LastProjection=VP;bHasView=true;
 FConvexVolume Frustum;GetViewFrustumBounds(Frustum,VP,false);
 const FVector Origin=GetOwner()->GetActorLocation(),LocalEye=(Eye-Origin)*.01;
 const double CX=LocalEye.X,CZ=-LocalEye.Y;
 const int32 I0=FMath::Max(0,FMath::FloorToInt((CX-FarM-OriginM)/CellM));
 const int32 I1=FMath::Min(Side-1,FMath::FloorToInt((CX+FarM-OriginM)/CellM));
 const int32 J0=FMath::Max(0,FMath::FloorToInt((CZ-FarM-OriginM)/CellM));
 const int32 J1=FMath::Min(Side-1,FMath::FloorToInt((CZ+FarM-OriginM)/CellM));
 TArray<int32> Selected[3];for(int32 L=0;L<3;++L)Selected[L].Reserve(MaximumCells[L]);
 for(int32 J=J0;J<=J1;++J)for(int32 I=I0;I<=I1;++I)
 {
  const int32 Index=CellLookup[J*Side+I];if(Index==INDEX_NONE)continue;
  const auto& C=Data->Cells[Index];const double X=OriginM+I*CellM,Z=OriginM+J*CellM;
  const double DX=FMath::Max3(X-CX,0.,CX-X-CellM),DZ=FMath::Max3(Z-CZ,0.,CZ-Z-CellM);
  const double Distance=FMath::Sqrt(DX*DX+DZ*DZ);if(Distance>FarM)continue;
  const double HalfHeight=(C.MaximumHeightM-C.MinimumHeightM)*.5,Radius=Data->MaximumPlantRadiusM;
  const FVector Center=Origin+FVector((X+CellM*.5)*100,-(Z+CellM*.5)*100,(C.MinimumHeightM+HalfHeight)*100);
  if(!Frustum.IntersectBox(Center,FVector((CellM*.5+Radius)*100,(CellM*.5+Radius)*100,(HalfHeight+Radius)*100)))continue;
  int32 L=INDEX_NONE;
  if(Distance<NearM && Selected[0].Num()<MaximumCells[0])L=0;
  else if(Distance<MidM && Selected[1].Num()<MaximumCells[1])L=1;
  else if(Distance>=MidM && Selected[2].Num()<MaximumCells[2])L=2;
  if(L!=INDEX_NONE)Selected[L].Add(Index);
 }
 for(int32 L=0;L<3;++L)if(Selected[L]!=ActiveCells[L])
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(IslandGrassApplyCells);
  if(bUseIncrementalInstances)
  {
   CellInstances[L].Apply(*Patches[L],Selected[L],Data->Cells);
   ActiveCells[L]=MoveTemp(Selected[L]);continue;
  }
  UInstancedStaticMeshComponent* Patch=Patches[L];TArray<FTransform> Transforms;Transforms.Reserve(Selected[L].Num());
  for(int32 Index:Selected[L])
  {
   const auto& C=Data->Cells[Index];const float X=OriginM+(C.Index%Side)*CellM,Z=OriginM+(C.Index/Side)*CellM;
   Transforms.Emplace(FQuat::Identity,FVector(X*100,-Z*100,(C.MinimumHeightM+C.MaximumHeightM)*50));
  }
  Patch->ClearInstances();Patch->AddInstances(Transforms,false,false,false);
  for(int32 I=0;I<Selected[L].Num();++I)
  {
   const int32 Cell=Data->Cells[Selected[L][I]].Index;
   const float Values[]={OriginM+(Cell%Side)*CellM,OriginM+(Cell/Side)*CellM,0,0};
   Patch->SetCustomData(I,MakeArrayView(Values),false);
  }
  Patch->MarkRenderStateDirty();ActiveCells[L]=MoveTemp(Selected[L]);
 }
}

int32 UNeonCityReferenceGrassComponent::GetActiveCellCount(int32 Level) const
{
 return Level>=0 && Level<3?ActiveCells[Level].Num():0;
}

void UNeonCityReferenceGrassComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 for(auto Patch:Patches)if(IsValid(Patch))Patch->DestroyComponent();
 for(auto& State:CellInstances)State.Reset();
 Patches.Reset();Material=nullptr;Data=nullptr;CellLookup.Reset();
 Super::EndPlay(Reason);
}
