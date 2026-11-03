#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Vehicles/Marine/MarineTestArea.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Water/CoastalOceanSurface.h"
#include "Water/CoastalWaveData.h"
#include "World/NeonCityMainlandSite.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BrushComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "RHI.h"

namespace MainlandOceanNorth923
{
/** Read-only actual bootstrap coverage, not a substitute water fixture.
 * The former native volume ends atY=-25m and fails the north-edge checks.
 * Every comparison uses the same ready FFT provider at the same world point;
 * changing volume centre must never rebase displacement, normal or velocity. */
class FCoverage : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 double Started=FPlatformTime::Seconds();
 float ObservedAt=-1;
 bool Failed=false;
 bool ObservedLiveWave=false;
 TWeakObjectPtr<ANeonCityWaterVolume> Volume;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;
 const UCoastalWaveData* OriginalField=nullptr;
 const UCoastalWaveData* AuthoredField=nullptr;
 FTransform OriginalTransform;
 FVector OriginalExtent=FVector::ZeroVector;
 uint64 OriginalSequence=0;
 TArray<TSharedPtr<FJsonValue>> Rows;

 bool Check(bool Passed,const FString& Description)
 {
  if(!Test->TestTrue(Description,Passed))Failed=true;
  return Passed;
 }
 bool Finish(const FString& Failure=FString())
 {
  if(!Failure.IsEmpty()){Test->AddError(Failure);Failed=true;}
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  Label=FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("OceanNorthCoverage923"):Label,TEXT('_'));
  Label.ReplaceInline(TEXT(".."),TEXT("_"));
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC923")/Label;
  IFileManager::Get().MakeDirectory(*Directory,true);
  const FString File=Directory/TEXT("OceanNorthCoverage.json");
  auto Receipt=MakeShared<FJsonObject>();
  Receipt->SetBoolField(TEXT("passed"),!Failed);
  Receipt->SetStringField(TEXT("failure"),Failure);
  Receipt->SetStringField(TEXT("volume"),GetPathNameSafe(Volume.Get()));
  Receipt->SetStringField(TEXT("ocean"),GetPathNameSafe(Ocean.Get()));
  Receipt->SetStringField(TEXT("field"),GetPathNameSafe(OriginalField));
  Receipt->SetStringField(TEXT("authored_mainland_site_field"),GetPathNameSafe(AuthoredField));
  Receipt->SetBoolField(TEXT("authored_field_opt_in"),AuthoredField!=nullptr);
  Receipt->SetStringField(TEXT("actual_volume_origin_cm"),Volume.IsValid()?Volume->GetActorLocation().ToString():TEXT("absent"));
  Receipt->SetStringField(TEXT("actual_volume_extent_cm"),Volume.IsValid()?Volume->Extent.ToString():TEXT("absent"));
  Receipt->SetNumberField(TEXT("expected_south_edge_cm"),-377500);
  Receipt->SetNumberField(TEXT("expected_north_edge_cm"),35000);
  Receipt->SetNumberField(TEXT("first_query_sequence"),double(OriginalSequence));
  Receipt->SetNumberField(TEXT("last_query_sequence"),Ocean.IsValid()?double(Ocean->GetQuerySequence()):0.);
  Receipt->SetBoolField(TEXT("actual_nonconstant_fft_witness"),ObservedLiveWave);
  Receipt->SetArrayField(TEXT("world_point_checks"),Rows);
  Receipt->SetStringField(TEXT("scope"),TEXT("Actual connected MarineTestArea bootstrap and native brush/registry boundaries. Existing south/X/Z/sea datum retained; original-water, new-north-water and outside points checked against the same live FFT world samples across normal ticks. No spawned fixture, actor/boat pose, input, physics, wave, material, save or time-of-day mutation. This is water-service coverage evidence, not swimming/boating traversal or a performance sample."));
  FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
  if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Json,*File))
   Test->AddError(TEXT("Could not retain unique C923 water coverage evidence"));
  return true;
 }
 void SamplePoint(UWorld* World,const TCHAR* Name,const FVector& Point,bool ExpectedInside,bool AfterTicks)
 {
  auto* Water=Volume.Get();auto* Surface=Ocean.Get();
  FWaterSurfaceSample Actual,Registered;
  const bool Found=Water->QueryWaterSurface_Implementation(Point,Actual);
  const bool RegistryFound=World->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Point,Registered);
  const FString Description=FString::Printf(TEXT("%s%s at%s"),AfterTicks?TEXT("after normal ticks "):TEXT(""),Name,*Point.ToString());
  Check(Found==ExpectedInside,Description+TEXT(" has the exact native XY membership"));
  Check(RegistryFound==ExpectedInside,Description+TEXT(" has the same real registry membership"));
  float Height=-130.f;FVector Normal=FVector::UpVector,Velocity=FVector::ZeroVector;
  if(ExpectedInside && Found)
  {
   Surface->Sample(Point,Height,Normal,Velocity);
   if(FMath::Abs(Height+130.f)>.001f || !Normal.Equals(FVector::UpVector,.00001f) || !Velocity.IsNearlyZero(.001f))
    ObservedLiveWave=true;
   Check(FMath::Abs(Actual.SurfacePoint.X-Point.X)<.001 && FMath::Abs(Actual.SurfacePoint.Y-Point.Y)<.001,
    Description+TEXT(" preserves absolute world XY"));
   Check(FMath::Abs(Actual.SurfacePoint.Z-Height)<.01 && Actual.Normal.Equals(Normal,.0001f)
    && Actual.Flow.Equals(Velocity,.001f),Description+TEXT(" retains unchanged FFT world height/normal/velocity"));
   Check(!Actual.ContainedWater && Actual.InsideVolume,Description+TEXT(" is the real submerged ocean column"));
   if(RegistryFound)
    Check(Registered.SurfacePoint.Equals(Actual.SurfacePoint,.01f) && Registered.Normal.Equals(Actual.Normal,.0001f)
     && Registered.Flow.Equals(Actual.Flow,.001f),Description+TEXT(" is exposed unchanged through MarineWorldSubsystem"));
  }
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);
  Row->SetStringField(TEXT("point_cm"),Point.ToString());Row->SetBoolField(TEXT("after_normal_ticks"),AfterTicks);
  Row->SetBoolField(TEXT("expected_inside"),ExpectedInside);Row->SetBoolField(TEXT("native_query"),Found);
  Row->SetBoolField(TEXT("registry_query"),RegistryFound);
  if(Found){Row->SetStringField(TEXT("surface_point_cm"),Actual.SurfacePoint.ToString());Row->SetStringField(TEXT("normal"),Actual.Normal.ToString());Row->SetStringField(TEXT("flow_cm_s"),Actual.Flow.ToString());}
  if(ExpectedInside){Row->SetNumberField(TEXT("direct_fft_height_cm"),Height);Row->SetStringField(TEXT("direct_fft_normal"),Normal.ToString());Row->SetStringField(TEXT("direct_fft_velocity_cm_s"),Velocity.ToString());}
  Rows.Add(MakeShared<FJsonValueObject>(Row));
 }
 void SampleWitnesses(UWorld* World,bool AfterTicks)
 {
  // Existing old-envelope witnesses avoid finite pools and authored dry land.
  SamplePoint(World,TEXT("old west water"),FVector(-25000,-8000,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("old southern water"),NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-260000,-1000)),true,AfterTicks);
  SamplePoint(World,TEXT("old offshore water"),NeonCityCoastalLayout::FromLegacyIsland(FVector(130000,-130000,-1000)),true,AfterTicks);
  SamplePoint(World,TEXT("old north boundary"),FVector(50000,-2500,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("new eastern headland water"),FVector(50000,30000,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("new north boundary inside"),FVector(50000,34999,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("new north boundary outside"),FVector(50000,35001,-1000),false,AfterTicks);
  SamplePoint(World,TEXT("south boundary inside (follows the island)"),FVector(0,-377499,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("south boundary outside (follows the island)"),FVector(0,-377501,-1000),false,AfterTicks);
  SamplePoint(World,TEXT("old east X boundary inside"),FVector(219999,-8000,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("old east X boundary outside"),FVector(220001,-8000,-1000),false,AfterTicks);
  SamplePoint(World,TEXT("old west X boundary inside"),FVector(-219999,-8000,-1000),true,AfterTicks);
  SamplePoint(World,TEXT("old west X boundary outside"),FVector(-220001,-8000,-1000),false,AfterTicks);
  // Real registered hull leases supply displaced old-world witnesses. Far
  // coverage points may legitimately use mean-water fallback; they alone
  // would not expose an accidental rebase of a live wave sample.
  int32 LiveBoats=0;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(World);It;++It)
  {
   FVector Point=It->GetActorLocation();Point.Z=-1000;
   const FString Name=TEXT("existing native hull world sample ")+It->VehicleID.ToString();
   SamplePoint(World,*Name,Point,true,AfterTicks);++LiveBoats;
  }
  Check(LiveBoats>=2,TEXT("Existing physical boats supply real old-world sample witnesses without a new fixture"));
 }
public:
 explicit FCoverage(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>90)return Finish(TEXT("Actual ocean north coverage did not become ready within90s"));
  auto* World=NeonCityValidationWorld();if(!World || World->GetTimeSeconds()<8)return false;
  if(ObservedAt<0)
  {
   const auto* Site=ANeonCityMainlandSite::Find(World);
   if(!Site || !ANeonCityCoastalRegion::Find(World))
    return Finish(TEXT("Coverage requires the actual connected authored mainland"));
   int32 Areas=0,Providers=0;
   for(TActorIterator<AMarineTestArea> It(World);It;++It)++Areas;
   for(TActorIterator<ACoastalOceanSurface> It(World);It;++It){Ocean=*It;++Providers;}
   if(!Check(Areas==1 && Providers==1,TEXT("One actual bootstrap and one original FFT provider")))return Finish();
   if(!Ocean.IsValid() || !Ocean->IsReady() || !Ocean->GetField())return false;
   AuthoredField=Site->OceanField.Get();
   if(AuthoredField && !Check(Ocean->GetField()==AuthoredField,
    TEXT("Existing runtime provider received the persistent site's exact optional field before BeginPlay")))return Finish();
   int32 Volumes=0;
   for(TActorIterator<ANeonCityWaterVolume> It(World);It;++It)
    if(!It->bContainedPoolWater && It->OceanSurface==Ocean.Get()){Volume=*It;++Volumes;}
   if(!Check(Volumes==1 && Volume.IsValid(),TEXT("One real connected ocean volume; finite pools stay separate")))return Finish();
   const FVector Origin=Volume->GetActorLocation(),Extent=Volume->Extent;
   Check(Origin.Equals(FVector(0,-171250,-20130),.001f) && Extent.Equals(FVector(220000,206250,20000),.001f),
    TEXT("Only connected ocean Y centre and half-extent change (south edge follows the moved island); X/Z and sea datum stay original"));
   Check(Volume->GetActorQuat().Equals(FQuat::Identity,.000001f) && Volume->GetActorScale3D().Equals(FVector::OneVector,.000001f),
    TEXT("Native ocean column retains unrotated unit world coordinates"));
   Check(Volume->CurrentVelocity.IsNearlyZero(.0001f),TEXT("Original zero authored current is unchanged"));
   const FBox Brush=Volume->GetBrushComponent()->Bounds.GetBox();
   Check(FMath::Abs(Brush.Min.Y+377500.)<.1 && FMath::Abs(Brush.Max.Y-35000.)<.1
    && FMath::Abs(Brush.Min.X+220000.)<.1 && FMath::Abs(Brush.Max.X-220000.)<.1,
    TEXT("Actual native brush overlap spans the island-following south edge, X edges and 350m north edge"));
   Check(FMath::Abs(Origin.Z+Extent.Z+130.)<.001,TEXT("Mean native ocean surface remains Z=-130cm"));
   OriginalField=Ocean->GetField();OriginalTransform=Volume->GetActorTransform();OriginalExtent=Extent;
   OriginalSequence=Ocean->GetQuerySequence();SampleWitnesses(World,false);ObservedAt=World->GetTimeSeconds();return false;
  }
  if(World->GetTimeSeconds()-ObservedAt<2)return false;
  if(!Volume.IsValid() || !Ocean.IsValid())return Finish(TEXT("Original ocean provider/volume disappeared during normal ticks"));
  Check(Volume->GetActorTransform().Equals(OriginalTransform,.001f) && Volume->Extent.Equals(OriginalExtent,.001f)
   && Volume->OceanSurface==Ocean.Get() && Ocean->GetField()==OriginalField,TEXT("Normal updates retain the same volume, field and FFT world provider"));
  const auto* Site=ANeonCityMainlandSite::Find(World);
  Check(Site && Site->OceanField.Get()==AuthoredField && (!AuthoredField || Ocean->GetField()==AuthoredField),
   TEXT("Normal ticks preserve the optional site configuration and its same provider/registry field"));
  Check(Ocean->GetQuerySequence()>OriginalSequence,TEXT("Original FFT publication continues after the north coverage extension"));
  SampleWitnesses(World,true);
  Check(ObservedLiveWave,TEXT("At least one world-sample comparison observed actual displaced FFT height/normal/velocity, not only flat fallback"));
  return Finish();
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandOceanNorthCoverage923,
 "NeonCity.Phase205c.MainlandC923.Standalone.OceanNorthCoverage",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandOceanNorthCoverage923::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || GUsingNullRHI)
 {AddError(TEXT("OceanNorthCoverage requires isolated native play, real rendering and automatic persistence disabled"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandOceanNorth923::FCoverage(this));return true;
}
#endif
