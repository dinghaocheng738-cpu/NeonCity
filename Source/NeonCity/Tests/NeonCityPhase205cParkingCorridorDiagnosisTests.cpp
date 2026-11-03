#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Adventure/IslandSupercar.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"

namespace ParkingCorridorDiagnosis
{
FString CSV(FString Value){Value.ReplaceInline(TEXT("\""),TEXT("\"\""));return TEXT("\"")+Value+TEXT("\"");}
FString XYZ(FVector P){return FString::Printf(TEXT("%.3f,%.3f,%.3f"),P.X,P.Y,P.Z);}
FString Blocker(const FHitResult& H){auto* S=Cast<UStaticMeshComponent>(H.GetComponent());return CSV(GetNameSafe(H.GetActor()))+TEXT(",")+CSV(GetNameSafe(H.GetComponent()))+TEXT(",")+CSV(S?GetPathNameSafe(S->GetStaticMesh()):TEXT(""));}
struct FFoot
{
 FVector P=FVector::ZeroVector;float Low=BIG_NUMBER,High=-BIG_NUMBER,Normal=1;bool Valid=false;FString Failure;FHitResult Surface;
};
struct FSurvey
{
 UWorld* W;FCollisionQueryParams Q;
 FSurvey(UWorld* World,AIslandSupercar* Car):W(World),Q(SCENE_QUERY_STAT(ParkingReadOnlyVehicleCorridor),false,Car){}
 FFoot Foot(FVector XY)const
 {
  FFoot F;F.P=XY;
  // Same conservative cross survey as native226. Ignore only this test car;
  // nearby cars, actors and Vehicle-channel collisions remain fully queried.
  for(const auto Offset:{FVector::ZeroVector,FVector(115,0,0),FVector(-115,0,0),FVector(0,115,0),FVector(0,-115,0)})
  {
   const FVector At=XY+Offset;FHitResult H;
   if(!W->LineTraceSingleByChannel(H,FVector(At.X,At.Y,2500),FVector(At.X,At.Y,-600),ECC_Vehicle,Q)){F.Failure=TEXT("Missing Vehicle-channel support");return F;}
   F.Normal=FMath::Min(F.Normal,float(H.ImpactNormal.Z));if(H.ImpactNormal.Z<.94){F.Surface=H;F.Failure=TEXT("Support normal below0.94");return F;}
   F.Low=FMath::Min(F.Low,float(H.ImpactPoint.Z));if(H.ImpactPoint.Z>F.High){F.High=H.ImpactPoint.Z;F.Surface=H;}
  }
  F.P.Z=F.High;if(F.High-F.Low>65){F.Failure=TEXT("Cross support height spread above65cm");return F;}
  float Water=0;FVector Current;auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();if(Marine && Marine->SampleWater(F.P,Water,Current) && F.Low<Water+15){F.Failure=TEXT("Support below water clearance");return F;}
  F.Valid=true;return F;
 }
 bool Body(const FVector& From,const FVector& To,const FVector& Heading,FHitResult& BoxHit,FHitResult& CapsuleHit)const
 {
  const FVector D=Heading.GetSafeNormal();const FQuat BoxRotation=D.Rotation().Quaternion(),CapsuleRotation=FQuat::FindBetweenNormals(FVector::UpVector,D);
  // Conservative complete car length490/width250cm. The capsule is horizontal:
  // length490/width250/height250, bottom20cm above highest measured support.
  // These are survey volumes, never new collision shapes or actor corrections.
  const auto Box=FCollisionShape::MakeBox(FVector(245,125,65)),Capsule=FCollisionShape::MakeCapsule(125,245);
  const bool BoxBlocked=W->SweepSingleByChannel(BoxHit,From+FVector(0,0,85),To+FVector(0,0,85),BoxRotation,ECC_Vehicle,Box,Q);
  const bool CapsuleBlocked=W->SweepSingleByChannel(CapsuleHit,From+FVector(0,0,145),To+FVector(0,0,145),CapsuleRotation,ECC_Vehicle,Capsule,Q);
  return !BoxBlocked && !CapsuleBlocked;
 }
 bool Segment(FVector A,FVector B,TArray<FVector>* Samples,FString* Report,FString& Failure)const
 {
  const float Length=FVector::Dist2D(A,B);if(Length<.01){Failure=TEXT("Zero-length parking chord");return false;}
  // At the permitted20% grade this also keeps3D sample spacing<=25cm.
  const float HorizontalStep=25/FMath::Sqrt(1.04f);const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length/HorizontalStep));const FVector Heading=(B-A).GetSafeNormal2D();FFoot Previous;
  for(int32 Step=0;Step<=Steps;++Step)
  {
   const FVector At=FMath::Lerp(A,B,float(Step)/Steps);const FFoot F=Foot(At);FHitResult Box,Capsule;float Grade=0;bool Bodies=false;
   if(F.Valid){Grade=Step?FMath::Abs(F.P.Z-Previous.P.Z)/FMath::Max(.01f,float(FVector::Dist2D(F.P,Previous.P))):0;const FVector Tangent=Step?F.P-Previous.P:FVector::VectorPlaneProject(Heading,F.Surface.ImpactNormal);Bodies=Body(Step?Previous.P:F.P,F.P,Tangent,Box,Capsule);}
   const bool Valid=F.Valid && Grade<=.20f && Bodies;
   if(Report)*Report+=FString::Printf(TEXT("%d,"),Step)+XYZ(F.P)+FString::Printf(TEXT(",%d,%.3f,%.3f,%.4f,%.4f,%d,"),F.Valid,F.Low,F.High,F.Normal,Grade,Box.bBlockingHit)+Blocker(Box)+FString::Printf(TEXT(",%d,"),Capsule.bBlockingHit)+Blocker(Capsule)+TEXT(",")+Blocker(F.Surface)+TEXT(",")+CSV(Valid?TEXT(""):(!F.Valid?F.Failure:Grade>.20f?TEXT("Grade above20percent"):TEXT("Actual Vehicle body/capsule obstruction")))+TEXT("\n");
   if(!Valid){Failure=!F.Valid?F.Failure:Grade>.20f?TEXT("Grade above20percent"):TEXT("Actual Vehicle body/capsule obstruction");return false;}
   if(Samples && (Samples->IsEmpty() || !Samples->Last().Equals(F.P,.01)))Samples->Add(F.P);Previous=F;
  }
  return true;
 }
 bool Plan(FVector From,FVector Goal,TArray<FVector>& Path,FString& Method,FString& Failure)const
 {
  Method=TEXT("Direct measured corridor");if(Segment(From,Goal,&Path,nullptr,Failure))return true;Path.Reset();const FString DirectFailure=Failure;
  constexpr float Cell=100;const FIntPoint End(FMath::RoundToInt((Goal.X-From.X)/Cell),FMath::RoundToInt((Goal.Y-From.Y)/Cell));
  struct FNode{FFoot Foot;FIntPoint Parent;float Cost=BIG_NUMBER;bool Probed=false;};TMap<FIntPoint,FNode> Nodes;TArray<FIntPoint> Open;TSet<FIntPoint> Closed;
  auto Probe=[&](FIntPoint K){auto& N=Nodes.FindOrAdd(K);if(!N.Probed){N.Probed=true;N.Foot=Foot(K==End?Goal:From+FVector(K.X*Cell,K.Y*Cell,0));}return N.Foot.Valid;};
  const FIntPoint Start(0,0);if(!Probe(Start) || !Probe(End)){Failure=TEXT("Actual parked or Main endpoint support invalid: ")+DirectFailure;return false;}Nodes[Start].Cost=0;Open.Add(Start);int Visited=0;bool Found=false;
  while(!Open.IsEmpty() && ++Visited<=3500)
  {
   int Best=0;float Score=BIG_NUMBER;for(int I=0;I<Open.Num();++I){const auto K=Open[I];const float S=Nodes[K].Cost+FVector2D(K.X-End.X,K.Y-End.Y).Size()*Cell;if(S<Score){Score=S;Best=I;}}
   const FIntPoint K=Open[Best];Open.RemoveAtSwap(Best);if(Closed.Contains(K))continue;Closed.Add(K);if(K==End){Found=true;break;}
   const FVector P=Nodes[K].Foot.P;const float Cost=Nodes[K].Cost;
   for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
   {
    if(!X&&!Y)continue;const FIntPoint Next(K.X+X,K.Y+Y);if(Next.X<FMath::Min(0,End.X)-12 || Next.X>FMath::Max(0,End.X)+12 || Next.Y<FMath::Min(0,End.Y)-12 || Next.Y>FMath::Max(0,End.Y)+12 || Closed.Contains(Next) || !Probe(Next))continue;
    const FVector To=Nodes[Next].Foot.P;FString Why;if(!Segment(P,To,nullptr,nullptr,Why))continue;const float New=Cost+FVector::Dist2D(P,To);if(New<Nodes[Next].Cost){Nodes[Next].Cost=New;Nodes[Next].Parent=K;Open.AddUnique(Next);}
   }
  }
  if(!Found){Failure=FString::Printf(TEXT("Direct:%s; no complete actual Vehicle corridor in%d nodes"),*DirectFailure,Visited);return false;}
  TArray<FVector> Raw;FIntPoint At=End;while(At!=Start){Raw.Add(Nodes[At].Foot.P);At=Nodes[At].Parent;}Raw.Add(Nodes[Start].Foot.P);Algo::Reverse(Raw);
  for(int I=1;I<Raw.Num();++I)if(!Segment(Raw[I-1],Raw[I],&Path,nullptr,Failure)){Path.Reset();return false;}Method=TEXT("Conservative100cm grid; every edge physically probed at<=25cm, no smoothing");return true;
 }
};
class FInspect:public IAutomationLatentCommand
{
public:explicit FInspect(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();UWorld* W=NeonCityValidationWorld();if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("Parking survey stream initialization timeout"));return true;}
  if(!W || W->GetTimeSeconds()<4)return false;auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57000,-153500,700))))return false;
  ANeonCityTrafficRoute* Main=nullptr;for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)if(I->RoadID==TEXT("Island.Mountain.Main"))Main=*I;
  if(!Main || !Main->GetRoadSpline() || Main->GetRoadSpline()->GetNumberOfSplinePoints()<2){Test->AddError(TEXT("Actual Main spline absent"));return true;}
  auto* Spline=Main->GetRoadSpline();TArray<FVector> MainPoints;for(int I=0;I<Spline->GetNumberOfSplinePoints();++I)MainPoints.Add(Spline->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World));
  TArray<AIslandSupercar*> Cars;for(TActorIterator<AIslandSupercar> C(W);C;++C){const FVector P=C->GetActorLocation();if(P.X>54000 && P.X<63000 && P.Y>NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-155000,0)).Y && P.Y<NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-151000,0)).Y)Cars.Add(*C);}Cars.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});
  if(Cars.Num()!=3){Test->AddError(FString::Printf(TEXT("Expected3 authored village cars, found%d; no car fixture was created"),Cars.Num()));return true;}
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("ParkingCorridorSurvey");IFileManager::Get().MakeDirectory(*Dir,true);
  FString Receipt=TEXT("READ ONLY ACTUAL VEHICLE PARKING CORRIDOR SURVEY. No actor/input/collision/road/nav state changed. Ignore only tested car's own existing collision. Highest native Vehicle support sampled across230cm cross width, normal>=.94, height spread<=65cm, grade<=20percent, above water. Full490x250x130cm oriented body box plus horizontal490x250x250cm capsule queried. Every emitted segment/sample<=25cm. Grid heading/corners remain unsmoothed; no road, steering, driving, cruise or job PASS.\n");
  FString Joins=TEXT("car,from_x,from_y,from_z,main_segment,segment_alpha,main_world_x,main_world_y,main_world_z,park_gap_2d,found,method,failure\n");
  for(auto* Car:Cars)
  {
   const FVector From=Car->GetActorLocation();FVector Goal=FVector::ZeroVector;int Segment=INDEX_NONE;double Alpha=0,Best=BIG_NUMBER;
   for(int I=1;I<MainPoints.Num();++I){const FVector A=MainPoints[I-1],AB=MainPoints[I]-A;const double T=FMath::Clamp(FVector::DotProduct(From-A,AB)/FMath::Max(1.,AB.SizeSquared()),0.,1.);const FVector P=A+AB*T;const double D=FVector::DistSquared(From,P);if(D<Best){Best=D;Goal=P;Segment=I-1;Alpha=T;}}
   FSurvey Survey(W,Car);TArray<FVector> Path;FString Method,Failure;const bool Found=Survey.Plan(From,Goal,Path,Method,Failure);
   FString Points=TEXT("x,y,z\n"),Report=TEXT("index,x,y,z,support_valid,lowest_support_z,highest_support_z,min_support_normal_z,grade,box_blocked,box_actor,box_component,box_mesh,capsule_blocked,capsule_actor,capsule_component,capsule_mesh,ground_actor,ground_component,ground_mesh,failure\n");
   if(Found){for(const FVector P:Path)Points+=XYZ(P)+TEXT("\n");for(int I=1;I<Path.Num();++I){FString Why;Survey.Segment(Path[I-1],Path[I],nullptr,&Report,Why);}}
   else {FString Why;Survey.Segment(From,Goal,nullptr,&Report,Why);}
   const FString Name=Car->GetName();FFileHelper::SaveStringToFile(Points,*(Dir/(Name+TEXT("-Main.csv"))));FFileHelper::SaveStringToFile(Report,*(Dir/(Name+TEXT("-support-width.csv"))));
   Joins+=CSV(Name)+TEXT(",")+XYZ(From)+FString::Printf(TEXT(",%d,%.8f,"),Segment,Alpha)+XYZ(Goal)+FString::Printf(TEXT(",%.3f,%d,"),FVector::Dist2D(From,Goal),Found)+CSV(Method)+TEXT(",")+CSV(Failure)+TEXT("\n");Receipt+=FString::Printf(TEXT("%s=%d samples%d gap%.3fcm actualMainSegment%d alpha%.8f method%s failure%s\n"),*Name,Found,Path.Num(),FVector::Dist2D(From,Goal),Segment,Alpha,*Method,*Failure);
  }
  FFileHelper::SaveStringToFile(Joins,*(Dir/TEXT("main-joins.csv")));FFileHelper::SaveStringToFile(Receipt,*(Dir/TEXT("scope.txt")));Test->AddInfo(TEXT("Read-only parking results retained. Corridor found/blocked belongs to each receipt; no driving PASS is granted."));return true;
 }
 FAutomationTestBase* Test;double Start=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParkingCorridorDiagnosis,"NeonCity.Phase205c.ParkingCorridorSurveyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FParkingCorridorDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(ParkingCorridorDiagnosis::FInspect(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParkingCorridorStandaloneDiagnosis,"NeonCity.Phase205c.Estate.Standalone.ParkingCorridorSurvey",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FParkingCorridorStandaloneDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(ParkingCorridorDiagnosis::FInspect(this));return true;}
#endif
