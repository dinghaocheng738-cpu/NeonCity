#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandDoor.h"
#include "Adventure/IslandAdventureLighting.h"
#include "Adventure/IslandSupercar.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace IslandLandFinalEvidence
{
// These are isolated visual/safety fixtures in the normal PIE game mode.
// Their explicit setup teleports are never continuous driving/flying evidence.
static const FVector MansionOrigin=NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480));
FVector M(double X,double Y,double Z=0) { return MansionOrigin+FVector(100*X,-100*Y,100*Z); }
TSharedPtr<FJsonValue> VectorJSON(const FVector& V)
{
 TArray<TSharedPtr<FJsonValue>> A;
 for(double N:{V.X,V.Y,V.Z})A.Add(MakeShared<FJsonValueNumber>(N));
 return MakeShared<FJsonValueArray>(A);
}
TSharedPtr<FJsonObject> NewManifest(const TCHAR* Scope)
{
 auto J=MakeShared<FJsonObject>();
 J->SetStringField(TEXT("scope"),Scope);
 J->SetStringField(TEXT("execution_mode"),TEXT("NO COMPUTER CONTROL; isolated RenderOffscreen PIE"));
 J->SetStringField(TEXT("visual_evidence_status"),TEXT("CAPTURED IMAGES REQUIRE EVIDENCE REVIEW"));
 J->SetStringField(TEXT("user_subjective_status"),TEXT("USER FINAL PLAYTEST / AUDIO / HANDLING REVIEW REQUIRED"));
 J->SetBoolField(TEXT("continuous_driving_evidence"),false);
 J->SetBoolField(TEXT("continuous_helicopter_circuit_evidence"),false);
 J->SetBoolField(TEXT("performance_evidence"),false);
 J->SetBoolField(TEXT("simulation_paused"),false);
 J->SetNumberField(TEXT("width"),1600);J->SetNumberField(TEXT("height"),900);
 J->SetField(TEXT("mansion_origin_world_cm"),VectorJSON(MansionOrigin));
 return J;
}
bool WriteManifest(const TSharedPtr<FJsonObject>& J,const TCHAR* Name)
{
 FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 if(Label.IsEmpty())return false;
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label);
 IFileManager::Get().MakeDirectory(*Folder,true);
 return FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text))
  && FFileHelper::SaveStringToFile(Text,*(Folder/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool ReadJSON(const FString& Path,TSharedPtr<FJsonObject>& J)
{
 FString Text;return FFileHelper::LoadFileToString(Text,*Path)
  && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) && J.IsValid();
}
bool ReadRoad(TArray<FVector>& Points)
{
 TSharedPtr<FJsonObject> J;if(!ReadJSON(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/road-alignment.json"),J))return false;
 const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!J->TryGetArrayField(TEXT("points"),Rows))return false;
 for(const auto& Row:*Rows)
 {
  const auto& P=Row->AsArray();if(P.Num()!=3)return false;
  Points.Add(FVector(NeonCityCoastalLayout::IslandOrigin.X+100*P[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*P[1]->AsNumber(),-130+100*P[2]->AsNumber()));
 }
 return Points.Num()==842;
}
UStaticMeshComponent* FindMesh(UWorld* W,const TCHAR* Name)
{
 for(TActorIterator<AStaticMeshActor> It(W);It;++It)
 {
  auto* C=It->GetStaticMeshComponent();if(C && C->GetStaticMesh() && C->GetStaticMesh()->GetFName()==FName(Name))return C;
 }
 return nullptr;
}
void Key(APlayerController* PC,FKey K,EInputEvent Event)
{
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));
}
TArray<TSharedPtr<FJsonValue>> MeshFacts(UWorld* W,FAutomationTestBase* Test)
{
 TArray<TSharedPtr<FJsonValue>> Facts;
 const TSet<FName> Names={TEXT("SM_LandDiningChair"),TEXT("SM_LandMansionGallery"),TEXT("SM_LandKitchenIsland"),
  TEXT("SM_LandMansionWood"),TEXT("SM_LandMansionMetal"),TEXT("SM_LandMansionGlass"),TEXT("SM_LandMansionStructure"),
  TEXT("SM_LandMansionFloors"),TEXT("SM_LandMansionDriveway"),TEXT("SM_LandMansionTerrace"),
  TEXT("SM_LandMansionFoundation"),TEXT("SM_LandLookoutBench"),TEXT("SM_LandSummitWalk")};
 TMap<FName,int32> Counts;
 for(TActorIterator<AStaticMeshActor> It(W);It;++It)
 {
  auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C?C->GetStaticMesh().Get():nullptr;
  if(!Mesh || !Names.Contains(Mesh->GetFName()))continue;
  // The village cafe legitimately reuses KitchenIsland. This audit counts
  // the hero residence instances, using physical world bounds rather than
  // an asset name shared by independent finished locations.
  if(C->Bounds.Origin.Z<28000 || FVector::DistSquared2D(C->Bounds.Origin,MansionOrigin)>FMath::Square(10000.))continue;
  ++Counts.FindOrAdd(Mesh->GetFName());auto J=MakeShared<FJsonObject>();
  J->SetStringField(TEXT("actor"),It->GetPathName());J->SetStringField(TEXT("mesh"),Mesh->GetPathName());
  J->SetField(TEXT("position_world_cm"),VectorJSON(C->GetComponentLocation()));
  J->SetField(TEXT("scale"),VectorJSON(C->GetComponentScale()));
  J->SetField(TEXT("bounds_center_world_cm"),VectorJSON(C->Bounds.Origin));
  J->SetField(TEXT("bounds_size_world_cm"),VectorJSON(C->Bounds.BoxExtent*2));
  J->SetNumberField(TEXT("lod_count"),Mesh->GetNumLODs());J->SetNumberField(TEXT("lod0_triangles"),Mesh->GetNumTriangles(0));
  J->SetBoolField(TEXT("nanite_enabled"),Mesh->IsNaniteEnabled());
  J->SetNumberField(TEXT("nanite_fallback_relative_error"),Mesh->GetNaniteSettings().FallbackRelativeError);
  J->SetNumberField(TEXT("nanite_fallback_target"),int32(Mesh->GetNaniteSettings().FallbackTarget));
  J->SetNumberField(TEXT("nanite_fallback_percent_triangles"),Mesh->GetNaniteSettings().FallbackPercentTriangles);
  J->SetNumberField(TEXT("collision_enabled"),int32(C->GetCollisionEnabled()));
  J->SetNumberField(TEXT("pawn_response"),int32(C->GetCollisionResponseToChannel(ECC_Pawn)));
  J->SetNumberField(TEXT("camera_response"),int32(C->GetCollisionResponseToChannel(ECC_Camera)));
  J->SetNumberField(TEXT("collision_complexity"),Mesh->GetBodySetup()?int32(Mesh->GetBodySetup()->GetCollisionTraceFlag()):-1);
  J->SetNumberField(TEXT("simple_shape_count"),Mesh->GetBodySetup()?Mesh->GetBodySetup()->AggGeom.GetElementCount():0);
  TArray<TSharedPtr<FJsonValue>> Materials;
  for(int32 I=0;I<C->GetNumMaterials();++I)
  {
   auto* Material=C->GetMaterial(I);auto F=MakeShared<FJsonObject>();
   F->SetNumberField(TEXT("slot"),I);F->SetStringField(TEXT("asset"),GetPathNameSafe(Material));
   F->SetNumberField(TEXT("blend_mode"),Material?int32(Material->GetBlendMode()):-1);
   Materials.Add(MakeShared<FJsonValueObject>(F));
   Test->TestNotNull(*FString::Printf(TEXT("%s material slot %d exists"),*Mesh->GetName(),I),Material);
  }
  J->SetArrayField(TEXT("materials"),Materials);Facts.Add(MakeShared<FJsonValueObject>(J));
  Test->TestTrue(*FString::Printf(TEXT("%s retains positive scale"),*Mesh->GetName()),C->GetComponentScale().GetMin()>0);
  Test->TestTrue(*FString::Printf(TEXT("%s has native geometry"),*Mesh->GetName()),Mesh->GetNumTriangles(0)>0);
  Test->TestTrue(*FString::Printf(TEXT("%s has real pawn collision"),*Mesh->GetName()),
   C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision && C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
  if(Mesh->GetFName()==TEXT("SM_LandDiningChair"))
  {
   Test->TestTrue(TEXT("Imported dining chair uses metre-scale placement"),C->GetComponentScale().Equals(FVector::OneVector,.001));
   Test->TestTrue(TEXT("Dining chair size is appropriate for the retained table"),C->Bounds.BoxExtent.Z*2>60 && C->Bounds.BoxExtent.Z*2<150);
  }
  if(Mesh->GetFName()==TEXT("SM_LandMansionGlass"))
  {
   Test->TestFalse(TEXT("Translucent sea glazing remains outside Nanite"),Mesh->IsNaniteEnabled());
   Test->TestEqual(TEXT("Camera can see through the retained glazing"),C->GetCollisionResponseToChannel(ECC_Camera),ECR_Ignore);
  }
 }
 Test->TestEqual(TEXT("All four replaced dining chairs are present in normal PIE"),Counts.FindRef(TEXT("SM_LandDiningChair")),4);
 for(const TCHAR* Name:{TEXT("SM_LandMansionGallery"),TEXT("SM_LandKitchenIsland"),TEXT("SM_LandMansionWood"),
  TEXT("SM_LandMansionMetal"),TEXT("SM_LandMansionGlass"),TEXT("SM_LandMansionDriveway"),TEXT("SM_LandLookoutBench"),TEXT("SM_LandSummitWalk")})
  Test->TestEqual(*FString::Printf(TEXT("Exactly one retained %s is present"),Name),Counts.FindRef(FName(Name)),1);
 return Facts;
}
void CheckImportedDetails(UWorld* W,FAutomationTestBase* Test,TSharedPtr<FJsonObject> Manifest)
{
 auto* Island=FindMesh(W,TEXT("SM_LandKitchenIsland"));auto* Wood=FindMesh(W,TEXT("SM_LandMansionWood"));
 TArray<TSharedPtr<FJsonValue>> Details;
 if(Island)
 {
  FHitResult Basin;const auto T=Island->GetComponentTransform();
  const bool Hit=Island->LineTraceComponent(Basin,T.TransformPosition(FVector(85,-4,160)),T.TransformPosition(FVector(85,-4,50)),FCollisionQueryParams(SCENE_QUERY_STAT(LandFinalSink),true));
  const FVector Local=Hit?T.InverseTransformPosition(Basin.ImpactPoint):FVector::ZeroVector;
  Test->TestTrue(TEXT("Sink has an actual recessed bowl instead of a sealed countertop"),Hit && Local.Z>70 && Local.Z<80);
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("detail"),TEXT("sink_basin"));J->SetBoolField(TEXT("trace_hit"),Hit);
  J->SetField(TEXT("impact_mesh_local_cm"),VectorJSON(Local));Details.Add(MakeShared<FJsonValueObject>(J));
 }
 if(Wood)for(double Y:{10.5,18.0})
 {
  FHitResult Beam;const bool Hit=Wood->LineTraceComponent(Beam,M(4,Y,4),M(4,Y,2.8),FCollisionQueryParams(SCENE_QUERY_STAT(LandFinalBeam),true));
  const double Height=Hit?(Beam.ImpactPoint.Z-MansionOrigin.Z)/100:0;
  Test->TestTrue(TEXT("Both pergola transverse beams have real geometry at the authored structural height"),Hit && Height>3.18 && Height<3.28);
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("detail"),TEXT("transverse_pergola_beam"));J->SetNumberField(TEXT("local_y_m"),Y);
  J->SetBoolField(TEXT("trace_hit"),Hit);J->SetNumberField(TEXT("top_height_m"),Height);Details.Add(MakeShared<FJsonValueObject>(J));
 }
 Manifest->SetArrayField(TEXT("imported_detail_geometry_checks"),Details);
}
TArray<TSharedPtr<FJsonValue>> ClockFacts(UWorld* W,FAutomationTestBase* Test)
{
 TArray<TSharedPtr<FJsonValue>> Facts;
 for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)
 {
  TArray<UDirectionalLightComponent*> Lights;It->GetComponents(Lights);
  auto* Sun=It->GetSun();auto** MoonEntry=Lights.FindByPredicate([](auto* C){return C->GetFName()==TEXT("Moon");});
  auto* Moon=MoonEntry?*MoonEntry:nullptr;Test->TestNotNull(TEXT("Ocean and glass retain a night moon source"),Moon);
  if(Sun && Moon)
  {
   auto* Active=Sun->Intensity>=Moon->Intensity?Sun:Moon;auto* Other=Active==Sun?Moon:Sun;
   Test->TestEqual(TEXT("One directional light dominates translucent lighting"),Active->ForwardShadingPriority,1);
   Test->TestEqual(TEXT("Secondary directional light has lower priority"),Other->ForwardShadingPriority,0);
   auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("hour"),It->GetTimeOfDay());J->SetNumberField(TEXT("sun_intensity"),Sun->Intensity);
   J->SetNumberField(TEXT("moon_intensity"),Moon->Intensity);J->SetStringField(TEXT("dominant_directional"),Active->GetName());Facts.Add(MakeShared<FJsonValueObject>(J));
  }
 }
 for(TActorIterator<AIslandAdventureLighting> It(W);It;++It)
 {
  It->Tick(0);auto* PP=It->FindComponentByClass<UPostProcessComponent>();
  if(PP)
  {
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),It->GetPathName());
   J->SetNumberField(TEXT("auto_exposure_min"),PP->Settings.AutoExposureMinBrightness);
   J->SetNumberField(TEXT("auto_exposure_max"),PP->Settings.AutoExposureMaxBrightness);
   J->SetNumberField(TEXT("bloom_intensity"),PP->Settings.BloomIntensity);J->SetBoolField(TEXT("overrides_bloom"),PP->Settings.bOverride_BloomIntensity);
   Test->TestEqual(TEXT("Mountain exposure retains the authored bright-sky ceiling"),PP->Settings.AutoExposureMaxBrightness,16.f);
   Facts.Add(MakeShared<FJsonValueObject>(J));
  }
 }
 auto* PC=W->GetFirstPlayerController();FVector Eye;FRotator EyeRotation;if(PC)PC->GetPlayerViewPoint(Eye,EyeRotation);
 for(TActorIterator<AActor> It(W);It;++It)
 {
  if(!It->ActorHasTag(TEXT("NeonCity.IslandLandPass1")) || FVector::DistSquared(It->GetActorLocation(),Eye)>FMath::Square(5000.0))continue;
  TArray<ULightComponent*> Lights;It->GetComponents(Lights);
  for(auto* Light:Lights)
  {
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("local_light_actor"),It->GetPathName());
   J->SetStringField(TEXT("component"),Light->GetName());J->SetNumberField(TEXT("intensity"),Light->Intensity);
   J->SetStringField(TEXT("color"),Light->GetLightColor().ToString());J->SetField(TEXT("position_world_cm"),VectorJSON(Light->GetComponentLocation()));
   J->SetBoolField(TEXT("visible"),Light->IsVisible());J->SetBoolField(TEXT("casts_shadow"),Light->CastShadows);
   Test->TestTrue(TEXT("Nearby authored light has finite non-negative intensity"),FMath::IsFinite(Light->Intensity) && Light->Intensity>=0);
   Facts.Add(MakeShared<FJsonValueObject>(J));
  }
 }
 return Facts;
}

