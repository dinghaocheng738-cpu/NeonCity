#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/WaterStateInterface.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace RidgeHouseChecks
{
const FVector Origin=NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480));
FVector M(double X,double Y,double Z) {return Origin+FVector(X*100,-Y*100,Z*100);}
class FLayout : public IAutomationLatentCommand
{
public:
 explicit FLayout(FAutomationTestBase* In,bool OnlyStairs=false):Test(In),bStairsOnly(OnlyStairs){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>260){Test->AddError(TEXT("Ridge-house focused fixture timed out"));return Finish();}
  if(!W)
  {
   W=GEditor?GEditor->PlayWorld.Get():nullptr;
   PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(!P || W->GetTimeSeconds()<4){W=nullptr;return false;}
   auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(Origin)){W=nullptr;return false;}
   if(!bStairsOnly)
   {
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
    if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.RidgeHouse")))++Meshes;
   if(!Test->TestEqual(TEXT("Ten original structural surface actors imported"),Meshes,10))return Finish();
   for(TActorIterator<ANeonCityWaterVolume> It(W);It;++It)if(It->bContainedPoolWater)Pool=*It;
   if(!Test->TestNotNull(TEXT("Contained pool reuses native swim volume"),Pool))return Finish();
   FWaterSurfaceSample Water;
   Test->TestTrue(TEXT("Actual pool water selected above the ocean"),W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(M(3,25,3.7),Water) && FMath::Abs(Water.SurfacePoint.Z-M(3,25,4.1).Z)<1 && Water.InsideVolume);
   Test->TestFalse(TEXT("Pool water never leaks below its basin"),Pool->QueryWaterSurface_Implementation(M(3,25,2.9),Water));
   Test->TestTrue(TEXT("Pool has zero ocean current and no ocean displacement"),!Pool->OceanSurface && Pool->CurrentVelocity.IsNearlyZero());
   int32 BadPad=0;FCollisionQueryParams Params(SCENE_QUERY_STAT(RidgeNativeFloors),true,P);
   for(double X=-10;X<=6;X+=1)for(double Y=-10;Y<=6;Y+=1)
   {
    FHitResult Hit;const FVector Point=M(X,Y,16.86);
    if(!W->LineTraceSingleByChannel(Hit,Point+FVector(0,0,40),Point-FVector(0,0,40),ECC_Visibility,Params) || FMath::Abs(Hit.ImpactPoint.Z-Point.Z)>2)++BadPad;
   }
   Test->TestEqual(TEXT("Helipad has an uninterrupted physically collidable clear landing square"),BadPad,0);
   FHitResult Basin;Test->TestTrue(TEXT("Pool is physically open down to the basin floor"),W->LineTraceSingleByChannel(Basin,M(3,25,4.8),M(3,25,2.8),ECC_Visibility,Params) && FMath::Abs(Basin.ImpactPoint.Z-M(3,25,3.05).Z)<2);
   }
   // One declared fixture placement. All four floors are subsequently reached
   // through ordinary W input and character movement, without relocation.
   P->SetActorLocation(M(-17.45,2.15,.98),false,nullptr,ETeleportType::TeleportPhysics);
   Camera=W->SpawnActor<ACameraActor>();At=W->GetTimeSeconds();Stage=bStairsOnly?2:1;return false;
  }
  Evidence.Record(W,PC,Stage);
  if(Stage==1)
  {
   struct FView{const TCHAR* Name;FVector Eye,Aim;};
   const FView Views[]={
    {TEXT("ExteriorArrival"),M(-36,13,8),M(-5,3,9)},
    {TEXT("ExteriorSea"),M(20,43,14),M(-4,9,9)},
    {TEXT("PublicLiving"),M(-11,2,5.9),M(5,15,5.4)},
    {TEXT("RoofPad"),M(-17,-9,18.8),M(-2,-2,16.86)},
    {TEXT("PoolTerrace"),M(-12,23,5.8),M(5,26,4.0)},
    {TEXT("StairFlight"),M(-17.45,1.9,1.8),M(-17.45,-3,2)}};
   const double Hours[]={12,18.5,22};const TCHAR* Times[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
   if(View==0 && !ViewSet)
    for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(Hours[Time]);}
   if(!ViewSet)
   {
    Camera->SetActorLocation(Views[View].Eye);Camera->SetActorRotation((Views[View].Aim-Views[View].Eye).Rotation());
    PC->SetViewTarget(Camera);ViewSet=true;At=W->GetTimeSeconds();return false;
   }
   if(W->GetTimeSeconds()-At<2.5)return false;
   Test->TestTrue(TEXT("Native rendered comparison saved"),FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Ridge-%s-%s.png"),Times[Time],Views[View].Name)));
   ViewSet=false;if(++View==UE_ARRAY_COUNT(Views)){View=0;if(++Time==3){PC->SetViewTarget(P);Stage=2;At=W->GetTimeSeconds();}}
   return false;
  }
  if(Stage==2)
  {
   if(Goals.IsEmpty())for(int32 Floor=0;Floor<4;++Floor)
   {
    const double Z=4.2*Floor;
    Goals.Add(M(-17.45,-3.7,Z+2.10));Goals.Add(M(-14.65,-3.7,Z+2.10));
    Goals.Add(M(-14.65,.8,Z+4.20));Goals.Add(M(-11.4,1.75,Z+4.20));
    if(Floor<3)Goals.Add(M(-17.45,1.75,Z+4.20));
   }
   const FVector Delta=Goals[Goal]-P->GetActorLocation();
   if(Delta.Size2D()<40)
   {
    if(!Test->TestTrue(TEXT("Native stair goal reached at its physical floor height"),FMath::Abs(Delta.Z+P->GetSimpleCollisionHalfHeight())<35))return Finish();
    Key(EKeys::W,IE_Released);HeldW=false;++Goal;At=W->GetTimeSeconds();
    if(bStairsOnly && FMath::Abs(Goals[Goal-1].X-M(-11.4,0,0).X)<1)
     FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Ridge-StairLanding-%02d.png"),Goal));
    if(Goal==Goals.Num()){Stage=3;At=W->GetTimeSeconds();}
    return false;
   }
   if(W->GetTimeSeconds()-At>12)
   {
    Test->AddError(FString::Printf(TEXT("Normal W stair traversal blocked at waypoint %d; position %s; target %s"),Goal,*P->GetActorLocation().ToString(),*Goals[Goal].ToString()));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Ridge-StairBlocked.png"));return Finish();
   }
   PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));if(!HeldW){Key(EKeys::W,IE_Pressed);HeldW=true;}return false;
  }
  if(Stage==3)
  {
   Test->TestTrue(TEXT("All floors reached using inherited player input and collision"),P->GetActorLocation().Z>Origin.Z+1670);
   return Finish();
  }
  return false;
 }
