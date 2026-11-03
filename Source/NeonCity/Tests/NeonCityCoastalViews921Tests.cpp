#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/NeonCityCoastalPresentation.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Adventure/IslandLandCluster.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "SceneView.h"
#include "ShaderCompiler.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityMarinaWaterAttribution923.h"
#include "Tests/NeonCityMarinaBoatShadowAB923.h"
#include "Tests/NeonCityMarinaSeabedAB923.h"
#include "Tests/NeonCityMarinaOpticalAB923.h"
#include "Tests/NeonCityPierInlay28Evidence.h"
#include "Tests/NeonCitySpeedboatDeck29Evidence.h"
#endif

namespace CoastalViews921
{
struct FShot{const TCHAR* Name;FVector Eye,Aim;float Hour;};
const FShot Shots[]={
 {TEXT("C920View00-Day12"),{-5000,-3000,225},{1000,-700,800},12},
 {TEXT("C920View00-Golden16_5"),{-5000,-3000,225},{1000,-700,800},16.5f},
 {TEXT("C920View00-Sunset17_8"),{-5000,-3000,225},{1000,-700,800},17.8f},
 {TEXT("C920View00-Night22"),{-5000,-3000,225},{1000,-700,800},22},
 {TEXT("CurvedQuay-Yachts-Facades"),{-4600,-2350,215},{-1600,-2300,230},12},
 {TEXT("Cafe-Umbrellas-Golden"),{-2700,-2230,215},{-2200,-1890,175},16.5f},
 {TEXT("Bougainvillea-Near"),{-2600,-1950,410},{-2500,-1640,420},12},
 {TEXT("Bougainvillea-Mid"),{-4200,-2250,215},{-2500,-1600,500},16.5f},
 // Original(-120,65..71)m shots looked into the deliberately grass-free
 // 24m helipad exclusion. These point into the actual delivered density mask.
 {TEXT("Grass-Near"),{-8800,5900,75},{-8800,6200,30},12},
 {TEXT("Grass-Mid"),{-8800,4400,220},{-8800,6200,35},12},
 {TEXT("Grass-Far"),{-12000,6000,1200},{-8800,6200,35},12},
 {TEXT("Grass-NormalEye160cm"),{-8800,5700,185},{-8800,6200,40},12},
 {TEXT("C920View15-CourtyardInterior-Day"),{-270,-1250,640},{-680,-1180,565},12},
 {TEXT("C920View15-CourtyardInterior-Night"),{-270,-1250,640},{-680,-1180,565},22},
 {TEXT("ContainerOlive-Near"),{-5850,-2160,200},{-5600,-1810,210},12},
 {TEXT("ContainerHerbs-Near"),{-3000,-2140,160},{-2800,-1810,155},12},
 {TEXT("ContainerOlive-Mid"),{-6400,-2300,215},{-5600,-1810,210},16.5f},
 {TEXT("ContainerOlive-NormalEye"),{-5800,-2050,215},{-5600,-1810,215},12},
 // Additional scenery fixtures; every earlier comparison identity stays put.
 {TEXT("Headland-Seaward"),{17200,-6500,215},{19100,1700,1250},12},
 {TEXT("Headland-PromenadeEnd"),{14500,-2350,215},{19300,1500,1200},16.5f},
 {TEXT("Headland-Aerial"),{15000,-6500,6500},{18700,4600,1200},12},
 {TEXT("Headland-Night"),{14500,-2350,215},{19300,1500,1200},22},
 {TEXT("Headland-WideAerial"),{16000,-18000,11000},{30000,4000,1800},12},
 // Ground-supported1.6m eyes, sphere-clear of all actual source terrain/rock
 // triangles. Original0..22 comparisons keep their exact identities/poses.
 {TEXT("Outcrop04-Close"),{27528.1013,1631.2525,280.5940},{27818.0046,1906.8538,389.6493},12},
 {TEXT("Outcrop02-ShoreClose"),{23589.9121,-6270.1059,185.4614},{23816.8198,-6073.8589,384.8958},12},
 {TEXT("Outcrop02-SlopeClose"),{25530.9352,-5434.2156,1061.9015},{25620.8068,-5147.9935,1170.8479},12},
 {TEXT("Outcrop02-RockTipClose"),{32122.7644,-1082.8817,529.1449},{32405.2739,-1183.8192,455.4751},12},
 // Exact retained candidate0; source-derived1.6m eyes, verified again against
 // actual native collision below. These inspect the base and full crown.
 {TEXT("PineB-RootClose"),{29407.57136,-80.23087,266.1210},{29807.57136,719.76913,571.42082},12},
 {TEXT("PineB-CrownMid"),{28807.57136,-1880.23087,565.41851},{29807.57136,719.76913,1101.42082},12},
 {TEXT("Iceplant-StandingClose"),{25592.74010,-6411.88310,668.14767},{25592.74010,-6171.88310,588.51095},12},
 {TEXT("Rooibos-StandingClose"),{21268.47343,2091.13252,367.82143},{21268.47343,2331.13252,260.38562},12},
 // Additional Town9 fixtures use the actual frozen lane/finished-floor
 // elevations. Earlier0..30 camera identities remain unchanged.
 {TEXT("Town9-RearLane-Day"),{-6500,9400,358.441722},{0,10800,750},12},
 {TEXT("Town9-Rear01-Stairs"),{-9800,9400,352.514583},{-9800,10450,650},12},
 {TEXT("Town9-Rear01-FirstFloor"),{-9800,10480,676.524615},{-9600,11000,660},12},
 {TEXT("Town9-Loggia"),{17600,10650,625.677226},{17600,10900,625},12},
 {TEXT("Town9-RearLane-Night"),{-6500,9400,358.441722},{0,10800,750},22},
 // Frozen Grass11 density/height witnesses. The35cm eye is an explicitly
 // low diagnostic camera;36/38 are normal1.6m standing eyes.
 {TEXT("Grass11-East-Standing"),{21506.25,-2243.75,564.022360},{21906.25,-2193.75,519.667966},12},
 {TEXT("Grass11-East-GrazingDiagnostic"),{34606.25,6806.25,352.093039},{34906.25,6806.25,310.452837},12},
 {TEXT("Grass11-East-MidFar"),{36106.25,4506.25,473.837874},{39006.25,7006.25,301.954610},12},
 // Source Walk triangles support these shore cameras at +0.55m. Actual
 // native floor, clear eye and adopted stern bridges are checked below.
 {TEXT("Stern13-ShoreJoint-Near"),{-2000,-2425,215},{-2000,-2665,-10},12},
 {TEXT("Stern13-Oblique-Mid"),{-2400,-2425,215},{-2000,-2820,15},12},
 {TEXT("Stern13-Marina-ReferenceWide"),{-5300,-2590,215},{-1100,-2300,180},16.5f},
 {TEXT("Stern13-ShoreJoint-Night"),{-2000,-2425,215},{-2000,-2665,-10},22},
 // Actual promenade eyes for the independent bark-only near/far copies.
 {TEXT("PalmBark23-Trunk-NormalEye"),{6500,-2285,215},{6500,-1786.979827,315},12},
 {TEXT("PalmBark23-FarRepresentation"),{-7500,-2250,215},{6500,-1786.979827,500},12},
 // Independent Pier21 detail views. Quay eyes are1.6m above the actual
 // +.55m walk plane; lower-deck eye is1.6m above its retained-.40m top.
 {TEXT("Pier21-ShoreHinge-NormalEye"),{5000,-2250,215},{5000,-2700,20},12},
 // Native line support at this exact point is-39.400cm (the retained
 // flush seam), recorded in Pier21CameraDiagnostic1; keep the strict gate.
 {TEXT("Pier21-DeckFinish-NormalEye"),{5000,-4400,120.6},{4670,-4700,20},12},
 {TEXT("Pier21-QuayOblique-NormalEye"),{5500,-2250,215},{5000,-4300,-20},12},
 {TEXT("Pier21-ShoreHinge-Night"),{5000,-2250,215},{5000,-2700,20},22}
};
// The same immutable tree mesh now fills the authored headland forest.
// This is a scenery inventory ceiling, not a runtime quality/LOD reduction.
constexpr int32 HeadlandForestInstanceBudget=384;

class FViews : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<APlayerController> Controller;TWeakObjectPtr<ACharacter> Player;
 TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<AActor> OldView;TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 FTransform OldPlayerPose;float OldHour=12,AtGame=0;double Began=FPlatformTime::Seconds(),AtWall=0;bool OldCycle=true,Fixture=false,Cleaned=false;int32 Index=0,Frames=0,Failed=0;
 int32 First=0,Limit=14,SelectionCursor=0,PreviousShot=INDEX_NONE;TArray<int32> Selection;FString Directory;TArray<TSharedPtr<FJsonValue>> Captures;TSharedPtr<FJsonObject> ViewDiagnostics;
 bool bBoatShadowDiagnostic=false,bBoatShadowRestorePass=true,bSeabedDiagnostic=false,bSeabedRestorePass=true,bOpticalDiagnostic=false,bOpticalRestorePass=true;
#if WITH_EDITOR
 C923MarinaWaterAttribution::FBoatShadowAB1 BoatShadowProbe;
 C923MarinaOptical::FAbsorptionAB OpticalProbe;
 C923MarinaWaterAttribution::FSeabedAB1 SeabedProbe;
#endif
 struct FGrassGuard{TWeakObjectPtr<UStaticMeshComponent> Component;TWeakObjectPtr<UStaticMesh> Mesh;bool Shadow=false;};TArray<FGrassGuard> GrassGuards;
 void Cleanup()
 {
  if(Cleaned)return;Cleaned=true;
#if WITH_EDITOR
  bBoatShadowRestorePass=BoatShadowProbe.Restore() && bBoatShadowRestorePass;
  bOpticalRestorePass=OpticalProbe.Restore() && bOpticalRestorePass;
  bSeabedRestorePass=SeabedProbe.Restore() && bSeabedRestorePass;
#endif
  if(Controller.IsValid() && OldView.IsValid())Controller->SetViewTarget(OldView.Get());
  if(Camera.IsValid())Camera->Destroy();
  if(Clock.IsValid()){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  if(Fixture && Player.IsValid())Player->SetActorTransform(OldPlayerPose,false,nullptr,ETeleportType::TeleportPhysics);
 }
 bool End(const FString& Error=FString())
 {
  if(!Error.IsEmpty()){Test->AddError(Error);++Failed;}
  const int32 ExpectedViews=Selection.IsEmpty()?Limit-First:Selection.Num();
  if(Error.IsEmpty() && Captures.Num()!=ExpectedViews){Test->AddError(TEXT("Comparison did not capture every selected fixture"));++Failed;}
  bool GrassUnchanged=true;for(const auto& G:GrassGuards)GrassUnchanged&=G.Component.IsValid() && G.Component->GetStaticMesh()==G.Mesh.Get() && G.Component->CastShadow==G.Shadow;
  if(!GrassUnchanged){Test->AddError(TEXT("Actual grass mesh/shadow policy changed during comparison capture"));++Failed;}
  Cleanup();auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failed==0);J->SetStringField(TEXT("failure"),Error);J->SetArrayField(TEXT("views"),Captures);
  if(!bBoatShadowRestorePass){Test->AddError(TEXT("Diagnostic boat shadow flags or restore receipt did not restore successfully"));++Failed;J->SetBoolField(TEXT("passed"),false);}
  if(!bSeabedRestorePass){Test->AddError(TEXT("Diagnostic seabed visibility or unchanged-state receipt did not restore successfully"));++Failed;J->SetBoolField(TEXT("passed"),false);}
  if(!bOpticalRestorePass){Test->AddError(TEXT("Runtime absorption diagnostic failed exact restoration"));++Failed;J->SetBoolField(TEXT("passed"),false);}
  J->SetBoolField(TEXT("diagnostic_absorption_only"),bOpticalDiagnostic);J->SetBoolField(TEXT("absorption_parameter_restored"),bOpticalRestorePass);
  J->SetBoolField(TEXT("diagnostic_b_boat_shadow_off"),bBoatShadowDiagnostic);
  J->SetBoolField(TEXT("diagnostic_seabed_hidden"),bSeabedDiagnostic);
  J->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);
  J->SetBoolField(TEXT("boat_shadow_flags_restored"),bBoatShadowRestorePass);
  J->SetBoolField(TEXT("seabed_visibility_and_other_flags_restored"),bSeabedRestorePass);
#if WITH_EDITOR
  J->SetNumberField(TEXT("actual_optical_parameter_writes"),OpticalProbe.GetWrites());J->SetNumberField(TEXT("actual_optical_restore_writes"),OpticalProbe.GetRestores());
  J->SetNumberField(TEXT("actual_boat_shadow_disable_writes"),BoatShadowProbe.GetDisableWrites());
  J->SetNumberField(TEXT("actual_boat_shadow_restore_writes"),BoatShadowProbe.GetRestoreWrites());
  J->SetNumberField(TEXT("actual_seabed_visibility_hide_writes"),SeabedProbe.GetHideWrites());
  J->SetNumberField(TEXT("actual_seabed_visibility_restore_writes"),SeabedProbe.GetRestoreWrites());
#endif
  if(ViewDiagnostics.IsValid())J->SetObjectField(TEXT("last_actual_view_diagnostics"),ViewDiagnostics);
  J->SetNumberField(TEXT("expected_views"),ExpectedViews);J->SetNumberField(TEXT("initial_supported_player_fixtures"),Fixture?1:0);J->SetNumberField(TEXT("player_pose_restores"),Fixture?1:0);
  TArray<TSharedPtr<FJsonValue>> Indices;if(Selection.IsEmpty()){for(int32 I=First;I<Limit;++I)Indices.Add(MakeShared<FJsonValueNumber>(I));}
  else for(int32 I:Selection)Indices.Add(MakeShared<FJsonValueNumber>(I));J->SetArrayField(TEXT("selected_fixture_indices"),Indices);
  J->SetNumberField(TEXT("npc_pose_writes"),0);J->SetNumberField(TEXT("grass_mesh_writes"),0);J->SetNumberField(TEXT("grass_shadow_writes"),0);J->SetBoolField(TEXT("observed_grass_policy_unchanged"),GrassUnchanged);
  J->SetNumberField(TEXT("lighting_quality_writes"),0);J->SetNumberField(TEXT("exposure_bloom_override_writes"),0);J->SetBoolField(TEXT("clock_and_view_restored"),Cleaned);
  J->SetStringField(TEXT("scope"),bOpticalDiagnostic
   ?TEXT("DIAGNOSTIC ABSORPTION ONLY: one temporary runtime MID vector, exact40/41 normal Lit fixtures; restore original vector, no native asset/geometry/field/physics/input/quality changes; never beauty or performance PASS.")
   :bSeabedDiagnostic
   ?TEXT("DIAGNOSTIC B ONLY, fixed40/41: exact two actual mainland NoCollision cosmetic seabed components temporarily invisible, then original raw visibility restored. All other observed flags/mesh/material/transform/collision and Ocean/Field/depth unchanged. Same normal Lit camera/hour/quality and20s settling. No boat/Ocean pose, physics, input, quality, CVar, asset or save writes. Never production beauty, art acceptance or performance PASS. Existing observer/camera/clock cleanup matches normal A.")
   :bBoatShadowDiagnostic
   ?TEXT("DIAGNOSTIC B ONLY, fixture40: actual two main marina boats' runtime mesh CastShadow temporarily disabled, then original flags restored. Same normal Lit camera/hour/quality and20s settling. No material/visibility/collision/physics/input/render-CVar/asset/save writes. Never production beauty, art acceptance or performance PASS. Observer/camera/clock cleanup matches normal comparison.")
   :TEXT("Fixed comparison camera fixtures, including exact C920 View00 and courtyard View15. One supported player observer fixture keeps normal nearby cafe population active; no NPC moves/state writes. Camera/clock restored, no lighting/quality/exposure/grass overrides. This is visual evidence, not continuous player traversal or a performance sample."));
  FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));if(!Directory.IsEmpty())
  {const FString File=Directory/TEXT("receipt.json");if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Json,*File))Test->AddError(TEXT("Comparison receipt already exists or cannot be written; never overwrite earlier evidence"));}
  return true;
 }
 void Place()
 {
  const auto& S=Shots[Index];Camera->GetCameraComponent()->FieldOfView=65;Camera->SetActorLocationAndRotation(S.Eye,(S.Aim-S.Eye).Rotation());Clock->SetTimeOfDay(S.Hour);
  AtWall=FPlatformTime::Seconds();AtGame=World->GetTimeSeconds();Frames=0;
 }