struct FView
{
 FString Name;FVector Camera,Target,Fixture;float FOV=65;
};
class FCloseVisualAudit final:public IAutomationLatentCommand
{
public:
 explicit FCloseVisualAudit(FAutomationTestBase* T,bool PlatformOnly=false):Test(T),Manifest(NewManifest(PlatformOnly?TEXT("FIX-SPECIFIC: changed platform/bench surroundings only; unchanged 105-view evidence retained"):TEXT("Fixed camera comparison in normal PIE; explicitly isolated visual fixtures"))),bPlatformOnly(PlatformOnly){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>1200){Test->AddError(TEXT("Close visual evidence timed out"));return Finish(PC);}
  if(!P || W->GetTimeSeconds()<4)return false;
  if(!Initialized)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")))
   {Test->AddError(TEXT("Close visual final requires opt-in native stills without SkipHUD"));return Finish(PC);}
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(MansionOrigin))return false;
   Manifest->SetArrayField(TEXT("native_meshes"),MeshFacts(W,Test));CheckImportedDetails(W,Test,Manifest);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OriginalHour=It->GetTimeOfDay();OriginalCycle=It->bCycleEnabled;It->SetCycleEnabled(false);break;}
   if(!Clock.IsValid()){Test->AddError(TEXT("Clock actor missing from normal PIE"));return Finish(PC);}
   FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;Camera=W->SpawnActor<ACameraActor>(Spawn);
   if(!Camera.IsValid()){Test->AddError(TEXT("Transient fixed camera could not be spawned"));return Finish(PC);}
   Camera->GetCameraComponent()->PostProcessBlendWeight=0;OriginalTarget=PC->GetViewTarget();PC->SetViewTarget(Camera.Get());
   const FVector Ground=M(-18.0,1.1,1.2);
   auto Add=[&](const TCHAR* Name,FVector C,FVector T,float FOV=65,FVector Fixture=FVector::ZeroVector)
   {Views.Add({Name,C,T,Fixture.IsZero()?Ground:Fixture,FOV});};
   Add(TEXT("Mansion-Exterior-Arrival"),M(-23,1.4,2),M(-8,2.5,2.8),72);
   Add(TEXT("Mansion-Exterior-Sea"),M(23,26,9.0),M(3,5.5,3.6),62);
   Add(TEXT("Mansion-Foyer-Gallery-Close"),M(-13.1,7.3,1.65),M(-15.35,7.3,1.4),66);
   Add(TEXT("Mansion-Foyer-Gallery-Context"),M(-14,1.2,1.7),M(-14.7,7.3,1.5),72);
   Add(TEXT("Mansion-Living-Ocean"),M(-4,2.5,1.7),M(3.7,10.5,1.3),78);
   Add(TEXT("Mansion-Living-Material-Close"),M(1.0,8.8,1.6),M(3.7,6.1,.9),68);
   Add(TEXT("Mansion-Dining-Chair-Close"),M(8.45,1.8,1.5),M(9.45,3.0,.8),60);
   Add(TEXT("Mansion-Dining-Four-Places"),M(7.8,7.0,1.7),M(10.4,4.1,1.0),74);
   Add(TEXT("Mansion-Kitchen-Sink-Close"),M(10.05,-5.15,1.85),M(10.05,-4.04,.91),58);
   Add(TEXT("Mansion-Kitchen-Context"),M(5.3,-1.6,1.65),M(9.7,-6,1.15),77);
   Add(TEXT("Mansion-Terrace-Beam-Close"),M(4,13.0,2.4),M(4,10.5,3.13),68);
   Add(TEXT("Mansion-Terrace-Beams-Context"),M(-5.5,17.0,1.65),M(6,14.2,3.1),85);
   Add(TEXT("Mansion-Terrace-Glass-Ocean"),M(3,13.7,1.7),M(3,9.7,1.8),75);
   Add(TEXT("Mansion-Terrace-Ocean-View"),M(1,16,1.7),M(1,40,1.0),75);
   Add(TEXT("Mansion-BBQ-Close"),M(10.4,13.15,1.65),M(12.2,14.5,.95),66);
   Add(TEXT("Mansion-Driveway-Parking"),M(-22,-6.0,1.7),M(-17.5,-3.5,1.0),80);
   Add(TEXT("Mansion-Arrival-Pavement-Seam"),M(-22.5,13.0,1.6),M(-19.5,13.3,.1),72);
   Add(TEXT("Bench-Front"),M(-25.1,15.7,1.4),M(-25.1,17,.45),68);
   Add(TEXT("Bench-Side"),M(-23.6,16.8,1.0),M(-25.1,17,.45),70);
   Add(TEXT("Bench-Back"),M(-25.1,18.2,1.3),M(-25.1,17,.45),70);
   Add(TEXT("Bench-Player-Height-Context"),M(-23.6,16.1,1.65),M(-25.1,17,.45),75);
   ANeonCityReferenceVillage* VillageActor=nullptr;for(TActorIterator<ANeonCityReferenceVillage> It(W);It;++It){VillageActor=*It;break;}
   if(!VillageActor){Test->AddError(TEXT("Native village transform missing"));return Finish(PC);}
   const auto VT=VillageActor->GetActorTransform();
   const FVector Dock=VT.TransformPosition(FVector(5950,-3510,330)),Village=VT.TransformPosition(FVector(5500,14500,400));
   Add(TEXT("Dock-Pier-Player-Height"),Dock+FVector(-500,-250,180),Dock+FVector(1000,0,80),75,Dock+FVector(0,0,170));
   Add(TEXT("Dock-Village-Ocean-Transition"),Dock+FVector(2400,-2600,1800),Village,70,Dock+FVector(0,0,170));
   Add(TEXT("Village-Boardwalk-Facades"),Village+FVector(-650,-600,500),Village+FVector(250,350,140),80,Dock+FVector(0,0,170));
   int32 DoorNumber=0;
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(It->GetActorLocation().Z<1000)
   {
    const auto DT=It->GetActorTransform();Views.Add({FString::Printf(TEXT("Village-Physical-Door-%d-Facade"),++DoorNumber),
     DT.TransformPosition(FVector(65,-500,180)),DT.TransformPosition(FVector(65,0,120)),DT.TransformPosition(FVector(65,-190,100)),78});
   }
   Test->TestEqual(TEXT("All three native village door facades are required in every comparison hour"),DoorNumber,3);
   TArray<FVector> Road;
   if(!ReadRoad(Road)){Test->AddError(TEXT("Exact retained mountain alignment unavailable"));return Finish(PC);}
   AIslandSupercar* Aster=nullptr;AIslandSupercar* Kestrel=nullptr;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)
   {
    auto*& Selected=It->bRoadster?Kestrel:Aster;
    if(!Selected || FVector::DistSquared2D(It->GetActorLocation(),Road[0])<FVector::DistSquared2D(Selected->GetActorLocation(),Road[0]))Selected=*It;
   }
   Test->TestNotNull(TEXT("Aster car-paint DSN comparison is required"),Aster);
   Test->TestNotNull(TEXT("Kestrel car-paint DSN comparison is required"),Kestrel);
   for(auto* Car:{Aster,Kestrel})if(Car)
   {
    const FVector C=Car->GetActorLocation(),Side=Car->GetActorRightVector(),Forward=Car->GetActorForwardVector();
    Views.Add({Car->bRoadster?TEXT("Village-Kestrel-Car-Paint"):TEXT("Village-Aster-Car-Paint"),C-Side*380-Forward*220+FVector(0,0,180),
     C+FVector(0,0,45),C-Side*320+FVector(0,0,100),70});
   }
   for(int32 Index:{778,793,808})
   {
    const FVector Centre=Road[Index],Tangent=(Road[Index+1]-Road[Index-1]).GetSafeNormal2D(),Normal(-Tangent.Y,Tangent.X,0);
    Views.Add({FString::Printf(TEXT("Mountain-Road-%d-Cut-Close"),Index),Centre+Normal*600+FVector(0,0,280),Centre+Normal*150+FVector(0,0,180),Centre+FVector(0,0,130),75});
    // Upper near-bridge cameras at 16m were inside the retained structure.
    // Move only these comparison cameras above/outside it, never the road.
    const FVector AerialEye=Centre-Normal*(Index>=793?4500:1600)+FVector(0,0,Index>=793?5500:1600);
    Views.Add({FString::Printf(TEXT("Mountain-Road-%d-Local-Aerial"),Index),AerialEye,Centre,Centre+FVector(0,0,130),74});
   }
   Manifest->SetStringField(TEXT("aerial_scope"),TEXT("Local modified road/mansion view fixtures; accepted full 6 km helicopter circuit is not repeated"));
   Test->TestEqual(TEXT("Exact complete close-view coverage is 35 cameras per hour"),Views.Num(),35);
   if(bPlatformOnly)Views.RemoveAll([](const FView& V)
   {return !V.Name.StartsWith(TEXT("Bench-")) && V.Name!=TEXT("Mansion-Arrival-Pavement-Seam") && V.Name!=TEXT("Mansion-Exterior-Sea")
     && V.Name!=TEXT("Mansion-Terrace-Glass-Ocean") && V.Name!=TEXT("Mansion-Terrace-Ocean-View") && V.Name!=TEXT("Mansion-BBQ-Close")
     && V.Name!=TEXT("Mountain-Road-793-Local-Aerial") && V.Name!=TEXT("Mountain-Road-808-Local-Aerial");});
   Test->TestEqual(TEXT("Selected native camera coverage is exact"),Views.Num(),bPlatformOnly?11:35);
   Manifest->SetNumberField(TEXT("required_views_per_hour"),Views.Num());Manifest->SetNumberField(TEXT("required_total_images"),Views.Num()*3);
   Initialized=true;BeginView(W,PC,P);return false;
  }
  auto* R=ANeonCityCoastalRegion::Find(W);if(R)R->EnsureDestinationReady(Views[ViewIndex].Fixture);
   if(FPlatformTime::Seconds()-ViewAt<(ViewIndex==0?8.0:3.0) || GFrameCounter-ViewFrame<8)return false;
  const TCHAR* Period=HourIndex==0?TEXT("Day12"):HourIndex==1?TEXT("Sunset17"):TEXT("Night22");
  const auto& View=Views[ViewIndex];const FString Image=FString::Printf(TEXT("Final15-%s-%s.png"),*View.Name,Period);
  const bool Captured=FNeonCityVisualEvidence::CaptureHUD(W,Image);
  Test->TestTrue(*FString::Printf(TEXT("Native fixed-camera still saved: %s"),*Image),Captured);
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("image"),TEXT("UI/")+Image);J->SetStringField(TEXT("view"),View.Name);
  J->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());J->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());
  J->SetField(TEXT("camera_world_cm"),VectorJSON(View.Camera));J->SetField(TEXT("target_world_cm"),VectorJSON(View.Target));
  J->SetField(TEXT("player_fixture_world_cm"),VectorJSON(View.Fixture));J->SetNumberField(TEXT("fov"),View.FOV);
  J->SetBoolField(TEXT("fixed_camera"),true);J->SetBoolField(TEXT("setup_teleport"),true);J->SetBoolField(TEXT("simulation_paused"),false);
   J->SetNumberField(TEXT("warmup_engine_frames"),double(GFrameCounter-ViewFrame));J->SetNumberField(TEXT("warmup_wall_seconds"),FPlatformTime::Seconds()-ViewAt);
  J->SetBoolField(TEXT("captured"),Captured);J->SetArrayField(TEXT("lighting"),ClockFacts(W,Test));Shots.Add(MakeShared<FJsonValueObject>(J));
  Manifest->SetArrayField(TEXT("views"),Shots);WriteManifest(Manifest,ManifestName());
  if(++ViewIndex>=Views.Num()){ViewIndex=0;if(++HourIndex==3)return Finish(PC);}
  BeginView(W,PC,P);return false;
 }
