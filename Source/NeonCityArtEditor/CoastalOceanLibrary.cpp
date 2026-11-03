#include "CoastalOceanLibrary.h"
#include "NeonCityOceanCDLOD.h"
#include "Engine/Texture2DArray.h"
#include "Water/CoastalWaveData.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Math/Float16Color.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace
{
bool NewOwnedPath(const FString& Path)
{
 // A CDO's initial lookup may create an empty package before native authoring.
 // Preserve real in-memory assets and every existing package on disk.
 const FString ObjectPath=Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path);
 return GEditor && Path.StartsWith(TEXT("/Game/NeonCity/Art/Immersion/Ocean/"))
  && !FindObject<UObject>(nullptr,*ObjectPath) && !FPackageName::DoesPackageExist(Path);
}
}
UStaticMesh* UCoastalOceanLibrary::CreateOceanGrid(const FString& Path)
{
 if(!NewOwnedPath(Path))return nullptr;
 auto* Mesh=NewObject<UStaticMesh>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
 FMeshDescription Desc;FStaticMeshAttributes Attributes(Desc);Attributes.Register();
 auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
 auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
 auto UV=Attributes.GetVertexInstanceUVs();UV.SetNumChannels(1);
 const FPolygonGroupID Group=Desc.CreatePolygonGroup();
 TArray<FVector3f> SourceVertices;TArray<uint32> SourceIndices;
 FNeonCityOceanCDLOD::BuildGrid(SourceVertices,SourceIndices);
 TArray<FVertexID> Vertices;
 for(const auto P:SourceVertices)
 {const auto V=Desc.CreateVertex();Positions[V]=FVector3f(P.X*100,-P.Z*100,0);Vertices.Add(V);}
 auto Triangle=[&](FVertexID A,FVertexID B,FVertexID C)
 {
  TArray<FVertexInstanceID> Instances;
  for(auto ID:{A,B,C})
  {auto I=Desc.CreateVertexInstance(ID);Normals[I]=FVector3f(0,0,1);Tangents[I]=FVector3f(1,0,0);Signs[I]=1;
   UV.Set(I,0,FVector2f(Positions[ID].X/100,-Positions[ID].Y/100));Instances.Add(I);}
  Desc.CreatePolygon(Group,Instances);
 };
 // Reflection into UE changes handedness; reversed winding keeps the surface up.
 for(int32 I=0;I<SourceIndices.Num();I+=3)
  Triangle(Vertices[SourceIndices[I]],Vertices[SourceIndices[I+2]],Vertices[SourceIndices[I+1]]);
 Mesh->SetNumSourceModels(1);auto& Build=Mesh->GetSourceModel(0).BuildSettings;
 Build.bRecomputeNormals=false;Build.bRecomputeTangents=false;Build.bGenerateLightmapUVs=false;
 Mesh->CreateMeshDescription(0,MoveTemp(Desc));Mesh->CommitMeshDescription(0);
 Mesh->GetStaticMaterials().Add(FStaticMaterial());Mesh->NaniteSettings.bEnabled=false;
 Mesh->SetPositiveBoundsExtension(FVector(100,100,2500));Mesh->SetNegativeBoundsExtension(FVector(100,100,2500));
 Mesh->Build(false);Mesh->PostEditChange();Mesh->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Mesh);return Mesh;
}
UTexture2D* UCoastalOceanLibrary::BakeBathymetry(UCoastalWaveData* Field,const FString& Path)
{
 if(!Field || !NewOwnedPath(Path))return nullptr;
 UWorld* W=GEditor->GetEditorWorldContext().World();if(!W || W->WorldType!=EWorldType::Editor)return nullptr;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CoastalBathymetryBake),true);
 // Bake only native ground. Props, vegetation, pier decks and the old ocean
 // visual must never masquerade as seabed. The ground list is audited below.
 for(TActorIterator<AActor> It(W);It;++It)
 {
  const FString Label=It->GetActorLabel();
  const bool Ground=It->IsA<ALandscape>() || Label==TEXT("Beach") || Label.StartsWith(TEXT("Art205B - Beach West"))
   || Label.StartsWith(TEXT("Art205B - Beach East"));
  if(!Ground)Params.AddIgnoredActor(*It);
  else UE_LOG(LogTemp,Display,TEXT("Coastal bathymetry ground: %s"),*It->GetPathName());
 }
 Field->GridSize=513;Field->DepthCm.SetNum(Field->GridSize*Field->GridSize);
 TArray<FFloat16Color> Texels;Texels.SetNum(Field->DepthCm.Num());int32 Hits=0;
 for(int32 Y=0;Y<Field->GridSize;++Y)for(int32 X=0;X<Field->GridSize;++X)
 {
  const FVector2D P=Field->GridMinimum+(Field->GridMaximum-Field->GridMinimum)*FVector2D(double(X),double(Y))/(Field->GridSize-1);
  FHitResult Hit;float Depth=20000;
  if(W->LineTraceSingleByChannel(Hit,FVector(P,40000),FVector(P,-25000),ECC_Visibility,Params))
  {Depth=FMath::Clamp(float(Field->SeaLevel-Hit.ImpactPoint.Z),0.f,20000.f);++Hits;}
  const int32 I=Y*Field->GridSize+X;Texels[I]=FFloat16Color(FLinearColor(Depth,0,0,1));
  Field->DepthCm[I]=Texels[I].R.GetFloat(); // Identical half-float depth on CPU/GPU.
 }
 if(Hits<100)return nullptr;
 auto* Texture=NewObject<UTexture2D>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
 Texture->Source.Init(Field->GridSize,Field->GridSize,1,1,TSF_RGBA16F,reinterpret_cast<const uint8*>(Texels.GetData()));
 Texture->SRGB=false;Texture->CompressionSettings=TC_HDR;Texture->MipGenSettings=TMGS_NoMipmaps;
 Texture->Filter=TF_Bilinear;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;Texture->NeverStream=true;
 Texture->PostEditChange();Texture->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Texture);
 Field->DepthTexture=Texture;Field->MarkPackageDirty();
 UE_LOG(LogTemp,Display,TEXT("Coastal bathymetry: %d / %d ground hits"),Hits,Texels.Num());return Texture;
}

