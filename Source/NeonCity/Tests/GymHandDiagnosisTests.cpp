#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
namespace GymHandDiagnosis227
{
class FFlow : public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* T):Test(T),Evidence(TEXT("GymHandDiagnosis227")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>75)return End(TEXT("Five-second diagnostic timeout"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
   for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)if(I->Kind==EIslandLeisureKind::ExerciseBike)Facility=*I;
   if(!Facility || !Facility->Furniture || !Facility->ExerciseLoop)return End(TEXT("Native bike/clip missing"));
   const FVector Ground=Facility->GetActorTransform().TransformPosition(FVector(-20,85,0));FCollisionQueryParams Q(SCENE_QUERY_STAT(GymHandDiagnosisStanding),false,P);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,Ground+FVector(0,0,200),Ground-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Actual standing approach lacks support"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual standing approach blocked"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((Facility->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Facility)return End(TEXT("Normal F prompt blocked"));Key(true);Stage=1;At=W->GetTimeSeconds();return false;
  }
  if(Stage==1 && W->GetTimeSeconds()-At>.3)
  {
   Key(false);if(!Facility->IsOccupied() || !Facility->Occupant->GetDriverMesh())return End(TEXT("Normal F occupancy failed"));Stage=2;return false;
  }
  if(Stage==2 && Facility->GetStage()==EIslandLeisureStage::Active && Facility->GetElapsed()>5.)
  {
   if(!Dump())return End(TEXT("Actual pose/mesh/refpose missing"));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=60;
   Camera->SetActorLocation(Facility->GetActorTransform().TransformPosition(FVector(80,-80,135)));
   Camera->SetActorRotation((Facility->GetActorTransform().TransformPosition(FVector(44,0,110))-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);Stage=3;At=W->GetTimeSeconds();return false;
  }
  if(Stage==3 && W->GetTimeSeconds()-At>.3)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Gym227-Actual-Left-Hand.png"));
   Camera->SetActorLocation(Facility->GetActorTransform().TransformPosition(FVector(80,80,135)));
   Camera->SetActorRotation((Facility->GetActorTransform().TransformPosition(FVector(44,0,110))-Camera->GetActorLocation()).Rotation());
   if(!Dump())return End(TEXT("Second diagnostic pose missing"));Stage=4;At=W->GetTimeSeconds();return false;
  }
  if(Stage==4 && W->GetTimeSeconds()-At>.3)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Gym227-Actual-Right-Hand.png"));PC->SetViewTarget(P);Key(true);Stage=5;At=W->GetTimeSeconds();return false;
  }
  if(Stage==5 && W->GetTimeSeconds()-At>.3){Key(false);Stage=6;At=W->GetTimeSeconds();return false;}
  if(Stage==6 && !Facility->IsOccupied())
  {
   if(P->IsHidden() || !P->GetActorEnableCollision() || P->GetAttachParentActor() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal F exit failed walking restoration"));return End(TEXT(""));
  }
  return false;
 }
 bool Dump()
 {
  auto* B=Facility->Occupant->GetDriverMesh();auto* Mesh=B?B->GetSkeletalMeshAsset():nullptr;auto* Anim=B?B->GetSingleNodeInstance():nullptr;if(!Mesh || !Anim || !Mesh->GetSkeleton())return false;
  const auto& Ref=Mesh->GetRefSkeleton();const auto& SkeletonRef=Mesh->GetSkeleton()->GetReferenceSkeleton();TArray<FName> Names={TEXT("root"),TEXT("pelvis"),TEXT("clavicle_l"),TEXT("clavicle_r"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")};
  for(const auto Side:{FString(TEXT("l")),FString(TEXT("r"))})for(const auto Digit:{FString(TEXT("thumb")),FString(TEXT("index")),FString(TEXT("middle")),FString(TEXT("ring")),FString(TEXT("pinky"))})for(int Joint=1;Joint<=3;++Joint)Names.Add(FName(*FString::Printf(TEXT("%s_%02d_%s"),*Digit,Joint,*Side)));
  if(Meta.IsEmpty())
  {
   Meta=FString::Printf(TEXT("driver_mesh=%s\ndriver_skeleton=%s\nplayer_mesh=%s\nclip=%s\nanimation_mode=%d\npostprocess_instance=%s\nrelative_transform=%s\ncomponent_transform=%s\nfixture_transform=%s\nfurniture_transform=%s\nseat_hip_local=%s\nseat_body_yaw=%.6f\n"),*Mesh->GetPathName(),*Mesh->GetSkeleton()->GetPathName(),*GetPathNameSafe(P->GetMesh()->GetSkeletalMeshAsset()),*GetPathNameSafe(Facility->ExerciseLoop),int32(B->GetAnimationMode()),*GetPathNameSafe(B->GetPostProcessInstance()),*B->GetRelativeTransform().ToString(),*B->GetComponentTransform().ToString(),*Facility->GetActorTransform().ToString(),*Facility->Furniture->GetComponentTransform().ToString(),*Facility->SeatHipLocal.ToString(),Facility->SeatBodyYaw);
   Reference=TEXT("bone,parent,mesh_ref_local_x,mesh_ref_local_y,mesh_ref_local_z,mesh_ref_qx,mesh_ref_qy,mesh_ref_qz,mesh_ref_qw,retarget_mode,skeleton_ref_local_x,skeleton_ref_local_y,skeleton_ref_local_z,skeleton_ref_qx,skeleton_ref_qy,skeleton_ref_qz,skeleton_ref_qw\n");
   for(int Index=0;Index<Ref.GetNum();++Index)
   {
    const FName Name=Ref.GetBoneName(Index);const int SI=SkeletonRef.FindBoneIndex(Name);
    const auto& T=Ref.GetRefBonePose()[Index];const auto ST=SI==INDEX_NONE?FTransform::Identity:SkeletonRef.GetRefBonePose()[SI];const FVector V=T.GetLocation(),SV=ST.GetLocation();const FQuat Q=T.GetRotation(),SQ=ST.GetRotation();const int Parent=Ref.GetParentIndex(Index);
    Reference+=FString::Printf(TEXT("%s,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),*Name.ToString(),Parent==INDEX_NONE?TEXT("NONE"):*Ref.GetBoneName(Parent).ToString(),V.X,V.Y,V.Z,Q.X,Q.Y,Q.Z,Q.W,SI==INDEX_NONE?-1:int32(Mesh->GetSkeleton()->GetBoneTranslationRetargetingMode(SI)),SV.X,SV.Y,SV.Z,SQ.X,SQ.Y,SQ.Z,SQ.W);
   }
   const auto T=Facility->Furniture->GetComponentTransform();for(float Y:{-27.f,27.f})for(const FVector Local:{FVector(36,Y,108),FVector(50,Y,110.4)}){const FVector V=T.TransformPosition(Local);Meta+=FString::Printf(TEXT("grip_local=%s world=%s\n"),*Local.ToString(),*V.ToString());}
  }
  for(const FName Name:Names)
  {
   const auto CT=B->GetSocketTransform(Name,RTS_Component);const auto WT=B->GetSocketTransform(Name,RTS_World);const FVector CV=CT.GetLocation(),WV=WT.GetLocation(),FV=Facility->Furniture->GetComponentTransform().InverseTransformPosition(WV);const FQuat Q=CT.GetRotation();
   Samples+=FString::Printf(TEXT("%.6f,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f\n"),Facility->GetElapsed(),Anim->GetCurrentTime(),*Name.ToString(),WV.X,WV.Y,WV.Z,CV.X,CV.Y,CV.Z,FV.X,FV.Y,FV.Z,Q.X,Q.Y,Q.Z,Q.W);
  }
  ++SampleSets;return true;
 }
 void Key(bool Down){if(Held!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool End(const FString& Error)
 {
  Key(false);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("GymHandDiagnosis227");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(Meta,*(Dir/TEXT("mesh-and-alignment.txt")));FFileHelper::SaveStringToFile(Reference,*(Dir/TEXT("reference-bones.csv")));FFileHelper::SaveStringToFile(Samples,*(Dir/TEXT("actual-bones.csv")));
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("collected=%d\nsample_sets=%d\nfailure=%s\nFive seconds of actual normal-F cycle, read-only body/refpose/socket measurements and normal-F exit only. No save/load/30-second completion, threshold changes or source/map mutation. Collection success is not hand-contact PASS.\n"),Error.IsEmpty(),SampleSets,*Error),*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLeisureFacility* Facility=nullptr;ACameraActor* Camera=nullptr;int Stage=0,SampleSets=0;float At=0;double Start=0;bool Held=false;FString Meta,Reference,Samples=TEXT("elapsed,clip_seconds,bone,world_x,world_y,world_z,component_x,component_y,component_z,fixture_x,fixture_y,fixture_z,qx,qy,qz,qw\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGymHandDiagnosis227,"NeonCity.Phase205c.Leisure.GymHandDiagnosis227PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGymHandDiagnosis227::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GymHandDiagnosis227::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
