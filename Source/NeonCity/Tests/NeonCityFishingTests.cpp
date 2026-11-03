#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Fishing/FishingSessionModel.h"
#include "Fishing/FishData.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityFishingRules,"NeonCity.Fishing.InputRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityFishingRules::RunTest(const FString&)
{
 const auto Advance=[](FNeonCityFishingSession& Session,ENeonCityFishingState Until)
 { for(int32 Frame=0;Frame<400 && Session.State!=Until && !Session.IsTerminal();++Frame)Session.Step(.05f); };
 FNeonCityFishingSession S;S.Reset(17,1);S.Press();S.Step(1000);
 TestTrue(TEXT("Press starts windup and hitch cannot auto-release it"),S.State==ENeonCityFishingState::Windup && S.CastPower<.1f);
 for(int32 I=0;I<23;++I)S.Step(.05f);
 S.Release(true);TestTrue(TEXT("Animation-confirmed timed release enters a perfect cast"),S.State==ENeonCityFishingState::Casting && S.PerfectCast);
 Advance(S,ENeonCityFishingState::Waiting);S.Press();TestTrue(TEXT("Hooking before a bite fails"),S.Failure==ENeonCityFishingFailure::EarlyHook);
 S.Reset(17,1);S.Press();for(int32 I=0;I<60;++I)S.Step(.05f);
 TestTrue(TEXT("Overcharging never throws without releasing input"),S.State==ENeonCityFishingState::Windup && S.CastPower==1);
 S.Release();TestFalse(TEXT("Holding past the timing window loses the accuracy bonus"),S.PerfectCast);
 S.Reset(17,1);S.Press();S.Release();Advance(S,ENeonCityFishingState::Bite);
 for(int32 Frame=0;Frame<60;++Frame)S.Step(.05f);
 TestTrue(TEXT("An unhooked bite escapes"),S.Failure==ENeonCityFishingFailure::MissedBite);
 FNeonCityFishFightConfig Heavy;Heavy.WeightKg=35;Heavy.SpeciesMaximumKg=45;Heavy.SpeciesFight=1;Heavy.SpeciesStaminaSeconds=24;
 S.Reset(17,Heavy);S.Press();S.Release();Advance(S,ENeonCityFishingState::Bite);S.Press();
 for(int32 Frame=0;Frame<3600 && !S.IsTerminal();++Frame)S.Step(.05f);
 TestTrue(TEXT("Holding reel on a fish beyond starter line rating breaks the line"),S.Failure==ENeonCityFishingFailure::BrokenLine);
 for(int32 Seed=1;Seed<=32;++Seed)
 {
  FNeonCityFishFightConfig Gear;Gear.WeightKg=Seed%2?1:12;Gear.SpeciesMaximumKg=14;
  Gear.SpeciesFight=.85;Gear.SpeciesStaminaSeconds=16;Gear.LineRatingKg=Seed%2?7:26;Gear.ReelSpeedMps=Seed%2?1.1:1.6;
  S.Reset(Seed,Gear);S.Press();S.Release();Advance(S,ENeonCityFishingState::Bite);S.Press();
  for(int32 Frame=0;Frame<3600 && !S.IsTerminal();++Frame)
  {
   // Player-readable tension strategy; no progress injection or direct catch call.
   if(S.Tension>=.68f || S.Surge>=.6f)S.Release();else S.Press();S.Step(.05f);
  }
  TestTrue(FString::Printf(TEXT("Controlled reel catches seed %d"),Seed),S.State==ENeonCityFishingState::Caught);
 }
 S.Reset(1,1);S.Press();S.Cancel();S.Press();S.Step(.1f);
 TestTrue(TEXT("Cancellation cannot turn into a catch"),S.State==ENeonCityFishingState::Cancelled);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityFishingSchema,"NeonCity.Fishing.ProfileAndSchema",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityFishingSchema::RunTest(const FString&)
{
 TStrongObjectPtr<UFishData> Profile(NewObject<UFishData>());Profile->SpeciesID=TEXT("Fish.Test");Profile->Name=FText::FromString(TEXT("Test fish"));
 TestTrue(TEXT("A bounded species profile is valid"),Profile->IsValidProfile());
 TestEqual(TEXT("Wrong habitat has zero encounter weight"),Profile->EncounterWeight(TEXT("River"),12,TEXT("Clear")),0.f);
 Profile->TimePreference=ENeonCityFishTime::Night;
 TestTrue(TEXT("Time preference changes encounter weights"),Profile->EncounterWeight(TEXT("Coastal"),23,TEXT("Clear"))>Profile->EncounterWeight(TEXT("Coastal"),12,TEXT("Clear")));
 Profile->WeightKg.X=20;Profile->WeightKg.Y=1;TestFalse(TEXT("Reversed weight bounds rejected"),Profile->IsValidProfile());
 TStrongObjectPtr<UNeonCitySaveGame> Old(NewObject<UNeonCitySaveGame>()),Defaults(NewObject<UNeonCitySaveGame>());
 TArray<FString> Warnings;Defaults->HasFishing=true;Old->FillMissingSections(*Defaults,Warnings);
 TestTrue(TEXT("Old V1 receives explicit empty fish section"),Old->HasFishing && Old->Fishing.Fish.IsEmpty() && Old->Fishing.Meals.IsEmpty());
 FNeonCityFishingSaveData Huge;Huge.Fish.SetNum(129);FString Error;TestFalse(TEXT("Oversized inventory rejected"),UNeonCityFishingSubsystem::ValidateEnvelope(Huge,Error));
 return true;
}
#endif