public:
 explicit FViews(FAutomationTestBase* T,bool FacadeOnly=false,bool GrassOnly=false,bool InteriorOnly=false,bool ContainerOnly=false,bool HeadlandOnly=false,bool BoatShadowDiagnostic=false,bool SeabedDiagnostic=false,bool OpticalDiagnostic=false):Test(T),bBoatShadowDiagnostic(BoatShadowDiagnostic),bSeabedDiagnostic(SeabedDiagnostic),bOpticalDiagnostic(OpticalDiagnostic)
 {if(bOpticalDiagnostic){First=18;Index=40;Limit=UE_ARRAY_COUNT(Shots);Selection={40,41};}else if(bSeabedDiagnostic){First=18;Index=40;Limit=UE_ARRAY_COUNT(Shots);Selection={40,41};}else if(bBoatShadowDiagnostic){First=18;Index=40;Limit=UE_ARRAY_COUNT(Shots);Selection={40};}else if(FacadeOnly){First=0;Index=First;Limit=8;}else if(GrassOnly){First=8;Index=First;Limit=12;}else if(InteriorOnly){First=12;Index=First;Limit=14;}else if(ContainerOnly){First=14;Index=First;Limit=18;}else if(HeadlandOnly){First=18;Index=First;Limit=UE_ARRAY_COUNT(Shots);}}
 virtual ~FViews(){Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>480)return End(TEXT("Coastal visual reference capture timed out"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;World=W;
  auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!PC || !P)return false;
  if(!Camera.IsValid())
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return End(TEXT("ReferenceViews requires actual rendered stills; launch with NeonCityArtStills"));
   FString Requested;
   // Distinct diagnostics have their own fixed selection. The process-wide
   // normal A selection must not expand or replace either diagnostic B.
   const TCHAR* Selector=First==18?TEXT("C923HeadlandViewIndices="):First==0 && Limit==8?TEXT("C923FacadeViewIndices="):First==14?TEXT("C923ContainerViewIndices="):nullptr;
   if(Selector && !bBoatShadowDiagnostic && !bSeabedDiagnostic && !bOpticalDiagnostic && FParse::Value(FCommandLine::Get(),Selector,Requested,false))
   {
    TArray<FString> Parts;Requested.ParseIntoArray(Parts,TEXT(","),true);
    for(const FString& Part:Parts)
    {
     int32 Value=0;if(!Part.IsNumeric() || !LexTryParseString(Value,*Part) || Value<First || Value>=Limit || Selection.Contains(Value))
      return End(TEXT("Selected comparison views must be unique valid fixture indices"));
     Selection.Add(Value);
    }
    if(Selection.IsEmpty())return End(TEXT("Comparison view selection cannot be empty"));
    Index=Selection[0];
   }
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);if(Run.IsEmpty())return End(TEXT("ReferenceViews requires a unique NeonCityArtRun evidence label"));
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/(bOpticalDiagnostic?TEXT("MarinaOpticalDiagnostic923"):bSeabedDiagnostic?TEXT("MarinaSeabedDiagnostic923"):bBoatShadowDiagnostic?TEXT("MarinaBoatShadowDiagnostic923"):First==18?TEXT("HeadlandViews923"):First==14?TEXT("ContainerPlantingViews922"):First==8?TEXT("GrassCoverageViews921"):Limit==8?TEXT("ReferenceFacadeViews921"):TEXT("ReferenceViews921"));
   IFileManager::Get().MakeDirectory(*Directory,true);if(IFileManager::Get().FileExists(*(Directory/TEXT("receipt.json"))))return End(TEXT("Completed comparison evidence must be cached, not repeated under the same label"));
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   if(!Clock.IsValid())return End(TEXT("Actual persistent day/night clock missing"));
   Controller=PC;Player=P;OldView=PC->GetViewTarget();OldPlayerPose=P->GetActorTransform();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   FVector Start(-2200,-2250,200);FCollisionQueryParams Q(SCENE_QUERY_STAT(C921VisualObserver),false,P);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,Start+FVector(0,0,100),Start-FVector(0,0,400),ECC_Pawn,Q) || H.ImpactNormal.Z<.94)return End(TEXT("Declared cafe observer fixture has no real quay floor"));
   Start.Z=H.ImpactPoint.Z+P->GetSimpleCollisionHalfHeight()+2;
   if(W->OverlapBlockingTestByChannel(Start,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()-1),Q))return End(TEXT("Declared cafe observer fixture is physically blocked"));
   P->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);Fixture=true;
   for(TActorIterator<AActor> I(W);I;++I)
   {TInlineComponentArray<UStaticMeshComponent*> Components(*I);for(auto* C:Components)if(C->GetStaticMesh() && (C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_C921_Lawn")) || C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_C923_Grass11")) || C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_C923_Ground32Grass"))))GrassGuards.Add({C,C->GetStaticMesh(),bool(C->CastShadow)});}
   if(GrassGuards.Num()!=3)return End(TEXT("Expected actual three delivered near/mid/far lawn components"));
   Camera=W->SpawnActor<ACameraActor>();if(!Camera.IsValid())return End(TEXT("Comparison camera could not spawn"));Clock->SetCycleEnabled(false);PC->SetViewTarget(Camera.Get());
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("C923Optics27Fix")) && !C923MarinaOptical::ValidateOwnedMaterial(W,Directory/TEXT("actual-native-ocean-binding.json")))
    return End(TEXT("Local optics27 normal evidence requires exact cold-start owned material/field and unchanged base absorption"));
   if(bOpticalDiagnostic && !OpticalProbe.Begin(W,Directory/TEXT("OpticalAttribution")))return End(TEXT("Absorption diagnostic refused its actual provider or readable parameter"));
   if(bBoatShadowDiagnostic && !BoatShadowProbe.Begin(W,Directory/TEXT("WaterAttribution")))return End(TEXT("Diagnostic boat shadow probe refused its actual scene or unique receipt"));
   if(bSeabedDiagnostic && !SeabedProbe.Begin(W,Directory/TEXT("WaterAttribution")))return End(TEXT("Diagnostic seabed probe refused exact two observed NoCollision components, unchanged ocean field or unique receipt"));
