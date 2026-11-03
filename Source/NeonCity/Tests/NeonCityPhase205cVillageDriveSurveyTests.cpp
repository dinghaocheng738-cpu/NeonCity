#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandActivityJob.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"
namespace VillageDriveSurvey
{
// Read-only survey of existing land. This never grants a playable-road PASS.
bool Plan(UWorld* W,FVector From,FVector Goal,TArray<FVector>& Path,FString& Failure)
{
 constexpr float Cell=150;const FIntPoint End(FMath::RoundToInt((Goal.X-From.X)/Cell),FMath::RoundToInt((Goal.Y-From.Y)/Cell));
 struct FNode{FVector P;FIntPoint Parent;float Cost=BIG_NUMBER;bool Probed=false,Valid=false;};TMap<FIntPoint,FNode> Nodes;TArray<FIntPoint> Open;TSet<FIntPoint> Closed;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(VillageActualVehicleCorridor),false);if(auto* PC=W->GetFirstPlayerController())Q.AddIgnoredActor(PC->GetPawn());for(TActorIterator<AIslandSupercar> C(W);C;++C)Q.AddIgnoredActor(*C);
 auto Probe=[&](FIntPoint K)
 {
  auto& N=Nodes.FindOrAdd(K);if(N.Probed)return N.Valid;N.Probed=true;const FVector XY=K==End?Goal:From+FVector(K.X*Cell,K.Y*Cell,0);float Low=BIG_NUMBER,High=-BIG_NUMBER;
  for(const auto Offset:{FVector::ZeroVector,FVector(115,0,0),FVector(-115,0,0),FVector(0,115,0),FVector(0,-115,0)})
  {FHitResult H;const FVector At=XY+Offset;if(!W->LineTraceSingleByChannel(H,FVector(At.X,At.Y,2500),FVector(At.X,At.Y,-600),ECC_Vehicle,Q) || H.ImpactNormal.Z<.94)return false;Low=FMath::Min(Low,float(H.ImpactPoint.Z));High=FMath::Max(High,float(H.ImpactPoint.Z));}
  if(High-Low>65)return false;float Water=0;FVector Current;if(W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(FVector(XY.X,XY.Y,Low),Water,Current) && Low<Water+15)return false;
  N.P=FVector(XY.X,XY.Y,High);N.Valid=!W->OverlapBlockingTestByChannel(N.P+FVector(0,0,85),FQuat::Identity,ECC_Vehicle,FCollisionShape::MakeBox(FVector(125,125,65)),Q);return N.Valid;
 };
 const FIntPoint Start(0,0);if(!Probe(Start)||!Probe(End)){Failure=FString::Printf(TEXT("Actual endpoint lacks supported vehicle-width land or is obstructed: startValid%d%s endValid%d%s"),Nodes[Start].Valid,*From.ToString(),Nodes.FindRef(End).Valid,*Goal.ToString());return false;}Nodes[Start].Cost=0;Open.Add(Start);
 int Count=0;while(!Open.IsEmpty() && ++Count<12000)
 {
  int Best=0;float Score=BIG_NUMBER;for(int I=0;I<Open.Num();++I){const auto K=Open[I];const float S=Nodes[K].Cost+FVector2D(K.X-End.X,K.Y-End.Y).Size()*Cell;if(S<Score){Score=S;Best=I;}}
  const FIntPoint K=Open[Best];Open.RemoveAtSwap(Best);if(Closed.Contains(K))continue;Closed.Add(K);
  if(K==End){FIntPoint At=K;while(At!=Start){Path.Add(Nodes[At].P);At=Nodes[At].Parent;}Path.Add(Nodes[Start].P);Algo::Reverse(Path);return true;}
  const FVector Position=Nodes[K].P;const float Cost=Nodes[K].Cost;
  for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
  {
   if(!X&&!Y)continue;const FIntPoint Next(K.X+X,K.Y+Y);if(Next.X<FMath::Min(0,End.X)-16 || Next.X>FMath::Max(0,End.X)+16 || Next.Y<FMath::Min(0,End.Y)-16 || Next.Y>FMath::Max(0,End.Y)+16 || Closed.Contains(Next) || !Probe(Next))continue;
   const FVector To=Nodes[Next].P;const float Length=FVector::Dist2D(Position,To);if(FMath::Abs(To.Z-Position.Z)>Length*.20)continue;FHitResult H;
   if(W->SweepSingleByChannel(H,Position+FVector(0,0,85),To+FVector(0,0,85),FQuat::Identity,ECC_Vehicle,FCollisionShape::MakeBox(FVector(125,125,65)),Q))continue;
   const float New=Cost+Length;if(New<Nodes[Next].Cost){Nodes[Next].Cost=New;Nodes[Next].Parent=K;Open.AddUnique(Next);}
  }
 }
 Failure=FString::Printf(TEXT("No conservative existing-land corridor; explored%d nodes"),Count);return false;
}
class FInspect:public IAutomationLatentCommand
{
public:FInspect(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("Survey initialization timeout"));return true;}if(!PC || W->GetTimeSeconds()<4)return false;auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-160000,500))))return false;
  auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav){Test->AddError(TEXT("Actual navigation component missing"));return true;}Nav->RefreshRoadGuide();FVector Mountain=FVector::ZeroVector;bool Found=false;for(const auto& Road:Nav->GetRoadGuide().GetRoads())if(Road.RoadID==TEXT("Island.Mountain.Main") && !Road.Points.IsEmpty()){Mountain=Road.Points[0];Found=true;}if(!Found){Test->AddError(TEXT("Actual mountain graph missing"));return true;}
  AIslandActivityPoint* Taxi=nullptr;AIslandActivityPoint* Delivery=nullptr;AIslandActivityPoint* TaxiEnd=nullptr;AIslandActivityPoint* DeliveryEnd=nullptr;
  for(TActorIterator<AIslandActivityPoint> P(W);P;++P)if(P->Job){if(P->Job->Kind==ENeonCityIslandActivity::Taxi){if(P->Index==0)Taxi=*P;else TaxiEnd=*P;}if(P->Job->Kind==ENeonCityIslandActivity::Delivery){if(P->Index==0)Delivery=*P;else DeliveryEnd=*P;}}
  if(!Taxi || !Delivery || !TaxiEnd || !DeliveryEnd){Test->AddError(TEXT("Actual job endpoints missing"));return true;}
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("VillageDriveSurvey");IFileManager::Get().MakeDirectory(*Dir,true);
  const FVector TP=Taxi->GetActorLocation()+FVector(0,-300,0),DP=Delivery->GetActorLocation()+FVector(0,-300,0),TE=TaxiEnd->GetActorLocation()+FVector(0,-300,0),DE=DeliveryEnd->GetActorLocation()+FVector(0,-300,0);
  FVector Froms[]={Mountain,TP,TP,TE};const FVector Goals[]={TP,DP,TE,DE};const TCHAR* Names[]={TEXT("Mountain-Taxi"),TEXT("Taxi-Delivery"),TEXT("Taxi-Dropoff"),TEXT("Dropoff-Clerk")};FString Receipt=TEXT("READ ONLY LAND SURVEY; not road, driving, navigation or gameplay acceptance. Vehicles ignored only in survey geometry. No poses, collision or graph written. Actual Vehicle collision channel, body above highest native support; conservative230cm cross support, normal>=.94, grade<=20percent, real125cm horizontal clearance.\n");
  for(int I=0;I<4;++I){TArray<FVector> Path;FString Failure;bool Pass=Plan(W,Froms[I],Goals[I],Path,Failure);if(!Pass){for(const float Radius:{200.f,350.f}){for(int Angle=0;Angle<360;Angle+=45){const FVector Candidate=Goals[I]+FVector(Radius,0,0).RotateAngleAxis(Angle,FVector::UpVector);if(FVector::Dist2D(Candidate,I==0?Taxi->GetActorLocation():I==1?Delivery->GetActorLocation():I==2?TaxiEnd->GetActorLocation():DeliveryEnd->GetActorLocation())>390)continue;if(Plan(W,Froms[I],Candidate,Path,Failure)){Pass=true;Failure=TEXT("Supported nearby curb candidate; actual driving required");break;}}if(Pass)break;}}if(Pass && I==2)Froms[3]=Path.Last();Receipt+=FString::Printf(TEXT("%s=%d points%d %s\n"),Names[I],Pass,Path.Num(),*Failure);FString CSV=TEXT("x,y,z\n");for(const auto P:Path)CSV+=FString::Printf(TEXT("%.3f,%.3f,%.3f\n"),P.X,P.Y,P.Z);FFileHelper::SaveStringToFile(CSV,*(Dir/(FString(Names[I])+TEXT(".csv"))));}
  FFileHelper::SaveStringToFile(Receipt,*(Dir/TEXT("scope.txt")));Test->TestTrue(TEXT("Read-only survey retained; no driving PASS"),true);return true;
 }
 FAutomationTestBase* Test;double Start=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVillageDriveSurvey,"NeonCity.Phase205c.VillageDriveSurveyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVillageDriveSurvey::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(VillageDriveSurvey::FInspect(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