private:
 const TCHAR* ManifestName()const{return bPlatformOnly?TEXT("PlatformRepairVisual16.json"):TEXT("CloseVisualFinal15.json");}
 void BeginView(UWorld* W,APlayerController* PC,ANeonCityPlayerCharacter* P)
 {
  const auto& View=Views[ViewIndex];Clock->SetTimeOfDay(HourIndex==0?12:HourIndex==1?17:22);
  for(TActorIterator<AIslandAdventureLighting> It(W);It;++It)It->Tick(0);
  P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(View.Fixture,false,nullptr,ETeleportType::TeleportPhysics);
  Camera->SetActorLocation(View.Camera);Camera->SetActorRotation((View.Target-View.Camera).Rotation());Camera->GetCameraComponent()->SetFieldOfView(View.FOV);
   PC->SetViewTarget(Camera.Get());ViewAt=FPlatformTime::Seconds();ViewFrame=GFrameCounter;
 }
 bool Finish(APlayerController* PC)
 {
  Manifest->SetArrayField(TEXT("views"),Shots);Manifest->SetNumberField(TEXT("captured_view_count"),Shots.Num());
   if(Initialized)
   {
    const int32 Required=bPlatformOnly?11:35;
    Test->TestEqual(TEXT("All required native Day/Sunset/Night views were actually captured"),Shots.Num(),Required*3);
    for(double Hour:{12.0,17.0,22.0})
    {
     TSet<FString> Names;int32 Count=0;
     for(const auto& Shot:Shots)if(FMath::Abs(Shot->AsObject()->GetNumberField(TEXT("hour"))-Hour)<.01)
     {++Count;if(Shot->AsObject()->GetBoolField(TEXT("captured")))Names.Add(Shot->AsObject()->GetStringField(TEXT("view")));}
     Test->TestEqual(*FString::Printf(TEXT("Hour %.0f has all distinct successful native views"),Hour),Names.Num(),Required);
     Test->TestEqual(*FString::Printf(TEXT("Hour %.0f has all image records"),Hour),Count,Required);
    }
   }
  Manifest->SetStringField(TEXT("technical_status"),Test->HasAnyErrors()?TEXT("FAIL"):TEXT("PASS"));
  Test->TestTrue(TEXT("Close visual manifest is saved"),WriteManifest(Manifest,ManifestName()));
  if(Clock.IsValid()){Clock->SetTimeOfDay(OriginalHour);Clock->SetCycleEnabled(OriginalCycle);}
  if(PC && OriginalTarget.IsValid())PC->SetViewTarget(OriginalTarget.Get());
  if(Camera.IsValid())Camera->Destroy();return true;
 }
 FAutomationTestBase* Test;TSharedPtr<FJsonObject> Manifest;TArray<FView> Views;TArray<TSharedPtr<FJsonValue>> Shots;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<AActor> OriginalTarget;
  double Started=0,ViewAt=0;uint64 ViewFrame=0;float OriginalHour=10;bool OriginalCycle=true,Initialized=false,bPlatformOnly=false;int32 ViewIndex=0,HourIndex=0;
};

