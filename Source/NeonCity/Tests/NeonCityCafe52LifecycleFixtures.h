#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCData.h"
#include "AI/CoastalAmbientSeatComponent.h"
#include "Presentation/CoastalAmbientAnimInstance.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Dom/JsonObject.h"

/** Fix-specific evidence for the four additional Cafe52 guests. This fixture
 * observes ordinary production seating/tea/IK and never writes an NPC pose,
 * state, input, route, asset, collision shape or animation clock. The caller
 * retains the existing complete tea/normal-expiry/departure/return test. */
namespace NeonCityCafe52
{
inline bool IsGroup(FName Group)
{
 return Group==TEXT("C923.Cafe52.Table0") || Group==TEXT("C923.Cafe52.Table1");
}

inline FString CheckAuthoredGuest(const APedestrianSpawnPoint* Marker)
{
 if(!Marker)return TEXT("Cafe52 marker missing");
 const TCHAR* Keys[]={TEXT("Male_Adult_01"),TEXT("Female_Adult_02"),TEXT("Female_Adult_08"),TEXT("Business_Male_06")};
 int32 Guest=INDEX_NONE;
 for(int32 I=0;I<4;++I)if(Marker->PedestrianId==FName(*FString::Printf(TEXT("Mainland916.Riviera.%02d"),I)))Guest=I;
 if(Guest==INDEX_NONE || !Marker->bAmbientEnabled || !Marker->bDistanceManaged || Marker->BehaviorSeed!=91600+Guest*79
  || Marker->AmbientActivity!=ECoastalAmbientActivity::CafeConversation || !Marker->bAmbientGroundFeet
  || Marker->AmbientConversationGroup!=FName(Guest<2?TEXT("C923.Cafe52.Table0"):TEXT("C923.Cafe52.Table1"))
  || Marker->ActivationRadius!=6500.f || GetPathNameSafe(Marker->NPCData.Get())!=TEXT("/Game/NeonCity/AI/DA_SouthBeachPedestrian.DA_SouthBeachPedestrian"))
  return TEXT("Cafe52 retained identity/data/seed or new opt-in policy mismatch");
 const FString Expected=FString::Printf(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SK_C923_Summer37_%s_Gameplay.SK_C923_Summer37_%s_Gameplay"),Keys[Guest],Keys[Guest]);
 if(GetPathNameSafe(Marker->AmbientMaleSkin.Get())!=Expected || GetPathNameSafe(Marker->AmbientFemaleSkin.Get())!=Expected
  || GetPathNameSafe(Marker->AmbientAnimationClass.Get())!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/ABP_C923_Cafe52Grounded.ABP_C923_Cafe52Grounded_C")
  || !Marker->AmbientSeatSupport || Marker->AmbientLeftShoeSoleFootLocal.ContainsNaN() || Marker->AmbientRightShoeSoleFootLocal.ContainsNaN()
  || Marker->AmbientLeftShoeSoleFootLocal.IsNearlyZero() || Marker->AmbientRightShoeSoleFootLocal.IsNearlyZero())
  return TEXT("Cafe52 measured summer skin/private graph/chair/shoe binding mismatch");
 return FString();
}

/** Sample actual skin-weighted footwear, not an ankle marker proxy. Invoke
 * after seated IK has settled, at most twice per second during the unchanged
 * complete tea window; retain every returned row in the lifecycle receipt. */
inline TSharedRef<FJsonObject> SampleGroundedShoes(APedestrianSpawnPoint* Marker,FString& Error)
{
 auto Row=MakeShared<FJsonObject>();Error=CheckAuthoredGuest(Marker);
 Row->SetStringField(TEXT("id"),Marker?Marker->PedestrianId.ToString():TEXT("None"));
 Row->SetStringField(TEXT("method"),TEXT("Actual LOD0 CPU-skinned footwear surface and actual Pawn floor trace; no pose writes"));
 auto Fail=[&](const FString& Why){Error=Why;Row->SetBoolField(TEXT("passed"),false);Row->SetStringField(TEXT("failure"),Why);return Row;};
 if(!Error.IsEmpty())return Fail(Error);
 auto* NPC=Marker->GetSpawnedPedestrian();auto* Occupant=NPC?NPC->FindComponentByClass<UTransportOccupantComponent>():nullptr;
 auto* Body=Occupant?Occupant->GetDriverMesh():nullptr;
 auto* Anim=Body?Cast<UCoastalAmbientAnimInstance>(Body->GetAnimInstance()):nullptr;
 auto* Skin=Body?Body->GetSkeletalMeshAsset():nullptr;
 const auto* Seat=NPC?NPC->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;
 if(!NPC || !Body || !Skin || !Anim || !Seat || !Seat->IsSeatedPresentationActive() || Seat->GetSeatAlpha()<.99f
  || !Body->IsVisible() || !Anim->bAmbientFeetGrounded || Anim->AmbientFeetAlpha<.99f)
  return Fail(TEXT("Cafe52 actual seated mesh/grounded leg IK not settled"));
 if(NPC->GetSpawnPoint()!=Marker || NPC->GetMesh()->GetSkeletalMeshAsset()!=Marker->AmbientMaleSkin.Get() || Skin!=Marker->AmbientMaleSkin.Get()
  || NPC->GetCapsuleComponent()->GetScaledCapsuleRadius()!=34.f || NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()!=90.f)
  return Fail(TEXT("Cafe52 real actor skin/source binding or full-size 34/90cm NPC capsule changed"));
 const auto* Render=Skin->GetResourceForRendering();auto* Weights=Body->GetSkinWeightBuffer(0);
 if(!Render || Render->LODRenderData.IsEmpty() || !Weights || !Render->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer.GetVertexData())
  return Fail(TEXT("Cafe52 actual CPU skin buffers unavailable; no geometry pass claimed"));
 TArray<FMatrix44f> RefToLocal;Body->GetCurrentRefToLocalMatrices(RefToLocal,0);
 if(RefToLocal.Num()!=Skin->GetRefSkeleton().GetNum())return Fail(TEXT("Cafe52 current skin matrix count mismatch"));
 TArray<FVector3f> Vertices;USkinnedMeshComponent::ComputeSkinnedPositions(Body,Vertices,RefToLocal,Render->LODRenderData[0],*Weights);
 if(Vertices.Num()<1000)return Fail(TEXT("Cafe52 actual skinned footwear vertices unavailable"));
 const FName Feet[]={FName(TEXT("foot_l")),FName(TEXT("foot_r"))};FVector Foot[2];
 for(int32 I=0;I<2;++I){if(Body->GetBoneIndex(Feet[I])==INDEX_NONE)return Fail(TEXT("Cafe52 foot binding missing"));Foot[I]=Body->GetSocketLocation(Feet[I]);}
 const FVector Forward=NPC->GetActorForwardVector(),Right=NPC->GetActorRightVector();const FTransform Frame=Body->GetComponentTransform();
 TArray<TSharedPtr<FJsonValue>> Samples;
 for(int32 I=0;I<2;++I)
 {
  FVector Sole=FVector::ZeroVector;double MinimumZ=TNumericLimits<double>::Max();int32 Candidates=0,VertexID=INDEX_NONE;
  for(int32 V=0;V<Vertices.Num();++V)
  {
   const FVector World=Frame.TransformPosition(FVector(Vertices[V]));if(World.ContainsNaN())return Fail(TEXT("Cafe52 actual skin geometry is nonfinite"));
   const FVector Delta=World-Foot[I];
   if(FMath::Abs(FVector::DotProduct(Delta,Forward))>26. || FMath::Abs(FVector::DotProduct(Delta,Right))>12.
    || Delta.Z < -26. || Delta.Z>6. || FVector::DistSquared2D(World,Foot[I])>FVector::DistSquared2D(World,Foot[1-I]))continue;
   ++Candidates;if(World.Z<MinimumZ){MinimumZ=World.Z;Sole=World;VertexID=V;}
  }
  if(Candidates<15 || VertexID==INDEX_NONE)return Fail(FString::Printf(TEXT("Cafe52 %s actual footwear neighbourhood empty (%d vertices)"),*Feet[I].ToString(),Candidates));
  FCollisionQueryParams Q(SCENE_QUERY_STAT(Cafe52ActualShoes),false,NPC);Q.AddIgnoredActor(Marker->AmbientSeatSupport.Get());FHitResult Hit;
  if(!NPC->GetWorld()->LineTraceSingleByChannel(Hit,Sole+FVector(0,0,60),Sole-FVector(0,0,160),ECC_Pawn,Q)
   || Hit.bStartPenetrating || Hit.ImpactNormal.Z<.94 || Hit.ImpactPoint.ContainsNaN())return Fail(TEXT("Cafe52 actual footwear has no valid native Pawn floor"));
  const double Gap=Sole.Z-Hit.ImpactPoint.Z;auto S=MakeShared<FJsonObject>();
  S->SetStringField(TEXT("foot"),Feet[I].ToString());S->SetNumberField(TEXT("actual_shoe_vertex"),VertexID);S->SetNumberField(TEXT("actual_neighbourhood_vertices"),Candidates);
  S->SetStringField(TEXT("actual_shoe_world_cm"),Sole.ToString());S->SetStringField(TEXT("actual_floor_world_cm"),Hit.ImpactPoint.ToString());
  S->SetNumberField(TEXT("actual_floor_normal_z"),Hit.ImpactNormal.Z);S->SetNumberField(TEXT("shoe_floor_gap_cm"),Gap);
  S->SetStringField(TEXT("floor_component"),GetPathNameSafe(Hit.GetComponent()));Samples.Add(MakeShared<FJsonValueObject>(S));
  Row->SetArrayField(TEXT("feet"),Samples);
  if(!FMath::IsFinite(Gap) || Gap<-.5 || Gap>1.5)return Fail(FString::Printf(TEXT("Cafe52 %s actual shoe/floor gap %.6fcm outside [-.5,1.5]"),*Feet[I].ToString(),Gap));
 }
 Row->SetNumberField(TEXT("actual_seated_seconds"),Seat->GetSeatedSeconds());Row->SetNumberField(TEXT("actual_feet_alpha"),Anim->AmbientFeetAlpha);
 Row->SetNumberField(TEXT("production_foot_floor_queries"),Anim->AmbientFootFloorQueries);Row->SetNumberField(TEXT("npc_pose_writes"),0);
 Row->SetBoolField(TEXT("passed"),true);return Row;
}
}
#endif
