#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Adventure/IslandLandLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/RectLight.h"
#include "Components/RectLightComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "ShaderCompiler.h"
#include "Engine/GameViewportClient.h"
#include "ShowFlags.h"
namespace TheOneLayout
{
class FCurrentRooms:public IAutomationLatentCommand
{
public:
 explicit FCurrentRooms(FAutomationTestBase* In,bool InRefined=false,bool InFocused=false,bool InLighting=false):Test(In),Refined(InRefined),Focused(InFocused),Lighting(InLighting){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>(Refined?300:120))return End(TEXT("Current room evidence timeout"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !Cast<ANeonCityPlayerCharacter>(PC->GetPawn()) || W->GetTimeSeconds()<5)return false;
  if(!Ready)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   bool Found=false;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
    if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_GroundLiftPit481")){Pose=It->GetActorTransform();Found=true;break;}
   if(!Found || !Plants())return End(TEXT("Reloaded native absolute vegetation roots failed"));
   TimeIndex=Lighting?1:0;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OldTime=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(Lighting?17.8:12);break;}
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=68;PC->SetViewTarget(Camera);Ready=true;SetView();return false;
  }
  Evidence.Record(W,PC,TimeIndex*8+View);if(W->GetTimeSeconds()-At<3.5)return false;
  const TCHAR* Time=TimeIndex==0?TEXT("Day"):TimeIndex==1?TEXT("Sunset"):TEXT("Night");
  if(!FNeonCityVisualEvidence::CaptureHUD(W,Refined?FString::Printf(TEXT("TheOne-%s-%s-%02d.png"),Lighting?TEXT("Lighting"):TEXT("Finish"),Time,ViewIndex()):FString::Printf(TEXT("TheOne-Layout-%02d.png"),View)))return End(TEXT("Actual room HUD capture unavailable"));
  ++Captured;
  if(++View==(Lighting?3:Focused?2:8))
  {
   if(!Refined || ++TimeIndex==3)return End(TEXT(""));View=0;Clock->SetTimeOfDay(TimeIndex==1?17.8:22);
  }
  SetView();return false;
 }
