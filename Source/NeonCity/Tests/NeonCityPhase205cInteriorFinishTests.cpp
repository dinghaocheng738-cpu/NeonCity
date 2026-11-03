#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandResidence.h"
#include "Adventure/IslandHomeSeat.h"
#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace InteriorFinishNative
{
static FVector VectorField(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field)
{
 const auto& Values=Row->GetArrayField(Field);
 return FVector(Values[0]->AsNumber(),Values[1]->AsNumber(),Values[2]->AsNumber());
}

/** Static finish evidence only. No player relocation/input, facility use, save,
 * reward or completion fixture. Explicit fixed cameras and clock are restored. */
class FFlow : public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* InTest):Test(InTest),Evidence(TEXT("InteriorFinish")) {}

 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;
  PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>240)return End(TEXT("Fixed interior evidence timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  if(!Initialized)
  {
   const bool Motion=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"));
   const bool Stills=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"));
   const bool Metrics=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")) || (!Motion && !(Stills && Metrics)))
    return End(TEXT("Offscreen rendered evidence requires ArtMotion or ArtStills+ArtMetrics; SkipHUD is incompatible"));
   // One call establishes the existing 1600x900 Epic evidence viewport. Static
   // rooms need 21 lossless stills, not repeated synchronous movie readbacks.
   Evidence.Record(W,PC,0);
   FString Text;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/InteriorFinish01/plan.json")))
    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Plan))
    return End(TEXT("Measured InteriorFinish01 plan unavailable"));
   const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
   const TArray<TSharedPtr<FJsonValue>>* Views=nullptr;
   const TArray<TSharedPtr<FJsonValue>>* Times=nullptr;
   if(!Plan->TryGetArrayField(TEXT("placements"),Rows) || Rows->Num()!=19
    || !Plan->TryGetArrayField(TEXT("camera_plan"),Views) || Views->Num()!=7
    || !Plan->TryGetArrayField(TEXT("hours"),Times) || Times->Num()!=3)
    return End(TEXT("Expected 19 measured furnishings and 7 x 3 fixed views"));
   Placements=*Rows;Cameras=*Views;Hours=*Times;
   for(TActorIterator<AIslandLandResidence> I(W);I;++I)
   {
    if(Residence)return End(TEXT("Ambiguous residence origin"));
    Residence=*I;
   }
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)Clock=*I;
   if(!Residence || !Clock)return End(TEXT("Actual residence/clock unavailable"));
   Origin=Residence->GetActorLocation();
   for(TActorIterator<AStaticMeshActor> I(W);I;++I)
    if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.InteriorFinish01")))Added.Add(*I);
   if(Added.Num()!=19)return End(FString::Printf(TEXT("Native finish batch missing or duplicated: %d actors"),Added.Num()));
   Survey();
   OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   Clock->SetCycleEnabled(false);ClockChanged=true;
   Camera=W->SpawnActor<ACameraActor>();
   if(!Camera)return End(TEXT("Comparison camera unavailable"));
   Initialized=true;SetView();return false;
  }
  if(W->GetTimeSeconds()-ViewStarted<3)return false;
  const auto View=Cameras[ViewIndex]->AsObject(),Time=Hours[HourIndex]->AsObject();
  const FString Name=FString::Printf(TEXT("InteriorFinish-%s-%s.png"),*View->GetStringField(TEXT("name")),*Time->GetStringField(TEXT("label")));
  const bool Captured=FNeonCityVisualEvidence::CaptureHUD(W,Name);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("image"),TEXT("../UI/")+Name);
  Row->SetStringField(TEXT("view"),View->GetStringField(TEXT("name")));
  Row->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());
  Row->SetNumberField(TEXT("fov"),Camera->GetCameraComponent()->FieldOfView);
  Row->SetStringField(TEXT("actual_eye_cm"),Camera->GetActorLocation().ToString());
  Row->SetBoolField(TEXT("captured"),Captured);Screenshots.Add(MakeShared<FJsonValueObject>(Row));
  if(!Captured)Failures.Add(TEXT("Actual rendered still failed: ")+Name);
  ++HourIndex;if(HourIndex==Hours.Num()){HourIndex=0;++ViewIndex;}
  if(ViewIndex==Cameras.Num())return End(TEXT(""));
  SetView();return false;
 }