private:
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool Finish()
 {
  Key(EKeys::W,IE_Released);
  TSharedPtr<FJsonObject> J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("Native map/lighting/collision comparisons and isolated normal-W stair traversal. Fixture placement is explicit; no whole-route, helicopter or pool-swimming PASS is claimed."));
  J->SetStringField(TEXT("user_subjective"),TEXT("USER REVIEW REQUIRED"));J->SetStringField(TEXT("visual"),TEXT("NATIVE CAPTURES REQUIRE REVIEW"));J->SetNumberField(TEXT("stair_goals_reached"),Goal);J->SetNumberField(TEXT("stair_goals_total"),Goals.Num());J->SetNumberField(TEXT("rendered_comparisons"),Time*6+View);
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);
  if(FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)))FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("RidgeLayout.json")));
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityWaterVolume* Pool=nullptr;ACameraActor* Camera=nullptr;
 double Started=0;float At=0;int32 Stage=0,Meshes=0,View=0,Time=0,Goal=0;bool ViewSet=false,bStairsOnly=false,HeldW=false;TArray<FVector> Goals;
 FNeonCityVisualEvidence Evidence{TEXT("RidgeHouseLayout")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase205cRidgeLayout,"NeonCity.Phase205c.RidgeLayoutPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPhase205cRidgeLayout::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeHouseChecks::FLayout(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase205cRidgeStairs,"NeonCity.Phase205c.RidgeStairsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPhase205cRidgeStairs::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeHouseChecks::FLayout(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