int32 UCoastalOceanLibrary::RetraceBathymetryRegion(UCoastalWaveData* Field,FVector2D RegionMin,FVector2D RegionMax)
{
 if(!GEditor || !Field || !Field->DepthTexture || Field->GridSize<2 || Field->DepthCm.Num()!=Field->GridSize*Field->GridSize
  || !Field->DepthTexture->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/")))return -1;
 UTexture2D* Texture=Field->DepthTexture;
 if(Texture->Source.GetFormat()!=TSF_RGBA16F || Texture->Source.GetSizeX()!=Field->GridSize || Texture->Source.GetSizeY()!=Field->GridSize)return -1;
 UWorld* W=GEditor->GetEditorWorldContext().World();if(!W || W->WorldType!=EWorldType::Editor)return -1;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CoastalBathymetryRetrace),true);
 for(TActorIterator<AActor> It(W);It;++It)                // the same native-ground policy as BakeBathymetry
 {
  const FString Label=It->GetActorLabel();
  const bool Ground=It->IsA<ALandscape>() || Label==TEXT("Beach") || Label.StartsWith(TEXT("Art205B - Beach West"))
   || Label.StartsWith(TEXT("Art205B - Beach East"));
  if(!Ground)Params.AddIgnoredActor(*It);
 }
 // Contained (inland) water is not sea: its footprint reads as dry ground for the ocean.
 TArray<FBox2D> Inland;
 for(TActorIterator<ANeonCityWaterVolume> It(W);It;++It)if(It->bContainedPoolWater)
 {
  FBox2D Box(ForceInit);
  for(int32 C=0;C<4;++C)
   Box+=FVector2D(It->GetActorTransform().TransformPosition(FVector((C&1)?It->Extent.X:-It->Extent.X,(C&2)?It->Extent.Y:-It->Extent.Y,0)));
  Inland.Add(Box);
 }
 FFloat16Color* Texels=reinterpret_cast<FFloat16Color*>(Texture->Source.LockMip(0));
 if(!Texels)return -1;
 int32 Changed=0;
 for(int32 Y=0;Y<Field->GridSize;++Y)for(int32 X=0;X<Field->GridSize;++X)
 {
  const FVector2D P=Field->GridMinimum+(Field->GridMaximum-Field->GridMinimum)*FVector2D(double(X),double(Y))/(Field->GridSize-1);
  if(P.X<RegionMin.X || P.X>RegionMax.X || P.Y<RegionMin.Y || P.Y>RegionMax.Y)continue;
  FHitResult Hit;float Depth=20000;
  if(Inland.ContainsByPredicate([&](const FBox2D& B){return B.IsInside(P);}))Depth=0;
  else if(W->LineTraceSingleByChannel(Hit,FVector(P,40000),FVector(P,-25000),ECC_Visibility,Params))
   Depth=FMath::Clamp(float(Field->SeaLevel-Hit.ImpactPoint.Z),0.f,20000.f);
  const int32 I=Y*Field->GridSize+X;const FFloat16Color Texel(FLinearColor(Depth,0,0,1));
  if(Texel.R.GetFloat()!=Field->DepthCm[I])++Changed;
  Texels[I]=Texel;Field->DepthCm[I]=Texel.R.GetFloat();
 }
 Texture->Source.UnlockMip(0);
 Texture->PostEditChange();Texture->MarkPackageDirty();Field->MarkPackageDirty();
 UE_LOG(LogTemp,Display,TEXT("Coastal bathymetry retrace: %d cells changed"),Changed);
 return Changed;
}