#else
   if(bBoatShadowDiagnostic)return End(TEXT("Boat shadow diagnostic requires the editor evidence capture build"));
   if(bSeabedDiagnostic)return End(TEXT("Seabed diagnostic requires the editor evidence capture build"));
#endif
   Place();return false;
  }
  ++Frames;const double WallAge=FPlatformTime::Seconds()-AtWall;const float GameAge=W->GetTimeSeconds()-AtGame;
  if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
  {AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();Frames=0;return false;}
  // Abrupt fixture clock changes need the same GI/exposure settling that
  // normal gradual gameplay gets. A 20s unchanged Nanite baseline proved
  // the early night grid was stale lighting, not damaged facade geometry.
  const bool Night=Shots[Index].Hour>=21.f;
  const bool AfterNight=PreviousShot!=INDEX_NONE && Shots[PreviousShot].Hour>=21.f;
  const bool LongSettle=Night || AfterNight || !Selection.IsEmpty();
  if(WallAge<(LongSettle?20.0:3.25) || GameAge<(LongSettle?20.f:3.f) || Frames<24)return false;
  if(Index>=8 && Index<12)
  {
   // A cell counter can be nonzero while the shot itself aims at a masked
   // helipad. The actual delivered data must contain this target, and its
   // native collision floor must match the source mask/height proof.
   ANeonCityReferenceVegetation* Lawn=nullptr;
   for(TActorIterator<ANeonCityReferenceVegetation> I(W);I;++I)
    if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("NeonCityReferenceVegetation_0")){Lawn=*I;break;}
   const FVector Target=Shots[Index].Aim;const int32 CellX=FMath::FloorToInt((Target.X*.01f+1024.f)/8.f);
   const int32 CellY=FMath::FloorToInt((-Target.Y*.01f+1024.f)/8.f);const int32 CellIndex=CellY*256+CellX;
   if(!Lawn || !Lawn->GrassData || !(Lawn->GrassData->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/"))
      || Lawn->GrassData->GetName()==TEXT("DA_C923_Grass11") || Lawn->GrassData->GetName()==TEXT("DA_C923_Ground32Grass"))
    || !Lawn->GrassData->Cells.ContainsByPredicate([&](const FNeonCityReferenceGrassCell& C){return C.Index==CellIndex;}))
    return End(TEXT("Corrected grass target is not in the actual delivered mainland grass cell data"));
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C921GrassTargetSupport),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,FVector(Target.X,Target.Y,200),FVector(Target.X,Target.Y,-50),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
    || Ground.ImpactNormal.Z<.98f || FMath::Abs(Ground.ImpactPoint.Z-25.f)>.1f)
    return End(TEXT("Corrected grass target lacks its actual25cm native static terrain support"));
  }
  const auto& S=Shots[Index];
