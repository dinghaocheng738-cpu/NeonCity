#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace RidgeRoofFlight
{
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+X*100,-118800-Y*100,30480+Z*100));}
class FFlight:public IAutomationLatentCommand
{
public:
 explicit FFlight(FAutomationTestBase* T,bool Cold=false):Test(T),bCold(Cold){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<4)return false;
  if(FPlatformTime::Seconds()-Start>220)return End(false,TEXT("Roof flight timed out"));
  const float Now=W->GetTimeSeconds(),DT=FMath::Clamp(Now-Previous,0.f,.1f);Previous=Now;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(M(0,0,0)))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It){Heli=*It;break;}
   if(bCold)
   {
    Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
    if(!Heli || !Save->ConfigureSlot(TEXT("NeonCity_Test_RidgeRoofUpgrade"),false) || !Save->DoesSaveExist())return End(false,TEXT("Prior actual native roof landing/house walk checkpoint required"));
    Key(EKeys::F7,IE_Pressed);return Next(20);
   }
   if(!Heli || !Heli->GetAircraftMovement()->IsGrounded())return End(false,TEXT("Original helicopter must begin physically on its existing pad"));
   const FVector Side=Heli->GetActorLocation()+Heli->GetActorRightVector()*240;FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(RoofFlightEntry),false,Player);Q.AddIgnoredActor(Heli);
   if(!W->LineTraceSingleByChannel(Floor,Side+FVector(0,0,400),Side-FVector(0,0,550),ECC_Pawn,Q))return End(false,TEXT("No physical player entry beside authored pad"));
   Player->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   InitialHealth=Heli->GetHealthComponent()->GetCurrentHealth();InitialHeli=Heli->GetActorLocation();Last=InitialHeli;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Save->ConfigureSlot(TEXT("NeonCity_Test_RidgeRoofUpgrade"),false) || Save->DoesSaveExist())return End(false,TEXT("Fresh scoped roof test slot required; preserve existing save evidence"));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   return Next(1);
  }
  Evidence.Record(W,PC,Stage);const float Elapsed=Now-At;
  if(Stage==20 && Elapsed>2)
  {
   Key(EKeys::F7,IE_Released);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Player->GetCharacterMovement()->IsMovingOnGround() || !Heli->GetAircraftMovement()->IsGrounded() || FVector::Dist2D(Heli->GetActorLocation(),M(-2,-2,16.8))>250 || FVector::Dist2D(Player->GetActorLocation(),Heli->GetActorLocation())>350)
    return End(false,TEXT("Independent process did not restore real grounded roof helicopter and safe return position"));
   SavedRoofPosition=Heli->GetActorLocation();InitialHealth=Heli->GetHealthComponent()->GetCurrentHealth();
   if(!FocusHeli())return End(false,TEXT("Restored roof helicopter is unreachable through normal F trace"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Roof-Cold-Actual-Restored.png"));Key(EKeys::F,IE_Pressed);return Next(9);
  }
  if(Stage==1 && Elapsed>.6)
  {if(!FocusHeli())return End(false,TEXT("Normal eye trace failed to focus authored helicopter"));Key(EKeys::F,IE_Pressed);return Next(2);}
  if(Stage==2 && Elapsed>.2)
  {Key(EKeys::F,IE_Released);if(PC->GetPawn()!=Heli)return End(false,TEXT("Actual F mapping failed to board"));return Next(3);}
  if(Stage>=3 && Stage<=5)
  {
   Travel+=FVector::Dist(Heli->GetActorLocation(),Last);Last=Heli->GetActorLocation();
   if(!Heli->GetHull()->IsSimulatingPhysics() || Heli->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5)return End(false,TEXT("Roof approach lost physics or damaged the aircraft"));
   if(Stage==3)
   {const FVector Goal(InitialHeli.X,InitialHeli.Y,35000);Fly(Goal,DT,500);if(FMath::Abs(Heli->GetActorLocation().Z-Goal.Z)<100)return Next(4);}
   else if(Stage==4)
   {Fly(M(-2,-2,28),DT,2000);if(FVector::Dist2D(Heli->GetActorLocation(),M(-2,-2,28))<80 && FMath::Abs(Heli->GetActorLocation().Z-M(0,0,28).Z)<80 && Heli->GetVelocity().Size()<120)return Next(5);}
   else
   {
    Fly(M(-2,-2,16.8)+FVector(0,0,Heli->GetProfile()->BodyExtent.Z-3),DT,250);
    if(Elapsed>35)return End(false,TEXT("Physical roof landing did not establish normal ground support within 35 seconds"));
    const bool Stable=Heli->GetAircraftMovement()->IsGrounded() && Heli->GetVelocity().Size()<25 && FVector::Dist2D(Heli->GetActorLocation(),M(-2,-2,16.8))<120;
    StableSeconds=Stable?StableSeconds+DT:0;
    if(StableSeconds>2){Heli->SubmitFlightInput_Implementation(PC,FVector::ZeroVector,0);Heli->SubmitPowerInput(PC,0);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Roof-NativeLanded.png"));Key(EKeys::F,IE_Pressed);return Next(6);}
   }
   PC->SetControlRotation((M(-2,-2,16.8)-Heli->GetActorLocation()).Rotation());
  }
  if(Stage==6 && Elapsed>.2)
  {
   Key(EKeys::F,IE_Released);if(PC->GetPawn()!=Player || !Player->GetCharacterMovement()->IsMovingOnGround())return End(false,TEXT("Normal F did not exit onto actual roof walking support"));
   Walk={M(-11.4,1.75,16.8),M(-14.65,.8,16.8),M(-14.65,-3.7,14.7),M(-17.45,-3.7,14.7),M(-17.45,1.75,12.6),M(-11.4,1.75,12.6),M(-17.45,1.75,12.6),M(-17.45,-3.7,14.7),M(-14.65,-3.7,14.7),M(-14.65,.8,16.8),M(-11.4,1.75,16.8),M(-2,-4.65,16.8)};GoalIndex=0;GoalAt=Now;return Next(7);
  }
  if(Stage==7)
  {
   const FVector Delta=Walk[GoalIndex]-Player->GetActorLocation();PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));
   if(!HeldW){Key(EKeys::W,IE_Pressed);HeldW=true;}
   if(Delta.Size2D()<45)
   {
    if(GoalIndex==5)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Roof-NativeHouseEntry.png"));
    GoalAt=Now;if(++GoalIndex==Walk.Num())
    {Key(EKeys::W,IE_Released);HeldW=false;Key(EKeys::F6,IE_Pressed);return Next(8);}
   }
   else if(Now-GoalAt>18)return End(false,FString::Printf(TEXT("Normal roof/house circulation blocked waypoint%d at%s"),GoalIndex,*Player->GetActorLocation().ToString()));
  }
  if(Stage==8 && Elapsed>1)
  {
   Key(EKeys::F6,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Save->DoesSaveExist())return End(false,TEXT("Normal F6 roof/dryland save failed"));
   SavedRoofPosition=Heli->GetActorLocation();if(!FocusHeli())return End(false,TEXT("Normal trace could not focus roof helicopter on return"));Key(EKeys::F,IE_Pressed);return Next(9);
  }
  if(Stage==9 && Elapsed>.2)
  {Key(EKeys::F,IE_Released);if(PC->GetPawn()!=Heli)return End(false,TEXT("F failed to reboard parked roof helicopter"));Key(EKeys::SpaceBar,IE_Pressed);return Next(10);}
  if(Stage==10)
  {
   if(Heli->GetActorLocation().Z-SavedRoofPosition.Z>600)
   {Key(EKeys::SpaceBar,IE_Released);if(bCold && (!Heli->GetHull()->IsSimulatingPhysics() || Heli->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5))return End(false,TEXT("Restored roof takeoff lost native physics or damaged the aircraft"));if(bCold)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Roof-Cold-Native-Takeoff.png"));return End(true,TEXT(""));}
   if(Elapsed>12)return End(false,TEXT("Normal Space rise mapping did not retake off from roof"));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=GEditor->PlayWorld->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool FocusHeli(){PC->SetControlRotation((Heli->GetActorLocation()+FVector(0,0,25)-Player->GetPawnViewLocation()).Rotation());return Player->GetInteractionComponent()->RefreshFocusedInteractable()==Heli;}
 void Fly(FVector Destination,float DT,float Limit)
 {
  const FVector Error=Destination-Heli->GetActorLocation(),V=Heli->GetVelocity(),Horizontal(V.X,V.Y,0),Desired=(FVector(Error.X,Error.Y,0)*.55f).GetClampedToMaxSize(Limit);
  const auto* Profile=Heli->GetProfile();const float Current=Heli->GetAircraftMovement()->GetCurrentPower(),Boost=FMath::Clamp((Current-Profile->HoverPower)/(1-Profile->HoverPower),0.f,1.f);
  const float Acceleration=Profile->Acceleration*100*FMath::Lerp(1.f,Profile->FastFlightAccelerationScale,Boost);
  FVector Local=FRotator(0,Heli->GetActorRotation().Yaw,0).UnrotateVector((((Desired-Horizontal)*.9f+Horizontal*(Profile->AirResistance+.05f))/Acceleration).GetClampedToMaxSize(1));
  Local.Z=FMath::Clamp(Error.Z/(Profile->VerticalSpeed*100*1.5f),-1.,1.);
  // The native controller holds altitude below its 0.05 vertical dead zone.
  // Sustain a genuine gentle descent until real floor contact, as a player
  // holding the descend input does; do not substitute a pose or ground flag.
  if(Stage==5)Local.Z=Heli->GetAircraftMovement()->IsGrounded()?0:FMath::Min(Local.Z,-.12);
  const float Heading=Stage==3?Heli->GetActorRotation().Yaw:0;
  const float Yaw=FMath::Clamp(FMath::FindDeltaAngleDegrees(Heli->GetActorRotation().Yaw,Heading)/50.f,-1.f,1.f);
  Heli->SubmitFlightInput_Implementation(PC,Local,Yaw);Heli->SubmitPowerInput(PC,0);
 }
 bool End(bool Passed,const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);if(PC){for(auto K:{EKeys::F,EKeys::F6,EKeys::F7,EKeys::W,EKeys::SpaceBar})Key(K,IE_Released);}
  if(PC && Heli && PC->GetPawn()==Heli){Heli->SubmitFlightInput_Implementation(PC,FVector::ZeroVector,0);Heli->SubmitPowerInput(PC,0);}
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("continuous_flight_cm"),Travel);J->SetNumberField(TEXT("walk_waypoints"),GoalIndex);J->SetNumberField(TEXT("initial_player_fixtures"),1);J->SetNumberField(TEXT("aircraft_pose_writes"),0);J->SetStringField(TEXT("save_slot"),TEXT("NeonCity_Test_RidgeRoofUpgrade"));J->SetStringField(TEXT("scope"),TEXT("Normal F/Space/F6 mappings plus existing physical aircraft input ports; pad takeoff, real roof approach/landing, ordinary stairs house entry and return, save and takeoff. Cross-process restore separate."));
  if(bCold){J->SetNumberField(TEXT("initial_player_fixtures"),0);J->SetNumberField(TEXT("restored_native_takeoff_rise_cm"),Heli?Heli->GetActorLocation().Z-SavedRoofPosition.Z:0);J->SetStringField(TEXT("scope"),TEXT("Independent process of retained actual roof landing/house walk/F6 checkpoint. Genuine F7 restores grounded roof helicopter and safe walking position; normal F boards and Space physically takes off. No player or aircraft fixture/teleport; unchanged full stairs/approach tests remain cached."));}
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bCold?TEXT("RoofFlightCold.json"):TEXT("RoofFlight.json"))));return true;
 }
 FAutomationTestBase* Test;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityHelicopterPrototype* Heli=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 int32 Stage=0,GoalIndex=0;float At=0,Previous=0,GoalAt=0,StableSeconds=0,InitialHealth=0;double Start=0,Travel=0;bool HeldW=false,bCold=false;FVector InitialHeli,Last,SavedRoofPosition;TArray<FVector> Walk;FNeonCityVisualEvidence Evidence{TEXT("RoofFlight")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRidgeRoofFlight,"NeonCity.Phase205c.RoofFlightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRidgeRoofFlight::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeRoofFlight::FFlight(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRidgeRoofFlightCold,"NeonCity.Phase205c.RoofFlightColdPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRidgeRoofFlightCold::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeRoofFlight::FFlight(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