private:
 bool Plants()
 {
  TArray<uint8> Height;if(!FFileHelper::LoadFileToArray(Height,*(FPaths::ProjectDir()/(Refined?TEXT("ArtSource/Phase205cUpgrade/TheOne522Hillside/after.r16"):TEXT("ArtSource/Phase205cUpgrade/TheOne472Arrival/after.r16")))) || Height.Num()!=2041*2041*2)return false;
  auto H=[&](int32 X,int32 Y){const int32 B=(Y*2041+X)*2;return double(Height[B]|(uint16(Height[B+1])<<8));};
  ALandscape* Landscape=nullptr;for(TActorIterator<ALandscape> It(W);It;++It){Landscape=*It;break;}if(!Landscape)return false;
  int32 Owners=0;
  for(TActorIterator<ANeonCityReferenceVegetation> It(W);It;++It)
  {
   if(!It->Data || !(It->Data->GetPathName().Contains(Refined?TEXT("/TheOne525/"):TEXT("/TheOne502/")) || (Refined && (It->Data->GetPathName().Contains(TEXT("/TheOne533/")) || It->Data->GetPathName().Contains(TEXT("/TheOne545/"))))))return false;++Owners;
   for(int32 G=0;G<It->Data->Groups.Num();++G)
   {
    const auto& Group=It->Data->Groups[G];auto* Instances=It->GetPlantGroup(G);
    if(!Instances || Instances->GetInstanceCount()!=Group.Instances.Num())return false;
    for(const auto& Plant:Group.Instances)
    {
     const FVector P=Plant.Transform.GetLocation();if(P.X<=-26500 || P.X>=31500 || P.Y<=26000 || P.Y>=78000)continue;
     const double U=(P.X+102350)/204700*2040,V=(-P.Y+102350)/204700*2040;const int32 X=FMath::FloorToInt(U),Y=FMath::FloorToInt(V);const double A=U-X,B=V-Y;
     const double Q=H(X,Y),R=H(X+1,Y),S=H(X,Y+1),T=H(X+1,Y+1);
     const double Ground=(A+B<=1?Q+(R-Q)*A+(S-Q)*B:T+(S-T)*(1-A)+(R-T)*(1-B))/128*100-12800;
     MaxRawError=FMath::Max(MaxRawError,FMath::Abs(P.Z-Ground+3));
     const FVector WorldRoot=It->GetActorTransform().TransformPosition(P);const auto NativeGround=Landscape->GetHeightAtLocation(WorldRoot);if(!NativeGround.IsSet())return false;
     MaxRootError=FMath::Max(MaxRootError,FMath::Abs(WorldRoot.Z-NativeGround.GetValue()+3));++Roots;
    }
   }
  }
  // The raw independent interpolation uses exported double grid spacing;
  // native collision stores float component transforms. Keep its discrepancy
  // below one R16 vertical quantum, and native root contact below one mm.
  UE_LOG(LogTemp,Display,TEXT("TheOneLayout504 reload actual HISM owners%d roots%d maximum_native_ground_error_cm%.5f independent_raw_error_cm%.5f r16_quantum_cm%.5f"),Owners,Roots,MaxRootError,MaxRawError,100./128);
  return Owners==2 && Roots>1000 && MaxRootError<.1 && MaxRawError<100./128;
 }
 FVector World(FVector P){return Pose.TransformPosition(FVector(P.X*100,-P.Y*100,P.Z*100));}
 int32 ViewIndex()const{const int32 Indices[]={1,3,6};return Lighting?Indices[View]:Focused?(View?3:0):View;}
 void SetView()
 {
  const FVector Eyes[]={FVector(-88,-100,30),FVector(-60,85,24),FVector(-54,-18,2),FVector(9.8,16,4.7),FVector(8.5,15,9.3),FVector(22,-8.5,9.3),FVector(36.5,-17.5,4.9),FVector(-34,5,16.8)};
  const FVector Aims[]={FVector(0,0,6),FVector(0,0,6),FVector(-43.1,-10.66,1.3),FVector(7.25,9.34,4.2),FVector(5.4,8,8.6),FVector(17,-10.3,8.8),FVector(34,-15,4.1),FVector(-15,10,14)};
  const int32 Index=ViewIndex();const FVector E=World(Eyes[Index]),T=World(Aims[Index]);Camera->SetActorLocationAndRotation(E,(T-E).Rotation());At=W->GetTimeSeconds();
 }
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldTime);Clock->SetCycleEnabled(OldCycle);}
  UE_LOG(LogTemp,Display,TEXT("TheOneLayout504 result=%s roots=%d maximum_root_error_cm=%.5f images%d scope=current native asset reload and actual fixed PIE evidence; visual review remains separate"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"),Roots,MaxRootError,Captured);return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;
 TWeakObjectPtr<AActor> OldView;FTransform Pose;double Started=0,MaxRootError=0,MaxRawError=0;int32 Roots=0,View=0,TimeIndex=0,Captured=0;float At=0,OldTime=0;bool Ready=false,OldCycle=false,Refined=false,Focused=false,Lighting=false;
 FNeonCityVisualEvidence Evidence{TEXT("TheOneLayout504")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneLayout504,"NeonCity.Phase205cUpgrade.TheOneLayoutEvidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneLayout504::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOneLayout::FCurrentRooms(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneSiteFinish526,"NeonCity.Phase205cUpgrade.TheOneSiteFinishEvidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneSiteFinish526::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOneLayout::FCurrentRooms(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneFinish534,"NeonCity.Phase205cUpgrade.TheOneFinishMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneFinish534::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOneLayout::FCurrentRooms(this,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneLight539,"NeonCity.Phase205cUpgrade.TheOneLightingMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneLight539::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOneLayout::FCurrentRooms(this,true,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
namespace TheOneAllRooms
{
class FSurvey final:public IAutomationLatentCommand
{
public:
 explicit FSurvey(FAutomationTestBase* In):Test(In){}
 ~FSurvey(){RestoreProbe();}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>900)return End(TEXT("All-room evidence exceeded finite bound including first native shader compilation"));
  if(!PC || !W->HasBegunPlay() || W->GetTimeSeconds()<4)return false;
  if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling())return false;
  if(!PolicyReady)
  {
   for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage")})
   {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Required comparison quality variable absent"));OldPolicy.Add(Name,C->GetFloat());C->Set(FCString::Strcmp(Name,TEXT("r.ScreenPercentage"))==0?100.f:3.f,ECVF_SetByConsole);}
   PolicyReady=true;
  }
  for(const auto& Entry:OldPolicy){const auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Key);const float Required=Entry.Key==TEXT("r.ScreenPercentage")?100.f:3.f;if(!C || !FMath::IsNearlyEqual(C->GetFloat(),Required))return End(TEXT("Comparison quality changed during capture"));}
  if(!Ready)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   AStaticMeshActor* Floor=nullptr;for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetName()==TEXT("StaticMeshActor_433"))Floor=*I;
   if(!Floor || !Floor->GetStaticMeshComponent()->GetStaticMesh())return End(TEXT("Current native estate floor absent"));
   const FString FloorPath=Floor->GetStaticMeshComponent()->GetStaticMesh()->GetPathName();
   if(FloorPath!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/TheOne703/SM_TheOne_Interior703_Ground.SM_TheOne_Interior703_Ground")
    && FloorPath!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/TheOne786/SM_TheOne786_Ground.SM_TheOne786_Ground"))return End(TEXT("Exact reviewed703 or arrival786 native floor required"));Pose=Floor->GetActorTransform();
   FString Text;TSharedPtr<FJsonObject> Data;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/InteriorComparison641.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return End(TEXT("Versioned original all-room comparison definitions absent"));
   for(const auto& V:Data->GetArrayField(TEXT("rooms")))
   {
    auto R=V->AsObject();FView View;View.Name=R->GetStringField(TEXT("id"));View.Eye=Vector(R,TEXT("camera_m"));View.Aim=Vector(R,TEXT("camera_target_m"));View.Floor=R->GetArrayField(TEXT("centre_m"))[2]->AsNumber();View.CheckFloor=true;Views.Add(View);
   }
   if(Views.Num()!=33)return End(TEXT("All33 registered rooms required"));
   for(const auto& V:Data->GetArrayField(TEXT("cameras")))
   {auto R=V->AsObject();FView View;View.Name=R->GetStringField(TEXT("id"));View.Eye=Vector(R,TEXT("camera_m"));View.Aim=Vector(R,TEXT("camera_target_m"));Views.Add(View);}
   for(float Hour:{12.f,17.8f,22.f})
   {
    Views.Add({FString::Printf(TEXT("PhotoLeisure_%.1f"),Hour),FVector(78,-73,18),FVector(43,-35,3),Hour});
    Views.Add({FString::Printf(TEXT("EntryGlass_%.1f"),Hour),FVector(-28,28,5),FVector(-12,24,6),Hour});
    Views.Add({FString::Printf(TEXT("CoveredBBQ_%.1f"),Hour),FVector(57,-46,4.7),FVector(63,-41,4.1),Hour});
   }
   // Match the actual continuous-flight camera that exposed detached chaise parts.
   // This fixed comparison checks the local repair; it is not a new flight claim.
   const FVector Flight=NeonCityCoastalLayout::FromLegacyIsland(FVector(52343.923,-112234.064,28854.025)),FlightAim(45843.804,-119234.008,27509.198);
   const FVector AerialEye=Flight-(FlightAim-Flight).GetSafeNormal()*1050+FVector(0,0,200);
   auto SourceMetres=[&](FVector P){const FVector Q=Pose.InverseTransformPosition(P);return FVector(Q.X*.01,-Q.Y*.01,Q.Z*.01);};
   FView Aerial;Aerial.Name=TEXT("AerialChaiseClosure");Aerial.Eye=SourceMetres(AerialEye);Aerial.Aim=SourceMetres(FlightAim);Aerial.FOV=90;Views.Add(Aerial);
   // New bounded garden comparison cameras, sampled from the unchanged634d
   // heightfield and702 masks. Existing IDs0..53 remain stable. Native clearance
   // is still checked below; this is rendered evidence, never traversal proof.
   for(float Hour:{12.f,17.8f,22.f})
   {
    FView Near;Near.Name=FString::Printf(TEXT("GardenNear702_%.1f"),Hour);Near.Eye=FVector(-94,-4,1.10);Near.Aim=FVector(-89.5,1.5,-.25);Near.Hour=Hour;Near.FOV=58;Views.Add(Near);
    FView Middle;Middle.Name=FString::Printf(TEXT("GardenMiddle702_%.1f"),Hour);Middle.Eye=FVector(-98,-7,3);Middle.Aim=FVector(-89,1,-.1);Middle.Hour=Hour;Views.Add(Middle);
    FView Far;Far.Name=FString::Printf(TEXT("GardenFar702_%.1f"),Hour);Far.Eye=FVector(-112,-36,18);Far.Aim=FVector(-70,-7,.2);Far.Hour=Hour;Views.Add(Far);
   }
   //855 reuses the continuous-flight tripod and adds explicit shoulder/near
   //views. IDs0..62 are unchanged; no player pose or render-policy fixture.
   for(float Hour:{12.f,17.8f,22.f})
   {
    FView Wide;Wide.Name=FString::Printf(TEXT("SummitCover855_Aerial_%.1f"),Hour);Wide.Eye=SourceMetres(AerialEye);Wide.Aim=SourceMetres(FlightAim);Wide.FOV=90;Wide.Hour=Hour;Views.Add(Wide);
    FView Front;Front.Name=FString::Printf(TEXT("SummitCover855_Front_%.1f"),Hour);Front.Eye=SourceMetres(NeonCityCoastalLayout::FromLegacyIsland(FVector(57800,-122300,28800)));Front.Aim=SourceMetres(NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-120500,26000)));Front.FOV=70;Front.Hour=Hour;Views.Add(Front);
    FView Near;Near.Name=FString::Printf(TEXT("SummitCover855_Near_%.1f"),Hour);Near.Eye=SourceMetres(NeonCityCoastalLayout::FromLegacyIsland(FVector(50000,-111500,25200)));Near.Aim=SourceMetres(NeonCityCoastalLayout::FromLegacyIsland(FVector(50000,-113000,22700)));Near.FOV=58;Near.Hour=Hour;Views.Add(Near);
   }
   // Preserve stable evidence IDs while capturing only views affected by a fix.
   // The cinema comparison looks towards the actual existing screen.
   Views[2].Eye=FVector(-9.1,-2.5,1.6);Views[2].Aim=FVector(-1.65,0,1.8);
   Views[12].Eye=FVector(-.3,3.2,4.648);Views[12].Aim=FVector(-2.7,11,4.2);
   // Four new reading chairs occupy old comparison tripod positions. Keep
   // their real collision and choose a nearby supported, full-capsule camera
   // fixture; never ignore furniture to make the floor probe pass.
   // Room628 reflows dressing into the bedroom; this is the supported library
   // balcony overlooking its duplex void. Preserve the authored downward aim.
   Views[18].Name=TEXT("Upper:North suite library balcony");
   const APawn* ComparisonPawn=PC->GetPawn();
   if(!ComparisonPawn)return End(TEXT("Native player capsule absent for comparison clearance"));
   const float CapsuleRadius=ComparisonPawn->GetSimpleCollisionRadius();
   const float CapsuleHalfHeight=ComparisonPawn->GetSimpleCollisionHalfHeight();
   for(int32 Id:{3,11,15,18})
   {
    auto& V=Views[Id];const FVector Original=V.Eye;bool Found=false;
    for(int32 Ring=0;Ring<=4 && !Found;++Ring)for(int32 Step=0;Step<(Ring?16:1) && !Found;++Step)
    {
     const float Angle=Step*2*PI/16;const FVector Candidate=Original+FVector(Ring*.35f*FMath::Cos(Angle),Ring*.35f*FMath::Sin(Angle),0);
     const FVector Base=S(FVector(Candidate.X,Candidate.Y,V.Floor));FHitResult FloorHit;
     FCollisionQueryParams Probe(SCENE_QUERY_STAT(EstateComparisonTripod),false,PC->GetPawn());
     if(!W->LineTraceSingleByChannel(FloorHit,Base+FVector(0,0,42),Base-FVector(0,0,25),ECC_Pawn,Probe) || FloorHit.ImpactNormal.Z<.7 || FMath::Abs(FloorHit.ImpactPoint.Z-Base.Z)>15)continue;
     if(W->OverlapBlockingTestByChannel(FloorHit.ImpactPoint+FVector(0,0,CapsuleHalfHeight+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalfHeight),Probe) || W->OverlapBlockingTestByChannel(S(Candidate),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(12),Probe))continue;
     FHitResult NearOccluder;const FVector Eye=S(Candidate),Direction=(S(V.Aim)-Eye).GetSafeNormal();
     if(W->LineTraceSingleByChannel(NearOccluder,Eye,Eye+Direction*100,ECC_Visibility,Probe))continue;
     V.Eye=Candidate;Found=true;
    }
    if(!Found)return End(FString::Printf(TEXT("No bounded supported comparison tripod beside new chair in view%d"),Id));
   }
   FString Selection;FParse::Value(FCommandLine::Get(),TEXT("NeonCityRoomIndices="),Selection,false);
   if(!Selection.IsEmpty())
   {
    TArray<FString> Items;Selection.ParseIntoArray(Items,TEXT(","),true);TArray<FView> Selected;
    for(const auto& Item:Items)
    {
     if(!Item.IsNumeric())return End(TEXT("Room view index must be numeric"));
     const int32 Id=FCString::Atoi(*Item);if(!Views.IsValidIndex(Id) || OriginalIndices.Contains(Id))return End(TEXT("Room view index absent or repeated"));
     OriginalIndices.Add(Id);Selected.Add(Views[Id]);
    }
    Views=MoveTemp(Selected);
   }
   else for(int32 Id=0;Id<Views.Num();++Id)OriginalIndices.Add(Id);
   if(Views.IsEmpty())return End(TEXT("No room comparison views selected"));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomLightingProbe")))
   {
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || OriginalIndices.Num()!=1 || OriginalIndices[0]!=0 || !W->GetGameViewport())return End(TEXT("Lighting diagnosis requires isolated native garage view0 only"));
    ProbeViewport=W->GetGameViewport();ProbeFlags=ProbeViewport->EngineShowFlags;ProbeViewMode=ProbeViewport->ViewModeIndex;
    ProbeGI=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomGIProbe"));
    for(const TCHAR* Name:{TEXT("r.Lumen.DiffuseIndirect.Allow"),TEXT("r.Lumen.Reflections.Allow"),TEXT("r.DistanceFieldAO"),TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"),TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"),TEXT("r.Lumen.Visualize"),TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes"),TEXT("r.Lumen.HardwareRayTracing"),TEXT("r.Lumen.TraceMeshSDFs"),TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution"),TEXT("r.Lumen.ScreenProbeGather.RadianceCache.GridResolution"),TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Lighting attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    for(const TCHAR* Name:{TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO"),TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing"),TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.DownsampleFactor")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Occlusion attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    for(const TCHAR* Name:{TEXT("r.LumenScene.Radiosity"),TEXT("r.LumenScene.Radiosity.ProbeSpacing"),TEXT("r.LumenScene.Radiosity.Temporal.MaxFramesAccumulated")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Radiosity attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    for(const TCHAR* Name:{TEXT("r.Lumen.ScreenProbeGather.RadianceCache.SkyVisibility"),TEXT("r.Lumen.ScreenProbeGather.ReferenceMode"),TEXT("r.Lumen.ScreenProbeGather"),TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Sky transport attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    for(const TCHAR* Name:{TEXT("r.RayTracing.Nanite.Mode"),TEXT("r.RayTracing.Culling")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Ray geometry attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    for(const TCHAR* Name:{TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"),TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections"),TEXT("r.Lumen.ScreenProbeGather.DiffuseIntegralMethod"),TEXT("r.Lumen.ScreenProbeGather.IrradianceFormat"),TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ApplyDuringIntegration")})
    {auto* C=IConsoleManager::Get().FindConsoleVariable(Name);if(!C)return End(TEXT("Integration attribution variable absent"));ProbePolicy.Add(Name,C->GetInt());}
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure")))CacheExposureBefore=C->GetFloat();else return End(TEXT("Lighting cache exposure variable absent"));
    const FView Garage=Views[0];Views.Empty();OriginalIndices.Empty();
    for(const TCHAR* Suffix:{TEXT("VerifiedUnlit"),TEXT("DirectNoAO"),TEXT("IndirectNoScreenTrace")})
    {FView V=Garage;V.Name+=FString(TEXT("_Probe872_"))+Suffix;Views.Add(V);OriginalIndices.Add(0);}
    if(ProbeGI){const TCHAR* Names[]={TEXT("NoRadianceCache876"),TEXT("HighGather876"),TEXT("HighScene876")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeHit=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomHitLightingProbe"));
    if(ProbeHit){if(!ProbeGI)return End(TEXT("Hit-lighting attribution requires isolated GI probe"));Views.SetNum(1);OriginalIndices.SetNum(1);Views[0].Name=Garage.Name+TEXT("_HitLighting877");}
    ProbeSurface=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomSurfaceProbe"));
    if(ProbeSurface){if(!ProbeGI || ProbeHit)return End(TEXT("Surface attribution requires isolated GI probe without hit override"));const TCHAR* Names[]={TEXT("SurfaceCoverage878"),TEXT("GeometryNormals878"),TEXT("SurfaceDirect878")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeGather=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomGatherProbe"));
    if(ProbeGather){if(!ProbeGI || ProbeHit || ProbeSurface)return End(TEXT("Gather attribution requires isolated GI probe only"));const TCHAR* Names[]={TEXT("FinalGather4_886"),TEXT("FinalGather8_886"),TEXT("FinalGather4Temporal_886")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeTransport=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomTransportProbe"));
    if(ProbeTransport)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather)return End(TEXT("Transport attribution requires isolated GI probe only"));
     const TCHAR* Names[]={TEXT("NoFixtureEmissive887"),TEXT("RadianceResolution64_887"),TEXT("SoftwareDetail887")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
     ProbeDarkFixture=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/TheOne703/M_TheOne703_Finish703Wall.M_TheOne703_Finish703Wall"));
     if(!ProbeDarkFixture)return End(TEXT("Existing nonemissive fixture comparison material absent"));
     for(TActorIterator<AActor> A(W);A;++A)
     {
      TInlineComponentArray<UStaticMeshComponent*> Parts(*A);
      for(auto* Part:Parts)for(int32 Slot=0;Slot<Part->GetNumMaterials();++Slot)
       if(auto* Mat=Part->GetMaterial(Slot);Mat && Mat->GetName()==TEXT("M_TheOne703_Finish703Light"))ProbeBindings.Add({Part,Slot,Mat});
     }
     if(ProbeBindings.IsEmpty())return End(TEXT("No authored fixture emissive bindings found"));
     Test->AddInfo(FString::Printf(TEXT("TRANSPORT887 temporary fixture slots%d; restore between comparisons"),ProbeBindings.Num()));
    }
    ProbeSpatial=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomSpatialProbe"));
    if(ProbeSpatial){if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport)return End(TEXT("Spatial attribution requires isolated GI probe only"));const TCHAR* Names[]={TEXT("Spatial96_895"),TEXT("Spatial128_895"),TEXT("Spatial96Budget1000_895")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeOcclusion=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomOcclusionProbe"));
    if(ProbeOcclusion){if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial)return End(TEXT("Occlusion attribution requires isolated GI probe only"));const TCHAR* Names[]={TEXT("NoShortAO896"),TEXT("HardwareShortAO896"),TEXT("FullResolutionShortAO896")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeRadiosity=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomRadiosityProbe"));
    if(ProbeRadiosity){if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion)return End(TEXT("Radiosity attribution requires isolated GI probe only"));const TCHAR* Names[]={TEXT("NoRadiosity897"),TEXT("RadiositySpacing1_897"),TEXT("RadiosityTemporal32_897")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];}
    ProbeSources=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomSourcesProbe"));
    if(ProbeSources)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity)return End(TEXT("Source attribution requires isolated GI probe only"));
     const TCHAR* Names[]={TEXT("FlatNormals898"),TEXT("NoSkylight898"),TEXT("CachePreExposure0_898")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
     FlatWall.Reset(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/GarageProbe898/M_FlatWall898.M_FlatWall898")));
     FlatCeiling.Reset(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/GarageProbe898/M_FlatCeiling898.M_FlatCeiling898")));
     if(!FlatWall.IsValid() || !FlatCeiling.IsValid())return End(TEXT("Source-preserving flat-normal diagnostic copies absent"));
     for(TActorIterator<AActor> A(W);A;++A){TInlineComponentArray<UStaticMeshComponent*> Parts(*A);for(auto* Part:Parts)for(int32 Slot=0;Slot<Part->GetNumMaterials();++Slot)if(auto* Mat=Part->GetMaterial(Slot);Mat && (Mat->GetName()==TEXT("M_TheOne703_Finish703Wall") || Mat->GetName()==TEXT("M_TheOne703_Finish703Ceiling")))ProbeBindings.Add({Part,Slot,Mat});}
     if(ProbeBindings.IsEmpty())return End(TEXT("No original wall/ceiling finish bindings for attribution"));
    }
    ProbeSkyTransport=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomSkyTransportProbe"));
    if(ProbeSkyTransport)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity || ProbeSources)return End(TEXT("Sky transport attribution requires isolated GI probe only"));
     Views.Init(Garage,4);OriginalIndices.Init(0,4);const TCHAR* Names[]={TEXT("NoDFAO899"),TEXT("SeparateSkyVisibility899"),TEXT("ReferenceGather899"),TEXT("NoSkylightLeaking899")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
     for(const auto& P:ProbePolicy)Test->AddInfo(FString::Printf(TEXT("SKY899_NATIVE_CVAR %s=%d"),*P.Key,P.Value));
     for(TActorIterator<APostProcessVolume> V(W);V;++V)Test->AddInfo(FString::Printf(TEXT("SKY899_PP %s enabled%d unbound%d priority%.1f sky_leak_override%d leak%.6f scene_quality%.2f gather_quality%.2f cache_resolution%.2f"),*V->GetName(),V->bEnabled,V->bUnbound,V->Priority,V->Settings.bOverride_LumenSkylightLeaking,V->Settings.LumenSkylightLeaking,V->Settings.LumenSceneLightingQuality,V->Settings.LumenFinalGatherQuality,V->Settings.LumenSurfaceCacheResolution));
    }
    ProbeRayGeometry=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomRayGeometryProbe"));
    if(ProbeRayGeometry)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity || ProbeSources || ProbeSkyTransport)return End(TEXT("Ray geometry attribution requires isolated GI probe only"));
     Views.Init(Garage,2);OriginalIndices.Init(0,2);const TCHAR* Names[]={TEXT("NativeNaniteRay903"),TEXT("NoRayCulling903")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
     Test->AddInfo(FString::Printf(TEXT("RAY903 original NaniteMode%d Culling%d; temporary single-variable comparisons, restore at End"),ProbePolicy[TEXT("r.RayTracing.Nanite.Mode")],ProbePolicy[TEXT("r.RayTracing.Culling")]));
    }
    ProbePractical=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomPracticalProbe"));
    if(ProbePractical)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity || ProbeSources || ProbeSkyTransport || ProbeRayGeometry)return End(TEXT("Practical-light attribution requires isolated GI probe only"));
     Views.Init(Garage,2);OriginalIndices.Init(0,2);const TCHAR* Names[]={TEXT("NoSpecular904"),TEXT("PracticalArea904")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
    }
    ProbeFullPath=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomFullPathProbe"));
    if(ProbeFullPath)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity || ProbeSources || ProbeSkyTransport || ProbeRayGeometry || ProbePractical)return End(TEXT("Full-path attribution requires isolated GI probe only"));
     Views.Init(Garage,2);OriginalIndices.Init(0,2);const TCHAR* Names[]={TEXT("FullHardware905"),TEXT("FullHardwareReference905")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
    }
    ProbeIntegration=FParse::Param(FCommandLine::Get(),TEXT("NeonCityRoomIntegrationProbe"));
    if(ProbeIntegration)
    {
     if(!ProbeGI || ProbeHit || ProbeSurface || ProbeGather || ProbeTransport || ProbeSpatial || ProbeOcclusion || ProbeRadiosity || ProbeSources || ProbeSkyTransport || ProbeRayGeometry || ProbePractical || ProbeFullPath)return End(TEXT("Integration attribution requires isolated GI probe only"));
     Views.Init(Garage,3);OriginalIndices.Init(0,3);const TCHAR* Names[]={TEXT("Temporal64_906"),TEXT("NumericalIntegral906"),TEXT("OctahedralIrradiance906")};for(int32 I=0;I<Views.Num();++I)Views[I].Name=Garage.Name+TEXT("_")+Names[I];
    }
    FCollisionQueryParams Attribution(SCENE_QUERY_STAT(GarageSurfaceAttribution),true,PC->GetPawn());Attribution.bReturnFaceIndex=true;
    const FVector E=S(Garage.Eye);
    for(FVector Target:{E+FVector(0,0,600),S(Garage.Aim),S(Garage.Aim)+FVector(0,0,130)})
    {FHitResult H;if(W->LineTraceSingleByChannel(H,E,Target,ECC_Visibility,Attribution)){int32 Section=INDEX_NONE;auto* C=Cast<UStaticMeshComponent>(H.GetComponent());auto* Mat=C?C->GetMaterialFromCollisionFaceIndex(H.FaceIndex,Section):nullptr;Test->AddInfo(FString::Printf(TEXT("GARAGE872_SURFACE %s/%s material=%s face=%d section=%d normal=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(C),*GetPathNameSafe(Mat),H.FaceIndex,Section,*H.ImpactNormal.ToString()));}}
   }
   GarageMotion=FParse::Param(FCommandLine::Get(),TEXT("NeonCityGarageTemporalMotion907"));
   if(GarageMotion)
   {
    if(ProbeViewport.IsValid() || OriginalIndices.Num()!=1 || OriginalIndices[0]!=0 || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")))return End(TEXT("Temporal motion comparison requires normal native garage0 without renderer probe overrides"));
    for(const TCHAR* N:{TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"),TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections")})if(IConsoleManager::Get().FindConsoleVariable(N)->GetInt()!=64)return End(TEXT("Normal authored temporal candidate64 was not loaded"));
    const FView Garage=Views[0];Views.Init(Garage,3);OriginalIndices.Init(0,3);const float Hours[]={12,17.8f,22};for(int32 I=0;I<3;++I){Views[I].Hour=Hours[I];Views[I].Name=FString::Printf(TEXT("GarageTemporal907_%.1f"),Hours[I]);}
   }
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;break;}if(!Clock)return End(TEXT("Actual shared lighting clock absent"));OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);
   if(ProbeSources){ProbeSky=Clock->FindComponentByClass<USkyLightComponent>();if(!ProbeSky.IsValid())return End(TEXT("Shared actual skylight absent"));SkyIntensityBefore=ProbeSky->Intensity;}
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=68;PC->SetViewTarget(Camera);Ready=true;Place();return false;
  }
  // A direct noon-to-night cut needs real eye adaptation/Lumen settling.
  // Keep the actual exposure policy; do not brighten the world for a still.
  // Low-sun adaptation after a bright rock close-up can outlive2.5s. Give
  // explicit Sunset fixtures the same retained settling time as Night.
  const bool LowSun=Views[Index].Hour>=17.5f && Views[Index].Hour<=18.1f;
  float ExplicitSettle=0;FParse::Value(FCommandLine::Get(),TEXT("NeonCityRoomSettleSeconds="),ExplicitSettle);
  const float Settle=FMath::Max(Views[Index].Hour>21 || LowSun?20.f:2.5f,FMath::Clamp(ExplicitSettle,0.f,30.f));
  const float Elapsed=W->GetTimeSeconds()-At;
  if(Elapsed<Settle)return false;
  if(GarageMotion)
  {
   const auto& V=Views[Index];const float Move=Elapsed-Settle;
   const FVector Offset=FVector(0,1.5f*FMath::Sin(PI*FMath::Clamp(Move/4.f,0.f,1.f)),0);
   const FVector Eye=S(V.Eye+Offset);Camera->SetActorLocationAndRotation(Eye,(S(V.Aim+Offset)-Eye).Rotation());
   if(Move<4)
   {
    if(MotionFrame<8 && Move>=MotionFrame*.5f)
    {
     const FString File=FString::Printf(TEXT("Garage-Temporal907-%02d-Move-%02d.png"),Index,MotionFrame);
     if(!FNeonCityVisualEvidence::CaptureHUD(W,File))return End(TEXT("Normal native temporal-motion capture failed"));
     auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("image"),File);Row->SetNumberField(TEXT("hour"),V.Hour);Row->SetNumberField(TEXT("move_seconds"),Move);Row->SetStringField(TEXT("camera_cm"),Eye.ToString());MotionRows.Add(MakeShared<FJsonValueObject>(Row));++MotionFrame;
    }
    return false;
   }
   if(MotionFrame!=8)return End(TEXT("Temporal motion window must contain all eight real captured frames"));
   Camera->SetActorLocationAndRotation(S(V.Eye),(S(V.Aim)-S(V.Eye)).Rotation());
  }
  const auto& View=Views[Index];FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneRooms641Native),false,PC->GetPawn());Q.AddIgnoredActor(Camera);
  const FVector Eye=S(View.Eye);const bool EyeClear=!W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(12),Q);
  FString Failure;if(!EyeClear)Failure=TEXT("Camera intersects physical geometry");FHitResult Hit;
  if(View.CheckFloor)
  {
   const FVector Base=S(FVector(View.Eye.X,View.Eye.Y,View.Floor));
   if(!W->LineTraceSingleByChannel(Hit,Base+FVector(0,0,42),Base-FVector(0,0,25),ECC_Pawn,Q) || Hit.ImpactNormal.Z<.7 || FMath::Abs(Hit.ImpactPoint.Z-Base.Z)>15)
    Failure+=FString::Printf(TEXT(" Native dry floor mismatch: %s"),*GetNameSafe(Hit.GetActor()));
  }
  const FString File=FString::Printf(TEXT("TheOne-AllRooms-%02d-%s.png"),OriginalIndices[Index],*FPaths::MakeValidFileName(View.Name));
  if(View.Hour>21)
  {
   FVector ActualEye;FRotator ActualRotation;PC->GetPlayerViewPoint(ActualEye,ActualRotation);auto* Budget=W->GetSubsystem<UWorldLightingSubsystem>();
   UE_LOG(LogTemp,Display,TEXT("Night evidence%s camera%s budget%d/%d night%.2f"),*File,*ActualEye.ToString(),Budget->GetActiveLightCount(),Budget->GetRegisteredLightCount(),Budget->GetNightAmount());
   for(TActorIterator<AIslandLandLight> I(W);I;++I)if(FVector::Dist(I->GetActorLocation(),Eye)<1800){auto* C=Cast<UPointLightComponent>(I->GetLightComponent());UE_LOG(LogTemp,Display,TEXT("Night local light%s distance%.0f lumens%.1f visible%d cull%.0f"),*I->GetName(),FVector::Dist(I->GetActorLocation(),Eye),C->Intensity,C->IsVisible(),I->CullDistance);}
  }
  if(!FNeonCityVisualEvidence::CaptureHUD(W,File))return End(TEXT("Native rendered screenshot failed"));
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("room_or_facility"),View.Name);R->SetNumberField(TEXT("hour"),View.Hour);R->SetStringField(TEXT("image"),File);R->SetStringField(TEXT("eye_cm"),Eye.ToString());R->SetBoolField(TEXT("clear_camera"),EyeClear);R->SetBoolField(TEXT("floor_checked"),View.CheckFloor);R->SetStringField(TEXT("native_floor_actor"),GetNameSafe(Hit.GetActor()));R->SetStringField(TEXT("failure"),Failure);Rows.Add(MakeShared<FJsonValueObject>(R));
  if(!Failure.IsEmpty()){Test->AddError(View.Name+TEXT(": ")+Failure);++Failures;}
  if(++Index==Views.Num())return End(TEXT(""));Place();return false;
 }
