#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Navigation/NeonCityRoadGuide.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeasuredInteriorJunction,"NeonCity.Phase205c.Cruise.MeasuredInteriorJunction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMeasuredInteriorJunction::RunTest(const FString&)
{
 auto Graph=[](double Height)
 {
  FNeonCityRoadGuide G;FNeonCityGuideRoad Main;Main.RoadID=TEXT("Main");Main.Bidirectional=true;
  Main.Points={FVector(0,0,0),FVector(10000,0,0)};G.AddRoad(Main);
  FNeonCityGuideRoad Park;Park.RoadID=TEXT("MeasuredParking");Park.Bidirectional=true;Park.NativeMeasured=true;
  Park.Points={FVector(5000,-800,Height),FVector(5000,0,Height)};G.AddRoad(Park);return G;
 };
 TArray<FVector> Path;float Limit=0;bool Loop=false;
 auto G=Graph(0);const FVector East(9500,0,0),Park(5000,-800,0);
 TestTrue(TEXT("Measured spur joins the actual main-road interior"),G.FindDriveRoute(Park,FVector(0,1,0),&East,Path,Limit,Loop));
 TestTrue(TEXT("Route actually includes its surveyed junction"),Path.ContainsByPredicate([](const FVector& P){return P.Equals(FVector(5000,0,0),.01);}));
 TestTrue(TEXT("Main continuation across virtual fragments remains connected"),G.FindDriveRoute(FVector(1000,0,0),FVector::ForwardVector,&East,Path,Limit,Loop));
 TestTrue(TEXT("A physical approach can branch back to parking"),G.FindDriveRoute(FVector(1000,0,0),FVector::ForwardVector,&Park,Path,Limit,Loop));
 TestEqual(TEXT("Original actor-road identity/count retained"),G.GetRoads().Num(),2);
 TestEqual(TEXT("Original main samples are never rewritten"),G.GetRoads()[0].Points.Num(),2);
 TestTrue(TEXT("Original main endpoint retained exactly"),G.GetRoads()[0].Points.Last()==FVector(10000,0,0));
 auto Elevated=Graph(120);const FVector HighPark(5000,-800,120);
 TestFalse(TEXT("Nearby elevated spur never joins the lower hairpin"),Elevated.FindDriveRoute(HighPark,FVector(0,1,0),&East,Path,Limit,Loop));
 return true;
}
#endif
