#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Navigation/NeonCityRoadGuide.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClosedRoadPorts798,"NeonCity.Navigation.ClosedRoadPorts798",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FClosedRoadPorts798::RunTest(const FString&)
{
 FNeonCityRoadGuide Guide;FNeonCityGuideRoad Loop;Loop.RoadID=TEXT("Loop");Loop.Closed=Loop.Bidirectional=true;Loop.Points={FVector(0,0,0),FVector(10000,0,0),FVector(10000,10000,0),FVector(0,10000,0)};Guide.AddRoad(Loop);
 FNeonCityGuideRoad Main;Main.RoadID=TEXT("Main");Main.Bidirectional=Main.NativeMeasured=true;Main.Points={FVector(-10000,5000,0),FVector(0,5000,0)};Guide.AddRoad(Main);
 FNeonCityGuideRoad Villa;Villa.RoadID=TEXT("Villa");Villa.Bidirectional=Villa.NativeMeasured=true;Villa.Points={FVector(10000,5000,0),FVector(14000,5000,0)};Guide.AddRoad(Villa);
 TArray<FVector> Route;float Limit=0;bool Closed=false;const FVector Home(13500,5000,0),Village(-9500,5000,0);
 TestTrue(TEXT("Main reaches an interior loop port and a different private port"),Guide.FindDriveRoute(Village,FVector::ForwardVector,&Home,Route,Limit,Closed));
 TestTrue(TEXT("Destination remains the real private road projection"),!Route.IsEmpty() && Route.Last().Equals(Home,1));
 for(const FVector& P:Route)TestTrue(TEXT("Connected graph retains perimeter; no chord through buildings"),P.X<=1 || P.X>=9999 || P.Y<=1 || P.Y>=9999);
 TestTrue(TEXT("Private driveway can return through the closed-road junctions"),Guide.FindDriveRoute(Home,-FVector::ForwardVector,&Village,Route,Limit,Closed));
 TestTrue(TEXT("Return reaches actual main road"),!Route.IsEmpty() && Route.Last().Equals(Village,1));
 const FVector Origin(4000,0,0);TestTrue(TEXT("Unassigned cruise still follows a connected cycle"),Guide.FindDriveRoute(Origin,FVector::ForwardVector,nullptr,Route,Limit,Closed));
 TestTrue(TEXT("Split loop returns to its original projection"),Closed && !Route.IsEmpty() && Route.Last().Equals(Route[0],1));
 const FVector Behind(2000,0,0);TestTrue(TEXT("Forward cycle reaches a goal behind the starting projection"),Guide.FindDriveRoute(Origin,FVector::ForwardVector,&Behind,Route,Limit,Closed));TestTrue(TEXT("Forward return completes actual perimeter"),Route.Num()>4 && Route.Last().Equals(Behind,1) && Route.ContainsByPredicate([](const FVector& P){return P.Y>9999;}));
 const FVector ReverseBehind(6000,0,0);TestTrue(TEXT("Reverse cycle reaches a goal behind its starting projection"),Guide.FindDriveRoute(Origin,-FVector::ForwardVector,&ReverseBehind,Route,Limit,Closed));TestTrue(TEXT("Reverse return completes actual perimeter"),Route.Num()>4 && Route.Last().Equals(ReverseBehind,1) && Route.ContainsByPredicate([](const FVector& P){return P.Y>9999;}));
 FNeonCityGuideRoad Overpass;Overpass.RoadID=TEXT("Elevated");Overpass.Bidirectional=Overpass.NativeMeasured=true;Overpass.Points={FVector(5000,10000,600),FVector(5000,15000,600)};Guide.AddRoad(Overpass);const FVector High(5000,14000,600);
 TestFalse(TEXT("Elevated nearby connector does not invent a crossing"),Guide.FindDriveRoute(Village,FVector::ForwardVector,&High,Route,Limit,Closed));
 return true;
}
#endif
