#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Adventure/IslandTennisRules.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEstateTennisRulesTest,"NeonCity.Phase205c.Estate.TennisRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEstateTennisRulesTest::RunTest(const FString&)
{
 FIslandTennisRules R;
 TestTrue(TEXT("First serve begins behind deuce baseline"),R.CanServeFrom(FVector(-1280,-140,100)));
 TestFalse(TEXT("Wrong server half is rejected"),R.CanServeFrom(FVector(-1280,140,100)));
 TestFalse(TEXT("Return cannot create a rally"),R.Strike(0));
 TestTrue(TEXT("Player serves"),R.Strike(0,true));
 TestFalse(TEXT("No double strike"),R.Strike(0));
 TestTrue(TEXT("Deuce service box landing"),R.Bounce(FVector(420,180,3)));
 TestTrue(TEXT("Opponent returns before second bounce"),R.Strike(1));
 TestTrue(TEXT("Player side valid return bounce"),R.Bounce(FVector(-700,-280,3)));
 R.Bounce(FVector(-900,-300,3));TestEqual(TEXT("Missed return point is awarded to AI"),R.Points[1],1);
 TestTrue(TEXT("First fault starts at zero"),R.Strike(0,true));R.Bounce(FVector(900,180,3));
 TestEqual(TEXT("Long service is a first fault, no point"),R.Faults,1);TestEqual(TEXT("No winner on first fault"),R.Winner,-1);
 TestTrue(TEXT("First fault keeps the same ad serving half"),R.CanServeFrom(FVector(-1280,140,100)));
 R.Strike(0,true);R.Net();TestEqual(TEXT("Double fault awards receiver"),R.Points[1],2);
 R.Reset();
 for(int I=0;I<3;++I){R.Strike(0,true);R.Bounce(FVector(500,0,3));R.Bounce(FVector(700,0,3));R.Strike(1,true);R.Bounce(FVector(-500,0,3));R.Bounce(FVector(-700,0,3));}
 TestTrue(TEXT("40 all displays deuce"),R.Score().Contains(TEXT("Deuce")));
 R.Strike(0,true);R.Bounce(FVector(500,0,3));R.Bounce(FVector(700,0,3));
 TestTrue(TEXT("Single point is advantage, not game"),R.Score().Contains(TEXT("Advantage YOU")));TestEqual(TEXT("No premature game"),R.Games[0],0);
 R.Strike(0,true);R.Bounce(FVector(500,0,3));R.Bounce(FVector(700,0,3));
 TestEqual(TEXT("Two clear points win a game"),R.Games[0],1);TestEqual(TEXT("New game resets points"),R.Points[0],0);
 R.Strike(0,true);TestTrue(TEXT("Ball touching singles sideline is in"),R.Bounce(FVector(500,414,3)));
 R.Strike(1);R.Bounce(FVector(-500,416,3));TestEqual(TEXT("Outside singles sideline awards other player"),R.Winner,0);
 return true;
}
#endif