struct FBoundary { FString Name;TArray<FVector2D> Outline;UStaticMeshComponent* Mesh=nullptr;bool FlightGap=false,Court=false; };
class FTerraceSafetyAudit final:public IAutomationLatentCommand
{
public:
 explicit FTerraceSafetyAudit(FAutomationTestBase* T,bool BoundaryOnly=false):Test(T),Manifest(NewManifest(BoundaryOnly?TEXT("FIX-SPECIFIC: corrected whole platform boundary queries only; completed native 19-waypoint walk is retained separately"):TEXT("Measured whole platform safety plus ordinary physical door and player-input walk after one setup fixture"))),bBoundaryOnly(BoundaryOnly){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(FPlatformTime::Seconds()-Started>600){Test->AddError(FString::Printf(TEXT("Terrace safety timeout stage %d waypoint %d"),Stage,Waypoint));return Finish(PC);}
  if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")))
   {Test->AddError(TEXT("Final platform/ordinary-view evidence requires opt-in native stills without SkipHUD"));return Finish(PC);}
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(MansionOrigin))return false;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OriginalHour=It->GetTimeOfDay();OriginalCycle=It->bCycleEnabled;It->SetCycleEnabled(false);It->SetTimeOfDay(12);break;}
   if(!MeasureBoundaries(W,P))return Finish(PC);
   if(bBoundaryOnly)return Finish(PC);
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(It->GetActorLocation().Z>30000){Door=*It;break;}
   if(!Door.IsValid()){Test->AddError(TEXT("Native residence entry door missing"));return Finish(PC);}
   P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(Door->GetActorTransform().TransformPosition(FVector(65,-190,100)),false,nullptr,ETeleportType::TeleportPhysics);
   Manifest->SetNumberField(TEXT("player_setup_teleport_count"),1);At=FPlatformTime::Seconds();Stage=1;return false;
  }
  auto* R=ANeonCityCoastalRegion::Find(W);if(R)R->EnsureDestinationReady(P->GetActorLocation());
  if(Stage==1)
  {
   if(FPlatformTime::Seconds()-At<1.0 || !P->GetCharacterMovement()->IsMovingOnGround())return false;
   const FVector Aim=Door->GetActorTransform().TransformPosition(FVector(65,0,110));PC->SetControlRotation((Aim-P->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Ordinary player eye trace sees the finished residence door"),P->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(Door.Get())))return Finish(PC);
   Key(PC,EKeys::F,IE_Pressed);At=FPlatformTime::Seconds();Stage=2;return false;
  }
  if(Stage==2)
  {
   if(FPlatformTime::Seconds()-At<.2)return false;Key(PC,EKeys::F,IE_Released);
    if(!Door->IsOpen())
    {
     if(FPlatformTime::Seconds()-At<=20)return false;
     Test->AddError(FString::Printf(TEXT("Native F door did not physically open within 20 wall seconds; leaf yaw %.2f, game delta %.4f"),Door->Leaf->GetRelativeRotation().Yaw,W->GetDeltaSeconds()));return Finish(PC);
    }
    Test->TestTrue(TEXT("Native F input opens the actual physical mansion door"),Door->IsOpen());
   // Walk through the existing floor plan, leave via its physical door, then
   // approach the corrected bench along the existing court/garden route.
   Path={M(-14.5,1.15),M(-14.0,4.2),M(-7.0,5.0),M(-1.0,5.0),M(0,8.0),M(0,11.8),
    M(0,8.0),M(-1.0,5.0),M(-7.0,5.0),M(-14.0,4.2),M(-14.5,1.15),M(-17.8,1.15),
    M(-18.3,7.8),M(-18.3,11.0),M(-20.2,13.5),M(-23.1,15.6),M(-25.0,16.0),M(-25.65,16.0),M(-24.35,16.0)};
    Waypoint=0;At=FPlatformTime::Seconds();WaypointGameAt=W->GetTimeSeconds();Stage=3;return false;
  }
  if(Stage==3)
  {
   const FVector Position=P->GetActorLocation(),Target=Path[Waypoint];const double Distance=FVector::Dist2D(Position,Target);
   if(FPlatformTime::Seconds()-LastSample>.1)
   {
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());J->SetNumberField(TEXT("waypoint"),Waypoint);
    J->SetField(TEXT("player_world_cm"),VectorJSON(Position));J->SetNumberField(TEXT("movement_mode"),int32(P->GetCharacterMovement()->MovementMode));
     J->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-Started);J->SetNumberField(TEXT("game_delta_seconds"),W->GetDeltaSeconds());
     J->SetNumberField(TEXT("speed_cm_s"),P->GetVelocity().Size2D());J->SetNumberField(TEXT("target_distance_cm"),Distance);J->SetNumberField(TEXT("previous_native_forward_analog"),LastAnalog);
    Walk.Add(MakeShared<FJsonValueObject>(J));LastSample=FPlatformTime::Seconds();
   }
   if(Distance<28)
   {
    Key(PC,EKeys::W,IE_Released);HeldW=false;
     LastAnalog=0;
     // Let the unchanged character movement brake. Reaching/crossing a
     // segment does not count: the actual pawn must remain within 28 cm
     // and be physically stopped before a waypoint or still is accepted.
     if(P->GetVelocity().Size2D()>2)return false;
    Test->TestTrue(TEXT("Ordinary route remains on a physical floor"),P->GetCharacterMovement()->IsMovingOnGround());
    if(Waypoint==0 || Waypoint==5 || Waypoint==16)
    {
     const FString Image=FString::Printf(TEXT("Final15-OrdinaryPlayer-%s.png"),Waypoint==0?TEXT("MansionEntry"):Waypoint==5?TEXT("TerraceEntry"):TEXT("BenchApproach"));
     Test->TestTrue(TEXT("Native ordinary player view saved"),FNeonCityVisualEvidence::CaptureHUD(W,Image));
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("image"),TEXT("UI/")+Image);J->SetField(TEXT("player_world_cm"),VectorJSON(Position));
     J->SetBoolField(TEXT("fixed_camera"),false);J->SetBoolField(TEXT("setup_teleport"),false);Shots.Add(MakeShared<FJsonValueObject>(J));
    }
     if(++Waypoint>=Path.Num())return Finish(PC);At=FPlatformTime::Seconds();WaypointGameAt=W->GetTimeSeconds();return false;
   }
    if(W->GetTimeSeconds()-WaypointGameAt>18 || FPlatformTime::Seconds()-At>120)
   {
    Key(PC,EKeys::W,IE_Released);HeldW=false;
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LandFinalWalkBlock),false,P);
    W->SweepSingleByChannel(Hit,Position,FVector(Target.X,Target.Y,Position.Z),FQuat::Identity,ECC_Pawn,
     FCollisionShape::MakeCapsule(P->GetCapsuleComponent()->GetScaledCapsuleRadius(),P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q);
    Test->AddError(FString::Printf(TEXT("Native player walk blocked at waypoint %d distance %.1f: %s / %s; player=%s target=%s"),Waypoint,Distance,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Position.ToString(),*Target.ToString()));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Final15-OrdinaryPlayer-WalkBlocked.png"));return Finish(PC);
   }
   FRotator Direction=(Target-Position).Rotation();Direction.Pitch=-5;Direction.Roll=0;PC->SetControlRotation(Direction);
    const double Delta=FMath::Max(.016,double(W->GetDeltaSeconds()));
    const float MaxSpeed=FMath::Max(1.f,P->GetCharacterMovement()->GetMaxSpeed());
    LastAnalog=FMath::Clamp(float(Distance*.35/(Delta*MaxSpeed)),.025f,1.f);
    if(LastAnalog>=.999f)
    {if(!HeldW){Key(PC,EKeys::W,IE_Pressed);HeldW=true;}}
    else
    {
     if(HeldW){Key(PC,EKeys::W,IE_Released);HeldW=false;}
     P->DoMove(0,LastAnalog); // Existing native player API, no control/physics setting changes.
    }
    return false;
  }
  return false;
 }
