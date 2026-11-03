#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/NeonCityMainlandSite.h"
#include "Water/CoastalOceanSurface.h"
#include "Water/CoastalWaveData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RHI.h"

namespace MainlandOcean920
{
const FName Parameters[]={TEXT("Riviera915DryLandBounds"),TEXT("Riviera915MarinaNotch"),TEXT("Riviera915RoadConnector")};

bool InRectangle(const FVector2D& Point,const FLinearColor& Rect)
{
 return Point.X>=Rect.R && Point.Y>=Rect.G && Point.X<=Rect.B && Point.Y<=Rect.A;
}

bool RetiredMaskAllows(const FVector2D& Point,const FLinearColor* Rects)
{
 // Exact boolean gate from the retained Riviera915 authored dry-land node.
 return !((InRectangle(Point,Rects[0]) || InRectangle(Point,Rects[2])) && !InRectangle(Point,Rects[1]));
}

/** Read-only inspection of the real runtime ocean, including its normal ticks.
 * No synthetic ocean, site, player pose, material assignment or save mutation. */
class FCoverage : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 double Started=FPlatformTime::Seconds();
 float ObservedAt=-1;
 uint64 FirstSequence=0;
 TWeakObjectPtr<ACoastalOceanSurface> ObservedOcean;
 TWeakObjectPtr<UMaterialInstanceDynamic> ObservedMaterial;
 FLinearColor OceanOrigin,WaterBounds;
public:
 explicit FCoverage(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  auto* World=NeonCityValidationWorld();
  if(FPlatformTime::Seconds()-Started>60)
  {Test->AddError(TEXT("Actual mainland ocean did not complete bounded coverage validation"));return true;}
  if(!World || World->GetTimeSeconds()<8)return false;
  if(ObservedAt>=0 && World->GetTimeSeconds()-ObservedAt<2)return false;
  const auto* Site=ANeonCityMainlandSite::Find(World);
  if(!Test->TestNotNull(TEXT("Actual persistent authored mainland site"),Site))return true;
  if(!Test->TestTrue(TEXT("This authored mainland explicitly replaces only the legacy ocean rectangles"),Site->bReplaceLegacyMainlandOceanCoverage))return true;
  ACoastalOceanSurface* Ocean=nullptr;int32 Count=0;
  for(TActorIterator<ACoastalOceanSurface> It(World);It;++It){Ocean=*It;++Count;}
  if(!Ocean || !Ocean->IsReady())return false;
  if(!Test->TestEqual(TEXT("One original live FFT ocean provider"),Count,1))return true;
  auto* Surface=Ocean->FindComponentByClass<UInstancedStaticMeshComponent>();
  auto* Material=Surface?Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0)):nullptr;
  if(!Test->TestNotNull(TEXT("Existing ocean surface uses its actual runtime MID"),Material))return true;
  if(!Test->TestNotNull(TEXT("Runtime MID retains its shared ocean parent"),Material->Parent.Get()))return true;
  Test->TestEqual(TEXT("Shared ocean parent is retained"),Material->Parent->GetPathName(),
   FString(TEXT("/Game/NeonCity/Art/Immersion/Ocean/M_TidewaterOcean.M_TidewaterOcean")));
  FLinearColor Actual[3],Legacy[3];
  const FLinearColor OriginalRectangles[]={FLinearColor(-36000,-2529.999755859375,-6000.99951171875,15000.0029296875),
   FLinearColor(-23300,-3000,-22700,-1800),FLinearColor(-6000.99951171875,-640,-5720,640)};
  for(int32 I=0;I<UE_ARRAY_COUNT(Parameters);++I)
  {
   const bool bActual=Material->GetVectorParameterValue(FMaterialParameterInfo(Parameters[I]),Actual[I]);
   const bool bLegacy=Material->Parent->GetVectorParameterValue(FMaterialParameterInfo(Parameters[I]),Legacy[I]);
   Test->TestTrue(FString::Printf(TEXT("Actual and original parent expose %s"),*Parameters[I].ToString()),bActual && bLegacy);
   if(!bActual || !bLegacy)return true;
   Test->TestTrue(FString::Printf(TEXT("Actual MID disables only retired rectangle %s: %s"),*Parameters[I].ToString(),*Actual[I].ToString()),
    Actual[I].Equals(FLinearColor(1,1,-1,-1),.00001f));
   Test->TestTrue(FString::Printf(TEXT("Shared parent legacy parameter remains unchanged %s: %s"),*Parameters[I].ToString(),*Legacy[I].ToString()),
    Legacy[I].Equals(OriginalRectangles[I],.1f));
  }
  FLinearColor CurrentOrigin,CurrentBounds;
  const bool bOrigin=Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanOrigin")),CurrentOrigin);
  const bool bBounds=Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("WaterBounds")),CurrentBounds);
  const auto* Field=Ocean->GetField();
  Test->TestTrue(TEXT("Island source origin matches the shared coastal layout"),bOrigin && CurrentOrigin.Equals(FLinearColor(float(NeonCityCoastalLayout::IslandOrigin.X),float(NeonCityCoastalLayout::IslandOrigin.Y),float(NeonCityCoastalLayout::IslandOrigin.Z),0),.001f));
  Test->TestTrue(TEXT("Original bathymetry bounds remain field-driven"),Field && bBounds && CurrentBounds.Equals(
   FLinearColor(Field->GridMinimum.X,Field->GridMinimum.Y,Field->GridMaximum.X,Field->GridMaximum.Y),.001f));
  Test->TestTrue(TEXT("Actual ocean render grid is visible and has live selected nodes"),Surface->IsVisible() && Ocean->GetSurfaceNodeCount()>0);
  if(ObservedAt<0)
  {
   const FVector2D WetPoints[]={FVector2D(-25000,0),FVector2D(-25000,10000),FVector2D(-10000,14000)};
   for(const FVector2D& Point:WetPoints)
   {
    Test->TestFalse(FString::Printf(TEXT("Original rectangle demonstrably removed west/north water at %s"),*Point.ToString()),RetiredMaskAllows(Point,Legacy));
    Test->TestTrue(FString::Printf(TEXT("Actual MID preserves original coverage at that water point %s"),*Point.ToString()),RetiredMaskAllows(Point,Actual));
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(C920OceanCoverageDryLand),true);
    const bool bHit=World->LineTraceSingleByObjectType(Hit,FVector(Point.X,Point.Y,500),FVector(Point.X,Point.Y,-130),
     FCollisionObjectQueryParams(ECC_WorldStatic),Query);
    Test->TestFalse(FString::Printf(TEXT("Water witness is not above actual new dry land: %s hit%s z%.2f"),
     *Point.ToString(),*GetPathNameSafe(Hit.GetActor()),Hit.ImpactPoint.Z),bHit && Hit.ImpactPoint.Z>-129.9);
   }
   const FVector2D Island(NeonCityCoastalLayout::IslandOrigin);
   Test->TestTrue(TEXT("Canonical island point retains the same unmodified generated-coverage gate"),
    RetiredMaskAllows(Island,Legacy) && RetiredMaskAllows(Island,Actual));
   const auto* DefaultSite=GetDefault<ANeonCityMainlandSite>();
   Test->TestFalse(TEXT("Unconfigured worlds do not opt in"),DefaultSite->bReplaceLegacyMainlandOceanCoverage);
   auto* Unconfigured=UMaterialInstanceDynamic::Create(Material->Parent.Get(),GetTransientPackage());
   DefaultSite->ApplyOceanCoverage(*Unconfigured);
   for(int32 I=0;I<UE_ARRAY_COUNT(Parameters);++I)
   {
    FLinearColor Value;const bool bPresent=Unconfigured->GetVectorParameterValue(FMaterialParameterInfo(Parameters[I]),Value);
    Test->TestTrue(FString::Printf(TEXT("Default site leaves legacy parameter intact: %s"),*Parameters[I].ToString()),bPresent && Value.Equals(Legacy[I],.00001f));
   }
   OceanOrigin=CurrentOrigin;WaterBounds=CurrentBounds;FirstSequence=Ocean->GetQuerySequence();
   ObservedOcean=Ocean;ObservedMaterial=Material;ObservedAt=World->GetTimeSeconds();return false;
  }
  if(World->GetTimeSeconds()-ObservedAt<2)return false;
  Test->TestTrue(TEXT("Normal ocean ticks keep the same runtime provider and MID"),ObservedOcean.Get()==Ocean && ObservedMaterial.Get()==Material);
  Test->TestTrue(TEXT("Original ocean origin and bathymetry stay unchanged across normal updates"),
   CurrentOrigin.Equals(OceanOrigin,.001f) && CurrentBounds.Equals(WaterBounds,.001f));
  Test->TestTrue(TEXT("Existing FFT sample publication continues after the local coverage override"),Ocean->GetQuerySequence()>FirstSequence);
  Test->AddInfo(TEXT("Actual runtime MID coverage validation only; west-shore image is separately retained as fixed-camera evidence. No shared material package or island level is modified."));
  return true;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandOceanCoverage920,
 "NeonCity.Phase205c.MainlandC920.Standalone.OceanCoverage",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandOceanCoverage920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || GUsingNullRHI)
 {AddError(TEXT("OceanCoverage requires isolated normal play, disabled automatic persistence and the real RHI"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandOcean920::FCoverage(this));return true;
}
#endif