private:
 FVector WorldPoint(FVector M) const {return Origin+FVector(M.X*100,-M.Y*100,M.Z*100);}

 void SurveyFloor(const FString& Label,FVector Ground,bool Capsule)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(InteriorFinishActualFloor),false,P);
  // Ignore only this batch for the support ray; a new rug is not proof that
  // the existing floor beneath furniture or a standing camera is solid.
  for(auto* A:Added)Q.AddIgnoredActor(A);
  FHitResult H;
  const bool Hit=W->LineTraceSingleByChannel(H,Ground+FVector(0,0,50),Ground-FVector(0,0,25),ECC_Pawn,Q);
  const bool Supported=Hit && H.ImpactNormal.Z>.7 && FMath::Abs(H.ImpactPoint.Z-Ground.Z)<3;
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("sample"),Label);
  Row->SetStringField(TEXT("expected_ground_cm"),Ground.ToString());
  Row->SetStringField(TEXT("floor_actor"),GetNameSafe(H.GetActor()));
  Row->SetStringField(TEXT("actual_ground_cm"),H.ImpactPoint.ToString());
  Row->SetNumberField(TEXT("normal_z"),H.ImpactNormal.Z);Row->SetBoolField(TEXT("supported"),Supported);
  if(!Supported)Failures.Add(TEXT("Actual native floor absent/inconsistent: ")+Label);
  if(Capsule)
  {
   // No furniture, wall or facility is ignored for normal standing clearance.
   FCollisionQueryParams All(SCENE_QUERY_STAT(InteriorFinishActualCapsule),false,P);
   TArray<FOverlapResult> Blocks;FString Names;
   const FVector Stand=(Hit?H.ImpactPoint:Ground)+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   W->OverlapMultiByChannel(Blocks,Stand,FQuat::Identity,ECC_Pawn,
    FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),All);
   for(const auto& B:Blocks)if(B.bBlockingHit)Names+=GetNameSafe(B.GetActor())+TEXT("/")+GetNameSafe(B.GetComponent())+TEXT("|");
   Row->SetStringField(TEXT("blocking_components"),Names);
   Row->SetBoolField(TEXT("standing_clear"),Supported && Names.IsEmpty());
   if(!Names.IsEmpty())Failures.Add(TEXT("Normal capsule obstructed: ")+Label+TEXT(" by ")+Names);
  }
  FloorSamples.Add(MakeShared<FJsonValueObject>(Row));
 }

 void Survey()
 {
  for(const auto& Value:Placements)
  {
   auto Row=Value->AsObject();const FString Label=Row->GetStringField(TEXT("label")),Kind=Row->GetStringField(TEXT("kind"));
   AStaticMeshActor* Actor=nullptr;
   for(auto* A:Added)if(A->GetActorLabel()==TEXT("Aurelia ")+Label){Actor=A;break;}
   auto* C=Actor?Actor->GetStaticMeshComponent():nullptr;
   if(!C || !C->GetStaticMesh()){Failures.Add(TEXT("Actual new mesh missing: ")+Label);continue;}
   const bool Solid=Kind==TEXT("furniture");
   if((C->IsCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block)!=Solid)
    Failures.Add(TEXT("Actual furniture/overlay collision mismatch: ")+Label);
   if(!C->GetStaticMesh()->GetOutermost()->GetName().Equals(Row->GetStringField(TEXT("package"))))
    Failures.Add(TEXT("Native mesh differs from declared reuse: ")+Label);
   const FVector Position=VectorField(Row,TEXT("position_m")),Scale=VectorField(Row,TEXT("scale"));
   if(FVector::Dist(Actor->GetActorLocation(),WorldPoint(Position))>2
    || !Actor->GetActorScale3D().Equals(Scale,.001)
    || FMath::Abs(FMath::FindDeltaAngleDegrees(Actor->GetActorRotation().Yaw,Row->GetNumberField(TEXT("yaw_degrees"))))>.1)
    Failures.Add(TEXT("Actual native transform differs from measured plan: ")+Label);
   if(!Solid && Kind!=TEXT("rug"))continue;
   const double Floor=Row->GetNumberField(TEXT("floor_m"));
   const auto& Rect=Row->GetArrayField(TEXT("source_footprint_m"));
   const double X0=Rect[0]->AsNumber(),Y0=Rect[1]->AsNumber(),X1=Rect[2]->AsNumber(),Y1=Rect[3]->AsNumber();
   const FVector Points[]={FVector((X0+X1)*.5,(Y0+Y1)*.5,Floor),FVector(X0,Y0,Floor),
    FVector(X0,Y1,Floor),FVector(X1,Y0,Floor),FVector(X1,Y1,Floor)};
   for(int I=0;I<UE_ARRAY_COUNT(Points);++I)SurveyFloor(Label+FString::Printf(TEXT("-%d"),I),WorldPoint(Points[I]),false);
   const float Gap=C->Bounds.GetBox().Min.Z-WorldPoint(FVector(0,0,Floor)).Z;
   auto Foot=MakeShared<FJsonObject>();Foot->SetStringField(TEXT("actor"),Actor->GetName());
   Foot->SetStringField(TEXT("label"),Label);Foot->SetNumberField(TEXT("native_bottom_above_floor_cm"),Gap);
   Foot->SetStringField(TEXT("collision_profile"),C->GetCollisionProfileName().ToString());
   Foot->SetBoolField(TEXT("foot_contact"),FMath::Abs(Gap-.3f)<1.5f);Furniture.Add(MakeShared<FJsonValueObject>(Foot));
   if(FMath::Abs(Gap-.3f)>=1.5f)Failures.Add(TEXT("New object visibly floats or intersects floor: ")+Label);
  }
  for(const auto& Value:Cameras)
  {
   auto Row=Value->AsObject();FVector At=VectorField(Row,TEXT("eye_m"));At.Z=Row->GetNumberField(TEXT("floor_m"));
   SurveyFloor(TEXT("Camera-")+Row->GetStringField(TEXT("name")),WorldPoint(At),true);
   FCollisionQueryParams Q(SCENE_QUERY_STAT(InteriorFinishActualEye),false,P);
   if(W->OverlapBlockingTestByChannel(WorldPoint(VectorField(Row,TEXT("eye_m"))),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(12),Q))
    Failures.Add(TEXT("Fixed actual eye lies in blocking geometry: ")+Row->GetStringField(TEXT("name")));
  }
  // Read-only probes of the exact already-validated normal standing approaches.
  // No F use, finite completion, checkpoint or cross-process test is repeated.
  for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)
  {
   const FVector Local=I->Kind==EIslandLeisureKind::ExerciseBike?FVector(-20,85,0)
    :I->Kind==EIslandLeisureKind::Sauna?FVector(0,-125,0):FVector(125,0,0);
   FVector At=I->GetActorTransform().TransformPosition(Local);At.Z=Origin.Z+840;
   SurveyFloor(TEXT("Retained-")+I->GetName(),At,true);
  }
  for(TActorIterator<AIslandHomeSeat> I(W);I;++I)if(I->bSleepBed)
  {
   FVector At=I->GetActorTransform().TransformPosition(FVector(0,130,0));At.Z=Origin.Z+1260;
   SurveyFloor(TEXT("Retained-bed-left-approach"),At,true);
  }
  for(TActorIterator<AIslandHomeWardrobe> I(W);I;++I)
  {
   FVector At=I->GetActorLocation()+FVector(155,0,0);At.Z=Origin.Z+1260;
   SurveyFloor(TEXT("Retained-wardrobe-approach"),At,true);
  }
 }

 void SetView()
 {
  const auto View=Cameras[ViewIndex]->AsObject(),Time=Hours[HourIndex]->AsObject();
  const FVector Eye=WorldPoint(VectorField(View,TEXT("eye_m"))),Aim=WorldPoint(VectorField(View,TEXT("aim_m")));
  Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
  Camera->GetCameraComponent()->FieldOfView=View->GetNumberField(TEXT("fov"));
  Clock->SetTimeOfDay(Time->GetNumberField(TEXT("hour")));PC->SetViewTarget(Camera);
  ViewStarted=W->GetTimeSeconds();
 }

 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Failures.Add(Error);
  if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());
  if(Camera)Camera->Destroy();
  if(ClockChanged && IsValid(Clock)){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  for(const auto& Failure:Failures)Test->AddError(Failure);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
  Run=FPaths::MakeValidFileName(Run.IsEmpty()?TEXT("UnlabelledInteriorFinish"):Run);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("InteriorFinish");
  IFileManager::Get().MakeDirectory(*Dir,true);auto Receipt=MakeShared<FJsonObject>();
  Receipt->SetBoolField(TEXT("technical_capture_passed"),Failures.IsEmpty() && Screenshots.Num()==21);
  Receipt->SetStringField(TEXT("scope"),TEXT("Normal rendered PIE, 19 reused native static actors, real floor support and normal capsule/eye clearance. Seven explicit fixed cameras x Day10/Sunset17.8/Night22. Temporary shared-clock/view fixtures restored; no player relocation/input, facility completion, save/reward or new light/exposure/Bloom/material fixture. Cached finite facility, route and save tests are not repeated. Native captures require separate visual review; technical completion is not premium or subjective PASS."));
  Receipt->SetStringField(TEXT("visual_evidence_review"),TEXT("REQUIRED; inspect all 21 actual images"));
  Receipt->SetStringField(TEXT("user_subjective_review"),TEXT("REQUIRED"));
  Receipt->SetStringField(TEXT("remaining_known_issue"),TEXT("Sunset exterior/glass clipping remains unresolved; Private-Window-Control records it openly."));
  Receipt->SetStringField(TEXT("actual_residence_origin_cm"),Origin.ToString());
  Receipt->SetNumberField(TEXT("added_static_actor_count"),Added.Num());
  Receipt->SetArrayField(TEXT("native_floor_capsule_samples"),FloorSamples);
  Receipt->SetArrayField(TEXT("furniture_foot_contact"),Furniture);Receipt->SetArrayField(TEXT("fixed_views"),Screenshots);
  TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& Failure:Failures)Errors.Add(MakeShared<FJsonValueString>(Failure));
  Receipt->SetArrayField(TEXT("failures"),Errors);FString JSON;
  FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&JSON));
  if(!FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("receipt.json"))))Test->AddError(TEXT("Native interior receipt write failed"));
  return true;
 }

 FAutomationTestBase* Test=nullptr;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 AIslandLandResidence* Residence=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;
 TWeakObjectPtr<AActor> OldView;FVector Origin=FVector::ZeroVector;
 TSharedPtr<FJsonObject> Plan;TArray<AStaticMeshActor*> Added;TArray<FString> Failures;
 TArray<TSharedPtr<FJsonValue>> Placements,Cameras,Hours,FloorSamples,Furniture,Screenshots;
 FNeonCityVisualEvidence Evidence;double Started=0;float ViewStarted=0,OldHour=0;
 int32 ViewIndex=0,HourIndex=0;bool Initialized=false,ClockChanged=false,OldCycle=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorFinishVisualPIE,"NeonCity.Phase205c.InteriorFinish.VisualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInteriorFinishVisualPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(InteriorFinishNative::FFlow(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
