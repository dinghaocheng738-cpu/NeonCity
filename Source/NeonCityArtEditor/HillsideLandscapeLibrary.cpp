#include "HillsideLandscapeLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"

namespace
{
 bool LoadHeights(const FString& Path,int32 SizeX,int32 SizeY,TArray<uint16>& Out)
 {
  TArray<uint8> Bytes;
  if(SizeX<2 || SizeY<2 || !FFileHelper::LoadFileToArray(Bytes,*Path) || Bytes.Num()!=SizeX*SizeY*2)
  {
   UE_LOG(LogTemp,Error,TEXT("Hillside heightfile %s: expected %d bytes, got %d"),*Path,SizeX*SizeY*2,Bytes.Num());
   return false;
  }
  Out.SetNumUninitialized(SizeX*SizeY);
  FMemory::Memcpy(Out.GetData(),Bytes.GetData(),Bytes.Num());
  return true;
 }
}

ALandscape* UHillsideLandscapeLibrary::CreateLandscapeFromHeightfile(const FString& HeightfilePath,int32 SizeX,int32 SizeY,int32 SubsectionSizeQuads,int32 NumSubsections,
 FVector LocationCm,FVector ScaleCm,UMaterialInterface* Material,const FString& Label)
{
 const int32 ComponentQuads=SubsectionSizeQuads*NumSubsections;
 if(ComponentQuads<=0 || (SizeX-1)%ComponentQuads || (SizeY-1)%ComponentQuads)
 {
  UE_LOG(LogTemp,Error,TEXT("Hillside landscape size %dx%d is not a whole number of %d-quad components"),SizeX,SizeY,ComponentQuads);
  return nullptr;
 }
 UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
 TArray<uint16> Heights;
 if(!World || !LoadHeights(HeightfilePath,SizeX,SizeY,Heights))return nullptr;

 TMap<FGuid,TArray<uint16>> HeightLayers;HeightLayers.Add(FGuid(),MoveTemp(Heights));
 TMap<FGuid,TArray<FLandscapeImportLayerInfo>> MaterialLayers;MaterialLayers.Add(FGuid(),TArray<FLandscapeImportLayerInfo>());

 FActorSpawnParameters Params;Params.OverrideLevel=World->PersistentLevel;
 ALandscape* Landscape=World->SpawnActor<ALandscape>(LocationCm,FRotator::ZeroRotator,Params);
 if(!Landscape)return nullptr;
 Landscape->SetActorRelativeScale3D(ScaleCm);
 Landscape->LandscapeMaterial=Material;
 Landscape->Import(FGuid::NewGuid(),0,0,SizeX-1,SizeY-1,NumSubsections,SubsectionSizeQuads,HeightLayers,nullptr,MaterialLayers,
  ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
 if(ULandscapeInfo* Info=Landscape->CreateLandscapeInfo())Info->UpdateLayerInfoMap(Landscape);
 if(!Label.IsEmpty())Landscape->SetActorLabel(Label);
 Landscape->PostEditChange();
 return Landscape;
}

bool UHillsideLandscapeLibrary::ReplaceLandscapeHeights(ALandscape* Landscape,const FString& HeightfilePath,int32 SizeX,int32 SizeY)
{
 TArray<uint16> Heights;
 ULandscapeInfo* Info=Landscape?Landscape->GetLandscapeInfo():nullptr;
 if(!Info || !LoadHeights(HeightfilePath,SizeX,SizeY,Heights))return false;
 int32 MinX,MinY,MaxX,MaxY;
 if(!Info->GetLandscapeExtent(MinX,MinY,MaxX,MaxY) || MaxX-MinX+1!=SizeX || MaxY-MinY+1!=SizeY)
 {
  UE_LOG(LogTemp,Error,TEXT("Hillside heightfile %dx%d does not match the landscape extent"),SizeX,SizeY);
  return false;
 }
 // Edit-layer landscapes (every landscape since 5.5) merge their layers into the final data: write into the
 // base edit layer, else the next merge restores the old heights.
 const ULandscapeEditLayerBase* Layer=Landscape->HasLayersContent()?Landscape->GetEditLayerConst(0):nullptr;
 {
  FScopedSetLandscapeEditingLayer Scope(Landscape,Layer?Layer->GetGuid():FGuid(),[Landscape]{Landscape->RequestLayersContentUpdateForceAll();});
  FLandscapeEditDataInterface Edit(Info);
  Edit.SetHeightData(MinX,MinY,MaxX,MaxY,Heights.GetData(),0,true);
  Edit.Flush();
 }
 if(Layer)Landscape->ForceUpdateLayersContent();
 Landscape->PostEditChange();
 return true;
}
