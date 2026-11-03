#include "Misc/AutomationTest.h"
#include "World/NeonCityMainlandSite.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalConvertibleFleet921,
 "NeonCity.Phase205c.MainlandC921.ConvertibleFleetMigration",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoastalConvertibleFleet921::RunTest(const FString&)
{
 const FGuid Mainland(0x0889167F,0x430338D9,0x839BAEB8,0xF5500BB3);
 FNeonCityMainlandFleetReplacement Old;Old.VehicleID=Mainland;Old.PreviousProfile=TEXT("DA_StarterSedan");Old.ReplacementProfile=TEXT("DA_HeroKestrel");
 auto C920=Old;C920.PreviousProfile=TEXT("DA_HeroAster");
 TArray<FNeonCityMainlandFleetReplacement> Rules={Old,C920};FName Result;
 for(const FName Previous:{Old.PreviousProfile,C920.PreviousProfile})
 {
  TestTrue(TEXT("Both historical profiles for the same mainland identity reach the selected convertible"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,Previous,Result));
  TestEqual(TEXT("Historical profile routes agree on the selected convertible"),Result,FName(TEXT("DA_HeroKestrel")));
 }
 TestFalse(TEXT("The same model with an unlisted GUID is untouched"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,FGuid(1,2,3,4),C920.PreviousProfile,Result));
 TestFalse(TEXT("The replacement is not upgraded again"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,Old.ReplacementProfile,Result));
 Rules.Add(C920);TestFalse(TEXT("Identical duplicate GUID/profile pairs fail closed"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,C920.PreviousProfile,Result));
 TestTrue(TEXT("Ambiguity clears the result"),Result.IsNone());
 Rules.Last().ReplacementProfile=TEXT("DA_HeroNacre");TestFalse(TEXT("Conflicting duplicate pairs fail closed"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,C920.PreviousProfile,Result));
 Rules.SetNum(2);Rules[1].ReplacementProfile=TEXT("DA_HeroNacre");TestTrue(TEXT("A listed coupe can select the folding roof car"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,C920.PreviousProfile,Result));
 Rules[1].ReplacementProfile=TEXT("DA_HeroAster");TestFalse(TEXT("A same-profile coupe rule is not a migration"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,C920.PreviousProfile,Result));
 Rules[1].PreviousProfile=TEXT("DA_HeroNacre");Rules[1].ReplacementProfile=TEXT("DA_HeroKestrel");TestFalse(TEXT("Existing folding-roof saves cannot be replaced by another model"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Mainland,Rules[1].PreviousProfile,Result));
 for(const TCHAR* Text:{TEXT("5FF7803645833C8D593E77A50B006245"),TEXT("7F242340469FEA4D1A7D21A5D5DED892"),TEXT("EA3CB5934EC94A1987F7A19A34C2A1D8")})
 {
  FGuid Protected;FGuid::ParseExact(Text,EGuidFormats::Digits,Protected);Rules={C920};Rules[0].VehicleID=Protected;
  TestFalse(TEXT("All three island identities remain protected even from a valid coupe rule"),ANeonCityMainlandSite::ResolveFleetProfileReplacement(Rules,Protected,C920.PreviousProfile,Result));
 }
 return !HasAnyErrors();
}
#endif
