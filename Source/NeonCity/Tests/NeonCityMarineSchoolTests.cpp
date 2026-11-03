#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "AssetCompilingManager.h"
#include "Fishing/FishData.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/OceanHabitat.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/WaterInteractionComponent.h"
#include "Adventure/DivingComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineSpecimenAssets,"NeonCity.OceanExperience.SpecimenAssets",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineSpecimenAssets::RunTest(const FString&)
{
 struct FSpecimen {const TCHAR* Name;float Length;bool Benthic;};
 FString Audit;
 for(const FSpecimen S: {FSpecimen{TEXT("Sardine"),18,false},{TEXT("RedNapedWrasse"),25,false},
  {TEXT("Flyingfish"),32,false},{TEXT("Lionfish"),34,false},{TEXT("Parrotfish"),48,false},
  {TEXT("Seabream"),43,false},{TEXT("YellowfinTuna"),155,false},{TEXT("SpinyLobster"),45,true},{TEXT("SpearLobster"),64,true},{TEXT("BlueMannaCrab"),35,true}})
 {
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Specimens/%s/SM_Marine%s"),S.Name,S.Name));
  auto* Data=LoadObject<UFishData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Fishing/Data/DA_Fish_%s"),S.Name));
  if(!TestNotNull(S.Name,Mesh) || !TestNotNull(TEXT("Species profile"),Data))continue;
  FAssetCompilingManager::Get().FinishAllCompilation();
  TestTrue(TEXT("Fish profile passes persistence limits"),Data->IsValidProfile());
  TestEqual(TEXT("Five real reduced detail levels"),Mesh->GetNumLODs(),5);
  TestNotNull(TEXT("Capture collision cooked from imported shape"),Mesh->GetBodySetup());
  const auto& LOD=Mesh->GetRenderData()->LODResources[0];const auto& V=LOD.VertexBuffers;
  TestEqual(TEXT("Motion and joint UV channels survive import"),int32(V.StaticMeshVertexBuffer.GetNumTexCoords()),S.Benthic?7:2);
  TestTrue(TEXT("Distant mesh meaningfully reduced"),Mesh->GetRenderData()->LODResources[4].GetNumVertices()<LOD.GetNumVertices()/8);
  double EX=0,EY=0,EFlipY=0,ESwapX=0,ESwapY=0;
  for(uint32 I=0;I<V.PositionVertexBuffer.GetNumVertices();++I)
  {
   const FVector3f P=V.PositionVertexBuffer.VertexPosition(I);const FVector2f UV=V.StaticMeshVertexBuffer.GetVertexUV(I,1);
   const float X=(.5f-UV.X)*S.Length,Y=(1-UV.Y)*S.Length;
   EX+=FMath::Abs(P.X-X);EY+=FMath::Abs(P.Y-Y);EFlipY+=FMath::Abs(P.Y+Y);
   ESwapX+=FMath::Abs(P.Y-X);ESwapY+=FMath::Abs(P.X-Y);
  }
  const double Count=V.PositionVertexBuffer.GetNumVertices();
  const FString Row=FString::Printf(TEXT("%s verts=%u axis_error_cm X=%.6f Y=%.6f flipY=%.6f swapX=%.6f swapY=%.6f\n"),
   S.Name,V.PositionVertexBuffer.GetNumVertices(),EX/Count,EY/Count,EFlipY/Count,ESwapX/Count,ESwapY/Count);
  Audit+=Row;AddInfo(Row);
  TestTrue(TEXT("Body coordinate retains forward direction"),EX/Count<.06);
  TestTrue(TEXT("Blender lateral coordinate is negated in UE joint space"),EFlipY/Count<.06);
 }
 FFileHelper::SaveStringToFile(Audit,*(FPaths::ProjectSavedDir()/TEXT("OceanOverhaul/NativeSpecimenGeometry.txt")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineSchoolBounds,"NeonCity.OceanExperience.SchoolBounds",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineSchoolBounds::RunTest(const FString&)
{
 UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();auto* School=W->SpawnActor<AMarineSchool>();
 School->SetActorLocation(FVector(12000,-7000,-400));School->FloorResolution=2;School->FloorExtent=100;
 School->FloorHeights={-100,-80,-60,-40};
 TestEqual(TEXT("Terrain interpolated at school centre"),School->FloorAt(FVector::ZeroVector),-70.f);
 TestEqual(TEXT("Grid origin preserves depth"),School->FloorAt(FVector(-100,-100,0)),-100.f);
 TestTrue(TEXT("Out-of-grid sampling bounded to nearest edge"),FMath::IsNearlyEqual(School->FloorAt(FVector(10000,10000,0)),-40.f,.1f));
 School->CreatureMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Specimens/Sardine/SM_MarineSardine"));
 School->Starts={FTransform(FVector(20,0,0)),FTransform(FVector(60,0,-10)),FTransform(FVector(-50,0,10))};School->RebuildSchool();
 TestEqual(TEXT("One component contains every authored individual"),School->Bodies->GetInstanceCount(),3);
 TestEqual(TEXT("Simulation owns each individual"),School->LivingCount(),3);
 float Distance=10000;FVector Nearest;
 TestTrue(TEXT("Hydrophone queries individual world locations"),School->NearestCreature(School->GetActorLocation(),Distance,Nearest));
 TestTrue(TEXT("Nearest creature is the actual first instance"),Nearest.Equals(School->GetActorLocation()+FVector(20,0,0),.01f));
 TestFalse(TEXT("Invalid instance cannot be captured"),School->TryCapture(20,nullptr));
 TestEqual(TEXT("Rejected capture cannot remove or renumber instances"),School->Bodies->GetInstanceCount(),3);
 School->FloorHeights.Reset();TestTrue(TEXT("Invalid heightfield is exposed instead of creating fake floor"),School->FloorAt(FVector::ZeroVector)<-90000);
 School->Destroy();return !HasAnyErrors();
}
namespace
{
class FMarineEcologyPlayCheck : public IAutomationLatentCommand
{
public:
 explicit FMarineEcologyPlayCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>65){Test->AddError(TEXT("Ecology PIE did not become ready"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(!At)
  {
   FWaterSurfaceSample Water;
   Test->TestTrue(TEXT("Ordinary runtime water covers the authored reef"),W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(NeonCityCoastalLayout::FromLegacyIsland(FVector(55100,-175000,-400)),Water));
   Test->TestTrue(TEXT("Reef water uses the ocean datum"),FMath::Abs(Water.SurfacePoint.Z+130)<130);
   P->SetActorLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(54750,-174400,-450)),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation(FRotator(0,-90,0));
   for(TActorIterator<AMarineSchool> It(W);It;++It)
   {
    Schools.Add(*It);TArray<FVector> Positions;
    for(int32 I=0;I<It->Bodies->GetInstanceCount();++I)Positions.Add(It->CreaturePosition(I));
    Initial.Add(*It,MoveTemp(Positions));
   }
   Test->TestEqual(TEXT("All distributed habitats load in the ordinary world"),Schools.Num(),68);
   At=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-At<12)return false;
  auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
  Test->TestNotNull(TEXT("Ordinary player has water interaction"),Water);
  if(Water)Test->AddInfo(FString::Printf(TEXT("Player state=%d mode=%d at %s"),int32(Water->GetWaterState()),int32(P->GetCharacterMovement()->MovementMode),*P->GetActorLocation().ToString()));
  int32 Total=0,Moving=0,Benthic=0;TSet<FName> Species;
  AMarineSchool* CatchSchool=nullptr;
  for(auto Weak:Schools)if(auto* School=Weak.Get())
  {
   Total+=School->LivingCount();Species.Add(School->Species->SpeciesID);
   const bool Near=FVector::Dist2D(School->GetActorLocation(),P->GetActorLocation())<2000;
   for(int32 I=0;I<School->Bodies->GetInstanceCount();++I)
   {
    FTransform T;School->Bodies->GetInstanceTransform(I,T,false);
    const FVector Local=T.GetLocation();
    Test->TestFalse(TEXT("Creature transforms remain finite"),Local.ContainsNaN());
    Test->TestTrue(TEXT("Creature remains within its recorded floor grid"),FMath::Abs(Local.X)<School->FloorExtent && FMath::Abs(Local.Y)<School->FloorExtent);
    const float Clearance=School->FloorClearance*(School->Behaviour==EMarineBehaviour::Benthic?T.GetScale3D().X:1.f);
    Test->TestTrue(FString::Printf(TEXT("%s[%d] stays above sampled bottom (Z %.2f, limit %.2f)"),*School->HabitatID.ToString(),I,Local.Z,School->FloorAt(Local)+Clearance),Local.Z+.3f>=School->FloorAt(Local)+Clearance);
    Test->TestTrue(TEXT("Creature stays beneath its habitat surface clearance"),School->CreaturePosition(I).Z<=School->MeanSeaLevel-School->SurfaceClearance+.3f);
    if(School->Behaviour==EMarineBehaviour::Benthic)
    {
     ++Benthic;Test->TestNearlyEqual(TEXT("Lobster follows the seafloor"),float(Local.Z),School->FloorAt(Local)+Clearance,.3f);
    }
    if(Near && FVector::Dist(Initial.FindChecked(School)[I],School->CreaturePosition(I))>15)++Moving;
   }
   if(Near && School->bCapturable && School->LivingCount()>2)CatchSchool=School;
  }
  Test->TestEqual(TEXT("Every new species is present"),Species.Num(),9);
  Test->TestEqual(TEXT("No individuals lost during simulation"),Total,527);
  Test->TestEqual(TEXT("Both lobster populations remain present"),Benthic,24);
  Test->TestTrue(TEXT("Nearby organisms actually travel across the reef"),Moving>=30);
  if(Test->TestNotNull(TEXT("Near-reef catch population"),CatchSchool))
  {
   auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
   const int32 Count=CatchSchool->LivingCount(),Instances=CatchSchool->Bodies->GetInstanceCount(),Owned=Inventory->GetFish().Num();
   const FVector Survivor=CatchSchool->CreaturePosition(1);
   Test->TestTrue(TEXT("A real individual enters the real inventory"),CatchSchool->TryCapture(0,Inventory));
   Test->TestEqual(TEXT("Catch removes exactly one live individual"),CatchSchool->LivingCount(),Count-1);
   Test->TestEqual(TEXT("Capture keeps instance IDs stable"),CatchSchool->Bodies->GetInstanceCount(),Instances);
   Test->TestTrue(TEXT("Surviving individual is not renumbered"),CatchSchool->CreaturePosition(1).Equals(Survivor));
   Test->TestEqual(TEXT("Inventory gains exactly one specimen"),Inventory->GetFish().Num(),Owned+1);
   Test->TestFalse(TEXT("A captured instance cannot reward twice"),CatchSchool->TryCapture(0,Inventory));
   if(Inventory->GetFish().Num()>Owned)
   {
    const auto& Fish=Inventory->GetFish().Last();
    Test->TestEqual(TEXT("Catch preserves the actual habitat"),Fish.FishingPointID,CatchSchool->HabitatID);
    Test->TestEqual(TEXT("Catch preserves the actual species"),Fish.SpeciesID,CatchSchool->Species->SpeciesID);
   }
  }
  FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Ecology-NearReef.png"));
  Test->AddInfo(TEXT("Native integration and diagnostic view only; does not replace manual ecological or visual acceptance."));
  return true;
 }
 FAutomationTestBase* Test;double Started=0;float At=0;
 TArray<TWeakObjectPtr<AMarineSchool>> Schools;
 TMap<AMarineSchool*,TArray<FVector>> Initial;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarineEcologyPIE,"NeonCity.OceanExperience.EcologyPIE",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarineEcologyPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(FMarineEcologyPlayCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
namespace
{
class FBenthicNativeReview : public IAutomationLatentCommand
{
public:
 explicit FBenthicNativeReview(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Benthic close review timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  const TCHAR* Names[]={TEXT("BlueMannaCrab"),TEXT("SandSeaStar"),TEXT("SpinyLobster"),TEXT("SpearLobster")};
  const float Heights[]={6.65f,1.45f,5.625f,4.352f};
  if(Stage>=UE_ARRAY_COUNT(Names)){Test->AddInfo(TEXT("Asset inspection fixture on real sea floor; not a distribution/playthrough acceptance."));return true;}
  if(!School)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);const FVector XY=NeonCityCoastalLayout::FromLegacyIsland(FVector(54800,-178000,0));
   if(!Region || !Region->EnsureDestinationReady(XY))return false;
   auto* Dive=P->FindComponentByClass<UDivingComponent>();if(!Dive || (!Dive->IsEquipped() && !Dive->Equip()))return true;
   Dive->SetFirstPerson(true);P->GetCharacterMovement()->DisableMovement();
   FCollisionQueryParams Q(SCENE_QUERY_STAT(BenthicCloseReview),false,P);FHitResult Hit;
   if(!W->LineTraceSingleByChannel(Hit,XY+FVector(0,0,-300),XY-FVector(0,0,10000),ECC_WorldStatic,Q))return true;
   Centre=Hit.ImpactPoint;
   auto* Data=LoadObject<UFishData>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Fishing/Data/DA_Fish_%s"),Names[Stage]));if(!Test->TestNotNull(TEXT("Native benthic species exists"),Data))return true;
   const FTransform At(FRotator::ZeroRotator,Centre);
   School=W->SpawnActorDeferred<AMarineSchool>(AMarineSchool::StaticClass(),At,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   School->Species=Data;School->CreatureMesh=Data->Mesh.LoadSynchronous();School->Behaviour=EMarineBehaviour::Benthic;School->bSideways=Stage==0;
   School->CruiseSpeed=Stage==1?.15f:4.f;School->SwimFrequency=.8f;School->SwimAmplitude=0;School->FloorClearance=Heights[Stage];School->HomeRadius=160;
   School->FloorResolution=5;School->FloorExtent=250;School->FloorHeights.Reset();
   for(int32 Y=0;Y<5;++Y)for(int32 X=0;X<5;++X)
   {
    const FVector Pos=Centre+FVector((X-2)*125,(Y-2)*125,0);FHitResult Floor;
    const bool Found=W->LineTraceSingleByChannel(Floor,Pos+FVector(0,0,400),Pos-FVector(0,0,400),ECC_WorldStatic,Q);
    if(!Test->TestTrue(TEXT("Fixture follows real surrounding terrain"),Found))return true;
    School->FloorHeights.Add(Floor.ImpactPoint.Z-Centre.Z);
   }
   School->Starts={FTransform(FRotator(0,130,0),FVector(0,0,Heights[Stage]))};School->FinishSpawning(At);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   P->SetActorLocation(Centre+FVector(120,-95,35),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation((Centre+FVector(0,0,10)-(P->GetActorLocation()+FVector(5,0,64))).Rotation());
   AtTime=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-AtTime<6)return false;
  Test->TestTrue(TEXT("Native live benthic frame"),FNeonCityVisualEvidence::CaptureHUD(W,FString(TEXT("Benthic-"))+Names[Stage]+TEXT(".png")));
  const FVector Local=School->GetActorTransform().InverseTransformPosition(School->CreaturePosition(0));
  Test->TestNearlyEqual(TEXT("Benthic stays grounded while moving"),float(Local.Z),School->FloorAt(Local)+Heights[Stage],.3f);
  School->Destroy();School=nullptr;++Stage;return false;
 }
private:
 FAutomationTestBase* Test;AMarineSchool* School=nullptr;FVector Centre;double Started=0;float AtTime=0;int32 Stage=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBenthicReview,"NeonCity.OceanExperience.BenthicReviewPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBenthicReview::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FBenthicNativeReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
namespace {
class FGlobalSeabedReview : public IAutomationLatentCommand
{
public:
 explicit FGlobalSeabedReview(FAutomationTestBase* T):Test(T),Evidence(TEXT("GlobalSeabed")){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>260){Test->AddError(TEXT("Global ecology survey timed out"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);auto* Dive=P->FindComponentByClass<UDivingComponent>();
  auto* Ecology=W->GetSubsystem<UMarineEcologySubsystem>();
  Evidence.Record(W,PC,Stage);
  if(Stage>=5){Test->AddInfo(TEXT("Five ordinary-water locations; runtime-generated content only, no test-created animals/reefs. Manual traversal remains required."));return true;}
  const FVector2D Points[]={NeonCityCoastalLayout::FromLegacyIsland(FVector2D(38000,-185000)),NeonCityCoastalLayout::FromLegacyIsland(FVector2D(95000,-191000)),NeonCityCoastalLayout::FromLegacyIsland(FVector2D(115000,-192000)),NeonCityCoastalLayout::FromLegacyIsland(FVector2D(125000,-192000)),NeonCityCoastalLayout::FromLegacyIsland(FVector2D(142550,-211950))};
  const FVector XY(Points[Stage].X,Points[Stage].Y,0);
  if(!Positioned)
  {
   if(!Region || !Region->EnsureDestinationReady(XY))return false;
   if(!Dive->IsEquipped() && !Test->TestTrue(TEXT("Ordinary suit equips for survey"),Dive->Equip()))return true;
   Dive->SetFirstPerson(true);P->GetCharacterMovement()->DisableMovement();
   FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(GlobalEcologyFloor),false,P);
   if(!Test->TestTrue(TEXT("Survey location has a real seafloor"),W->LineTraceSingleByChannel(Hit,XY+FVector(0,0,-250),XY-FVector(0,0,20000),ECC_WorldStatic,Query)))return true;
   P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorRotation(FRotator::ZeroRotator);PC->SetControlRotation(FRotator(-10,0,0));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   if(auto* Torch=P->FindComponentByClass<UNeonCityFlashlightComponent>())Test->TestTrue(TEXT("Real suit lamp switches with depth"),Torch->SetEquipped(Stage>=2));
   Positioned=true;Aimed=false;At=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-At<18)return false;
  if(!Aimed)
  {
   int32 Creatures=0,Schools=0,Reefs=0,Instances=0;float Nearest=FLT_MAX;FVector NearestAt;TSet<FName> Species;
   const FVector Eye=P->GetFollowCamera()->GetComponentLocation();
   for(TActorIterator<AMarineSchool> It(W);It;++It)
   {
    if(FVector::Dist2D(Eye,It->GetActorLocation())>10500)continue;
    ++Schools;Creatures+=It->LivingCount();Species.Add(It->Species->SpeciesID);
    for(int32 I=0;I<It->Starts.Num();++I)if(It->IsAlive(I))
    {
     const FVector Pos=It->CreaturePosition(I);const float D=FVector::Dist(Eye,Pos);
     if(D<Nearest){Nearest=D;NearestAt=Pos;}
    }
   }
   for(TActorIterator<AOceanHabitat> It(W);It;++It)if(It->bStreamed && FVector::Dist2D(Eye,It->GetActorLocation())<10500)
   {++Reefs;for(const auto& Layer:It->HabitatLayers)Instances+=Layer.Instances.Num();}
   Test->AddInfo(FString::Printf(TEXT("region %d XY=%s: schools=%d species=%d animals=%d nearest=%.1fcm habitats=%d instances=%d"),Stage,*XY.ToString(),Schools,Species.Num(),Creatures,Nearest,Reefs,Instances));
   Test->TestTrue(TEXT("Ordinary ocean creates multiple regional species"),Species.Num()>=3);
   Test->TestTrue(TEXT("An animal can be encountered without a minute of empty travel"),Nearest<2500);
   Test->TestTrue(TEXT("Whole-water terrain includes real local habitat instances"),Reefs>0 && Instances>3);
   Test->TestTrue(TEXT("Nearby animal budget is respected"),Creatures<=260);
   Test->TestTrue(TEXT("Streaming school budget is respected"),Ecology->GetStreamedSchoolCount()<=24);
   if(Nearest<FLT_MAX)PC->SetControlRotation((NearestAt-Eye).Rotation());
   Aimed=true;At=W->GetTimeSeconds()-15;return false;
  }
  FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("GlobalSeabed-%d.png"),Stage));
  ++Stage;Positioned=false;return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;double Started=0;float At=0;int32 Stage=0;bool Positioned=false,Aimed=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlobalSeabedPIE,"NeonCity.OceanExperience.GlobalSeabedPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGlobalSeabedPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FGlobalSeabedReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
