#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace TheOnePhotoFrontage
{
class FMinimum : public IAutomationLatentCommand
{
public:
 explicit FMinimum(FAutomationTestBase* InTest):Test(InTest){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>100)return Finish(TEXT("Photographed frontage minimum exceeded100seconds"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   bool Found=false;for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_GroundLiftPit481")){Pose=It->GetActorTransform();IslandLevel=It->GetLevel();Found=true;break;}if(!Found)return Finish(TEXT("Reference estate frame missing"));
   const TSet<FName> Rejected={TEXT("StaticMeshActor_274"),TEXT("StaticMeshActor_275"),TEXT("StaticMeshActor_356"),TEXT("StaticMeshActor_357")};int32 RejectedCount=0;bool Water=false,NewMain=false;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    if(It->GetLevel()!=IslandLevel)continue;
    auto* C=It->GetStaticMeshComponent();if(Rejected.Contains(It->GetFName())){if(!It->IsHidden() || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)return Finish(FString::Printf(TEXT("Rejected former-mansion %s remains hidden=%d collision=%d profile=%s"),*It->GetName(),It->IsHidden(),int32(C->GetCollisionEnabled()),*C->GetCollisionProfileName().ToString()));++RejectedCount;}
    if(!C->GetStaticMesh())continue;const FString Name=C->GetStaticMesh()->GetName();
    if(Name==TEXT("SM_TheOne_PhotoMoatWater564")){Water=true;if(C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)return Finish(TEXT("Decorative moat water has solid surface collision"));}
    if(Name==TEXT("SM_TheOne_PhotoMainStructure564")){NewMain=true;if(C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || C->GetCollisionResponseToChannel(ECC_Vehicle)!=ECR_Block)return Finish(TEXT("Main court/floor lacks player/vehicle support"));}
   }
   if(!Water || !NewMain || RejectedCount!=4)return Finish(TEXT("Photographed frontage packages or rejected identities missing"));
   for(TActorIterator<ANeonCityReferenceVegetation> It(W);It;++It)if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.PhotoPalms565")))Palms=It->GetPlantCount();if(Palms!=3)return Finish(TEXT("Actual PIE did not build all three frontage palms"));
   FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOnePhotoFloor),true,P);
   for(FVector Here:{FVector(-46,7,3.065),FVector(-40,10,3.065),FVector(-30,15,3.065),FVector(13.15,16,3.048),FVector(13.15,10,3.048),FVector(13.15,2.5,3.048)})
   // The laid paver tops are1.7cm above the supporting slab; a probe at a
   // genuine6mm joint can hit that slab. Both are valid supported surfaces.
   {FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,S(Here)+FVector(0,0,40),S(Here)-FVector(0,0,30),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-S(Here).Z)>2.2)return Finish(FString::Printf(TEXT("Photo court/frontage lacks supported floor at%s, collider%s"),*Here.ToString(),*GetNameSafe(Hit.GetActor())));++FloorProbes;}
   for(float Y:{16.f,10.f,2.5f})
   {const FVector Here=S(FVector(13.15,Y,3.048))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2);FHitResult Hit;if(W->SweepSingleByChannel(Hit,Here,Here+FVector(.1,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return Finish(FString::Printf(TEXT("Photographed walking edge blocked by%s"),*GetNameSafe(Hit.GetActor())));++ClearanceProbes;}
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OldTime=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);break;}if(!Clock)return Finish(TEXT("Native time cycle unavailable"));
   P->SetActorLocation(S(FVector(13.15,16,3.048))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=62;PC->SetViewTarget(Camera);return SetShot(0);
  }
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   if(Age<3.5f)return false;
   const TCHAR* Views[]={TEXT("FrontageAerial"),TEXT("FrontageClose"),TEXT("EntryCourt")};const TCHAR* Times[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};const FString File=FString::Printf(TEXT("TheOne-Photo-%s-%s.png"),Views[Shot%3],Times[Shot/3]);
   if(!FNeonCityVisualEvidence::CaptureHUD(W,File))return Finish(TEXT("Native rendered photo comparison failed"));++Images;
   if(Shot<8)return SetShot(Shot+1);Clock->SetTimeOfDay(12);PC->SetViewTarget(P);Stage=2;At=W->GetTimeSeconds();return false;
  }
  if(Stage==2)
  {
   const FVector Goal=S(FVector(13.15,2.5,3.048));const FVector Delta=Goal-P->GetActorLocation();WalkDistance=FMath::Max(WalkDistance,FVector::Dist2D(S(FVector(13.15,16,3.048)),P->GetActorLocation()));
   if(Delta.Size2D()<35){Key(false);if(!P->GetCharacterMovement()->IsMovingOnGround() || WalkDistance<1250)return Finish(TEXT("Native frontage walk did not finish supported"));return Finish(TEXT(""));}
   if(Age>25)return Finish(FString::Printf(TEXT("Mapped frontage walk stalled at%s"),*P->GetActorLocation().ToString()));PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Key(true);return false;
  }
  return false;
 }
private:
 FVector S(FVector V)const{return Pose.TransformPosition(FVector(V.X*100,-V.Y*100,V.Z*100));}
 bool SetShot(int32 Index)
 {
  Shot=Index;const FVector Locations[]={FVector(65,5,21),FVector(47,7,5.2),FVector(-48,4,6.5)};const FVector Targets[]={FVector(15,6,5),FVector(14,8,4.5),FVector(-17,13,5.5)};
  Camera->SetActorLocation(S(Locations[Shot%3]));Camera->SetActorRotation((S(Targets[Shot%3])-Camera->GetActorLocation()).Rotation());Clock->SetTimeOfDay(Shot/3==0?12:Shot/3==1?17.8:22);Stage=1;At=W->GetTimeSeconds();return false;
 }
 void Key(bool Down){if(Held==Down || !PC)return;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}
 bool Finish(const FString& Error)
 {
  Key(false);if(!Error.IsEmpty())Test->AddError(Error);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldTime);Clock->SetCycleEnabled(OldCycle);}
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("floor_probes"),FloorProbes);J->SetNumberField(TEXT("capsule_clearance_probes"),ClearanceProbes);J->SetNumberField(TEXT("native_palm_count"),Palms);J->SetNumberField(TEXT("images"),Images);J->SetNumberField(TEXT("normal_input_walk_cm"),WalkDistance);J->SetNumberField(TEXT("initial_player_fixtures"),1);J->SetNumberField(TEXT("route_player_pose_writes"),0);J->SetStringField(TEXT("scope"),TEXT("Photographed frontage correction only; main pool, upper roof, guesthouse and complete estate not accepted"));FString Json;const auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(J,Writer);FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("UI");IFileManager::Get().MakeDirectory(*Directory,true);FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("TheOne-PhotoFrontage.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;ULevel* IslandLevel=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;FTransform Pose;double Start=0;float At=0,OldTime=12;bool OldCycle=true,Held=false;int32 Stage=0,Shot=0,Images=0,FloorProbes=0,ClearanceProbes=0,Palms=0;double WalkDistance=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOnePhotoFrontageMinimum,"NeonCity.Phase205cUpgrade.TheOnePhotoFrontageMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOnePhotoFrontageMinimum::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePhotoFrontage::FMinimum(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