#if WITH_EDITOR
  if(FParse::Param(FCommandLine::Get(),TEXT("C923Pier28Fix")) && !C923Pier28::Validate(W,Directory/FString::Printf(TEXT("actual-pier28-%02d.json"),Index),Night))
   return End(TEXT("Pier28 normal evidence failed actual support/collision/day-night shared light budget"));
  if(Index==46 && FParse::Param(FCommandLine::Get(),TEXT("C923Deck29Fix")) && !C923Deck29::Validate(W,Directory/TEXT("actual-speedboat-deck29.json")))
   return End(TEXT("Deck29 normal close evidence requires its actual native appearance and unchanged physical profile/hull/identity"));
#endif
  if(Index>=27 && Index<=30)
  {
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923PineEyeGround),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,500),S.Eye-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
    || !Ground.GetActor() || Ground.GetActor()->GetFName()!=TEXT("StaticMeshActor_183") || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-160.f)>.1f)
    return End(TEXT("Vegetation normal-eye fixture lacks its exact native terrain support"));
   if(W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("Vegetation normal-eye camera overlaps actual blocking geometry"));
  }
  if(Index>=31 && Index<=35)
  {
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923TownEyeGround),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,20),S.Eye-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
     || !Ground.GetActor() || !Ground.GetActor()->ActorHasTag(TEXT("NeonCity.MainlandC923.Town9"))
     || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-160.f)>.1f
     || W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("Town standing-eye fixture lacks the declared actual first-floor/lane support or overlaps native geometry"));
  }
  if(Index>=36 && Index<=38)
  {
   ANeonCityReferenceVegetation* Lawn=nullptr;
   for(TActorIterator<ANeonCityReferenceVegetation> I(W);I;++I)
    if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("NeonCityReferenceVegetation_0")){Lawn=*I;break;}
   const int32 CellX=FMath::FloorToInt((S.Aim.X*.01f+1024.f)/8.f);
   const int32 CellY=FMath::FloorToInt((-S.Aim.Y*.01f+1024.f)/8.f);const int32 CellIndex=CellY*256+CellX;
   if(!Lawn || !Lawn->GrassData || !(Lawn->GrassData->GetName()==TEXT("DA_C923_Grass11") || Lawn->GrassData->GetName()==TEXT("DA_C923_Ground32Grass")))
    return End(TEXT("East grass comparison requires the imported Grass11 data"));
   if(Lawn->GrassData->Cells.Num()!=1361 || !Lawn->GrassData->Cells.ContainsByPredicate([&](const FNeonCityReferenceGrassCell& C){return C.Index==CellIndex;}))
    return End(TEXT("East grass target is missing from the actual1361 source/native cells"));
   const float EyeHeight=Index==37?35.f:160.f;
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923Grass11EyeSupport),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,20),S.Eye-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
      || !Ground.GetActor() || Ground.GetActor()->GetFName()!=TEXT("StaticMeshActor_183")
      || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-EyeHeight)>.5f || Ground.ImpactNormal.Z<.94f
      || W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("East grass camera lacks its actual slope support/clearance within the frozen4mm height-field bound"));
   auto* Grass=Lawn->FindComponentByClass<UNeonCityReferenceGrassComponent>();
   if(!Grass || Grass->GetActiveCellCount(0)+Grass->GetActiveCellCount(1)+Grass->GetActiveCellCount(2)==0)
    return End(TEXT("East grass evidence has no actual rendered grass cells"));
  }
  if(Index>=39 && Index<=42)
  {
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923SternShoreEye),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,20),S.Eye-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
     || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-160.f)>.1f
     || W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("Stern bridge shore camera lacks its actual normal-height floor or clear eye"));
   TSet<FString> Bridges;
   for(TActorIterator<AActor> I(W);I;++I)
   {
    if(I->GetLevel()!=W->PersistentLevel || !I->ActorHasTag(TEXT("NeonCity.MainlandC923.SternPasserelles13")))continue;
    auto* C=I->FindComponentByClass<UStaticMeshComponent>();
    const FString Name=C?GetNameSafe(C->GetStaticMesh()):FString();
    if(!C || !(Name==TEXT("SM_C923_SternPasserelle13_0") || Name==TEXT("SM_C923_SternPasserelle13_1"))
      || !C->IsVisible() || I->IsHidden() || C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || Bridges.Contains(Name))
     return End(TEXT("Stern comparison has missing, duplicate or nonphysical adopted bridge geometry"));
    Bridges.Add(Name);
   }
   if(Bridges.Num()!=2)return End(TEXT("Stern comparison requires both actual adopted thin bridges"));
  }
  if(Index==43 || Index==44)
  {
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923PalmBarkEye),false,P);
   if(!W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,20),S.Eye-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
     || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-160.f)>.1f
     || W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q))
    return End(TEXT("Bark comparison requires actual supported normal-height promenade eyes"));
   ANeonCityReferenceVegetation* Palms=nullptr;
   for(TActorIterator<ANeonCityReferenceVegetation> I(W);I;++I)
    if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("NeonCityReferenceVegetation_4")){Palms=*I;break;}
   int32 Copies=0;
   if(Palms)
   {
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(Palms);
    for(auto* C:Components)
    {
     const FString Mesh=GetNameSafe(C->GetStaticMesh());
     if(Mesh!=TEXT("SM_C923_PalmNearBark23") && Mesh!=TEXT("SM_C923_PalmFarBark23"))continue;
     auto* Material=C->GetMaterial(0);
     if(!Material || !Material->GetMaterial() || Material->GetMaterial()->GetName()!=TEXT("M_C923_PalmBark23")
       || C->GetInstanceCount()!=12 || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || !C->IsVisible())
      return End(TEXT("Fresh BeginPlay palm material/instance/collision chain differs from the adopted bark copies"));
     ++Copies;
    }
   }
   if(Copies!=2)return End(TEXT("Both runtime near/far palm representations must retain the owned bark material"));
  }
  if(Index>=45 && Index<=48)
  {
   FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923Pier21DetailEye),false,P);
   const bool HasGround=W->LineTraceSingleByObjectType(Ground,S.Eye+FVector(0,0,20),S.Eye-FVector(0,0,220),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
   const bool EyeBlocked=W->OverlapBlockingTestByChannel(S.Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q);
   auto Support=MakeShared<FJsonObject>();Support->SetNumberField(TEXT("index"),Index);
   Support->SetBoolField(TEXT("actual_static_line_hit"),HasGround);Support->SetStringField(TEXT("actual_actor"),GetPathNameSafe(Ground.GetActor()));
   Support->SetStringField(TEXT("actual_actor_name"),GetNameSafe(Ground.GetActor()));Support->SetStringField(TEXT("actual_component"),GetPathNameSafe(Ground.GetComponent()));
   Support->SetStringField(TEXT("actual_impact_cm"),Ground.ImpactPoint.ToString());Support->SetStringField(TEXT("actual_normal"),Ground.ImpactNormal.ToString());
   Support->SetNumberField(TEXT("actual_eye_height_cm"),S.Eye.Z-Ground.ImpactPoint.Z);Support->SetBoolField(TEXT("actual_eye_overlap"),EyeBlocked);
   FString SupportText;FJsonSerializer::Serialize(Support,TJsonWriterFactory<>::Create(&SupportText));
   const FString SupportPath=Directory/FString::Printf(TEXT("Pier21-ActualEyeSupport-%02d.json"),Index);
   if(IFileManager::Get().FileExists(*SupportPath) || !FFileHelper::SaveStringToFile(SupportText,*SupportPath))return End(TEXT("Preserve prior camera support evidence; use a unique run label"));
   if(!HasGround
      || FMath::Abs(S.Eye.Z-Ground.ImpactPoint.Z-160.f)>.1f
      || (Index==46 && (!Ground.GetActor() || Ground.GetActor()->GetFName()!=TEXT("StaticMeshActor_1656")))
      || EyeBlocked)
    return End(TEXT("Pier21 detail eye lacks its actual normal-height static support or clear occupancy"));
   int32 Piers=0;
   for(TActorIterator<AActor> I(W);I;++I)
   {
    auto* C=I->FindComponentByClass<UStaticMeshComponent>();
    if(!C || GetNameSafe(C->GetStaticMesh())!=TEXT("SM_C923_SpeedboatPier21"))continue;
    if(I->GetLevel()!=W->PersistentLevel || I->GetFName()!=TEXT("StaticMeshActor_1656") || !C->IsVisible() || I->IsHidden()
       || C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || C->GetCollisionObjectType()!=ECC_WorldStatic)
     return End(TEXT("Pier21 comparison requires the adopted visible physical original pier actor"));
    ++Piers;
   }
   if(Piers!=1)return End(TEXT("Pier21 comparison requires exactly one adopted original pier"));
  }
  auto* Client=W->GetGameViewport();auto* Local=PC->GetLocalPlayer();
  if(!Client || !Client->Viewport || !Local || Client->ViewModeIndex!=VMI_Lit || Client->EngineShowFlags.VisualizeBuffer)
   return End(TEXT("Production comparison requires the normal Lit viewport"));
  int32 CalibrationRegions=0;for(TActorIterator<AActor> I(W);I;++I)
   if(I->GetLevel()==W->PersistentLevel && I->ActorHasTag(TEXT("NeonCity.MainlandC923.ExposureCalibration7")))++CalibrationRegions;
  if(CalibrationRegions!=1)return End(TEXT("Production comparison requires the one authored local exposure region"));
  FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport,W->Scene,Client->EngineShowFlags).SetRealtimeUpdate(true));
  FVector ActualEye;FRotator ActualRotation;auto* View=Local->CalcSceneView(&Family,ActualEye,ActualRotation,Client->Viewport);
  if(!View)return End(TEXT("Actual comparison scene view could not be calculated"));
  ViewDiagnostics=MakeShared<FJsonObject>();ViewDiagnostics->SetNumberField(TEXT("fixture_index"),Index);
  ViewDiagnostics->SetStringField(TEXT("actual_eye_cm"),ActualEye.ToString());
  ViewDiagnostics->SetNumberField(TEXT("actual_exposure_bias"),View->FinalPostProcessSettings.AutoExposureBias);
  TArray<TSharedPtr<FJsonValue>> Volumes;
  for(IInterface_PostProcessVolume& Volume:W->GetPostProcessVolumeIterator())
  {
   const auto Props=Volume.GetProperties();float Distance=0;const bool Inside=Volume.EncompassesPoint(ActualEye,0,&Distance);
   auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),Volume._getUObject()->GetPathName());
   V->SetBoolField(TEXT("enabled"),Props.bIsEnabled);V->SetBoolField(TEXT("unbound"),Props.bIsUnbound);
   V->SetBoolField(TEXT("inside"),Inside);V->SetNumberField(TEXT("distance_cm"),Distance);
   V->SetNumberField(TEXT("priority"),Props.Priority);V->SetNumberField(TEXT("blend_weight"),Props.BlendWeight);
   V->SetNumberField(TEXT("blend_radius_cm"),Props.BlendRadius);
   if(Props.Settings){V->SetBoolField(TEXT("overrides_bias"),Props.Settings->bOverride_AutoExposureBias);V->SetNumberField(TEXT("bias"),Props.Settings->AutoExposureBias);}
   if(auto* C=Cast<UNeonCityCoastalPostProcessComponent>(Volume._getUObject()))
   {
    auto* Box=Cast<UBoxComponent>(C->GetAttachParent());
    if(!C->GetOwner()->ActorHasTag(TEXT("NeonCity.MainlandC923.ExposureCalibration7")) || !C->bUseAnalyticBoxBounds || !Box
      || Box->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || !Inside || Distance>.01f)
     return End(TEXT("Owned analytic exposure region is not nonphysical or does not enclose the actual view"));
    // The real registered component must also blend outside its oriented box,
    // rather than accidentally acting as a global exposure override.
    const FVector Outside=Box->GetComponentTransform().TransformPosition(FVector(Box->GetUnscaledBoxExtent().X+100,0,0));
    float OutsideDistance=0;const bool OutsideInside=Volume.EncompassesPoint(Outside,0,&OutsideDistance);
    float RadiusDistance=0;const bool NearSphere=Volume.EncompassesPoint(Outside,110,&RadiusDistance);
    if(OutsideInside || !NearSphere || !FMath::IsNearlyEqual(OutsideDistance,100.f,.01f) || !FMath::IsNearlyEqual(RadiusDistance,100.f,.01f))
     return End(TEXT("Actual local exposure bounds failed the exterior distance/sphere blend check"));
    V->SetBoolField(TEXT("nonphysical_inside_and_outside_query_pass"),true);V->SetNumberField(TEXT("outside_distance_cm"),OutsideDistance);
   }
   Volumes.Add(MakeShared<FJsonValueObject>(V));
  }
  ViewDiagnostics->SetArrayField(TEXT("registered_postprocess_in_actual_priority_order"),Volumes);
  if(!ActualEye.Equals(S.Eye,.1f))return End(TEXT("Actual comparison scene-view eye differs from its fixed fixture"));
  if(!FMath::IsNearlyZero(View->FinalPostProcessSettings.AutoExposureBias,.001f))
  {
#if WITH_EDITOR
   ViewDiagnostics->SetBoolField(TEXT("failed_calibration_image_saved"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("C923-CalibrationFailure.jpg")));
#endif
   return End(TEXT("Actual comparison camera did not receive the authored exposure calibration"));
  }
  const FString Name=bOpticalDiagnostic
   ?FString::Printf(TEXT("C923-DIAGNOSTIC_ABSORPTION-%02d-%s.jpg"),Index,S.Name)
   :bSeabedDiagnostic
   ?FString::Printf(TEXT("C923-DIAGNOSTIC_SEABED_HIDDEN-%02d-%s.jpg"),Index,S.Name)
   :bBoatShadowDiagnostic
   ?FString::Printf(TEXT("C923-DIAGNOSTIC_B_BOAT_SHADOW_OFF-%02d-%s.jpg"),Index,S.Name)
   :FString::Printf(TEXT("C921-Reference-%02d-%s.jpg"),Index,S.Name);bool Saved=false;
