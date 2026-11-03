#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityValidationPSO916.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandFlight916
{
// One initial player approach fixture, native parked aircraft left untouched.
// Thereafter only normal F and public, driver-checked flight/power inputs.
class FCircuit:public IAutomationLatentCommand
{
public:explicit FCircuit(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<6)return false;
  const double Now=FPlatformTime::Seconds();if(!Start)Start=Now;
  if(Now-Start>480)return End(TEXT("Native mainland circuit timeout"));
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal on-foot player absent"));Original=P->GetActorTransform();
   for(TActorIterator<ANeonCityHelicopterPrototype> I(W);I;++I)if(I->GetActorLocation().X>-10000 && I->GetActorLocation().X<10000 && I->GetActorLocation().Y>-10000 && I->GetActorLocation().Y<10000 && I->GetAircraftMovement()->IsGrounded())H=*I;
   if(!H)return End(TEXT("Genuinely grounded mainland helicopter absent"));
   FVector Stand;if(!Phase205cVehicleApproach(W,P,H,P->GetActorLocation(),Stand))return End(TEXT("Native helicopter has no supported ordinary approach"));
   Origin=H->GetActorLocation();Last=Origin;InitialHealth=H->GetHealthComponent()->GetCurrentHealth();
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I){Clock=*I;OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}
   Stops={FVector(Origin.X,Origin.Y,8000),FVector(-40000,-8000,8000),FVector(-102000,2000,8000),FVector(-105000,40000,8000),FVector(-92000,46000,8000),FVector(-55000,50000,8000),FVector(-12000,35000,8000),FVector(Origin.X,Origin.Y,8000),Origin};
   return Next(1);
  }
  if(Stage==1 && Age>2)
  {
   FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);bool Focus=false;
   for(float Z:{0.f,45.f,90.f}){PC->SetControlRotation((H->GetActorLocation()+FVector(0,0,Z)-Eye).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()==H){Focus=true;break;}}
   if(!Focus)return End(TEXT("Normal F helicopter prompt cannot be focused"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {Key(EKeys::F,false);if(PC->GetPawn()!=H || H->GetAircraftInteraction()->GetDriver()!=P || !H->GetAircraftMovement()->IsEngineRunning())return End(TEXT("Normal F boarding/engine failed"));return Next(3);}
  if(Stage==3)
  {
   const FVector Here=H->GetActorLocation(),Velocity=H->GetVelocity();const bool Landing=Point==Stops.Num()-1;
   if(PC->GetPawn()!=H || Here.ContainsNaN() || !H->GetHull()->IsSimulatingPhysics() || H->GetHull()->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || H->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5f)return End(TEXT("Continuous flight lost possession/physics or suffered a collision"));
   Travel+=FVector::Dist(Here,Last);Last=Here;
   const FVector Error=Stops[Point]-Here,Horizontal(Velocity.X,Velocity.Y,0),Desired=FVector(Error.X,Error.Y,0).GetClampedToMaxSize(1)*FMath::Min(float(Error.Size2D()*.55),Landing?250.f:3500.f);
   const auto* Profile=H->GetProfile();auto* Movement=H->GetAircraftMovement();
   const float Boost=FMath::Clamp((Movement->GetCurrentPower()-Profile->HoverPower)/(1-Profile->HoverPower),0.f,1.f);
   FVector Input=FRotator(0,H->GetActorRotation().Yaw,0).UnrotateVector((((Desired-Horizontal)*.9f+Horizontal*(Profile->AirResistance+.05f))/(Profile->Acceleration*100*FMath::Lerp(1.f,Profile->FastFlightAccelerationScale,Boost))).GetClampedToMaxSize(1));
   Input.Z=FMath::Clamp(Error.Z/(Profile->VerticalSpeed*150),-1.,1.);
   if(Landing)Input.Z=Movement->IsGrounded()?0:FMath::Min(Input.Z,-.12);
   const float Heading=Landing?0:(FVector(-60000,20000,1000)-Here).Rotation().Yaw;
   const float Yaw=FMath::Clamp(FMath::FindDeltaAngleDegrees(H->GetActorRotation().Yaw,Heading)/50.f,-1.f,1.f);
   if(!H->SubmitFlightInput_Implementation(PC,Input,Yaw) || !H->SubmitPowerInput(PC,0))return End(TEXT("Normal driver-checked flight input rejected"));
   PC->SetControlRotation((FVector(-60000,20000,1000)-Here).Rotation());
   Metrics.Sample(W,Landing?TEXT("MainlandHelicopterLanding"):Point<3?TEXT("MainlandHelicopterDay"):Point<6?TEXT("MainlandHelicopterSunset"):TEXT("MainlandHelicopterNight"));
   const bool Arrived=Landing?Movement->IsGrounded() && Velocity.Size()<25 && FVector::Dist2D(Here,Origin)<110:Error.Size2D()<250 && FMath::Abs(Error.Z)<120 && Horizontal.Size()<220;
   if(Arrived)
   {
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("Mainland916-Flight-%02d.jpg"),Point));
#endif
    Test->AddInfo(FString::Printf(TEXT("Native circuit stop %d/%d at %s; travel %.1fm"),Point+1,Stops.Num(),*Here.ToString(),Travel/100));
    if(++Point>=Stops.Num()){H->SubmitFlightInput_Implementation(PC,FVector::ZeroVector,0);Key(EKeys::F,true);return Next(4);}
    if(Clock && Point==3)Clock->SetTimeOfDay(17.8f);if(Clock && Point==6)Clock->SetTimeOfDay(22);At=W->GetTimeSeconds();
   }
   if(Age>85)return End(FString::Printf(TEXT("Native circuit blocked at stop%d %s"),Point,*Here.ToString()));return false;
  }
  if(Stage==4 && Age>.5)
  {Key(EKeys::F,false);if(PC->GetPawn()!=P || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal landed F exit failed"));Test->TestTrue(TEXT("Physical whole-mainland perimeter exceeds 2km"),Travel>200000);return End(TEXT(""));}
  return false;
 }
private:
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error)
 {Key(EKeys::F,false);if(H && PC && PC->GetPawn()==H)H->SubmitFlightInput_Implementation(PC,FVector::ZeroVector,0);Test->TestTrue(TEXT("Flight samples retained"),Metrics.Flush());Test->TestTrue(TEXT("Requested PSO recording saved before forced automation exit"),NeonCitySaveValidationPSO916());if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}if(!Error.IsEmpty())Test->AddError(Error);if(P && PC->GetPawn()==P)P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityHelicopterPrototype* H=nullptr;ANeonCityDayNightCycle* Clock=nullptr;bool OldCycle=true;float OldHour=12,InitialHealth=0,Travel=0,At=0;double Start=0;int32 Stage=0,Point=0;FTransform Original;FVector Origin,Last;TArray<FVector> Stops;FNeonCityLandFrameMetrics Metrics{TEXT("Mainland916-Helicopter")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandFlight916,"NeonCity.Phase205c.Mainland916.Standalone.Helicopter",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandFlight916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandFlight916::FCircuit(this));return true;}
#endif
