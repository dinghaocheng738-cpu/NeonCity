#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/WaterInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace RidgePoolCheck
{
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+100*X,-118800-100*Y,30480+100*Z));}
class FSwim : public IAutomationLatentCommand
{
public:
 explicit FSwim(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("Contained pool entry/swim/exit timed out"));return Finish();}
  if(!W)
  {
   W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(!P || W->GetTimeSeconds()<4){W=nullptr;return false;}
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(M(0,0,0))){W=nullptr;return false;}
   for(TActorIterator<ANeonCityWaterVolume> It(W);It;++It)if(It->bContainedPoolWater)Pool=*It;
   if(!Test->TestNotNull(TEXT("Real finite native pool volume"),Pool))return Finish();
   FWaterSurfaceSample Water;auto* Registry=W->GetSubsystem<UMarineWorldSubsystem>();
   Test->TestTrue(TEXT("Pool's planar surface overrides the ocean only in its finite basin"),Registry->QueryWater(M(3,25,3.1),Water) && FMath::Abs(Water.SurfacePoint.Z-M(3,25,4.1).Z)<1 && Water.InsideVolume);
   Test->TestFalse(TEXT("Water cannot spill into the garage below the basin"),Pool->QueryWaterSurface_Implementation(M(3,25,2.3),Water));
   FCollisionQueryParams Params(SCENE_QUERY_STAT(PoolOpenBasin),true,P);int32 Blocked=0;
   for(double X=-1;X<=8;X+=1)for(double Y=23;Y<=27;Y+=1)
   {FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,M(X,Y,4.8),M(X,Y,2.2),ECC_Visibility,Params) || FMath::Abs(Hit.ImpactPoint.Z-M(X,Y,2.45).Z)>2)++Blocked;}
   Test->TestEqual(TEXT("Swimming area has no glass, frames, columns or sealed deck intrusions"),Blocked,0);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   P->SetActorLocation(M(-3.85,19.75,4.24)+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);
   Camera=W->SpawnActor<ACameraActor>();Camera->SetActorLocation(M(-9,24,7));Camera->SetActorRotation((M(2,25,3.9)-Camera->GetActorLocation()).Rotation());
   PC->SetViewTarget(Camera);At=W->GetTimeSeconds();Stage=1;return false;
  }
  Evidence.Record(W,PC,Stage);
  if(Stage==1 && W->GetTimeSeconds()-At>1)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pool-Day-Entry.png"));PC->SetViewTarget(P);Stage=2;At=W->GetTimeSeconds();return false;
  }
  if(Stage==2 || Stage==3)
  {
   const FVector Goal=Stage==2?M(-3.85,25.5,3.4):M(-3.85,19.7,4.24);const FVector Delta=Goal-P->GetActorLocation();
   if(P->GetCharacterMovement()->IsSwimming())
   {
    if(!Swam){SwimStart=P->GetActorLocation();Swam=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pool-NativeSwimming.png"));}
    SwamDistance=FMath::Max(SwamDistance,FVector::Dist2D(SwimStart,P->GetActorLocation()));
   }
   if(Delta.Size2D()<42)
   {
    Key(EKeys::W,IE_Released);Held=false;
    if(Stage==2){Test->TestTrue(TEXT("Walk down pool steps enters inherited swimming mode"),Swam && SwamDistance>120);Stage=3;At=W->GetTimeSeconds();return false;}
    Test->TestTrue(TEXT("Native steps return swimmer to dry ordinary walking"),P->GetCharacterMovement()->IsMovingOnGround() && P->GetActorLocation().Z>M(0,0,4.24).Z+70);
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pool-NativeExit.png"));Stage=4;At=W->GetTimeSeconds();return false;
   }
   if(W->GetTimeSeconds()-At>18){Test->AddError(FString::Printf(TEXT("Normal pool movement blocked in stage %d at %s"),Stage,*P->GetActorLocation().ToString()));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pool-NativeBlocked.png"));return Finish();}
   PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));if(!Held){Key(EKeys::W,IE_Pressed);Held=true;}return false;
  }
  if(Stage==4)
  {
   if(!ViewSet){for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(Time==0?18.5:22);PC->SetViewTarget(Camera);ViewSet=true;At=W->GetTimeSeconds();return false;}
   if(W->GetTimeSeconds()-At<2.5)return false;
   FNeonCityVisualEvidence::CaptureHUD(W,Time==0?TEXT("Pool-Sunset-Close.png"):TEXT("Pool-Night-Close.png"));ViewSet=false;if(++Time==2)return Finish();
  }
  return false;
 }
private:
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool Finish()
 {
  Key(EKeys::W,IE_Released);TSharedPtr<FJsonObject> J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("swimming_reached"),Swam);J->SetNumberField(TEXT("native_swim_distance_cm"),SwamDistance);J->SetBoolField(TEXT("normal_exit_reached"),Stage>=4);J->SetStringField(TEXT("scope"),TEXT("One explicit deck fixture; actual W mapping, physical steps, shared swimming, finite pool collision and Day/Sunset/Night close views. No teleport after entry, ocean waves or second swim system."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);
  if(FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)))FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("ContainedPool.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityWaterVolume* Pool=nullptr;ACameraActor* Camera=nullptr;
 double Started=0,SwamDistance=0;float At=0;int32 Stage=0,Time=0;bool Held=false,Swam=false,ViewSet=false;FVector SwimStart;FNeonCityVisualEvidence Evidence{TEXT("ContainedPool")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase205cPool,"NeonCity.Phase205c.ContainedPoolPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPhase205cPool::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgePoolCheck::FSwim(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
