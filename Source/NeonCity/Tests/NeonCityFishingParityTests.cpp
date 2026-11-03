#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Fishing/FishingFightModel.h"
#include "Fishing/FishingEcologyModel.h"
#include <limits>

namespace
{
 TSharedPtr<FJsonObject> Reference(FAutomationTestBase& Test)
 {
  FString Text;TSharedPtr<FJsonObject> Data;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/FishingReference.json")))
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data.IsValid())
  { Test.AddError(TEXT("Missing pinned JavaScript reference evidence"));return nullptr; }
  if(Data->GetStringField(TEXT("commit"))!=TEXT("4811ba48d795197de5621985f404e765c0b7c0ef"))
  { Test.AddError(TEXT("Unexpected Tidewater reference revision"));return nullptr; }
  return Data;
 }
 FString Outcome(ENeonCityFishFightResult Result)
 {
  switch(Result) {
  case ENeonCityFishFightResult::Fighting:return TEXT("fighting");
  case ENeonCityFishFightResult::Caught:return TEXT("caught");
  case ENeonCityFishFightResult::Snapped:return TEXT("snapped");
  case ENeonCityFishFightResult::Escaped:return TEXT("escaped");
  default:return TEXT("invalid"); }
 }
 bool Near(double A,double B) { return FMath::IsFinite(A) && FMath::IsFinite(B) && FMath::Abs(A-B)<1e-7*FMath::Max(1.,FMath::Abs(B)); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityFishingReferenceFight,"NeonCity.Fishing.ReferenceFight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityFishingReferenceFight::RunTest(const FString&)
{
 const auto Data=Reference(*this);if(!Data)return false;
 const auto& Cases=Data->GetArrayField(TEXT("cases"));TestEqual(TEXT("All species, policies, rates and gear comparisons present"),Cases.Num(),112);
 for(const auto& Value:Cases)
 {
  const auto C=Value->AsObject();FNeonCityFishFightConfig Config;
  Config.WeightKg=C->GetNumberField(TEXT("kg"));Config.SpeciesMaximumKg=C->GetNumberField(TEXT("speciesMaxKg"));
  Config.SpeciesFight=C->GetNumberField(TEXT("fight"));Config.SpeciesStaminaSeconds=C->GetNumberField(TEXT("staminaSeconds"));
  Config.LineRatingKg=C->GetNumberField(TEXT("lineKg"));Config.ReelSpeedMps=C->GetNumberField(TEXT("reelSpeed"));Config.DistanceM=C->GetNumberField(TEXT("distance"));
  FNeonCityFishFight Fight;TestTrue(TEXT("Valid source configuration accepted"),Fight.Reset(Config,uint32(C->GetNumberField(TEXT("seed")))));
  const FString Policy=C->GetStringField(TEXT("policy"));const double Dt=C->GetNumberField(TEXT("dt"));
  const FString Label=FString::Printf(TEXT("%s/%s/%.5f/line%.0f"),*C->GetStringField(TEXT("species")),*Policy,Dt,Config.LineRatingKg);
  const int32 Frames=int32(C->GetNumberField(TEXT("frames")));int32 SnapshotIndex=0;
  const auto& Samples=C->GetArrayField(TEXT("snapshots"));
  for(int32 Frame=1;Frame<=Frames;++Frame)
  {
   Fight.Step(Dt,Policy==TEXT("careful")?(Fight.Tension<.68 && Fight.Surge<.6):Policy==TEXT("mash"));
   if(SnapshotIndex<Samples.Num() && int32(Samples[SnapshotIndex]->AsObject()->GetNumberField(TEXT("frame")))==Frame)
   {
    const auto S=Samples[SnapshotIndex++]->AsObject();
    TestTrue(Label+TEXT(" trajectory"),Near(Fight.Tension,S->GetNumberField(TEXT("tension"))) && Near(Fight.DistanceM,S->GetNumberField(TEXT("distance")))
     && Near(Fight.Stamina,S->GetNumberField(TEXT("stamina"))) && Near(Fight.Surge,S->GetNumberField(TEXT("surge"))));
   }
  }
  TestEqual(Label+TEXT(" terminal outcome"),Outcome(Fight.Result),C->GetStringField(TEXT("result")));
  TestTrue(Label+TEXT(" terminal numeric state"),Near(Fight.Elapsed,C->GetNumberField(TEXT("time"))) && Near(Fight.Tension,C->GetNumberField(TEXT("tension")))
   && Near(Fight.DistanceM,C->GetNumberField(TEXT("remainingDistance"))) && Near(Fight.Stamina,C->GetNumberField(TEXT("stamina"))) && Near(Fight.Surge,C->GetNumberField(TEXT("surge"))));
  TestEqual(Label+TEXT(" all trajectory probes visited"),SnapshotIndex,Samples.Num());
 }
 FNeonCityFishFight Fight;FNeonCityFishFightConfig Config;Fight.Reset(Config,1);
 Fight.Step(std::numeric_limits<double>::quiet_NaN(),true);Fight.Step(-1,true);
 TestEqual(TEXT("Invalid elapsed time cannot advance a catch"),Fight.Elapsed,0.);
 Fight.Step(1000,true);TestTrue(TEXT("A hitch is capped at one reaction-safe step"),Near(Fight.Elapsed,.1) && Fight.Result==ENeonCityFishFightResult::Fighting);
 Config.LineRatingKg=0;TestFalse(TEXT("Invalid gear fails safely"),Fight.Reset(Config,1));
 TestTrue(TEXT("Invalid session cannot award catch"),Fight.Step(1,true)==ENeonCityFishFightResult::Invalid);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityFishingReferenceEcology,"NeonCity.Fishing.ReferenceEcology",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityFishingReferenceEcology::RunTest(const FString&)
{
 using namespace NeonCityFishingEcology;
 const auto Data=Reference(*this);if(!Data)return false;
 for(const auto& Value:Data->GetArrayField(TEXT("habitats")))
 {
  const auto C=Value->AsObject(),W=C->GetObjectField(TEXT("weights"));
  const auto H=HabitatAt(C->GetNumberField(TEXT("depth")),C->GetNumberField(TEXT("reefDist")),C->GetNumberField(TEXT("pierDist")));
  TestTrue(TEXT("Habitat overlap matches JS"),Near(H.Shallows,W->GetNumberField(TEXT("shallows"))) && Near(H.Reef,W->GetNumberField(TEXT("reef")))
   && Near(H.Pier,W->GetNumberField(TEXT("pier"))) && Near(H.Bay,W->GetNumberField(TEXT("bay"))) && Near(H.Deep,W->GetNumberField(TEXT("deep"))));
  const double Delay=BiteDelay(H,C->GetNumberField(TEXT("hour")),C->GetNumberField(TEXT("random")));
  if(C->HasTypedField<EJson::Null>(TEXT("delay")))TestFalse(TEXT("Dry ground never schedules a bite"),FMath::IsFinite(Delay));
  else TestTrue(TEXT("Bite timing matches JS"),Near(Delay,C->GetNumberField(TEXT("delay"))));
 }
 for(const auto& Value:Data->GetArrayField(TEXT("activities")))
 {
  const auto C=Value->AsObject();const auto P=C->GetStringField(TEXT("pref"));
  const auto Preference=P==TEXT("day")?ENeonCityFishActivity::Day:P==TEXT("night")?ENeonCityFishActivity::Night:P==TEXT("dawnDusk")?ENeonCityFishActivity::DawnDusk:ENeonCityFishActivity::Any;
  TestTrue(TEXT("Day/night activity matches JS"),Near(Activity(Preference,C->GetNumberField(TEXT("hour"))),C->GetNumberField(TEXT("value"))));
 }
 for(const auto& Value:Data->GetArrayField(TEXT("biology")))
 {
  const auto C=Value->AsObject();const double Min=C->GetNumberField(TEXT("minKg")),Max=C->GetNumberField(TEXT("maxKg")),Kg=C->GetNumberField(TEXT("kg"));
  TestTrue(TEXT("Skewed weight roll matches JS"),Near(RollWeight(Min,Max,C->GetNumberField(TEXT("random"))),Kg));
  TestTrue(TEXT("Length-weight biology matches JS"),Near(LengthCm(Kg,C->GetNumberField(TEXT("a")),C->GetNumberField(TEXT("b"))),C->GetNumberField(TEXT("cm"))));
  TestEqual(TEXT("Sale rounding and trophy bonus match JS"),SaleValue(Kg,Min,Max,C->GetNumberField(TEXT("price"))),int64(C->GetNumberField(TEXT("value"))));
 }
 TestEqual(TEXT("Invalid water data has no ecology"),HabitatAt(std::numeric_limits<double>::quiet_NaN(),0,0).Richness(),0.);
 TestEqual(TEXT("Invalid fish has no payout"),SaleValue(-1,1,2,20),int64(0));
 TestEqual(TEXT("Invalid biology has no length"),LengthCm(1,0,3),0.);
 TestTrue(TEXT("Clock wraps safely"),Near(Activity(ENeonCityFishActivity::Night,-1),Activity(ENeonCityFishActivity::Night,23)));
 return true;
}
#endif