#if WITH_EDITOR
  if(Index>=39 && Index<=41 && !(bSeabedDiagnostic
    ?SeabedProbe.WriteSnapshot(W,PC,Index,Directory/TEXT("WaterAttribution"))
    :C923MarinaWaterAttribution::Write(W,PC,Index,Directory/TEXT("WaterAttribution"),bBoatShadowDiagnostic)))
   return End(TEXT("Actual water attribution snapshot could not be written uniquely before capture"));
  Saved=FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Name);
#endif
  Test->TestTrue(FString::Printf(TEXT("Actual reference image %s after stable %.2f wall/%.2f game seconds and %d frames"),*Name,WallAge,GameAge,Frames),Saved);if(!Saved)++Failed;
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),S.Name);R->SetStringField(TEXT("output_file"),Name);R->SetBoolField(TEXT("captured"),Saved);R->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());R->SetStringField(TEXT("eye_cm"),S.Eye.ToString());R->SetStringField(TEXT("aim_cm"),S.Aim.ToString());R->SetNumberField(TEXT("fov"),65);
  R->SetBoolField(TEXT("diagnostic_absorption_only"),bOpticalDiagnostic);
  R->SetBoolField(TEXT("diagnostic_b_boat_shadow_off"),bBoatShadowDiagnostic);R->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);
  R->SetBoolField(TEXT("diagnostic_seabed_hidden"),bSeabedDiagnostic);
  R->SetNumberField(TEXT("actual_scene_view_exposure_bias"),View->FinalPostProcessSettings.AutoExposureBias);
  R->SetNumberField(TEXT("actual_eye_adaptation_exposure"),View->GetLastEyeAdaptationExposure());
  R->SetNumberField(TEXT("actual_scene_luminance"),View->GetLastAverageSceneLuminance());
  R->SetBoolField(TEXT("normal_lit_viewport"),true);
  R->SetNumberField(TEXT("stable_wall_seconds"),WallAge);R->SetNumberField(TEXT("stable_game_seconds"),GameAge);R->SetNumberField(TEXT("stable_frames"),Frames);TArray<TSharedPtr<FJsonValue>> Levels;
  for(TActorIterator<AActor> I(W);I;++I)if(auto* Grass=I->FindComponentByClass<UNeonCityReferenceGrassComponent>())
  {auto G=MakeShared<FJsonObject>();G->SetStringField(TEXT("owner"),I->GetPathName());G->SetNumberField(TEXT("near_cells"),Grass->GetActiveCellCount(0));G->SetNumberField(TEXT("mid_cells"),Grass->GetActiveCellCount(1));G->SetNumberField(TEXT("far_cells"),Grass->GetActiveCellCount(2));Levels.Add(MakeShared<FJsonValueObject>(G));}
  R->SetArrayField(TEXT("actual_grass_levels"),Levels);Captures.Add(MakeShared<FJsonValueObject>(R));
  if(First==18)
  {
   int32 TerrainCount=0,ClusterCount=0,PineClusterCount=0,RidgeClusterCount=0,ActualTreeCount=0,UnderstoryClusters=0,UnderstoryInstances=0,IndependentClusters=0,IndependentInstances=0;
   TSet<FString> ObservedVariants;TArray<TSharedPtr<FJsonValue>> Native;
   // Frozen scatter12 d1ea979e:12 genuinely accepted variants. The largest
   // Rooibos_A has zero accepted poses and must not be forced into the scene.
   const TMap<FString,int32> VariantCounts={
    {TEXT("SM_C923_Understory12_Iceplant_A"),162},{TEXT("SM_C923_Understory12_Iceplant_B"),158},
    {TEXT("SM_C923_Understory12_Iceplant_C"),190},{TEXT("SM_C923_Understory12_Iceplant_D"),227},
    {TEXT("SM_C923_Understory12_Iceplant_E"),229},{TEXT("SM_C923_Understory12_Iceplant_F"),214},
    {TEXT("SM_C923_Understory12_Iceplant_G"),169},{TEXT("SM_C923_Understory12_Iceplant_H"),145},
    {TEXT("SM_C923_Understory12_Rooibos_B"),26},{TEXT("SM_C923_Understory12_Rooibos_C"),56},
    {TEXT("SM_C923_Understory12_Rooibos_D"),80},{TEXT("SM_C923_Understory12_Rooibos_E"),105}};
   for(TActorIterator<AActor> I(W);I;++I)
   {
    if(I->GetLevel()!=W->PersistentLevel)continue;
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.EastHeadland")))
    {
     auto* C=I->FindComponentByClass<UStaticMeshComponent>();
     if(!C || !C->GetStaticMesh() || !C->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/")) || C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)
      return End(TEXT("Persisted headland is missing its owned mesh or player collision"));
     if(!C->GetMaterial(1) || C->GetMaterial(1)->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/M_C923_HeadlandBlend7.M_C923_HeadlandBlend7"))
      return End(TEXT("Persisted headland does not use the actual living-ground material"));
     ++TerrainCount;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("terrain_actor"),I->GetName());J->SetStringField(TEXT("terrain_mesh"),C->GetStaticMesh()->GetPathName());Native.Add(MakeShared<FJsonValueObject>(J));
    }
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.HeadlandTrees")))
    {
     auto* Cluster=Cast<AIslandLandCluster>(*I);auto* C=Cluster?Cluster->Instances.Get():nullptr;
     if(!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetName()!=TEXT("SM_C920_GardenTree") || C->GetInstanceCount()<1 || C->GetInstanceCount()>HeadlandForestInstanceBudget || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || I->IsHidden() || !C->IsVisible())
      return End(TEXT("Persisted headland tree cluster/visibility/budget differs from authored scenery"));
     ++ClusterCount;ActualTreeCount+=C->GetInstanceCount();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("tree_actor"),I->GetName());J->SetStringField(TEXT("tree_mesh"),C->GetStaticMesh()->GetPathName());J->SetNumberField(TEXT("tree_instances"),C->GetInstanceCount());Native.Add(MakeShared<FJsonValueObject>(J));
    }
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.PineCanopy6")))
    {
     auto* Cluster=Cast<AIslandLandCluster>(*I);auto* C=Cluster?Cluster->Instances.Get():nullptr;
     if(!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/SM_C923_PineB.SM_C923_PineB") || C->GetInstanceCount()<1 || C->GetInstanceCount()>HeadlandForestInstanceBudget || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || I->IsHidden() || !C->IsVisible())
      return End(TEXT("Persisted mature pine canopy differs from its owned source or visible instance policy"));
     ++PineClusterCount;ActualTreeCount+=C->GetInstanceCount();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("pine_actor"),I->GetName());J->SetStringField(TEXT("pine_mesh"),C->GetStaticMesh()->GetPathName());J->SetNumberField(TEXT("pine_instances"),C->GetInstanceCount());Native.Add(MakeShared<FJsonValueObject>(J));
    }
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.VisibleRidgeForest32")))
    {
     auto* Cluster=Cast<AIslandLandCluster>(*I);auto* C=Cluster?Cluster->Instances.Get():nullptr;
     const FString MeshName=C?GetNameSafe(C->GetStaticMesh()):FString();
     if(!C || !(MeshName==TEXT("SM_C920_GardenTree") || MeshName==TEXT("SM_C923_PineB"))
      || C->GetInstanceCount()<1 || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
      || I->IsHidden() || !C->IsVisible() || I->IsActorTickEnabled() || C->IsComponentTickEnabled()
      || I->ActorHasTag(TEXT("NeonCity.MainlandC923.HeadlandTrees")) || I->ActorHasTag(TEXT("NeonCity.MainlandC923.PineCanopy6")))
      return End(TEXT("Visible ridge forest lost its authentic mesh, static visibility or separate group identity"));
     ++RidgeClusterCount;ActualTreeCount+=C->GetInstanceCount();auto J=MakeShared<FJsonObject>();
     J->SetStringField(TEXT("ridge_tree_actor"),I->GetName());J->SetStringField(TEXT("ridge_tree_mesh"),C->GetStaticMesh()->GetPathName());
     J->SetNumberField(TEXT("ridge_tree_instances"),C->GetInstanceCount());Native.Add(MakeShared<FJsonValueObject>(J));
    }
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.Understory8")))
    {
     auto* Cluster=Cast<AIslandLandCluster>(*I);auto* C=Cluster?Cluster->Instances.Get():nullptr;
     if(!C || !C->GetStaticMesh() || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || I->IsHidden() || !C->IsVisible())
      return End(TEXT("Actual understory has missing geometry, visibility or unexpected blocking collision"));
     const FString Mesh=C->GetStaticMesh()->GetName();const int32 Expected=Mesh==TEXT("SM_Land_crystalline_iceplant")?303:Mesh==TEXT("SM_Land_wild_rooibos_bush")?230:0;
     if(Expected==0 || C->GetInstanceCount()!=Expected)return End(TEXT("Actual understory differs from its retained source subset"));
     for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot)
      if(!C->GetMaterial(Slot) || !C->GetMaterial(Slot)->GetName().StartsWith(TEXT("M_C923_UnderstoryStatic8_")))
       return End(TEXT("Actual understory lacks its owned root-safe material override"));
     ++UnderstoryClusters;UnderstoryInstances+=C->GetInstanceCount();auto J=MakeShared<FJsonObject>();
     J->SetStringField(TEXT("understory_actor"),I->GetName());J->SetStringField(TEXT("understory_mesh"),C->GetStaticMesh()->GetPathName());
     J->SetNumberField(TEXT("understory_instances"),C->GetInstanceCount());Native.Add(MakeShared<FJsonValueObject>(J));
    }
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.Understory12")))
    {
     auto* Cluster=Cast<AIslandLandCluster>(*I);auto* C=Cluster?Cluster->Instances.Get():nullptr;
     if(!C || !C->GetStaticMesh() || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
       || I->IsHidden() || !C->IsVisible() || I->IsActorTickEnabled() || C->IsComponentTickEnabled())
      return End(TEXT("Independent understory lost its visible nonblocking static instance policy"));
     const FString Mesh=C->GetStaticMesh()->GetName();const int32* Expected=VariantCounts.Find(Mesh);
     if(!Expected || C->GetInstanceCount()!=*Expected || ObservedVariants.Contains(Mesh)
       || !C->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/")))
      return End(TEXT("Independent understory differs from the actual accepted per-variant scatter"));
     for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot)
     {
      const auto* Material=C->GetMaterial(Slot);const FString MaterialName=GetNameSafe(Material);
      if(!Material || !(MaterialName.StartsWith(TEXT("M_C923_UnderstoryMasked12_"))
        || MaterialName==TEXT("M_C923_UnderstoryStatic8_wild_rooibos_bush")))
       return End(TEXT("Independent understory has unexpected alpha/root material bindings"));
     }
     ObservedVariants.Add(Mesh);++IndependentClusters;IndependentInstances+=C->GetInstanceCount();
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("independent_understory_actor"),I->GetName());
     J->SetStringField(TEXT("independent_understory_mesh"),C->GetStaticMesh()->GetPathName());
     J->SetNumberField(TEXT("independent_understory_instances"),C->GetInstanceCount());Native.Add(MakeShared<FJsonValueObject>(J));
    }
   }
   if(TerrainCount!=1 || ClusterCount!=1 || PineClusterCount!=1 || ActualTreeCount>HeadlandForestInstanceBudget)return End(TEXT("Actual headland terrain/mixed forest inventory differs from the bounded authored scenery"));
   R->SetNumberField(TEXT("actual_visible_ridge_clusters"),RidgeClusterCount);
   R->SetNumberField(TEXT("actual_total_headland_trees"),ActualTreeCount);
   if(IndependentClusters>0)
   {if(UnderstoryClusters!=0 || UnderstoryInstances!=0 || IndependentClusters!=12 || IndependentInstances!=1761 || ObservedVariants.Num()!=VariantCounts.Num())return End(TEXT("Independent understory adoption is incomplete or leaves the old display rows in the scene"));}
   else if(UnderstoryClusters!=2 || UnderstoryInstances!=533)return End(TEXT("Actual retained understory inventory is incomplete"));
   R->SetArrayField(TEXT("actual_headland_components"),Native);
  }
  if(First==14)
  {
   TArray<TSharedPtr<FJsonValue>> Plants;
   for(TActorIterator<AActor> I(W);I;++I)
   {
    if(I->GetLevel()!=W->PersistentLevel || !(I->ActorHasTag(TEXT("NeonCity.MainlandC922.ContainerPlanting1")) || I->ActorHasTag(TEXT("NeonCity.MainlandC922.SupersededContainerContents"))))continue;
    TInlineComponentArray<UStaticMeshComponent*> Components(*I);
    for(auto* C:Components)
    {
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),I->GetName());J->SetStringField(TEXT("mesh"),GetNameSafe(C->GetStaticMesh()));
     J->SetStringField(TEXT("position"),C->GetComponentLocation().ToString());J->SetBoolField(TEXT("actor_hidden"),I->IsHidden());
     J->SetBoolField(TEXT("component_visible"),C->IsVisible());J->SetBoolField(TEXT("component_hidden_in_game"),C->bHiddenInGame);
     J->SetNumberField(TEXT("cached_cull_distance_cm"),C->CachedMaxDrawDistance);Plants.Add(MakeShared<FJsonValueObject>(J));
    }
   }
   R->SetArrayField(TEXT("actual_container_components"),Plants);
  }
  PreviousShot=Index;
  if(!Selection.IsEmpty())
  {if(++SelectionCursor>=Selection.Num())return End();Index=Selection[SelectionCursor];Place();return false;}
  if(++Index<Limit){Place();return false;}return End();
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalReferenceViews921,"NeonCity.Phase205c.MainlandC921.Standalone.ReferenceViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalReferenceViews921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalReferenceFacadeViews921,"NeonCity.Phase205c.MainlandC921.Standalone.ReferenceFacadeViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalReferenceFacadeViews921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalGrassCoverageViews921,"NeonCity.Phase205c.MainlandC921.Standalone.GrassCoverageViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalGrassCoverageViews921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalInteriorReferenceViews921,"NeonCity.Phase205c.MainlandC921.Standalone.ReferenceInteriorViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalInteriorReferenceViews921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalContainerViews922,"NeonCity.Phase205c.MainlandC922.Standalone.ContainerPlantingViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalContainerViews922::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHeadlandViews923,"NeonCity.Phase205c.MainlandC923.Standalone.HeadlandViews",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalHeadlandViews923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarinaBoatShadowDiagnostic923,"NeonCity.Phase205c.MainlandC923.Standalone.MarinaBoatShadowDiagnostic",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMarinaBoatShadowDiagnostic923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,false,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarinaSeabedDiagnostic923,"NeonCity.Phase205c.MainlandC923.Standalone.MarinaSeabedDiagnostic",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMarinaSeabedDiagnostic923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,false,false,false,false,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarinaOpticalDiagnostic923,"NeonCity.Phase205c.MainlandC923.Standalone.MarinaOpticalDiagnostic",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMarinaOpticalDiagnostic923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalViews921::FViews(this,false,false,false,false,false,false,false,true));return true;}
#endif
