#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Components/SceneComponent.h"
#include <limits>

namespace WaterDispatch356
{
// Frozen pre-change algorithm. This deliberately retains reflected interface
// dispatch and its second pass; it is independent of the optimized helper.
static bool Legacy(const TArray<ANeonCityWaterVolume*>& Waters,FVector Point,FWaterSurfaceSample& Sample)
{
 Sample=FWaterSurfaceSample();if(Point.ContainsNaN())return false;
 for(const auto* Water:Waters)
 {
  FWaterSurfaceSample C;
  if(IsValid(Water)&&Water->bContainedPoolWater&&IWaterStateInterface::Execute_QueryWaterSurface(Water,Point,C)
   &&!C.SurfacePoint.ContainsNaN()&&!C.Normal.ContainsNaN()&&!C.Flow.ContainsNaN()&&C.Normal.Normalize()&&C.Normal.Z>0)
  {Sample=C;return true;}
 }
 for(const auto* Water:Waters)
 {
  FWaterSurfaceSample C;
  if(IsValid(Water)&&IWaterStateInterface::Execute_QueryWaterSurface(Water,Point,C)
   &&!C.SurfacePoint.ContainsNaN()&&!C.Normal.ContainsNaN()&&!C.Flow.ContainsNaN()&&C.Normal.Normalize()&&C.Normal.Z>0)
  {Sample=C;return true;}
 }
 return false;
}
static bool Equal(const FWaterSurfaceSample& A,const FWaterSurfaceSample& B)
{
 return A.SurfacePoint.Equals(B.SurfacePoint,1.e-6)&&A.Normal.Equals(B.Normal,1.e-6)
  &&A.Flow.Equals(B.Flow,1.e-6)&&A.InsideVolume==B.InsideVolume&&A.ContainedWater==B.ContainedWater;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWaterDispatchParity356,"NeonCity.Marine.NativeWaterDispatchParity356",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNativeWaterDispatchParity356::RunTest(const FString&)
{
 using namespace WaterDispatch356;
 const auto Initialization=UWorld::InitializationValues().CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("NativeWaterDispatch356")),nullptr,true,ERHIFeatureLevel::Num,&Initialization);
 if(!TestNotNull(TEXT("Isolated query fixture world"),W))return false;
 W->InitializeActorsForPlay(FURL());auto* S=W->GetSubsystem<UMarineWorldSubsystem>();
 if(!TestNotNull(TEXT("Real marine subsystem"),S)){W->DestroyWorld(false);return false;}
 TArray<ANeonCityWaterVolume*> Waters;
 auto Add=[&](FVector Position,FVector Extent,bool Pool,FRotator Rotation,FVector Scale)
 {
  auto* V=W->SpawnActor<ANeonCityWaterVolume>();V->GetRootComponent()->SetMobility(EComponentMobility::Movable);
  TestTrue(TEXT("Actual fixture transform applied"),V->SetActorTransform(FTransform(Rotation,Position,Scale)));
  TestTrue(TEXT("Fixture location and rotation match requested input"),V->GetActorTransform().Equals(FTransform(Rotation,Position,Scale),.001));
  V->Extent=Extent;V->bContainedPoolWater=Pool;V->CurrentVelocity=FVector(Waters.Num()*3,7,-2);
  Waters.Add(V);S->RegisterWater(V);return V;
 };
 Add(FVector(0,0,-2000),FVector(12000,12000,2000),false,FRotator::ZeroRotator,FVector::OneVector);
 auto* Basin=Add(FVector(200,100,1500),FVector(400,250,120),true,FRotator(8,37,4),FVector(1.2,.8,1.1));
 Add(FVector(250,100,1650),FVector(220,180,80),true,FRotator::ZeroRotator,FVector::OneVector);
 Add(FVector(700,700,100),FVector(250,250,100),false,FRotator(180,0,0),FVector::OneVector);
 auto* Invalid=Add(FVector(-100,0,500),FVector(200,200,100),true,FRotator::ZeroRotator,FVector::OneVector);
 Invalid->CurrentVelocity.X=std::numeric_limits<double>::quiet_NaN();
 int32 Compared=0,Mismatches=0;
 auto Compare=[&](FVector Point)
 {
  FWaterSurfaceSample Old,New;const bool A=Legacy(Waters,Point,Old),B=S->QueryWater(Point,New);++Compared;
  if(A!=B||!Equal(Old,New))
  {if(Mismatches++<8)AddError(FString::Printf(TEXT("Query mismatch at %s old=%d new=%d"),*Point.ToString(),A,B));}
 };
 FRandomStream Random(356);
 for(int32 I=0;I<10000;++I)Compare(FVector(Random.FRandRange(-14000,14000),Random.FRandRange(-14000,14000),Random.FRandRange(-5000,3000)));
 for(auto* V:Waters)
  for(double X:{-1.,0.,1.})for(double Y:{-1.,0.,1.})for(double Z:{-1.00001,-1.,0.,1.,1.00001,8.})
   Compare(V->GetActorTransform().TransformPosition(V->Extent*FVector(X,Y,Z)));
 Compare(FVector(std::numeric_limits<double>::quiet_NaN(),0,0));
 // Query state may change twice in a frame. No spatial cache is permitted.
 for(int32 I=0;I<24;++I)
 {
  const FVector Moved(100+I*37,100,1500+I*5);TestTrue(TEXT("Same-frame native volume move applied"),Basin->SetActorLocation(Moved));
  TestTrue(TEXT("Same-frame query uses the moved location"),Basin->GetActorLocation().Equals(Moved,.001));Basin->Extent.X=100+I*9;
  Basin->bContainedPoolWater=(I%2)==0;Basin->CurrentVelocity=FVector(I,-I,2);
  Compare(Basin->GetActorLocation());Compare(Basin->GetActorLocation()+FVector(0,0,500));
 }
 S->UnregisterWater(Waters[2]);auto* Removed=Waters[2];Waters.RemoveAt(2);
 Compare(FVector(250,100,1650));S->RegisterWater(Removed);Waters.Add(Removed);Compare(FVector(250,100,1650));
 TestEqual(TEXT("Legacy interface and optimized exact-native query parity"),Mismatches,0);
 // Measured microbenchmark, with identical providers, inputs and returned
 // samples. It does not establish a frame/GPU budget or an adapter PASS.
 for(int32 I=0;I<27;++I)Add(FVector(40000+I*1000,0,1000),FVector(200,200,80),true,FRotator::ZeroRotator,FVector::OneVector);
 TArray<FVector> Points;for(int32 I=0;I<256;++I)Points.Add(FVector(Random.FRandRange(-8000,8000),Random.FRandRange(-8000,8000),-100));
 constexpr int32 Iterations=50000;FWaterSurfaceSample Sample;double Checksum=0;
 auto Bench=[&](bool Old)
 {
  const double Start=FPlatformTime::Seconds();
  for(int32 I=0;I<Iterations;++I)
  {const bool Found=Old?Legacy(Waters,Points[I%Points.Num()],Sample):S->QueryWater(Points[I%Points.Num()],Sample);Checksum+=Found?Sample.SurfacePoint.Z+Sample.Flow.Y:1.;}
  return (FPlatformTime::Seconds()-Start)*1000.;
 };
 // Discard a short shared warmup before either measured route.
 for(const auto& Point:Points){Legacy(Waters,Point,Sample);S->QueryWater(Point,Sample);}
 const double LegacyMs=Bench(true),NativeMs=Bench(false);
 const FString Result=FString::Printf(TEXT("{\"comparisons\":%d,\"mismatches\":%d,\"iterations_per_route\":%d,\"legacy_ms\":%.6f,\"native_ms\":%.6f,\"checksum\":%.6f,\"scope\":\"exact-native water parity and isolated dispatch benchmark; no whole-frame or adapter validation inferred\"}\n"),Compared,Mismatches,Iterations,LegacyMs,NativeMs,Checksum);
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC923/WaterDispatch356");IFileManager::Get().MakeDirectory(*Directory,true);
 const FString File=Directory/FString::Printf(TEXT("Result-%s.json"),*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
 TestTrue(TEXT("Measured parity and timing result retained"),FFileHelper::SaveStringToFile(Result,*File));AddInfo(Result);
 W->DestroyWorld(false);return Mismatches==0;
}
#endif
