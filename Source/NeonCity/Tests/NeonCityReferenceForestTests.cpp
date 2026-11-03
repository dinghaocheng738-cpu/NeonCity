#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "AssetCompilingManager.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceForestData,"NeonCity.Marine.ReferenceVegetationData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceForestData::RunTest(const FString&)
{
 auto* Data=LoadObject<UNeonCityReferenceVegetationData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/DA_NCTW_Vegetation"));
 if(!TestNotNull(TEXT("Persisted canonical source scatter"),Data))return false;
 const FNeonCityReferencePlant Defaults;
 TestTrue(TEXT("New source records initialize finite transform and shader fields"),
  !Defaults.Transform.ContainsNaN() && FMath::IsFinite(Defaults.ShaderData0.R) && FMath::IsFinite(Defaults.ShaderData1.A));
 TestNotNull(TEXT("Exact source impostor quad retained"),Data->CanopyImpostor.LoadSynchronous());
 TestNotNull(TEXT("Original reduced far palm retained"),Data->PalmFar.LoadSynchronous());
 for(const TCHAR* Name:{TEXT("T_NCTW_ImpostorA"),TEXT("T_NCTW_ImpostorB")})
 {
  auto* Atlas=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/%s"),Name));
  if(TestNotNull(TEXT("Source canopy data atlas"),Atlas))
  {
   // Loading editor textures can return the 32px async-compilation placeholder.
   // Inspect the completed native resource, retaining exact dimensions/mip checks.
   FAssetCompilingManager::Get().FinishAllCompilation();
   TestEqual(TEXT("Thirty 128-pixel view columns"),Atlas->GetSizeX(),3840);
   TestEqual(TEXT("Six 128-pixel view rows"),Atlas->GetSizeY(),768);
   TestTrue(TEXT("Far data supports minified sampling"),Atlas->GetNumMips()>1);
   TestFalse(TEXT("Data atlas cannot acquire an sRGB colour transform"),Atlas->SRGB);
  }
 }
 TestEqual(TEXT("Fixed source SHA"),Data->ReferenceCommit,FString(TEXT("4811ba48d795197de5621985f404e765c0b7c0ef")));
 const TMap<FName,int32> Expected={{TEXT("trees"),12745},{TEXT("shrubs"),5598},{TEXT("palms"),233},{TEXT("bananas"),188},
  {TEXT("youngPalms"),268},{TEXT("ferns"),44548},{TEXT("monsteras"),1693},{TEXT("elephantEars"),24},{TEXT("heliconias"),70},{TEXT("strelitzias"),16}};
 TMap<FName,int32> Counts;int32 Total=0;bool Upright=true,Finite=true,Aligned=true,Scale=true,CanopyContract=true;
 for(const auto& Group:Data->Groups)
 {
  TestNotNull(TEXT("Native source mesh retained"),Group.Mesh.LoadSynchronous());
  if(Group.SourceFamily==TEXT("palms"))TestTrue(TEXT("Detailed palm geometry has a bounded native range"),Group.CullEndCm<=16000);
  Counts.FindOrAdd(Group.SourceFamily)+=Group.Instances.Num();Total+=Group.Instances.Num();
  for(const auto& P:Group.Instances)
  {
   Finite&=!P.Transform.ContainsNaN();Upright&=FVector::DotProduct(P.Transform.GetRotation().GetUpVector(),FVector::UpVector)>.99999;
   const auto B=P.ShaderData1;
   Aligned&=P.Transform.GetLocation().Equals(FVector(B.G,-B.A,B.B)*100,.05);
   Scale&=P.Transform.GetScale3D().GetMin()>0 && FMath::IsNearlyEqual(P.Transform.GetScale3D().X,double(P.ShaderData0.B),.0001);
   if(Group.SourceFamily==TEXT("trees") || Group.SourceFamily==TEXT("shrubs"))
   {
    const bool Shrub=Group.SourceFamily==TEXT("shrubs");
    CanopyContract&=FMath::IsNearlyEqual(FMath::Abs(double(B.R)),P.Transform.GetScale3D().Z/P.Transform.GetScale3D().X,.0001)
     && (B.R<0)==Shrub && FMath::Abs(FMath::UnwindRadians(FMath::DegreesToRadians(P.Transform.Rotator().Yaw)-double(P.ShaderData0.A)))<.0001;
    CanopyContract&=Group.CullEndCm<=(Shrub?7000:10000);
   }
  }
 }
 for(const auto& E:Expected)TestEqual(*FString::Printf(TEXT("Exact source count %s"),*E.Key.ToString()),Counts.FindRef(E.Key),E.Value);
 TestEqual(TEXT("All canonical plants retained"),Total,65383);
 TestTrue(TEXT("Source yaw never becomes native pitch/roll"),Upright);
 TestTrue(TEXT("Source metres and UE centimetres align for all plants"),Aligned);
 TestTrue(TEXT("All source transforms are finite and scaled consistently"),Finite && Scale);
 TestTrue(TEXT("Canopy yaw/stretch and bounded near geometry agree with source impostors"),CanopyContract);
 return true;
}