private:
 struct FView {FString Name;FVector Eye,Aim;float Hour=12,Floor=0;bool CheckFloor=false;float FOV=68;};
 static FVector Vector(const TSharedPtr<FJsonObject>& R,const TCHAR* Key){auto A=R->GetArrayField(Key);return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());}
 FVector S(FVector M)const{return Pose.TransformPosition(FVector(M.X*100,-M.Y*100,M.Z*100));}
 void Place(){const auto& V=Views[Index];const FVector Eye=S(V.Eye);Camera->SetActorLocationAndRotation(Eye,(S(V.Aim)-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(V.FOV);Clock->SetTimeOfDay(V.Hour);PC->SetViewTarget(Camera);At=W->GetTimeSeconds();MotionFrame=0;
  if(ProbeViewport.IsValid())
  {
   ProbeViewport->EngineShowFlags=ProbeFlags;
   if(ProbeGI)
   {
    ProbeViewport->ViewModeIndex=ProbeViewMode;
    for(const auto& P:ProbePolicy){auto* C=IConsoleManager::Get().FindConsoleVariable(*P.Key);if(C->GetInt()!=P.Value)C->Set(P.Value,ECVF_SetByConsole);}
    if(ProbeIntegration)
    {
     if(Index==0){IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"))->Set(64.f,ECVF_SetByConsole);IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections"))->Set(64,ECVF_SetByConsole);}
     if(Index==1)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.DiffuseIntegralMethod"))->Set(2,ECVF_SetByConsole);
     if(Index==2){IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.IrradianceFormat"))->Set(1,ECVF_SetByConsole);IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ApplyDuringIntegration"))->Set(0,ECVF_SetByConsole);}
     return;
    }
    if(ProbeFullPath)
    {
     IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"))->Set(0,ECVF_SetByConsole);
     IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache"))->Set(0,ECVF_SetByConsole);
     IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ReferenceMode"))->Set(Index==1?1:0,ECVF_SetByConsole);
     auto& P=Camera->GetCameraComponent()->PostProcessSettings;P.bOverride_LumenRayLightingMode=true;P.LumenRayLightingMode=ELumenRayLightingModeOverride::HitLighting;
     return;
    }
    if(ProbePractical)
    {
     if(Index==0)ProbeViewport->EngineShowFlags.SetSpecular(false);
     if(Index==1)
     {
      auto* Budget=W->GetSubsystem<UWorldLightingSubsystem>();
      for(float X:{-38.5f,-28.8f})for(float Y:{-11.f,-2.f,7.f,16.f})
      {
       auto* A=W->SpawnActor<ARectLight>(S(FVector(X,Y,2.70)),FRotator(-90,0,0));
       auto* L=CastChecked<URectLightComponent>(A->GetLightComponent());L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);L->SetSourceWidth(700);L->SetSourceHeight(12);L->SetAttenuationRadius(1400);L->SetUseTemperature(true);L->SetTemperature(4000);
       FNeonCityLocalLightPolicy Policy;Policy.Intensity=15000;Policy.DayMultiplier=1;Policy.ShadowDistance=2500;Policy.CullDistance=6000;Policy.VolumetricScattering=0;
       Budget->RegisterLight(L,Policy);ProbeAreaLights.Add(A);
      }
      Budget->RefreshLighting();
     }
     return;
    }
    if(ProbeRayGeometry)
    {
     IConsoleManager::Get().FindConsoleVariable(Index==0?TEXT("r.RayTracing.Nanite.Mode"):TEXT("r.RayTracing.Culling"))->Set(Index==0?1:0,ECVF_SetByConsole);
     return;
    }
    if(ProbeSkyTransport)
    {
     if(Index==0)IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"))->Set(0,ECVF_SetByConsole);
     if(Index==1)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.SkyVisibility"))->Set(1,ECVF_SetByConsole);
     if(Index==2)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ReferenceMode"))->Set(1,ECVF_SetByConsole);
     auto& P=Camera->GetCameraComponent()->PostProcessSettings;P.bOverride_LumenSkylightLeaking=Index==3;P.LumenSkylightLeaking=0;
     return;
    }
    if(ProbeSources)
    {
     RestoreFixtureBindings();IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure"))->Set(CacheExposureBefore,ECVF_SetByConsole);
     if(Index==0)for(const auto& Binding:ProbeBindings)if(auto* Part=Binding.Part.Get())Part->SetMaterial(Binding.Slot,Binding.Material->GetName().EndsWith(TEXT("Ceiling"))?FlatCeiling.Get():FlatWall.Get());
     if(Index==1)ProbeSky->SetIntensity(0);
     if(Index==2)IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure"))->Set(0.f,ECVF_SetByConsole);
     return;
    }
    if(ProbeRadiosity)
    {
     if(Index==0)IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity"))->Set(0,ECVF_SetByConsole);
     if(Index==1)IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity.ProbeSpacing"))->Set(1,ECVF_SetByConsole);
     if(Index==2)IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity.Temporal.MaxFramesAccumulated"))->Set(32,ECVF_SetByConsole);
     return;
    }
    if(ProbeOcclusion)
    {
     if(Index==0)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO"))->Set(0,ECVF_SetByConsole);
     if(Index==1)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing"))->Set(1,ECVF_SetByConsole);
     if(Index==2)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.DownsampleFactor"))->Set(1,ECVF_SetByConsole);
     return;
    }
    if(ProbeSpatial)
    {
     IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.GridResolution"))->Set(Index==1?128:96,ECVF_SetByConsole);
     IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget"))->Set(Index==2?1000:ProbePolicy[TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget")],ECVF_SetByConsole);return;
    }
    if(ProbeTransport)
    {
     RestoreFixtureBindings();
     if(Index==0){for(const auto& Binding:ProbeBindings)if(auto* Part=Binding.Part.Get())Part->SetMaterial(Binding.Slot,ProbeDarkFixture);}
     if(Index==1)IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution"))->Set(64,ECVF_SetByConsole);
     if(Index==2){IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"))->Set(0,ECVF_SetByConsole);IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.TraceMeshSDFs"))->Set(1,ECVF_SetByConsole);}
     return;
    }
    if(ProbeSurface){const int32 Modes[]={5,6,12};IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Visualize"))->Set(Modes[Index],ECVF_SetByConsole);return;}
    if(ProbeGather){auto& Settings=Camera->GetCameraComponent()->PostProcessSettings;Settings.bOverride_LumenFinalGatherQuality=true;Settings.LumenFinalGatherQuality=Index==1?8:4;IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes"))->Set(Index==2?1:ProbePolicy[TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes")],ECVF_SetByConsole);return;}
    if(ProbeHit){auto& Settings=Camera->GetCameraComponent()->PostProcessSettings;Settings.bOverride_LumenRayLightingMode=true;Settings.LumenRayLightingMode=ELumenRayLightingModeOverride::HitLighting;return;}
    IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.RadianceCache"))->Set(Index==0?0:ProbePolicy[TEXT("r.Lumen.ScreenProbeGather.RadianceCache")],ECVF_SetByConsole);
    IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"))->Set(Index==1?4:ProbePolicy[TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor")],ECVF_SetByConsole);
    auto& Settings=Camera->GetCameraComponent()->PostProcessSettings;
    Settings.bOverride_LumenSceneLightingQuality=Settings.bOverride_LumenSceneDetail=Index==2;
    Settings.LumenSceneLightingQuality=4;Settings.LumenSceneDetail=4;
    return;
   }
   ProbeViewport->ViewModeIndex=Index==0?VMI_Unlit:ProbeViewMode;
   if(Index==0)ApplyViewMode(VMI_Unlit,true,ProbeViewport->EngineShowFlags);
   IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"))->Set(Index==1?0:ProbePolicy[TEXT("r.Lumen.DiffuseIndirect.Allow")],ECVF_SetByConsole);
   IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"))->Set(0,ECVF_SetByConsole);
   IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"))->Set(Index==1?0:ProbePolicy[TEXT("r.DistanceFieldAO")],ECVF_SetByConsole);
   IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"))->Set(Index==2?0:ProbePolicy[TEXT("r.Lumen.ScreenProbeGather.ScreenTraces")],ECVF_SetByConsole);
  }
 }
 void RestoreFixtureBindings(){for(const auto& Binding:ProbeBindings)if(auto* Part=Binding.Part.Get())Part->SetMaterial(Binding.Slot,Binding.Material);}
 void RestoreProbe(){for(auto& A:ProbeAreaLights)if(A.IsValid()){if(auto* B=A->GetWorld()->GetSubsystem<UWorldLightingSubsystem>())B->UnregisterLight(CastChecked<URectLightComponent>(A->GetLightComponent()));A->Destroy();}ProbeAreaLights.Reset();RestoreFixtureBindings();ProbeBindings.Reset();if(ProbeSources){if(ProbeSky.IsValid())ProbeSky->SetIntensity(SkyIntensityBefore);ProbeSky.Reset();if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure")))C->Set(CacheExposureBefore,ECVF_SetByConsole);}if(ProbeViewport.IsValid()){ProbeViewport->EngineShowFlags=ProbeFlags;ProbeViewport->ViewModeIndex=ProbeViewMode;ProbeViewport.Reset();}for(const auto& P:ProbePolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*P.Key))C->Set(P.Value,ECVF_SetByConsole);ProbePolicy.Reset();}
 bool End(const FString& Failure)
 {
  if(Failure.IsEmpty() && GarageMotion && (Views.Num()!=3 || MotionRows.Num()!=24))
   return End(TEXT("Temporal comparison requires three complete eight-frame sequences"));
  if(!Failure.IsEmpty())Test->AddError(Failure);if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();RestoreProbe();if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  for(const auto& Entry:OldPolicy)if(auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Key))C->Set(Entry.Value,ECVF_SetByConsole);
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("UI");IFileManager::Get().MakeDirectory(*Dir,true);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("technical_camera_floor_pass"),Failure.IsEmpty() && !Failures && Index==Views.Num());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("registered_rooms"),33);J->SetNumberField(TEXT("selected_comparison_views"),Views.Num());J->SetNumberField(TEXT("images"),Rows.Num());J->SetNumberField(TEXT("player_pose_writes"),0);J->SetStringField(TEXT("world_mode"),W && W->WorldType==EWorldType::PIE?TEXT("PIE"):TEXT("NATIVE_STANDALONE_GAME"));J->SetStringField(TEXT("scope"),TEXT("Rendered native gameplay world,1600x900 Epic100: exact selected comparison views from33 registered rooms, original facilities and shared Day/Sunset/Night exterior views. Camera/floor probes only; physical traversal, subjective quality, BBQ/cruise and performance remain separate."));J->SetBoolField(TEXT("normal_temporal_motion907"),GarageMotion);J->SetArrayField(TEXT("camera_motion_frames"),MotionRows);J->SetArrayField(TEXT("views"),Rows);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("TheOne-AllRooms-Evidence.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;FTransform Pose;TWeakObjectPtr<AActor> OldView;TArray<FView> Views;TArray<int32> OriginalIndices;TArray<TSharedPtr<FJsonValue>> Rows;TMap<FString,float> OldPolicy;int32 Index=0,Failures=0;float At=0,OldHour=12;double Started=0;bool Ready=false,OldCycle=true,PolicyReady=false;
 struct FFixtureBinding{TWeakObjectPtr<UStaticMeshComponent> Part;int32 Slot;UMaterialInterface* Material;};
 TArray<FFixtureBinding> ProbeBindings;UMaterialInterface* ProbeDarkFixture=nullptr;
 TWeakObjectPtr<UGameViewportClient> ProbeViewport;FEngineShowFlags ProbeFlags{ESFIM_Game};TMap<FString,int32> ProbePolicy;int32 ProbeViewMode=VMI_Lit;bool ProbeGI=false,ProbeHit=false,ProbeSurface=false,ProbeGather=false,ProbeTransport=false,ProbeSpatial=false,ProbeOcclusion=false,ProbeRadiosity=false,ProbeSources=false,ProbeSkyTransport=false,ProbeRayGeometry=false,ProbePractical=false,ProbeFullPath=false,ProbeIntegration=false;
 bool GarageMotion=false;int32 MotionFrame=0;TArray<TSharedPtr<FJsonValue>> MotionRows;
 TArray<TWeakObjectPtr<ARectLight>> ProbeAreaLights;
 TWeakObjectPtr<USkyLightComponent> ProbeSky;float SkyIntensityBefore=0,CacheExposureBefore=0;TStrongObjectPtr<UMaterialInterface> FlatWall,FlatCeiling;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneAllRoomsEvidence,"NeonCity.Phase205cUpgrade.TheOneAllRoomsEvidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneAllRoomsEvidence::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOneAllRooms::FSurvey(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneAllRoomsGameEvidence,"NeonCity.Phase205c.Estate.Standalone.AllRoomsEvidence",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FTheOneAllRoomsGameEvidence::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(TheOneAllRooms::FSurvey(this));return true;}
#endif
