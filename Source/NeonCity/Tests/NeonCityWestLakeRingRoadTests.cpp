#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "World/NeonCityDayNightCycle.h"

namespace NeonCityWestLakeRingRoad
{
/** The lakeside road's centre line round the lake (Tools/.../LandRebuild/Data/LakeRing_WL.json, UE metres), at
 * road level (city ground + 3 cm), cm. */
bool ReadRing(TArray<FVector>& Points)
{
 FString Text;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Tools/Art/Phase205cUpgrade/LandRebuild/Data/LakeRing_WL.json"))))return false;
 TSharedPtr<FJsonObject> Root;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid())return false;
 for(const auto& V:Root->GetArrayField(TEXT("points")))
 {
  const auto& P=V->AsArray();
  Points.Add(FVector(P[0]->AsNumber()*100.,P[1]->AsNumber()*100.,52.5));
 }
 return Points.Num()>100;
}

/** Normal PIE: the whole lakeside road from the Broken Bridge round the north, west and south shores to Hubin Road.
 *  Survey: down traces on the centre and both lanes every 6 m find the road surface at road level (no gap, no
 *  raised slab), and a car-sized box swept along both lanes hits nothing (no wall, pavilion, tree or invisible
 *  collision in the carriageway).
 *  Drive: a sedan driven on the right-hand lane by a simple autopilot covers the whole road under Chaos physics
 *  without leaving it, sinking or stalling. -NeonCityRingCapture saves HUD stills along the way. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>1200){Test->AddError(FString::Printf(TEXT("Ring drive timed out at sample %d / %d"),Index,Points.Num()));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   ACharacter* Player=Cast<ACharacter>(PC->GetPawn());
   if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   if(!Test->TestTrue(TEXT("ring road centre line"),ReadRing(Points)))return true;
   Survey(W);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12.f);}
   // a sedan on the right-hand lane at the Broken Bridge, facing along the road
   Index=2;
   const FVector Dir=(Points[6]-Points[2]).GetSafeNormal2D();
   const FVector Right=FVector::CrossProduct(FVector::UpVector,Dir);
   FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
   Car=W->SpawnActor<ANeonCityStarterSedan>(Points[2]+Right*250+FVector(0,0,90),Dir.Rotation(),P);
   if(!Test->TestNotNull(TEXT("sedan spawned on the road"),Car))return true;
   Player->SetActorLocation(Car->GetActorLocation()-Car->GetActorRightVector()*255+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   Stage=1;At=W->GetTimeSeconds();return false;
  }
  if(Stage==1)
  {
   if(W->GetTimeSeconds()-At<1.5)return false;
   if(!Test->TestTrue(TEXT("sedan entered"),Car->GetVehicleInteraction()->Enter(Cast<ACharacter>(PC->GetPawn()))))return true;
   Stage=2;At=LastProgress=W->GetTimeSeconds();Last=Car->GetActorLocation();return false;
  }
  if(W->GetTimeSeconds()-At<2)return false;
  const FVector Here=Car->GetActorLocation();Distance+=FVector::Dist2D(Here,Last);Last=Here;
  int32 Best=Index;double BestD=DBL_MAX;
  for(int32 I=FMath::Max(0,Index-2);I<FMath::Min(Index+12,Points.Num());++I)
  {
   const double D=FVector::DistSquared2D(Here,Lane(I));if(D<BestD){BestD=D;Best=I;}
  }
  if(Best>Index){Index=Best;LastProgress=W->GetTimeSeconds();}
  // lane error: distance to the lane's polyline (the samples are 6 m apart)
  BestD=DBL_MAX;
  for(int32 I=FMath::Max(0,Index-2);I<FMath::Min(Index+2,Points.Num()-1);++I)
   BestD=FMath::Min(BestD,(double)FMath::Square(FMath::PointDistToSegment(FVector(Here.X,Here.Y,0),FVector(Lane(I).X,Lane(I).Y,0),FVector(Lane(I+1).X,Lane(I+1).Y,0))));
  if(FMath::Sqrt(BestD)>420 || Here.Z<Points[Index].Z-150 || W->GetTimeSeconds()-LastProgress>30)
  {
   Test->AddError(FString::Printf(TEXT("Drive failed at sample %d (%s): lane error %.0f cm, height %.0f cm, speed %.1f km/h"),Index,*(Points[Index]/100).ToString(),
    FMath::Sqrt(BestD),Here.Z-Points[Index].Z,Car->GetDrivingComponent()->GetSpeedKmh()));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Ring-Drive-Failure.png"));return true;
  }
  // aim a few samples ahead on the lane; slow for bends
  const FVector Aim=Lane(FMath::Min(Index+3,Points.Num()-1))-Here;
  const float Angle=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Aim.Rotation().Yaw);
  const FVector A=Points[FMath::Min(Index+2,Points.Num()-1)]-Points[Index],B=Points[FMath::Min(Index+8,Points.Num()-1)]-Points[FMath::Min(Index+2,Points.Num()-1)];
  const float Bend=FMath::Abs(FMath::FindDeltaAngleDegrees(A.Rotation().Yaw,B.Rotation().Yaw));
  const float Speed=Car->GetChaosMovement()->GetForwardSpeed();
  const float Desired=Bend>20?700:Bend>10?1000:1500;
  if(W->GetTimeSeconds()-LastLog>1.f)
  {
   LastLog=W->GetTimeSeconds();
   UE_LOG(LogTemp,Display,TEXT("RingDrive %d at %s yaw %.0f aim %.0f bend %.0f speed %.0f lane err %.0f"),Index,*(Here/100).ToString(),Car->GetActorRotation().Yaw,Angle,Bend,Speed,FMath::Sqrt(BestD));
  }
  Car->GetDrivingComponent()->SetSteerInput(FMath::Clamp(Angle/25.f,-1.f,1.f));
  Car->GetDrivingComponent()->SetDriveInput(FMath::Clamp((Desired-Speed)/300.f,0.f,1.f));
  Car->GetDrivingComponent()->SetBrakeInput(FMath::Clamp((Speed-Desired-60)/400.f,0.f,.7f));
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityRingCapture")) && Index/110>LastShot)
  {LastShot=Index/110;FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Ring-Drive-%02d.png"),LastShot));}
  if(Index>=Points.Num()-3)
  {
   Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);
   Test->TestTrue(FString::Printf(TEXT("drove the whole lakeside road (%.0f m)"),Distance/100),Distance>.9*Length());
   Test->AddInfo(FString::Printf(TEXT("ring driven in %.0f s"),W->GetTimeSeconds()-At));
   return true;
  }
  return false;
 }
private:
 FVector Lane(int32 I) const
 {
  const FVector Dir=(Points[FMath::Min(I+1,Points.Num()-1)]-Points[FMath::Max(I-1,0)]).GetSafeNormal2D();
  return Points[I]+FVector::CrossProduct(FVector::UpVector,Dir)*250;
 }
 double Length() const { double L=0;for(int32 I=1;I<Points.Num();++I)L+=FVector::Dist2D(Points[I],Points[I-1]);return L; }
 void Survey(UWorld* W)
 {
  FCollisionQueryParams Q(TEXT("RingSurvey"),true);
  int32 Gaps=0,Raised=0,Blocked=0;
  for(int32 I=1;I<Points.Num()-1;++I)
  {
   const FVector Dir=(Points[I+1]-Points[I-1]).GetSafeNormal2D();const FVector N=FVector::CrossProduct(FVector::UpVector,Dir);
   for(float Off:{-250.f,0.f,250.f})
   {
    const FVector P=Points[I]+N*Off;FHitResult Hit;
    if(!W->LineTraceSingleByChannel(Hit,P+FVector(0,0,600),P-FVector(0,0,200),ECC_WorldStatic,Q))
    {if(++Gaps<=12)Test->AddInfo(FString::Printf(TEXT("no road under sample %d lane %.0f at %s"),I,Off,*(P/100).ToString()));continue;}
    const float E=Hit.ImpactPoint.Z-P.Z;
    if(FMath::Abs(E)>12 && ++Raised<=12)Test->AddInfo(FString::Printf(TEXT("road surface off by %.0f cm at sample %d lane %.0f (%s, hit %s)"),E,I,Off,*(P/100).ToString(),*GetNameSafe(Hit.GetActor())));
   }
   for(float Off:{-250.f,250.f})
   {
    FHitResult Hit;const FVector S=Points[I]+N*Off+FVector(0,0,120),E=Points[I+1]+N*Off+FVector(0,0,120);
    if(W->SweepSingleByChannel(Hit,S,E,Dir.Rotation().Quaternion(),ECC_WorldStatic,FCollisionShape::MakeBox(FVector(150,85,55)),Q) && ++Blocked<=12)
     Test->AddInfo(FString::Printf(TEXT("lane %.0f blocked near sample %d (%s) by %s"),Off,I,*(Hit.ImpactPoint/100).ToString(),*GetNameSafe(Hit.GetActor())));
   }
  }
  Test->TestEqual(TEXT("road surface under every sample"),Gaps,0);
  Test->TestTrue(FString::Printf(TEXT("no raised piece or sunken stretch on the carriageway (%d)"),Raised),Raised==0);
  Test->TestEqual(TEXT("both lanes clear of obstacles"),Blocked,0);
 }
 FAutomationTestBase* Test;TArray<FVector> Points;ANeonCityStarterSedan* Car=nullptr;FVector Last=FVector::ZeroVector;
 double Started=0,Distance=0;float At=0,LastProgress=0,LastLog=0;int32 Stage=0,Index=0,LastShot=-1;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeRingRoadPIE,"NeonCity.WestLake.RingRoadPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeRingRoadPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeRingRoad::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
