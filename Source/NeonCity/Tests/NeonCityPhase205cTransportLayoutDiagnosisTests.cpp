#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandGarageDoor.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandActivityJob.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/StaticMeshComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
namespace TransportLayoutDiagnosis
{
class FInspect:public IAutomationLatentCommand
{
public:
 FInspect(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("TransportLayout");IFileManager::Get().MakeDirectory(*Dir,true);
  auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();Nav->RefreshRoadGuide();FString Roads=TEXT("id,closed,bidirectional,cruise,index,x,y,z\n"),All;
  for(const auto& R:Nav->GetRoadGuide().GetRoads())for(int I=0;I<R.Points.Num();++I)Roads+=FString::Printf(TEXT("%s,%d,%d,%d,%d,%.3f,%.3f,%.3f\n"),*R.RoadID.ToString(),R.Closed,R.Bidirectional,R.CruiseEligible,I,R.Points[I].X,R.Points[I].Y,R.Points[I].Z);
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)All+=I->GetName()+TEXT(" ")+I->RoadID.ToString()+TEXT("\n");
  FString Routes=TEXT("car,activity,route_found,car_x,car_y,car_z,goal_x,goal_y,goal_z\n");for(TActorIterator<AIslandSupercar> C(W);C;++C)for(TActorIterator<AIslandActivityPoint> P(W);P;++P)if(P->Index==0 && P->Job && (P->Job->Kind==ENeonCityIslandActivity::Taxi || P->Job->Kind==ENeonCityIslandActivity::Delivery))
  {TArray<FVector> Path;const FVector A=C->GetActorLocation(),B=P->GetActorLocation();Routes+=FString::Printf(TEXT("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),*C->GetName(),*P->Job->ActivityID.ToString(),Nav->GetRoadGuide().FindRoute(A,B,Path),A.X,A.Y,A.Z,B.X,B.Y,B.Z);}
  FString Blocks=TEXT("fraction,leaf,kind,actor,component,asset,x,y,z\n");int Doors=0;
  for(TActorIterator<AIslandGarageDoor> D(W);D;++D)
  {
   ++Doors;FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageExactGuardDiagnosis),false,*D);
   for(const float Fraction:{1.f,.995f,.9f,.5f,0.f})for(int I=0;I<D->Panels.Num();++I)
   {
    FVector P;FRotator R;D->PanelPose(I,Fraction,P,R);const FTransform T=FTransform(R,P)*D->GetActorTransform();const FVector Half(D->Width*.5f-.2f,2.8f,D->Height/20.f-.2f);FHitResult H;
    if(W->SweepSingleByChannel(H,D->Panels[I]->GetComponentLocation(),T.GetLocation(),T.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(Half),Q))Blocks+=FString::Printf(TEXT("%.3f,%d,sweep,%s,%s,%s,%.3f,%.3f,%.3f\n"),Fraction,I,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.GetComponent()->GetDetailedInfo(),T.GetLocation().X,T.GetLocation().Y,T.GetLocation().Z);
    TArray<FOverlapResult> O;W->OverlapMultiByChannel(O,T.GetLocation(),T.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(Half),Q);for(const auto& Overlap:O)if(Overlap.bBlockingHit)Blocks+=FString::Printf(TEXT("%.3f,%d,end,%s,%s,%s,%.3f,%.3f,%.3f\n"),Fraction,I,*GetNameSafe(Overlap.GetActor()),*GetNameSafe(Overlap.GetComponent()),*Overlap.GetComponent()->GetDetailedInfo(),T.GetLocation().X,T.GetLocation().Y,T.GetLocation().Z);
   }
  }
  FFileHelper::SaveStringToFile(Roads,*(Dir/TEXT("cached-roads.csv")));FFileHelper::SaveStringToFile(All,*(Dir/TEXT("all-road-actors.txt")));FFileHelper::SaveStringToFile(Routes,*(Dir/TEXT("existing-car-routes.csv")));FFileHelper::SaveStringToFile(Blocks,*(Dir/TEXT("garage-exact-guard.csv")));Test->TestTrue(TEXT("Read-only exact native diagnosis captured; no gameplay PASS"),Doors==1);return true;
 }
 FAutomationTestBase* Test;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransportLayoutDiagnosis,"NeonCity.Phase205c.TransportLayoutDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransportLayoutDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TransportLayoutDiagnosis::FInspect(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