UTexture2DArray* UCoastalOceanLibrary::CreateOceanPlaceholder(const FString& Path)
{
 if(!NewOwnedPath(Path))return nullptr;
 auto* T=NewObject<UTexture2DArray>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
 TArray<uint8> Data;int32 Bytes=0;
 for(int32 M=0;M<9;++M)Bytes+=(256>>M)*(256>>M)*4*8;
 Data.SetNumZeroed(Bytes);T->Source.Init(256,256,4,9,TSF_RGBA16F,Data.GetData());
 T->SRGB=false;T->CompressionSettings=TC_HDR;T->MipGenSettings=TMGS_LeaveExistingMips;
 T->NeverStream=true;T->Filter=TF_Trilinear;
 T->PostEditChange();T->MarkPackageDirty();FAssetRegistryModule::AssetCreated(T);return T;
}

UTexture2D* UCoastalOceanLibrary::ImportOceanFloatField(const FString& Path,const FString& RawFile,int32 Resolution,int32 Channels,int32 Height)
{
 if(Height==0)Height=Resolution;
 if(!NewOwnedPath(Path) || (Channels!=1 && Channels!=4) || Resolution<2 || Resolution>2048 || Height<2 || Height>2048)return nullptr;
 const FString File=FPaths::ConvertRelativePathToFull(RawFile);
 const FString Intake=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VerticalSlice/ShoreIntake"));
 if(!FPaths::IsUnderDirectory(File,Intake))return nullptr;
 TArray<uint8> Bytes;
 if(!FFileHelper::LoadFileToArray(Bytes,*File) || Bytes.Num()!=Resolution*Height*Channels*4)return nullptr;
 for(int32 I=0;I<Bytes.Num();I+=4){float V;FMemory::Memcpy(&V,Bytes.GetData()+I,4);if(!FMath::IsFinite(V))return nullptr;}
 auto* T=NewObject<UTexture2D>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);
 T->Source.Init(Resolution,Height,1,1,Channels==1?TSF_R32F:TSF_RGBA32F,Bytes.GetData());
 T->SRGB=false;T->CompressionSettings=Channels==1?TC_SingleFloat:TC_HDR_F32;T->MipGenSettings=TMGS_NoMipmaps;
 T->Filter=TF_Nearest;T->AddressX=TA_Clamp;T->AddressY=TA_Clamp;T->NeverStream=true;
 T->PostEditChange();T->MarkPackageDirty();FAssetRegistryModule::AssetCreated(T);return T;
}

