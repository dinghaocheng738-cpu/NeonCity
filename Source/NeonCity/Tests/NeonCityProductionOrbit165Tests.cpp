#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerController.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"

namespace ProductionOrbit165
{
/** Real game-viewport camera inspection, not a player route or timing run. */
class FOrbit final : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<ANeonCityPlayerController> PC;TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<AActor> OldView;bool Started=false,CapturedState=false,Cleaned=false,OldHidden=false,OldCycle=true,OldStat=false,RecorderStarted=false;
 float OldHour=12,GameAt=0;double Began=FPlatformTime::Seconds(),AtWall=0;FVector Previous;FString Directory,Rows=TEXT("game_seconds,wall_seconds,phase,camera_x,camera_y,camera_z,pitch,yaw,fov\n");
 FNeonCityVisualEvidence Evidence{TEXT("Production165-CapeHarbor"),true};
 const TArray<FVector> Eyes={{-5300,-2662,275},{-5300,-2662,1000},{5500,-11500,1200},{7200,-9300,700},{9100,-7200,750},{10800,-6900,700},{11239,-7369,400},{10500,-9000,1200}};
 const TArray<FVector> Aims={FVector(-5300,-2662,275)+FRotator(-1.5f,4.004173f,0).Vector()*20000,FVector(8500,-8392,460),FVector(8500,-8392,460),FVector(8500,-8392,460),FVector(8500,-8392,460),FVector(12000,-9000,370),FVector(12000,-9000,370),FVector(17200,-10400,1600)};
 void Cleanup()
 {
  if(Cleaned)return;Cleaned=true;
  if(PC.IsValid() && CapturedState)
  {
   if(OldView.IsValid())PC->SetViewTarget(OldView.Get());PC->SetPhotographyHUDHidden(OldHidden);
   auto* Client=PC->GetWorld()?PC->GetWorld()->GetGameViewport():nullptr;
   if(Client && Client->IsStatEnabled(TEXT("Unit"))!=OldStat)PC->ConsoleCommand(TEXT("stat unit"));
  }
  if(Clock.IsValid() && CapturedState){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  if(Camera.IsValid())Camera->Destroy();
 }
 bool End(const FString& Error=FString())
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  if(!Directory.IsEmpty())
  {
   Test->TestTrue(TEXT("Actual camera positions retained"),FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("camera.csv"))));
   TArray<FString> Images;IFileManager::Get().FindFiles(Images,*(Directory/TEXT("*.jpg")),true,false);
   if(Error.IsEmpty())
   {
    Test->TestTrue(TEXT("No motion evidence disk writes failed"),!Evidence.HasWriteError());
    Test->TestTrue(TEXT("Native moving-view sequence contains real rendered frames"),Images.Num()>=160);
   }
   FFileHelper::SaveStringToFile(TEXT("Actual standalone game-viewport camera inspection at1600x900/Epic100. Unpaused world; no target-image background. Camera flight is a visual fixture, not a continuous player route. JPEG/GPU readback timing is diagnostic only: use the separate clean all-frame performance runs.\n"),*(Directory/TEXT("scope.txt")));
  }
  Cleanup();return true;
 }
public:
 explicit FOrbit(FAutomationTestBase* In):Test(In){}
 ~FOrbit()override{Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>240)return End(TEXT("Local rendered orbit exceeded its bounded deadline"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  if(!Started)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")))return End(TEXT("Visual orbit requires isolated motion readback, never clean timing"));
   PC=Cast<ANeonCityPlayerController>(W->GetFirstPlayerController());if(!PC.IsValid() || !W->GetGameViewport() || !W->GetGameViewport()->Viewport)return false;
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   if(!Clock.IsValid())return End(TEXT("Actual persistent clock missing"));
   OldView=PC->GetViewTarget();OldHidden=PC->IsPhotographyHUDHidden();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;OldStat=W->GetGameViewport()->IsStatEnabled(TEXT("Unit"));CapturedState=true;
   Camera=W->SpawnActor<ACameraActor>();if(!Camera.IsValid())return End(TEXT("Inspection camera allocation failed"));
   Camera->GetCameraComponent()->FieldOfView=65;Camera->GetCameraComponent()->PostProcessBlendWeight=0;
   PC->SetViewTarget(Camera.Get());PC->SetPhotographyHUDHidden(true);Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(16.5f);
   Camera->SetActorLocationAndRotation(Eyes[0],FRotator(-1.5f,4.004173f,0));Previous=Eyes[0];GameAt=W->GetTimeSeconds();AtWall=FPlatformTime::Seconds();Started=true;
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("Motion/Production165-CapeHarbor");return false;
  }
  if(!RecorderStarted && (W->GetTimeSeconds()-GameAt<20 || FPlatformTime::Seconds()-AtWall<20))return false;
  if(!RecorderStarted)
  {
   Evidence.Record(W,PC.Get(),0);RecorderStarted=true;GameAt=W->GetTimeSeconds();AtWall=FPlatformTime::Seconds();
   if(W->GetGameViewport()->IsStatEnabled(TEXT("Unit")))PC->ConsoleCommand(TEXT("stat unit"));return false;
  }
  // The recorder samples real, unconstrained simulation at12Hz. CSV keeps
  // both game and wall timestamps instead of inventing a render frame rate.
  const float Age=W->GetTimeSeconds()-GameAt;constexpr float LegSeconds=3.5f;
  const int32 Leg=FMath::Min(int32(Age/LegSeconds),Eyes.Num()-2);const float T=FMath::Clamp((Age-Leg*LegSeconds)/LegSeconds,0.f,1.f);const float Ease=T*T*(3-2*T);
  const FVector Eye=FMath::Lerp(Eyes[Leg],Eyes[Leg+1],Ease);
  const FQuat Rotation=FQuat::Slerp((Aims[Leg]-Eyes[Leg]).Rotation().Quaternion(),(Aims[Leg+1]-Eyes[Leg+1]).Rotation().Quaternion(),Ease);
  FCollisionQueryParams Q(SCENE_QUERY_STAT(Production165Camera),false,PC->GetPawn());Q.AddIgnoredActor(Camera.Get());FHitResult Hit;
  if(W->SweepSingleByChannel(Hit,Previous,Eye,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(5),Q))return End(FString::Printf(TEXT("Inspection path intersects real geometry: %s at%s"),*GetPathNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString()));
  Camera->SetActorLocationAndRotation(Eye,Rotation);Previous=Eye;Evidence.Record(W,PC.Get(),Leg);
  const auto R=Camera->GetActorRotation();Rows+=FString::Printf(TEXT("%.6f,%.6f,%d,%.3f,%.3f,%.3f,%.4f,%.4f,65\n"),W->GetTimeSeconds(),FPlatformTime::Seconds()-AtWall,Leg,Eye.X,Eye.Y,Eye.Z,R.Pitch,R.Yaw);
  if(Age>=LegSeconds*(Eyes.Num()-1)+2)return End();return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionOrbit165,"NeonCity.Phase205c.MainlandC923.Standalone.ProductionVisualOrbit",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FProductionOrbit165::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(ProductionOrbit165::FOrbit(this));return true;}
#endif
