#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceGrassDataTest,"NeonCity.Marine.ReferenceGrassData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceGrassDataTest::RunTest(const FString&)
{
 auto* Data=LoadObject<UNeonCityReferenceGrassData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Grass/DA_NCTW_Grass"));
 if(!TestNotNull(TEXT("Native canonical grass library"),Data))return false;
 TestEqual(TEXT("Fixed source revision"),Data->ReferenceCommit,FString(TEXT("4811ba48d795197de5621985f404e765c0b7c0ef")));
 TestEqual(TEXT("Source three-tier patches"),Data->PatchMeshes.Num(),3);
 TestEqual(TEXT("Source nonempty mask cells"),Data->Cells.Num(),2724);
 const int32 Triangles[]={17560,4716,768};
 for(int32 I=0;I<Data->PatchMeshes.Num() && I<3;++I)
 {
  auto* Mesh=Data->PatchMeshes[I].LoadSynchronous();if(!TestNotNull(TEXT("Native source patch"),Mesh))continue;
  FAssetCompilingManager::Get().FinishAllCompilation();
  TestEqual(TEXT("Source blade topology survives nondegenerate import"),Mesh->GetNumTriangles(0),Triangles[I]);
  TestEqual(TEXT("All original vertex data channels retained"),Mesh->GetNumUVChannels(0),8);
 }
 TSet<int32> Seen;bool Valid=true;
 for(const auto& C:Data->Cells)
 {
  Valid&=C.Index>=0 && C.Index<256*256 && !Seen.Contains(C.Index)
   && FMath::IsFinite(C.MinimumHeightM) && FMath::IsFinite(C.MaximumHeightM) && C.MinimumHeightM<=C.MaximumHeightM;
  Seen.Add(C.Index);
 }
 TestTrue(TEXT("Unique valid occupancy and conservative source terrain bounds"),Valid);
 auto* Heights=LoadObject<UTexture2D>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Grass/T_NCTW_GrassHeights"));
 if(TestNotNull(TEXT("Exact float height data texture"),Heights))
 {
  FAssetCompilingManager::Get().FinishAllCompilation();
  TestEqual(TEXT("Native height width"),Heights->GetSizeX(),2041);TestEqual(TEXT("Native height rows"),Heights->GetSizeY(),2041);
  TestEqual(TEXT("Packed float bytes cannot be mip-averaged"),Heights->GetNumMips(),1);
  TestFalse(TEXT("No colour conversion of height bits"),Heights->SRGB);
 }
 return true;
}

namespace
{
class FReferenceGrassWalk : public IAutomationLatentCommand
{
public:
 explicit FReferenceGrassWalk(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Ground-flora route timed out"));return true;}
  auto* World=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || World->GetTimeSeconds()<.2f)return false;
  auto* Movement=Player->GetCharacterMovement();
  if(!Ready)
  {
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(GrassWalkStart),false,Player);
   for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)Params.AddIgnoredActor(*It);
   if(!World->LineTraceSingleByChannel(Hit,FVector(16000,14400,40000),FVector(16000,14400,-10000),ECC_Visibility,Params))
   {Test->AddError(TEXT("Missing authored coast terrain"));return true;}
   Movement->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Hit.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Movement->StopMovementImmediately();PC->SetControlRotation(FRotator(-12,180,0));
   auto* Clock=World->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(11);
   World->SpawnActor<ASkyAtmosphere>();
   for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)
    if(It->ActorHasTag(TEXT("NeonCity.ReferenceVegetation")))Grass=It->FindComponentByClass<UNeonCityReferenceGrassComponent>();
   if(!Test->TestNotNull(TEXT("Existing vegetation actor owns the grass renderer"),Grass.Get()))return true;
   Ready=true;WaitUntil=World->GetTimeSeconds()+3;ProgressAt=WaitUntil;return false;
  }
  Evidence.Record(World,PC,Point);if(World->GetTimeSeconds()<WaitUntil)return false;
  if(!StartCaptured){Capture(World,Player,TEXT("Grass-Dune.png"));StartCaptured=true;}
  const FVector2D Targets[]={FVector2D(14000,11600),FVector2D(13000,20000)};
  const FVector2D To=Targets[Point]-FVector2D(Player->GetActorLocation());
  if(World->GetTimeSeconds()-ProgressAt>35){Test->AddError(TEXT("Ordinary coastal grass movement blocked"));return true;}
  if(To.Size()>28)
  {
   PC->SetControlRotation(FRotator(-12,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X)),0));
   Player->DoMove(0,FMath::Clamp(To.Size()/130.,.2,1.));return false;
  }
  Capture(World,Player,Point==0?TEXT("Grass-OatsVine.png"):TEXT("Grass-Meadow.png"));
  ++Point;ProgressAt=World->GetTimeSeconds();return Point==2;
 }
private:
 void Capture(UWorld* World,ANeonCityPlayerCharacter* Player,const TCHAR* Name)
 {
  Test->TestTrue(TEXT("Native third-person ground support retained"),Player->GetCharacterMovement()->IsMovingOnGround());
  Test->TestEqual(TEXT("Ground flora does not damage player"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
  int32 Total=0;const int32 Maximum[]={48,160,420};
  if(Grass.IsValid())for(int32 L=0;L<3;++L)
  {
   const int32 Count=Grass->GetActiveCellCount(L);Total+=Count;
   Test->TestTrue(TEXT("Source cell budget enforced"),Count>=0 && Count<=Maximum[L]);
   Test->AddInfo(FString::Printf(TEXT("%s grass level %d: %d cells"),Name,L,Count));
  }
  Test->TestTrue(TEXT("Actual nonempty coastal grass is rendered"),Total>0);
  FNeonCityVisualEvidence::CaptureHUD(World,Name);
 }
 FAutomationTestBase* Test;TWeakObjectPtr<UNeonCityReferenceGrassComponent> Grass;
 double Started=0;float WaitUntil=0,ProgressAt=0;int32 Point=0;bool Ready=false,StartCaptured=false;
 FNeonCityVisualEvidence Evidence{TEXT("GroundFloraWalk")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceGrassWalkTest,"NeonCity.Marine.GroundFloraWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceGrassWalkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceGrassWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
