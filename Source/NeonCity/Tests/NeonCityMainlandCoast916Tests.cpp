#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"

namespace MainlandCoast916
{
class FCollision : public IAutomationLatentCommand
{
public: explicit FCollision(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  UWorld* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<6)return false;
  FString Manifest=FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/Mainland916/CoastFinish37/coast.json");
  FParse::Value(FCommandLine::Get(),TEXT("NeonCityCoastManifest="),Manifest);
  FString Text;TSharedPtr<FJsonObject> Root;
  if(!FFileHelper::LoadFileToString(Text,*Manifest) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid())
  {Test->AddError(TEXT("Measured coast manifest missing"));return true;}
  TArray<AStaticMeshActor*> Coast;
  for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Mainland916.CoastFinish36")))Coast.Add(*I);
  Test->TestEqual(TEXT("Both measured additive meshes exist"),Coast.Num(),2);
  FCollisionQueryParams Query(SCENE_QUERY_STAT(MainlandCoast916),true);
  for(TActorIterator<APawn> I(W);I;++I)Query.AddIgnoredActor(*I);
  int32 Floors=0,Walls=0;
  for(const auto& Value:Root->GetArrayField(TEXT("floor_checks")))
  {
   const auto Q=Value->AsArray();const FVector P(Q[0]->AsNumber()*100,Q[1]->AsNumber()*100,Q[2]->AsNumber()*100);FHitResult H;
   const bool Floor=W->LineTraceSingleByChannel(H,P+FVector(0,0,35),P-FVector(0,0,35),ECC_WorldStatic,Query);
   if(Floor && H.GetActor() && H.GetActor()->ActorHasTag(TEXT("NeonCity.Mainland916.CoastFinish36")) && FMath::Abs(H.ImpactPoint.Z-P.Z)<3)++Floors;
   else Test->AddError(FString::Printf(TEXT("Actual waterfront support missing at %s hit%s"),*P.ToString(),*GetNameSafe(H.GetActor())));
  }
  const auto Stations=Root->GetArrayField(TEXT("inlet_stations"));
  for(int32 Index=0;Index+1<Stations.Num();++Index)
  {
   const auto A=Stations[Index]->AsObject(),B=Stations[Index+1]->AsObject();
   const auto AL=A->GetArrayField(TEXT("land")),BL=B->GetArrayField(TEXT("land")),AW=A->GetArrayField(TEXT("water")),BW=B->GetArrayField(TEXT("water"));
   const float Z=(A->GetNumberField(TEXT("height"))+B->GetNumberField(TEXT("height")))*50;
   const FVector Land((AL[0]->AsNumber()+BL[0]->AsNumber())*50,(AL[1]->AsNumber()+BL[1]->AsNumber())*50,Z);
   const FVector Water((AW[0]->AsNumber()+BW[0]->AsNumber())*50,(AW[1]->AsNumber()+BW[1]->AsNumber())*50,Z),N=(Land-Water).GetSafeNormal2D();
   const FVector Height(0,0,70);FHitResult H;
   if(W->LineTraceSingleByChannel(H,Water+N*150+Height,Water-N*150+Height,ECC_Pawn,Query) && H.GetActor() && H.GetActor()->ActorHasTag(TEXT("NeonCity.Mainland916.CoastFinish36")))++Walls;
   else Test->AddError(FString::Printf(TEXT("Visible waterfront wall has no pawn collision at %s"),*Water.ToString()));
  }
  Test->TestTrue(TEXT("Dense measured walkway and wall probes exist"),Floors>300 && Walls>300);
  // Check only the added components against the already accepted carriageways;
  // this bounded repro does not repeat the continuous drive or change its cache.
  int32 RoadProbes=0,Intrusions=0;
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
  {
   if(!I->RoadID.ToString().StartsWith(TEXT("Mainland916.Road.")))continue;
   auto* S=I->GetRoadSpline();
   for(float D=0;D<=S->GetSplineLength();D+=400)
   {
    const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World),N=S->GetRightVectorAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
    for(float Lane:{-200.f,200.f})for(auto* A:Coast)
    {
     ++RoadProbes;FHitResult H;const FVector Q=P+N*Lane;
     if(A->GetStaticMeshComponent()->LineTraceComponent(H,Q+FVector(0,0,100),Q-FVector(0,0,84),Query))++Intrusions;
    }
   }
  }
  const auto Mouth=Root->GetArrayField(TEXT("mouth_midpoint"));
  const FVector Centre(Mouth[0]->AsNumber()*100,Mouth[1]->AsNumber()*100,70);
  int32 MouthBlocks=0;
  for(float X:{-900.f,0.f,900.f})for(auto* A:Coast)
  {
   FHitResult H;const FVector Q=Centre+FVector(X,0,0);
   if(A->GetStaticMeshComponent()->LineTraceComponent(H,Q+FVector(0,-1800,0),Q+FVector(0,800,0),Query))++MouthBlocks;
  }
  Test->TestEqual(TEXT("New geometry leaves both road lanes unchanged"),Intrusions,0);
  Test->TestEqual(TEXT("Three real rays retain an open water entrance"),MouthBlocks,0);
  Test->AddInfo(FString::Printf(TEXT("Measured waterfront floors%d walls%d, new-component road probes%d intrusions%d, mouth blocks%d"),Floors,Walls,RoadProbes,Intrusions,MouthBlocks));
  return true;
 }
private:FAutomationTestBase* Test;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandCoastCollision916,"NeonCity.Phase205c.Mainland916.Standalone.CoastCollisionRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandCoastCollision916::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandCoast916::FCollision(this));return true;}
#endif