namespace
{
FString C923DepthFailure(const TCHAR* Reason)
{return FString::Printf(TEXT("{\"passed\":false,\"failure\":\"%s\"}"),Reason);}

bool C923SameWaveField(const UCoastalWaveData& A,const UCoastalWaveData& B)
{
 return A.GetClass()==B.GetClass() && A.CalmProfile==B.CalmProfile && A.LightProfile==B.LightProfile
  && A.ModerateProfile==B.ModerateProfile && A.GridMinimum==B.GridMinimum && A.GridMaximum==B.GridMaximum
  && A.GridSize==B.GridSize && A.SeaLevel==B.SeaLevel && A.IslandHeight==B.IslandHeight
  && A.IslandShore==B.IslandShore && A.IslandNormal==B.IslandNormal;
}

bool C923SameDepthTexturePolicy(const UTexture2D& A,const UTexture2D& B)
{
 return A.SRGB==B.SRGB && A.CompressionSettings==B.CompressionSettings && A.MipGenSettings==B.MipGenSettings
  && A.Filter==B.Filter && A.AddressX==B.AddressX && A.AddressY==B.AddressY && A.NeverStream==B.NeverStream
  && A.LODGroup==B.LODGroup && A.LODBias==B.LODBias && A.VirtualTextureStreaming==B.VirtualTextureStreaming
  && A.MaxTextureSize==B.MaxTextureSize && A.CompressionNone==B.CompressionNone;
}

bool C923DepthSource(UTexture2D& Texture,TArray64<uint8>& Bytes)
{
 return Texture.Source.GetSizeX()==513 && Texture.Source.GetSizeY()==513 && Texture.Source.GetNumSlices()==1
  && Texture.Source.GetNumMips()==1 && Texture.Source.GetNumLayers()==1 && Texture.Source.GetNumBlocks()==1
  && Texture.Source.GetFormat()==TSF_RGBA16F && Texture.Source.GetMipData(Bytes,0) && Bytes.Num()==int64(513)*513*8;
}

struct FC923DepthContract
{
 const TCHAR* OriginalField;
 const TCHAR* OriginalTexture;
 const TCHAR* CandidateField;
 const TCHAR* CandidateTexture;
 int32 PatchCount;
 const TCHAR* PayloadSHA1;
 const TCHAR* PathFailure;
 const TCHAR* CountFailure;
 const TCHAR* StatusPrefix;
};

// Each public intake remains a finite, reviewed source contract. No public
// caller can supply asset paths, a replacement signature or arbitrary depths.
const FC923DepthContract Marina19={
 TEXT("/Game/NeonCity/Art/Immersion/Ocean/DA_CoastalWaves.DA_CoastalWaves"),
 TEXT("/Game/NeonCity/Art/Immersion/Ocean/T_CoastalDepth.T_CoastalDepth"),
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/DA_C923_MarinaWaves19.DA_C923_MarinaWaves19"),
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_MarinaDepth19.T_C923_MarinaDepth19"),
 288,TEXT("bca05a0952934b7588d0481ac0bcf8dd964ccfc6"),
 TEXT("Only_exact_owned_Bathymetry19_duplicates_allowed"),TEXT("Frozen288_count_required"),TEXT("C923_BATHYMETRY19")};

const FC923DepthContract Harbor26={
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/DA_C923_MarinaWaves19.DA_C923_MarinaWaves19"),
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_MarinaDepth19.T_C923_MarinaDepth19"),
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/DA_C923_HarborWaves26.DA_C923_HarborWaves26"),
 TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/T_C923_HarborDepth26.T_C923_HarborDepth26"),
 105,TEXT("d63ffbd9e2aee8da621b610a78178c462636bd92"),
 TEXT("Only_exact_owned_Harbor26_duplicates_allowed"),TEXT("Frozen105_count_required"),TEXT("C923_HARBOR26")};

FString C923MarinaDepth(UCoastalWaveData* Candidate,UTexture2D* CandidateTexture,UCoastalWaveData* Original,
 const TArray<int32>& Indices,const TArray<int32>& HalfBits,bool Apply,const FC923DepthContract& Contract)
{
 constexpr int32 Count=513*513;
 if(!GEditor || !IsInGameThread() || !GEditor->GetEditorWorldContext().World()
  || GEditor->GetEditorWorldContext().World()->WorldType!=EWorldType::Editor)
  return C923DepthFailure(TEXT("Editor_world_required"));
 if(!Candidate || !CandidateTexture || !Original || Candidate==Original || CandidateTexture==Original->DepthTexture
  || Original->GetPathName()!=Contract.OriginalField
  || GetPathNameSafe(Original->DepthTexture.Get())!=Contract.OriginalTexture
  || Candidate->GetPathName()!=Contract.CandidateField
  || CandidateTexture->GetPathName()!=Contract.CandidateTexture)
  return C923DepthFailure(Contract.PathFailure);
 if(!Original->IsValidField() || !C923SameWaveField(*Candidate,*Original) || Candidate->DepthCm.Num()!=Count
  || Original->GridSize!=513 || Original->GridMinimum!=FVector2D(-220000,-297500)
  || Original->GridMaximum!=FVector2D(220000,-2500) || Original->SeaLevel!=-130
  || !C923SameDepthTexturePolicy(*CandidateTexture,*Original->DepthTexture))
  return C923DepthFailure(TEXT("Original_field_profile_island_bounds_or_texture_policy_mismatch"));
 if(Indices.Num()!=Contract.PatchCount || HalfBits.Num()!=Indices.Num())return C923DepthFailure(Contract.CountFailure);
 TArray<int32> Signature;Signature.Reserve(Contract.PatchCount*2);TMap<int32,uint16> Changes;
 for(int32 I=0;I<Indices.Num();++I)
 {
  const int32 Index=Indices[I],Bits=HalfBits[I];FFloat16 Value;
  if(Index<0 || Index>=Count || Bits<0 || Bits>65535 || Changes.Contains(Index)
   || (I>0 && Index<=Indices[I-1]))return C923DepthFailure(TEXT("Invalid_or_reordered_patch_record"));
  Value.Encoded=uint16(Bits);
  if(!FMath::IsFinite(Value.GetFloat()) || Value.GetFloat()<=0 || Value.GetFloat()>20000)
   return C923DepthFailure(TEXT("Invalid_half_depth"));
  Signature.Append({Index,Bits});Changes.Add(Index,uint16(Bits));
 }
 // Same exact index/int32-half-bit interleaving as the SHA-locked source plan.
 // This prevents the public editor helper from becoming an arbitrary depth editor.
 if(FSHA1::HashBuffer(Signature.GetData(),Signature.Num()*sizeof(int32)).ToString().ToLower()
  !=Contract.PayloadSHA1)
  return C923DepthFailure(TEXT("Frozen_source_plan_payload_signature_mismatch"));
 const bool OriginalFieldDirty=Original->GetOutermost()->IsDirty();
 const bool OriginalTextureDirty=Original->DepthTexture->GetOutermost()->IsDirty();
 const FSHAHash OriginalDepthHash=FSHA1::HashBuffer(Original->DepthCm.GetData(),Count*sizeof(float));
 TArray64<uint8> Before,Current;
 if(!C923DepthSource(*Original->DepthTexture,Before) || !C923DepthSource(*CandidateTexture,Current))
  return C923DepthFailure(TEXT("Actual_native_RGBA16F_source_contract_failed"));
 TArray<float> Expected=Original->DepthCm;
 TArray64<uint8> ExpectedBytes=Before;
 int32 ActualChanges=0;
 for(int32 I=0;I<Count;++I)
 {
  uint16 OriginalBits;FMemory::Memcpy(&OriginalBits,Before.GetData()+int64(I)*8,2);FFloat16 Old;Old.Encoded=OriginalBits;
  const float OriginalFloat=Old.GetFloat();
  if(FMemory::Memcmp(&OriginalFloat,&Original->DepthCm[I],sizeof(float))!=0)
   return C923DepthFailure(TEXT("Original_CPU_and_texture_half_depth_disagree"));
  if(const uint16* Bits=Changes.Find(I))
  {
   if(*Bits==OriginalBits)return C923DepthFailure(TEXT("Reviewed_change_did_not_change_original_half_bits"));
   FFloat16 Value;Value.Encoded=*Bits;Expected[I]=Value.GetFloat();
   FMemory::Memcpy(ExpectedBytes.GetData()+int64(I)*8,Bits,2);++ActualChanges;
  }
 }
 if(Apply)
 {
  if(Candidate->DepthTexture!=Original->DepthTexture
   || FMemory::Memcmp(Candidate->DepthCm.GetData(),Original->DepthCm.GetData(),Count*sizeof(float))!=0
   || FMemory::Memcmp(Current.GetData(),Before.GetData(),Before.Num())!=0)
   return C923DepthFailure(TEXT("Candidate_must_be_unmodified_exact_original_duplicates"));
  CandidateTexture->Source.Init(513,513,1,1,TSF_RGBA16F,ExpectedBytes.GetData());
  CandidateTexture->PostEditChange();CandidateTexture->MarkPackageDirty();
  Candidate->DepthCm=Expected;Candidate->DepthTexture=CandidateTexture;Candidate->MarkPackageDirty();
 }
 TArray64<uint8> After,OriginalAfter;
 if(Candidate->DepthTexture!=CandidateTexture || !Candidate->IsValidField() || !C923SameWaveField(*Candidate,*Original)
  || !C923SameDepthTexturePolicy(*CandidateTexture,*Original->DepthTexture)
  || !C923DepthSource(*CandidateTexture,After) || !C923DepthSource(*Original->DepthTexture,OriginalAfter)
  || After.Num()!=ExpectedBytes.Num() || FMemory::Memcmp(After.GetData(),ExpectedBytes.GetData(),After.Num())!=0
  || FMemory::Memcmp(Candidate->DepthCm.GetData(),Expected.GetData(),Count*sizeof(float))!=0
  || OriginalAfter.Num()!=Before.Num() || FMemory::Memcmp(OriginalAfter.GetData(),Before.GetData(),Before.Num())!=0
  || FSHA1::HashBuffer(Original->DepthCm.GetData(),Count*sizeof(float))!=OriginalDepthHash
  || Original->GetOutermost()->IsDirty()!=OriginalFieldDirty
  || Original->DepthTexture->GetOutermost()->IsDirty()!=OriginalTextureDirty)
  return C923DepthFailure(TEXT("Actual_source_readback_or_original_field_guard_failed"));
 return FString::Printf(TEXT("{\"passed\":true,\"status\":\"%s_NATIVE_SOURCE_%s_PASS\",\"changed_texel_R_half_count\":%d,\"outside_texels_equal\":%d,\"all_GBA_half_components_equal\":%d,\"source_bytes\":%lld,\"original_RGBA16F_sha1\":\"%s\",\"new_RGBA16F_sha1\":\"%s\",\"profiles_bounds_sealevel_island_refs_equal\":true,\"original_source_and_dirty_flags_unchanged\":true,\"saved_or_map_bound_by_helper\":false,\"visual_gameplay_performance_pass\":false}"),
  Contract.StatusPrefix,Apply?TEXT("PATCH"):TEXT("VERIFY"),ActualChanges,Count-ActualChanges,Count*3,After.Num(),
  *FSHA1::HashBuffer(Before.GetData(),Before.Num()).ToString().ToLower(),*FSHA1::HashBuffer(After.GetData(),After.Num()).ToString().ToLower());
}
}

FString UCoastalOceanLibrary::PatchC923MarinaBathymetry(UCoastalWaveData* Candidate,UTexture2D* CandidateTexture,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits)
{return C923MarinaDepth(Candidate,CandidateTexture,Original,Indices,HalfBits,true,Marina19);}

FString UCoastalOceanLibrary::ValidateC923MarinaBathymetry(UCoastalWaveData* Candidate,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits)
{return C923MarinaDepth(Candidate,Candidate?Candidate->DepthTexture.Get():nullptr,Original,Indices,HalfBits,false,Marina19);}

FString UCoastalOceanLibrary::PatchC923HarborBathymetry(UCoastalWaveData* Candidate,UTexture2D* CandidateTexture,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits)
{return C923MarinaDepth(Candidate,CandidateTexture,Original,Indices,HalfBits,true,Harbor26);}

FString UCoastalOceanLibrary::ValidateC923HarborBathymetry(UCoastalWaveData* Candidate,UCoastalWaveData* Original,const TArray<int32>& Indices,const TArray<int32>& HalfBits)
{return C923MarinaDepth(Candidate,Candidate?Candidate->DepthTexture.Get():nullptr,Original,Indices,HalfBits,false,Harbor26);}