namespace
{
/** Matched normal-game camera fixtures, not the continuous forest acceptance route. */
class FReferenceForestViews : public IAutomationLatentCommand
{
public:
 explicit FReferenceForestViews(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Start>120){Test->AddError(TEXT("Forest comparison fixtures timed out"));return true;}
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || World->GetTimeSeconds()<.3f)return false;
  if(!Ready)
  {
   auto* Clock=World->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(11);
   World->SpawnActor<ASkyAtmosphere>();
   Evidence.Record(World,PC,0);Ready=true;
   int32 Plants=0,Impostors=0,CanonicalActors=0;for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)
   {
    // Authored density layers reuse this actor class, but cannot replace or
    // weaken the exact canonical source audit below.
    if(!It->ActorHasTag(TEXT("NeonCity.ReferenceVegetation")))continue;
    ++CanonicalActors;
    Plants+=It->GetPlantCount();Impostors+=It->GetImpostorCount();
    Test->TestEqual(TEXT("All source palms retain their far geometry"),It->GetFarPalmCount(),233);
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Groups(*It);
    int32 NearShadows=0,FarRepresentations=0;
    for(auto* Group:Groups)if(UStaticMesh* Mesh=Group->GetStaticMesh())
    {
     if(Mesh->GetName()==TEXT("SM_NCTW_CanopyImpostor"))
     {
      ++FarRepresentations;Test->TestFalse(TEXT("Canonical distant cards do not cast dynamic shadows"),bool(Group->CastShadow));
      Test->TestTrue(TEXT("View-facing far cards are excluded from physical light occlusion"),
       !Group->bVisibleInRayTracing && !Group->bAffectDynamicIndirectLighting && !Group->bAffectDistanceFieldLighting);
     }
     else if(Mesh->GetName()==TEXT("SM_NCTW_PalmFar"))
     {
      Test->TestFalse(TEXT("Source far palm does not cast dynamic shadows"),bool(Group->CastShadow));
      Test->TestFalse(TEXT("Far palm cannot duplicate near GI/ray geometry"),Group->bVisibleInRayTracing || Group->bAffectDynamicIndirectLighting || Group->bAffectDistanceFieldLighting);
     }
     else if(Group->CastShadow)++NearShadows;
    }
    Test->TestEqual(TEXT("Five source far variants retained"),FarRepresentations,5);
    Test->TestEqual(TEXT("Eleven near groups retain their original shadows"),NearShadows,11);
   }
   Test->TestEqual(TEXT("Exactly one original source scatter remains"),CanonicalActors,1);
   Test->TestEqual(TEXT("Source plants are counted once across representations"),Plants,65383);
   Test->TestEqual(TEXT("All tree/shrub variants have a far representation"),Impostors,18343);
   Test->AddInfo(FString::Printf(TEXT("Actual saved-map vegetation instances: %d; never hidden for this comparison"),Plants));
  }
  const FVector2D Points[]={FVector2D(5500,-3600),FVector2D(0,40000),FVector2D(14000,20000)};
  const TCHAR* Names[]={TEXT("IslandFar"),TEXT("IslandHill"),TEXT("Forest")};
  if(!Placed)
  {
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestViewFixture),false,Player);
   const FVector XY(Points[Stage].X,Points[Stage].Y,0);
   if(!World->LineTraceSingleByChannel(Hit,XY+FVector(0,0,45000),XY-FVector(0,0,10000),ECC_Visibility,Params))
   {Test->AddError(TEXT("Missing real island floor beneath comparison fixture"));return true;}
   auto* Move=Player->GetCharacterMovement();Move->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Hit.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();Player->SetActorRotation(FRotator(0,90,0));PC->SetControlRotation(FRotator(-8,90,0));
   SettledAt=World->GetTimeSeconds()+4;Placed=true;return false;
  }
  if(World->GetTimeSeconds()<SettledAt)return false;
  if(!Test->TestTrue(TEXT("Real character supported by authored ground"),Player->GetCharacterMovement()->IsMovingOnGround()))return true;
  FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
  Test->AddInfo(FString::Printf(TEXT("%s: actual player camera %s rotation %s, clock 11:00"),Names[Stage],*Eye.ToString(),*Rotation.ToString()));
  Test->TestTrue(TEXT("Normal game viewport capture"),FNeonCityVisualEvidence::CaptureHUD(World,FString(Names[Stage])+TEXT(".png")));
  ++Stage;Placed=false;return Stage==3;
 }