private:
 bool RoadInterface(FVector Local,const TArray<FVector>& Road)const
 {
  const FVector World=M(Local.X,Local.Y,Local.Z);
  for(int32 I=FMath::Max(0,Road.Num()-80);I+1<Road.Num();++I)
  {
   const FVector A=Road[I],B=Road[I+1],D=B-A;
   const double T=FMath::Clamp(FVector::DotProduct(FVector(World.X-A.X,World.Y-A.Y,0),FVector(D.X,D.Y,0))/FMath::Max(1.0,D.SizeSquared2D()),0.0,1.0);
   const FVector Closest=A+D*T;
   if(FVector::Dist2D(World,Closest)<435 && World.Z>Closest.Z-85 && World.Z<Closest.Z+460)return true;
  }
  return false;
 }
 bool MeasureBoundaries(UWorld* W,ANeonCityPlayerCharacter* P)
 {
   auto* Metal=FindMesh(W,TEXT("SM_LandMansionMetal"));auto* WalkMesh=FindMesh(W,TEXT("SM_LandSummitWalk"));auto* CourtMesh=FindMesh(W,TEXT("SM_LandMansionDriveway"));
   auto* Bench=FindMesh(W,TEXT("SM_LandLookoutBench"));auto* TerraceFloor=FindMesh(W,TEXT("SM_LandMansionTerrace"));
  if(!Test->TestNotNull(TEXT("Terrace railing native mesh exists"),Metal) || !Test->TestNotNull(TEXT("Lookout native mesh exists"),WalkMesh)
    || !Test->TestNotNull(TEXT("Arrival court native mesh exists"),CourtMesh) || !Test->TestNotNull(TEXT("Retained bench native mesh exists"),Bench)
    || !Test->TestNotNull(TEXT("Terrace floor native mesh exists"),TerraceFloor))return false;
  TSharedPtr<FJsonObject> Prepared;TArray<FVector> Road;
  if(!ReadJSON(FPaths::ProjectDir()/TEXT("ArtSource/IslandLandPass1/prepared.json"),Prepared) || !ReadRoad(Road))
  {Test->AddError(TEXT("Authored outline/road provenance missing"));return false;}
  const auto Revision=Prepared->GetObjectField(TEXT("summit_site_revision"));TArray<FBoundary> Boundaries;
  for(const TCHAR* Name:{TEXT("court_outline"),TEXT("lookout_outline")})
  {
   FBoundary B;B.Name=Name;B.Court=FString(Name).StartsWith(TEXT("court"));B.FlightGap=!B.Court;B.Mesh=B.Court?CourtMesh:WalkMesh;
   for(const auto& V:Revision->GetArrayField(Name)){const auto& XY=V->AsArray();B.Outline.Add(FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber()));}
   if(B.Outline.Num()<8){Test->AddError(TEXT("Canonical rounded platform outline is incomplete"));return false;}Boundaries.Add(MoveTemp(B));
  }
  FBoundary Terrace;Terrace.Name=TEXT("terrace_three_open_edges");Terrace.Mesh=Metal;
  Terrace.Outline={FVector2D(-7.5,10.15),FVector2D(-7.5,18.925),FVector2D(16.5,18.925),FVector2D(16.5,10.15)};
  Boundaries.Add(Terrace);FCollisionQueryParams Q(SCENE_QUERY_STAT(LandFinalBoundary),true,P);
  const float Radius=P->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   FCollisionQueryParams PawnQ(SCENE_QUERY_STAT(LandFinalNativePawnBoundary),false,P);FCollisionResponseParams PawnResponse;
   P->GetCapsuleComponent()->InitSweepCollisionParams(PawnQ,PawnResponse);
   Manifest->SetBoolField(TEXT("pawn_sweep_bTraceComplex"),PawnQ.bTraceComplex);
   Manifest->SetBoolField(TEXT("pawn_sweep_uses_native_capsule_collision_parameters"),true);
   Manifest->SetStringField(TEXT("native_rail_collision_proof"),TEXT("Normal full-scene Pawn sweep plus direct native physics-body capsule sweep on a blocking rail component; furniture cannot substitute for railing collision."));
   TArray<UStaticMeshComponent*> RailMeshes={WalkMesh,CourtMesh,Metal};
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C?C->GetStaticMesh().Get():nullptr;
    if(Mesh && Mesh->GetName().StartsWith(TEXT("SM_AdventureRoad_"))
     && C->Bounds.GetBox().ComputeSquaredDistanceToPoint(Bench->GetComponentLocation())<FMath::Square(500.0))RailMeshes.AddUnique(C);
   }
  for(const auto& B:Boundaries)
  {
   const bool IsTerrace=B.Name.StartsWith(TEXT("terrace"));
    double SignedArea=0;
    for(int32 I=0;I<B.Outline.Num();++I)
    {const auto& A=B.Outline[I];const auto& Z=B.Outline[(I+1)%B.Outline.Num()];SignedArea+=A.X*Z.Y-Z.X*A.Y;}
    if(FMath::Abs(SignedArea)<.001){Test->AddError(TEXT("Platform outline has no unambiguous winding"));continue;}
   const int32 Segments=IsTerrace?B.Outline.Num()-1:B.Outline.Num();
   for(int32 I=0;I<Segments;++I)
   {
    const FVector2D A=B.Outline[I],Z=B.Outline[(I+1)%B.Outline.Num()];const double Length=(Z-A).Size();
    const int32 Count=FMath::Max(1,FMath::CeilToInt(Length/.25));
    for(int32 K=0;K<Count;++K)
    {
     const FVector2D Point=FMath::Lerp(A,Z,(K+.5)/Count);FString Skip;
     FVector2D Inward=FVector2D(-(Z-A).Y,(Z-A).X).GetSafeNormal();if(SignedArea<0)Inward=-Inward;
     if(B.Court && FMath::Abs(Point.X+16)<.03 && Point.Y<10)Skip=TEXT("Adjoining residence wall / authored garage or entry interface");
     if(B.FlightGap && Point.Y>18 && Point.X>-24.6 && Point.X<-22.1)Skip=TEXT("Preserved Wingsuit launch path");
     const FVector2D SampleA=FMath::Lerp(A,Z,double(K)/Count),SampleZ=FMath::Lerp(A,Z,double(K+1)/Count);
     if(!IsTerrace && (RoadInterface(FVector(Point.X,Point.Y,0),Road) || RoadInterface(FVector(SampleA.X,SampleA.Y,0),Road) || RoadInterface(FVector(SampleZ.X,SampleZ.Y,0),Road)))
      Skip=TEXT("Preserved road / guardrail interface: entire authored short rail segment endpoints checked");
     if(!IsTerrace && Skip.IsEmpty())
     {
      // An outline inside the other finished platform is an internal join,
      // not a cliff edge. Require actual physical paving across it before
      // treating this as access; authored polygons alone cannot hide a gap.
      auto* Other=B.Court?WalkMesh:CourtMesh;FHitResult SharedFloor;
      const FVector2D Outside=Point-Inward*.12;
      if(Other->LineTraceComponent(SharedFloor,M(Outside.X,Outside.Y,.18),M(Outside.X,Outside.Y,-.60),Q)
       && SharedFloor.ImpactNormal.Z>.6)
       Skip=TEXT("Court/lookout internal join on actual overlapping physical paving; native player walk separately verifies access");
     }
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("boundary"),B.Name);J->SetField(TEXT("point_local_m"),VectorJSON(FVector(Point.X,Point.Y,0)));
     if(!Skip.IsEmpty()){J->SetStringField(TEXT("intentional_open_interface"),Skip);Contacts.Add(MakeShared<FJsonValueObject>(J));continue;}
      // A vertex-average centre can lie outside the concave court. Its
      // signed winding defines the true interior for every local edge.
     // Measure the actual graded paving immediately inside the edge before
     // querying its rail. Apron12 legitimately lowered rail/paving together.
      double FloorZ=IsTerrace?.04:-.015;FHitResult Floor;
      const bool FloorHit=(IsTerrace?TerraceFloor:B.Mesh)->LineTraceComponent(Floor,M(Point.X+Inward.X*.12,Point.Y+Inward.Y*.12,.18),M(Point.X+Inward.X*.12,Point.Y+Inward.Y*.12,-1.1),Q);
      if(FloorHit)FloorZ=(Floor.ImpactPoint.Z-MansionOrigin.Z)/100;
     const FVector Across(Inward.X*45,-Inward.Y*45,0);FHitResult Middle,Top;
     const FVector Mid=M(Point.X,Point.Y,FloorZ+(IsTerrace?.58:.58));
     const FVector High=M(Point.X,Point.Y,FloorZ+(IsTerrace?1.02:1.08));
     // Thin rail centre heights differ slightly between the retained sea
     // edge and side edges. Use a small vertical sweep, keeping real geometry.
     auto RailHit=[&](const FVector& At,FHitResult& Hit)
     {
      // The graded court's edge and its floor 12cm inside can differ in Z.
      // Locate the actual tube, then independently require its measured safe
      // height. A narrow fixed-height ray must not misreport a real rail gap.
      for(int32 Step=0;Step<=40;++Step)
      {
       const double Dz=Step==0?0:(Step%2?-double((Step+1)/2):double(Step/2));
       if(B.Mesh->LineTraceComponent(Hit,At-Across+FVector(0,0,Dz),At+Across+FVector(0,0,Dz),Q))return true;
      }
      return false;
     };
     const bool MidHit=RailHit(Mid,Middle),TopHit=RailHit(High,Top);
     const double MiddleHeight=MidHit?Middle.ImpactPoint.Z-(MansionOrigin.Z+FloorZ*100):-1;
     const double TopHeight=TopHit?Top.ImpactPoint.Z-(MansionOrigin.Z+FloorZ*100):-1;
     const FVector Start=M(Point.X+Inward.X*1.1,Point.Y+Inward.Y*1.1,FloorZ)+FVector(0,0,Half+1);
     const FVector End=M(Point.X-Inward.X*1.0,Point.Y-Inward.Y*1.0,FloorZ)+FVector(0,0,Half+1);
      FHitResult Safety;const bool Blocked=W->SweepSingleByChannel(Safety,Start,End,FQuat::Identity,P->GetCapsuleComponent()->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Radius,Half),PawnQ,PawnResponse);
      const double ImpactHeight=Blocked?Safety.ImpactPoint.Z-(MansionOrigin.Z+FloorZ*100):-1;
      bool NativeRailBlocked=false;double RailImpactHeight=-1;UStaticMeshComponent* PhysicalRail=nullptr;
      // A planter/bench may be the first normal world-sweep hit. Keep that
      // result, but independently require native physics-body rail contact
      // with the same simple/complex choice and actual Pawn responses.
      for(auto* C:RailMeshes)
      {
       if(!C->K2_IsQueryCollisionEnabled() || C->GetCollisionResponseToChannel(P->GetCapsuleComponent()->GetCollisionObjectType())!=ECR_Block
        || PawnResponse.CollisionResponse.GetResponse(C->GetCollisionObjectType())!=ECR_Block)continue;
       FHitResult Rail;
       if(C->SweepComponent(Rail,Start,End,FQuat::Identity,FCollisionShape::MakeCapsule(Radius,Half),PawnQ.bTraceComplex))
       {
        const double H=Rail.ImpactPoint.Z-(MansionOrigin.Z+FloorZ*100);
        if(H>15 && H<140){NativeRailBlocked=Blocked;RailImpactHeight=H;PhysicalRail=C;break;}
       }
       // At a wall/corner the inside start can already overlap unrelated
       // geometry in this joined mesh. Probe the same native capsule from
       // outside as well, retaining the ordinary world-sweep requirement.
       if(C->SweepComponent(Rail,End,Start,FQuat::Identity,FCollisionShape::MakeCapsule(Radius,Half),PawnQ.bTraceComplex))
       {
        const double H=Rail.ImpactPoint.Z-(MansionOrigin.Z+FloorZ*100);
        if(H>15 && H<140){NativeRailBlocked=Blocked;RailImpactHeight=H;PhysicalRail=C;break;}
       }
      }
      J->SetNumberField(TEXT("floor_local_z_m"),FloorZ);J->SetBoolField(TEXT("inside_edge_floor_hit"),FloorHit);
      J->SetBoolField(TEXT("middle_rail_contact"),MidHit);J->SetBoolField(TEXT("top_rail_contact"),TopHit);
     J->SetBoolField(TEXT("pawn_capsule_blocked"),Blocked);J->SetStringField(TEXT("pawn_blocker"),GetPathNameSafe(Safety.GetComponent()));
      J->SetBoolField(TEXT("native_rail_blocks_pawn"),NativeRailBlocked);J->SetNumberField(TEXT("pawn_impact_above_floor_cm"),ImpactHeight);
      J->SetNumberField(TEXT("native_rail_impact_above_floor_cm"),RailImpactHeight);J->SetStringField(TEXT("native_rail_collision_component"),GetPathNameSafe(PhysicalRail));
     J->SetNumberField(TEXT("middle_height_above_floor_cm"),MiddleHeight);J->SetNumberField(TEXT("top_height_above_floor_cm"),TopHeight);
     Contacts.Add(MakeShared<FJsonValueObject>(J));
      if(!FloorHit || !MidHit || !TopHit || MiddleHeight<38 || MiddleHeight>80 || TopHeight<88 || TopHeight>128
       || !NativeRailBlocked || B.Mesh->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)
       Test->AddError(FString::Printf(TEXT("Platform safety gap %s segment %d sample %d at %.3f,%.3f floor %.3f: floor=%d mid=%d top=%d native_rail=%d blocker=%s impact_z=%.2f"),
        *B.Name,I,K,Point.X,Point.Y,FloorZ,FloorHit,MidHit,TopHit,NativeRailBlocked,*GetNameSafe(Safety.GetComponent()),ImpactHeight));
    }
   }
  }
  // Real three-metre living/terrace access remains open at walking height.
  FHitResult Entry;const bool EntryBlocked=W->SweepSingleByChannel(Entry,M(0,9.0,.04)+FVector(0,0,Half+1),M(0,11.3,.04)+FVector(0,0,Half+1),
    FQuat::Identity,P->GetCapsuleComponent()->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Radius,Half),PawnQ,PawnResponse);
  Test->TestFalse(TEXT("Authored living-to-terrace player entrance stays open"),EntryBlocked);
  // Probe the full measured seat envelope against the rail-containing native
  // mesh, rather than declaring overlapping coarse component AABBs a defect.
   int32 BenchRailContacts=0;TArray<TSharedPtr<FJsonValue>> BenchSources;
   for(auto* C:RailMeshes){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("component"),C->GetPathName());J->SetStringField(TEXT("mesh"),C->GetStaticMesh()->GetPathName());BenchSources.Add(MakeShared<FJsonValueObject>(J));}
   const FTransform BenchTransform=Bench->GetComponentTransform();
   for(int32 Z=43;Z<=49;++Z)for(int32 Y=0;Y<=56;++Y)for(auto* C:RailMeshes)
  {
    FHitResult Hit;
    if(C->LineTraceComponent(Hit,BenchTransform.TransformPosition(FVector(-160,-28+Y,Z)),BenchTransform.TransformPosition(FVector(160,-28+Y,Z)),Q))++BenchRailContacts;
  }
  Test->TestEqual(TEXT("Corrected bench seat does not intersect the platform railing"),BenchRailContacts,0);
  Manifest->SetNumberField(TEXT("bench_seat_rail_contacts"),BenchRailContacts);Manifest->SetBoolField(TEXT("terrace_player_entry_open"),!EntryBlocked);
   Manifest->SetArrayField(TEXT("bench_neighbor_rail_meshes"),BenchSources);Manifest->SetNumberField(TEXT("bench_seat_probe_grid_cm"),1);
   Manifest->SetNumberField(TEXT("bench_seat_probe_rays_per_neighbor_mesh"),399);
  Manifest->SetArrayField(TEXT("boundary_samples"),Contacts);WriteManifest(Manifest,TEXT("TerraceSafetyFinal15.json"));
  // Continue the native walk even if a measured geometry gate failed, so the
  // report retains useful independent evidence without claiming a PASS.
  return true;
 }
 bool Finish(APlayerController* PC)
 {
  if(PC){Key(PC,EKeys::W,IE_Released);Key(PC,EKeys::F,IE_Released);}HeldW=false;
  if(Clock.IsValid()){Clock->SetTimeOfDay(OriginalHour);Clock->SetCycleEnabled(OriginalCycle);}
  Manifest->SetArrayField(TEXT("boundary_samples"),Contacts);Manifest->SetArrayField(TEXT("ordinary_input_walk"),Walk);Manifest->SetArrayField(TEXT("ordinary_player_views"),Shots);
  Manifest->SetNumberField(TEXT("completed_waypoints"),Waypoint);Manifest->SetNumberField(TEXT("required_waypoints"),Path.Num());
  Manifest->SetStringField(TEXT("technical_status"),Test->HasAnyErrors()?TEXT("FAIL"):TEXT("PASS"));
  Manifest->SetBoolField(TEXT("boundary_only_fix_specific"),bBoundaryOnly);
  Test->TestTrue(TEXT("Terrace safety manifest is saved"),WriteManifest(Manifest,bBoundaryOnly?TEXT("TerraceBoundaryRepair16.json"):TEXT("TerraceSafetyFinal15.json")));return true;
 }
 FAutomationTestBase* Test;TSharedPtr<FJsonObject> Manifest;TArray<TSharedPtr<FJsonValue>> Contacts,Walk,Shots;TArray<FVector> Path;
 TWeakObjectPtr<AIslandLandDoor> Door;TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
  double Started=0,At=0,LastSample=0,WaypointGameAt=0;float OriginalHour=10,LastAnalog=0;bool OriginalCycle=true,HeldW=false,bBoundaryOnly=false;int32 Stage=0,Waypoint=0;
};
class FMansionAssetFactsAudit final:public IAutomationLatentCommand
{
public:
 explicit FMansionAssetFactsAudit(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>40){Test->AddError(TEXT("Mansion asset facts did not become ready"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;if(!W || W->GetTimeSeconds()<3)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(MansionOrigin))return false;
  auto Facts=NewManifest(TEXT("FIX-SPECIFIC: mansion-only instance/material/scale/collision facts; reuse unchanged 105 native camera images; no fresh camera or subjective-review claim"));
  Facts->SetArrayField(TEXT("native_meshes"),MeshFacts(W,Test));CheckImportedDetails(W,Test,Facts);
  Facts->SetBoolField(TEXT("new_camera_images_captured"),false);
  Facts->SetStringField(TEXT("technical_status"),Test->HasAnyErrors()?TEXT("FAIL"):TEXT("PASS"));
  Test->TestTrue(TEXT("Focused mansion asset facts saved"),WriteManifest(Facts,TEXT("MansionAssetFacts16.json")));return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandMansionAssetFactsPIE,"NeonCity.IslandLand.MansionAssetFactsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandMansionAssetFactsPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandLandFinalEvidence::FMansionAssetFactsAudit(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandPlatformRepairVisualPIE,"NeonCity.IslandLand.PlatformRepairVisualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandPlatformRepairVisualPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandLandFinalEvidence::FCloseVisualAudit(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandTerraceBoundaryRepairPIE,"NeonCity.IslandLand.TerraceBoundaryRepairPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandTerraceBoundaryRepairPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandLandFinalEvidence::FTerraceSafetyAudit(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandCloseVisualFinalPIE,"NeonCity.IslandLand.CloseVisualFinalPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandCloseVisualFinalPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandLandFinalEvidence::FCloseVisualAudit(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandTerraceSafetyFinalPIE,"NeonCity.IslandLand.TerraceSafetyFinalPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandTerraceSafetyFinalPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(IslandLandFinalEvidence::FTerraceSafetyAudit(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
