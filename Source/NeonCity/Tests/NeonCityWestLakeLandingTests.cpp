#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/OverlapResult.h"

namespace NeonCityWestLakeLanding
{
/** Landing sweep: the helicopter is set down (engine on, collective down) on a spread of real West Lake land spots
 * around the residence - lawn, paths, roads, kerbs, gentle slopes - and must report Grounded once its skids rest on
 * the surface (that is what lets the pilot shut down and step out). Every spot logs what the ground probe saw. */
class FSweep:public IAutomationLatentCommand
{
public:
 explicit FSweep(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>420){Test->AddError(TEXT("Landing sweep timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  if(!W || W->GetTimeSeconds()<5)return false;
  if(!Heli.IsValid())
  {
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It){Heli=*It;break;}
   if(!Test->TestNotNull(TEXT("a helicopter in the map"),Heli.Get()))return true;
   PickSpots(W);
   Test->AddInfo(FString::Printf(TEXT("%d land spots"),Spots.Num()));
   if(Spots.Num()<6){Test->AddError(TEXT("too few land spots found"));return true;}
  }
  auto* Move=Heli->GetAircraftMovement();auto* Hull=Heli->GetHull();
  if(Index>=Spots.Num())
  {
   Test->TestTrue(FString::Printf(TEXT("grounded on %d of %d land spots"),Landed,Spots.Num()),Landed==Spots.Num());
   return true;
  }
  const double E=FPlatformTime::Seconds()-At;
  if(!bPlaced)
  {
   const FVector S=Spots[Index];
   Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Heli->SetActorLocationAndRotation(S+FVector(0,0,350),FRotator(0,float(Index*37%360),0),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StartEngine();bPlaced=true;At=FPlatformTime::Seconds();Rest=0;return false;
  }
  Move->SetInput(FVector(0,0,-1),0);
  const bool bStill=Hull->GetPhysicsLinearVelocity().Size()<15;
  Rest=bStill?Rest+W->GetDeltaSeconds():0;
  if(Move->IsGrounded() && E>1){++Landed;Report(true,E);Next();return false;}
  if(E<12 && Rest<3)return false;
  Report(false,E);Next();return false;
 }
private:
 void PickSpots(UWorld* W)
 {
  // a 400 m grid around the helicopter's pad; keep flat-ish dry land a helicopter could reasonably set down on
  const FVector C=Heli->GetActorLocation();auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();
  FCollisionQueryParams Q(SCENE_QUERY_STAT(LandingSweep),false,Heli.Get());
  for(int32 I=-8;I<=8 && Spots.Num()<18;++I)for(int32 J=-8;J<=8 && Spots.Num()<18;++J)
  {
   const FVector P=C+FVector(I*2500.f+J*370.f,J*2500.f-I*290.f,0);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,P+FVector(0,0,8000),P-FVector(0,0,8000),ECC_WorldStatic,Q) || H.ImpactNormal.Z<.9f)continue;
   float WaterZ;FVector Cur;
   if(Marine && Marine->SampleWater(H.ImpactPoint,WaterZ,Cur) && WaterZ>H.ImpactPoint.Z-30)continue;
   // room for the rotor: nothing taller than 1.5 m within 8 m
   FHitResult Block;
   if(W->SweepSingleByChannel(Block,H.ImpactPoint+FVector(0,0,1700),H.ImpactPoint+FVector(0,0,950),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(800),Q))continue;
   if(Spots.ContainsByPredicate([&](const FVector& S){return FVector::Dist2D(S,H.ImpactPoint)<2000;}))continue;
   Spots.Add(H.ImpactPoint);Kinds.Add(GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent()));
  }
 }
 void Report(bool bOk,double E)
 {
  auto* Move=Heli->GetAircraftMovement();auto* Hull=Heli->GetHull();UWorld* W=Heli->GetWorld();
  const FVector Ctr=Hull->GetComponentLocation();FHitResult F;FCollisionQueryParams Q(SCENE_QUERY_STAT(LandingProbe),false,Heli.Get());
  W->LineTraceSingleByChannel(F,Ctr,Ctr-FVector(0,0,2000),ECC_WorldStatic,Q);
  if(!bOk)
  {
   TArray<FOverlapResult> Under;const FVector Ext=Heli->GetProfile()->BodyExtent;
   W->OverlapMultiByObjectType(Under,Ctr-FVector(0,0,Ext.Z+20),Hull->GetComponentQuat(),FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeBox(FVector(Ext.X,Ext.Y,40)),Q);
   for(const auto& O:Under)Test->AddInfo(FString::Printf(TEXT("   under: %s/%s type %d"),*GetNameSafe(O.GetActor()),*GetNameSafe(O.GetComponent()),O.GetComponent()?int32(O.GetComponent()->GetCollisionObjectType()):-1));
  }
  Test->AddInfo(FString::Printf(TEXT("spot %d %s %s after %.1fs: ground %s n.z %.2f gap %.0f cm (limit %.0f) up.z %.2f v %.0f state %d alt %.0f"),
   Index,bOk?TEXT("GROUNDED"):TEXT("NOT GROUNDED"),*Kinds[Index],E,*(GetNameSafe(F.GetActor())+TEXT("/")+GetNameSafe(F.GetComponent())),F.ImpactNormal.Z,
   F.bBlockingHit?float(Ctr.Z-F.ImpactPoint.Z):-1.f,Heli->GetProfile()->BodyExtent.Z+12,Hull->GetUpVector().Z,Hull->GetPhysicsLinearVelocity().Size(),
   int32(Move->GetFlightState()),Move->GetAltitude()));
 }
 void Next(){++Index;bPlaced=false;}
 FAutomationTestBase* Test;double Started=0,At=0;float Rest=0;int32 Index=0,Landed=0;bool bPlaced=false;
 TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;TArray<FVector> Spots;TArray<FString> Kinds;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeLandingSweepPIE,"NeonCity.WestLake.HelicopterLandingSweepPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeLandingSweepPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeLanding::FSweep(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
