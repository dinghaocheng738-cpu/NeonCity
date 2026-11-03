#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "EngineUtils.h"
#include "Editor.h"

namespace
{
class FReferenceVillageWalk : public IAutomationLatentCommand
{
public:
 explicit FReferenceVillageWalk(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150)
  { Test->AddError(FString::Printf(TEXT("Village walk timeout at waypoint %d"),Point));return true; }
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || World->GetTimeSeconds()<.2f)return false;
  auto* Movement=Player->GetCharacterMovement();
  if(Route.IsEmpty())
  {
   ANeonCityReferenceVillage* Village=nullptr;
   for(TActorIterator<ANeonCityReferenceVillage> It(World);It;++It)Village=*It;
   if(!Test->TestNotNull(TEXT("Saved canonical village loads in PIE"),Village))return true;
   Test->TestTrue(TEXT("Island-local source origin"),Village->GetActorLocation().IsNearlyZero());
   int32 Surfaces=0;
   TInlineComponentArray<UStaticMeshComponent*> Meshes(Village);
   for(auto* Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("VillageSurface_")))
   { ++Surfaces;Test->TestNotNull(TEXT("Saved source surface has a native asset"),Mesh->GetStaticMesh().Get()); }
   Test->TestEqual(TEXT("All canonical surface pieces"),Surfaces,31);
   // Review-only lighting. The island sublevel will share the mainland's sky.
   auto* Clock=World->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(11);
   World->SpawnActor<ASkyAtmosphere>();
   Route={FVector(5500,-2900,230),FVector(5500,-1500,230),FVector(5500,0,230),
    FVector(5500,2000,230),FVector(5500,4200,230),FVector(5500,6250,230)};
   FString Text;TSharedPtr<FJsonObject> Source;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Village/Source.json")))
    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Source))
   { Test->AddError(TEXT("Missing canonical route data"));return true; }
   bool InMainWalk=false;
   for(const auto& Value:Source->GetObjectField(TEXT("colliders"))->GetArrayField(TEXT("boxes")))
   {
    const auto Box=Value->AsObject();const bool Walk=Box->GetStringField(TEXT("tag"))==TEXT("boardwalk");
    if(InMainWalk && !Walk)break;
    if(!Walk)continue;
    InMainWalk=true;const auto C=Box->GetObjectField(TEXT("center"));const auto H=Box->GetObjectField(TEXT("half"));
    Route.Add(FVector(C->GetNumberField(TEXT("x"))*100,-C->GetNumberField(TEXT("z"))*100,
     (C->GetNumberField(TEXT("y"))+H->GetNumberField(TEXT("y")))*100));
   }
   if(!Test->TestTrue(TEXT("Source main boardwalk supplies a real continuous route"),Route.Num()>40))return true;
   // One initial fixture placement on the pier head; the route itself uses only
   // normal character movement. No actor relocation, floor override or collision bypass.
   Movement->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(FVector(5500,-3600,230+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Movement->StopMovementImmediately();LastProgress=World->GetTimeSeconds();return false;
  }
  Evidence.Record(World,PC,Point);
  if(World->GetTimeSeconds()-LastProgress>15)
  {
   Test->AddError(FString::Printf(TEXT("Village route blocked at %d, player %s, target %s, mode %d"),Point,
    *Player->GetActorLocation().ToString(),*Route[Point].ToString(),int32(Movement->MovementMode)));return true;
  }
  const FVector To=Route[Point]-Player->GetActorLocation();
  if(To.Size2D()>28)
  {
   PC->SetControlRotation(FRotator(-8,To.Rotation().Yaw,0));
   Player->DoMove(0,FMath::Clamp(To.Size2D()/130.,.2,1.));return false;
  }
  if(!Test->TestTrue(FString::Printf(TEXT("Waypoint %d has native walking support"),Point),Movement->IsMovingOnGround()))return true;
  const float Feet=Player->GetActorLocation().Z-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  if(!Test->TestTrue(TEXT("Feet remain on the authored pier/boardwalk profile"),FMath::Abs(Feet-Route[Point].Z)<48))return true;
  if(Point==0)FNeonCityVisualEvidence::CaptureHUD(World,TEXT("ReferenceVillage-Pier.png"));
  ++Point;LastProgress=World->GetTimeSeconds();
  if(Point<Route.Num())return false;
  Movement->StopMovementImmediately();
  Test->TestEqual(TEXT("Traversal preserves player health"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
  Test->AddInfo(FString::Printf(TEXT("Walked pier, shore steps and %d source route supports without repositioning"),Route.Num()));
  FNeonCityVisualEvidence::CaptureHUD(World,TEXT("ReferenceVillage-Market.png"));return true;
 }
private:
 FAutomationTestBase* Test;TArray<FVector> Route;int32 Point=0;double Started=0;float LastProgress=0;
 FNeonCityVisualEvidence Evidence{TEXT("ReferenceVillage")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceVillageWalkTest,"NeonCity.Marine.ReferenceVillageWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceVillageWalkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceVillageWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
