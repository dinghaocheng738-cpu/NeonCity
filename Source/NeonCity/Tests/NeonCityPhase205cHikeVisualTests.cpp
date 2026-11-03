#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandActivityJob.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace HikeNeedle
{
class FDiagnose:public IAutomationLatentCommand
{
public:
 explicit FDiagnose(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Needle diagnosis timed out"));return true;}
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Camera)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-130000,27000))))return false;
   AIslandActivityJob* Job=nullptr;for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Hike.Summit"))Job=*It;if(!Job)return false;
   FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)){Test->AddError(TEXT("Road source missing"));return true;}
   const auto& Rows=Data->GetArrayField(TEXT("points"));auto Source=[&](int I){auto A=Rows[I]->AsArray();return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());};
   auto Point=[&](int I){const FVector V=Source(I),T=(Source(I+1)-Source(I-1)).GetSafeNormal2D();return Job->GetActorTransform().TransformPosition(FVector((V.X-T.Y)*100,-(V.Y+T.X)*100,V.Z*100+.055*(1-1./3.5)*100+2));};
   const FVector P0=Point(700),Mid=Point(704),Direction=(Point(705)-Point(703)).GetSafeNormal2D(),Side(-Direction.Y,Direction.X,0);
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;Eye=P0-Direction*250+Side*180+FVector(0,0,165);Camera->SetActorLocationAndRotation(Eye,(Mid+FVector(0,0,90)-Eye).Rotation());PC->SetViewTarget(Camera);At=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-At<6)return false;
  FString CSV=TEXT("pixel_x,pixel_y,hit,actor,component,mesh,face,x,y,z\n");FCollisionQueryParams Q(SCENE_QUERY_STAT(HikeVisibleNeedle),true,PC->GetPawn());Q.bReturnFaceIndex=true;TSet<FString> Meshes;
  for(int X=900;X<=1130;X+=10)for(int Y=375;Y<=460;Y+=10)
  {
   FVector Origin,Direction;FHitResult Hit;const bool Found=PC->DeprojectScreenPositionToWorld(X,Y,Origin,Direction) && W->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*30000,ECC_Visibility,Q);
   const auto* C=Cast<UStaticMeshComponent>(Hit.GetComponent());const FString Mesh=C && C->GetStaticMesh()?C->GetStaticMesh()->GetPathName():TEXT("");if(Found && !Mesh.IsEmpty())Meshes.Add(Mesh);
   CSV+=FString::Printf(TEXT("%d,%d,%d,%s,%s,%s,%d,%.3f,%.3f,%.3f\n"),X,Y,Found?1:0,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Mesh,Hit.FaceIndex,Hit.ImpactPoint.X,Hit.ImpactPoint.Y,Hit.ImpactPoint.Z);
  }
  FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hike-Needle-Diagnostic.png"));FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("HikeNeedleDiagnosis");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("actual-pixel-rays.csv")));
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("Read-only real PIE fixed-camera pixel-ray diagnosis of visible needle geometry. Same retained107front camera; no player/vehicle pose, collision, reward, material or scene mutation. This is diagnostic completion, never a visual PASS."));J->SetStringField(TEXT("eye"),Eye.ToString());TArray<TSharedPtr<FJsonValue>> Values;for(const auto& M:Meshes)Values.Add(MakeShared<FJsonValueString>(M));J->SetArrayField(TEXT("hit_meshes"),Values);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));PC->SetViewTarget(PC->GetPawn());Camera->Destroy();return true;
 }
 FAutomationTestBase* Test;ACameraActor* Camera=nullptr;double Start=0;float At=0;FVector Eye;
};
class FExteriorViews:public IAutomationLatentCommand
{
public:
 explicit FExteriorViews(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Start>100)return Finish(PC,TEXT("External road views timed out"));if(!PC || W->GetTimeSeconds()<4)return false;
  if(!Camera)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-130000,27000))))return false;
   AIslandActivityJob* Job=nullptr;for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Hike.Summit"))Job=*It;if(!Job)return false;
   FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return Finish(PC,TEXT("Actual road source unavailable"));
   const auto& Rows=Data->GetArrayField(TEXT("points"));auto Point=[&](int I){auto A=Rows[I]->AsArray();return Job->GetActorTransform().TransformPosition(FVector(A[0]->AsNumber()*100,-A[1]->AsNumber()*100,A[2]->AsNumber()*100+5));};
   const FVector Direction=(Point(705)-Point(703)).GetSafeNormal2D(),Side(-Direction.Y,Direction.X,0);Aim=Point(704)+FVector(0,0,70);
   Eyes.Add(Point(704)-Side*850-Direction*500+FVector(0,0,650));Eyes.Add(Point(704)+Side*1600-Direction*900+FVector(0,0,950));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;PC->SetViewTarget(Camera);
  }
  if(!bPlaced)
  {
   FVector Eye=Eyes[View];FHitResult Surface;FCollisionQueryParams Q(SCENE_QUERY_STAT(HikeExteriorCameraClearance),true,PC->GetPawn());Q.AddIgnoredActor(Camera);
   if(!W->LineTraceSingleByChannel(Surface,Eye+FVector(0,0,10000),Eye-FVector(0,0,20000),ECC_Visibility,Q))return Finish(PC,TEXT("External camera needs an actual supported terrain survey"));
   const float Proposed=Eye.Z;Eye.Z=FMath::Max(Eye.Z,Surface.ImpactPoint.Z+800);
   if(W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(25),Q))return Finish(PC,TEXT("Surveyed exterior camera still intersects real geometry"));
   CSV+=FString::Printf(TEXT("%d,%.3f,%.3f,%.3f,%.3f,%.3f,%s\n"),View,Eye.X,Eye.Y,Proposed,Eye.Z,Surface.ImpactPoint.Z,*GetNameSafe(Surface.GetActor()));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());At=W->GetTimeSeconds();bPlaced=true;return false;
  }
  if(W->GetTimeSeconds()-At<6)return false;FNeonCityVisualEvidence::CaptureHUD(W,View==0?TEXT("Hike-External-Uphill-Overview.png"):TEXT("Hike-External-Ocean-Oblique.png"));bPlaced=false;if(++View<2)return false;
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("HikeExternalViews");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("camera-clearance.csv")));FFileHelper::SaveStringToFile(TEXT("Read-only actual normal-rendered PIE exterior views. The retained old overview was inside the terrain. Each corrected camera is at least8m above actual visibility-traced geometry with25cm sphere clearance; no terrain/player/vehicle/material/exposure/state mutation. Technical capture completion is not a subjective or whole-island visual PASS."),*(Dir/TEXT("scope.txt")));return Finish(PC,TEXT(""));
 }
 bool Finish(APlayerController* PC,const FString& Error){if(PC && Camera){PC->SetViewTarget(PC->GetPawn());Camera->Destroy();Camera=nullptr;}if(!Error.IsEmpty())Test->AddError(Error);return true;}
 FAutomationTestBase* Test;ACameraActor* Camera=nullptr;TArray<FVector> Eyes;FVector Aim;int View=0;bool bPlaced=false;double Start=0;float At=0;FString CSV=TEXT("view,x,y,original_camera_z,external_camera_z,surface_z,surface_actor\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHikeNeedleDiagnosis,"NeonCity.Phase205c.Activity.HikeNeedleDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHikeNeedleDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(HikeNeedle::FDiagnose(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHikeExteriorViews,"NeonCity.Phase205c.Activity.HikeExternalViewsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHikeExteriorViews::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(HikeNeedle::FExteriorViews(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
