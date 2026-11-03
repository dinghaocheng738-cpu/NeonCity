#include "CoastalAmbientArtLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Misc/ScopeExit.h"
#include "AssetCompilingManager.h"

namespace CoastalCafeMeasurement921
{
FString Point(FVector P){return FString::Printf(TEXT("[%.6f,%.6f,%.6f]"),P.X,P.Y,P.Z);}
FString Mesh(UStaticMesh* M)
{
 const FMeshDescription* D=M?M->GetMeshDescription(0):nullptr;if(!D)return FString();
 const FStaticMeshConstAttributes A(*D);const auto Positions=A.GetVertexPositions();TArray<FString> Rows;
 for(const FTriangleID ID:D->Triangles().GetElementIDs())
 {
  const auto Vertices=D->GetTriangleVertices(ID);if(Vertices.Num()!=3)return FString();FString P[3];
  for(int32 I=0;I<3;++I){if(!D->Vertices().IsValid(Vertices[I]))return FString();const FVector V(Positions[Vertices[I]]);if(V.ContainsNaN())return FString();P[I]=Point(V);}
  Rows.Add(TEXT("[")+P[0]+TEXT(",")+P[1]+TEXT(",")+P[2]+TEXT("]"));
 }
 return TEXT("[")+FString::Join(Rows,TEXT(","))+TEXT("]");
}
}

FString UCoastalAmbientArtLibrary::MeasureCoastalCafeAssets(UStaticMesh* Chair,UStaticMesh* Cup,USkeletalMesh* Skin,UAnimSequence* Sitting)
{
 using namespace CoastalCafeMeasurement921;
 UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
 // Only the adopted chair and four imported summer skins extend this editor
 // measurement. All skeleton/animation/cup and transient-preview guards remain.
 const bool bAllowedChair=Chair && (Chair->GetPathName()==TEXT("/Game/NeonCity/Art/IslandLandPass1/Meshes/SM_Land_OutdoorChair.SM_Land_OutdoorChair")
  || Chair->GetPathName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SM_C923_RattanCafeChair32.SM_C923_RattanCafeChair32"));
 const bool bAllowedSkin=Skin && (Skin->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/SK_NCTW_"))
  || Skin->GetPathName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_Female_Adult_08_Gameplay.SK_C923_Summer37_Female_Adult_08_Gameplay")
  || Skin->GetPathName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_Female_Adult_02_Gameplay.SK_C923_Summer37_Female_Adult_02_Gameplay")
  || Skin->GetPathName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_Male_Adult_01_Gameplay.SK_C923_Summer37_Male_Adult_01_Gameplay")
  || Skin->GetPathName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_Business_Male_06_Gameplay.SK_C923_Summer37_Business_Male_06_Gameplay"));
 if(!W || W->WorldType!=EWorldType::Editor || !Chair || !Cup || !Skin || !Sitting
  || !bAllowedChair
  || Cup->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/EstateAmenity701/SM_Estate701_Cup.SM_Estate701_Cup")
  || !bAllowedSkin
  || Sitting->GetPathName()!=TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Sitting_Idle_Loop.AN_NC205C_Sitting_Idle_Loop")
  || Skin->GetSkeleton()!=Sitting->GetSkeleton())return FString();
 FAssetCompilingManager::Get().FinishAllCompilation();const FString CT=Mesh(Chair),UT=Mesh(Cup);if(CT.IsEmpty() || UT.IsEmpty())return FString();
 const auto& Ref=Skin->GetRefSkeleton();int32 HeadIndex=Ref.FindBoneIndex(TEXT("head"));if(HeadIndex==INDEX_NONE)return FString();
 FTransform HeadRef=Ref.GetRefBonePose()[HeadIndex];for(int32 Parent=Ref.GetParentIndex(HeadIndex);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))HeadRef=HeadRef*Ref.GetRefBonePose()[Parent];
 const auto* Render=Skin->GetResourceForRendering();if(!Render || Render->LODRenderData.IsEmpty())return FString();
 const auto& RefPositions=Render->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;if(!RefPositions.GetVertexData())return FString();TArray<FString> RefPoints;
 for(uint32 I=0;I<RefPositions.GetNumVertices();++I){const FVector P(RefPositions.VertexPosition(I));if(P.ContainsNaN())return FString();RefPoints.Add(Point(P));}
 FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 AActor* Preview=W->SpawnActor<AActor>(FVector::ZeroVector,FRotator::ZeroRotator,Params);if(!Preview)return FString();
 ON_SCOPE_EXIT{Preview->Destroy();};
 auto* C=NewObject<USkeletalMeshComponent>(Preview,NAME_None,RF_Transient);Preview->AddInstanceComponent(C);Preview->SetRootComponent(C);
 C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetSkeletalMesh(Skin);C->RegisterComponent();
 C->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;C->SetVisibility(false);
 C->PlayAnimation(Sitting,true);C->SetPosition(0,false);C->TickAnimation(0,false);C->RefreshBoneTransforms();C->UpdateBounds();
 TArray<FString> Bones;
 for(const FName Name:{FName(TEXT("pelvis")),FName(TEXT("head")),FName(TEXT("hand_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r")),FName(TEXT("thumb_02_r")),FName(TEXT("index_02_r")),FName(TEXT("middle_02_r")),FName(TEXT("ring_02_r"))})
 {
  if(C->GetBoneIndex(Name)==INDEX_NONE)return FString();const FTransform T=C->GetSocketTransform(Name,RTS_Component);const FQuat Q=T.GetRotation();
  if(T.ContainsNaN())return FString();Bones.Add(FString::Printf(TEXT("\"%s\":{\"position_cm\":%s,\"rotation_xyzw\":[%.8f,%.8f,%.8f,%.8f]}"),*Name.ToString(),*Point(T.GetLocation()),Q.X,Q.Y,Q.Z,Q.W));
 }
 // Hidden editor previews have no MeshObject. Read the real current pose and
 // skin-weight buffers directly instead of recreating a CPU render proxy.
 auto* Weights=C->GetSkinWeightBuffer(0);if(!Weights)return FString();
 TArray<FMatrix44f> RefToLocal;C->GetCurrentRefToLocalMatrices(RefToLocal,0);
 if(RefToLocal.Num()!=Ref.GetNum())return FString();
 TArray<FVector3f> Vertices;USkinnedMeshComponent::ComputeSkinnedPositions(C,Vertices,RefToLocal,Render->LODRenderData[0],*Weights);
 if(Vertices.Num()<1000)return FString();TArray<FString> SkinPoints;
 for(const auto& Vertex:Vertices){const FVector P(Vertex);if(P.ContainsNaN())return FString();SkinPoints.Add(Point(P));}
 const FQuat HQ=HeadRef.GetRotation();
 return FString::Printf(TEXT("{\"chair_triangles_local_cm\":%s,\"cup_triangles_local_cm\":%s,\"skin\":\"%s\",\"seat_clip\":\"%s\",\"sample_seconds\":0,\"reference_head\":{\"position_cm\":%s,\"rotation_xyzw\":[%.8f,%.8f,%.8f,%.8f]},\"reference_vertices_component_cm\":[%s],\"bones\":{%s},\"skinned_vertices_component_cm\":[%s],\"persistent_mutations\":0}"),
  *CT,*UT,*Skin->GetPathName(),*Sitting->GetPathName(),*Point(HeadRef.GetLocation()),HQ.X,HQ.Y,HQ.Z,HQ.W,*FString::Join(RefPoints,TEXT(",")),*FString::Join(Bones,TEXT(",")),*FString::Join(SkinPoints,TEXT(",")));
}