private:
 FAutomationTestBase* Test;double Start=0;float SettledAt=0;int32 Stage=0;bool Ready=false,Placed=false;
 FNeonCityVisualEvidence Evidence{TEXT("ForestComparisonFixtures")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceForestViewsTest,"NeonCity.Marine.ForestComparisonViewsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceForestViewsTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceForestViews>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

namespace
{
/** Continuous normal movement across canopy handoff distances; no camera actor. */
class FReferenceForestWalk : public IAutomationLatentCommand
{
public:
 explicit FReferenceForestWalk(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Forest walk timed out"));return true;}
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || World->GetTimeSeconds()<.2f)return false;
  auto* Move=Player->GetCharacterMovement();
  if(!Ready)
  {
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestWalkFixture),false,Player);
   if(!World->LineTraceSingleByChannel(Hit,FVector(14000,20000,45000),FVector(14000,20000,-10000),ECC_Visibility,Params))
   {Test->AddError(TEXT("Missing forest ground"));return true;}
   // Initial fixture only. Every subsequent location comes from template movement.
   Move->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Hit.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();PC->SetControlRotation(FRotator(-8,0,0));
   auto* Clock=World->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(11);
   World->SpawnActor<ASkyAtmosphere>();Ready=true;WaitUntil=World->GetTimeSeconds()+3;LastProgress=WaitUntil;return false;
  }
  Evidence.Record(World,PC,Point);
  if(World->GetTimeSeconds()<WaitUntil)return false;
  if(!CapturedStart)
  {
   Test->TestTrue(TEXT("Initial forest fixture settles on native ground"),Move->IsMovingOnGround());
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("ForestWalk-Start.png"));CapturedStart=true;
  }
  const FVector2D Targets[]={FVector2D(17000,20000),FVector2D(20000,20000),FVector2D(20000,22000)};
  const FVector2D To=Targets[Point]-FVector2D(Player->GetActorLocation());
  if(World->GetTimeSeconds()-LastProgress>25)
  {Test->AddError(FString::Printf(TEXT("Forest route blocked at %d: %s"),Point,*Player->GetActorLocation().ToString()));return true;}
  if(To.Size()>28)
  {
   PC->SetControlRotation(FRotator(-8,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X)),0));
   Player->DoMove(0,FMath::Clamp(To.Size()/130.,.2,1.));return false;
  }
  Test->TestTrue(TEXT("Canopy traversal retains native walking support"),Move->IsMovingOnGround());
  Test->TestEqual(TEXT("Forest traversal preserves player health"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
  FNeonCityVisualEvidence::CaptureHUD(World,FString::Printf(TEXT("ForestWalk-%d.png"),Point+1));
  Test->AddInfo(FString::Printf(TEXT("Continuous forest waypoint %d: %s"),Point,*Player->GetActorLocation().ToString()));
  ++Point;LastProgress=World->GetTimeSeconds();return Point==3;
 }
private:
 FAutomationTestBase* Test;double Started=0;float WaitUntil=0,LastProgress=0;int32 Point=0;bool Ready=false,CapturedStart=false;
 FNeonCityVisualEvidence Evidence{TEXT("ForestHandoffWalk")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceForestWalkTest,"NeonCity.Marine.ForestHandoffWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceForestWalkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceForestWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
