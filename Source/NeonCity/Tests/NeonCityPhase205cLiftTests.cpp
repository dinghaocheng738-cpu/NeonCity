#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandResidenceLift.h"
#include "Adventure/IslandLandDoor.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace ResidenceLiftTest
{
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+X*100,-118800-Y*100,30480+Z*100));}
class FRide:public IAutomationLatentCommand
{
public:
 explicit FRide(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;
  PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<4)return false;
  if(FPlatformTime::Seconds()-Started>130)return End(TEXT("Lift acceptance timed out"));
  const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(M(0,0,0)))return false;
   for(TActorIterator<AIslandResidenceLift> It(W);It;++It)Lift=*It;
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(FVector::Dist2D(It->GetActorLocation(),M(-19.82,1,4.2))<40)Door=*It;
   if(!Lift || !Door)return End(TEXT("Native authored lift and entry door required"));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LiftUpgrade01"),false) || Save->DoesSaveExist())return End(TEXT("Fresh lift slot required; keep existing evidence"));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   // Stand outside the physical leaf's swing; opening must not disable its
   // real anti-crush collision to accommodate the fixture.
   Player->SetActorLocation(M(-18,2.4,4.2)+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==1 && Age>.6)
  {
   if(!Focus(Door,Door->Leaf->Bounds.Origin))return End(TEXT("Normal entry door prompt inaccessible"));
   Key(EKeys::F,IE_Pressed);return Next(2);
  }
  if(Stage==2 && Age>.2){Key(EKeys::F,IE_Released);return Next(3);}
  if(Stage==3)
  {
   if(Age>5)return End(TEXT("Actual entrance door did not open"));
   if(Door->IsOpen())return Next(4);
  }
  if(Stage==4)
  {
   if(!Walk(M(-20.55,2,4.2)))return Age>10?End(TEXT("Normal door-to-lift bridge walk obstructed")):false;
   return Next(5);
  }
  if(Stage==5 && Age>.5)
  {
   if(!Focus(Lift,Lift->CallPanels[1]->GetComponentLocation()))return End(TEXT("Physical living-floor lift call panel inaccessible"));
   Key(EKeys::F,IE_Pressed);return Next(6);
  }
  if(Stage==6 && Age>.2){Key(EKeys::F,IE_Released);return Next(7);}
  if(Stage==7)
  {
   if(Age>15)return End(TEXT("Empty lift failed to reach living floor and open its interlocked doors"));
   if(Lift->GetCurrentFloor()==1 && Lift->AreDoorsOpen())return Next(8);
  }
  if(Stage==8)
  {
   if(!Walk(Lift->Cabin->GetComponentLocation()))return Age>10?End(TEXT("Normal cabin entry obstructed")):false;
   return Next(9);
  }
  if(Stage==9 && Age>.5)
  {
   if(Player->GetMovementBaseObject()!=Lift->Floor)return End(TEXT("Cabin did not provide a real CMC movement base"));
   FloorBefore=Lift->GetCurrentFloor();GoalFloor=(FloorBefore+1)%5;
   if(!Focus(Lift,Lift->Cabin->GetComponentTransform().TransformPosition(FVector(117,103,140))))return End(TEXT("Actual cabin panel inaccessible"));
   Key(EKeys::F,IE_Pressed);return Next(10);
  }
  if(Stage==10 && Age>.2){Key(EKeys::F,IE_Released);return Next(11);}
  if(Stage==11)
  {
   const float Relative=Player->GetActorLocation().Z-Lift->Cabin->GetComponentLocation().Z;
   MaximumSupportError=FMath::Max(MaximumSupportError,FMath::Abs(Relative-Player->GetSimpleCollisionHalfHeight()));
   if(MaximumSupportError>20)return End(TEXT("Player lost physical cabin support during travel"));
   if(Lift->IsTravelling() && !UnsafeSaveChecked)
   {Key(EKeys::F6,IE_Pressed);UnsafeSaveChecked=true;return Next(12);}
   if(Lift->GetCurrentFloor()==GoalFloor && Lift->AreDoorsOpen())
   {
    ++Rides;FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Lift-Arrival-%d.png"),GoalFloor));
    if(GoalFloor==4 && !RoofSaved){Key(EKeys::F6,IE_Pressed);return Next(13);}
    if(GoalFloor==0)return Next(14);return Next(9);
   }
   if(Age>23)return End(FString::Printf(TEXT("Lift obstructed while travelling floor%d to%d at%s"),FloorBefore,GoalFloor,*Lift->Cabin->GetComponentLocation().ToString()));
  }
  if(Stage==12 && Age>.3)
  {
   Key(EKeys::F6,IE_Released);
   if(Save->GetLastResult()!=ENeonCitySaveResult::UnsafeState || Save->DoesSaveExist())return End(TEXT("Transit save must reject unsafe moving-cabin state"));
   return Next(11);
  }
  if(Stage==13 && Age>.8)
  {
   Key(EKeys::F6,IE_Released);auto* Model=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model)return End(TEXT("Normal F6 parked-lift safe save failed"));
   const FVector P=Lift->GetActorTransform().InverseTransformPosition(Model->Player.Transform.GetLocation());
   if(P.X<200 || P.X>255 || FMath::Abs(P.Y)>5 || FMath::Abs(P.Z-1680-Player->GetSimpleCollisionHalfHeight()-3)>5)return End(TEXT("Lift save did not use its actual clear static roof landing"));
   RoofSaved=true;return Next(15);
  }
  if(Stage==15)
  {
   if(!Walk(M(-19.35,2,16.8)))return Age>12?End(TEXT("Roof bridge exit blocked")):false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Roof exit lacks native walkable support"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Lift-Roof-Exit.png"));return Next(16);
  }
  if(Stage==16)
  {if(!Walk(Lift->Cabin->GetComponentLocation()))return Age>12?End(TEXT("Return from actual roof into cabin blocked")):false;return Next(9);}
  if(Stage==14)
  {
   if(!Walk(M(-19.35,2,0)))return Age>12?End(TEXT("Ground garage bridge exit obstructed")):false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || Rides!=4)return End(TEXT("All five functional stops and real garage exit required"));
   return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool Focus(AActor* A,FVector P){PC->SetControlRotation((P-Player->GetPawnViewLocation()).Rotation());return Player->GetInteractionComponent()->RefreshFocusedInteractable()==A;}
 bool Walk(FVector Goal)
 {
  const FVector D=Goal-Player->GetActorLocation();
  if(D.Size2D()<24){if(HeldW){Key(EKeys::W,IE_Released);HeldW=false;}return true;}
  PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));if(!HeldW){Key(EKeys::W,IE_Pressed);HeldW=true;}return false;
 }
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);for(auto K:{EKeys::W,EKeys::F,EKeys::F6})Key(K,IE_Released);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("rides"),Rides);J->SetNumberField(TEXT("maximum_support_error_cm"),MaximumSupportError);J->SetBoolField(TEXT("transit_save_rejected"),UnsafeSaveChecked);J->SetBoolField(TEXT("roof_save_created"),RoofSaved);
  J->SetStringField(TEXT("save_slot"),TEXT("NeonCity_Test_LiftUpgrade01"));J->SetStringField(TEXT("scope"),TEXT("One initial player fixture; normal F calls, hinged house entry, W cabin/roof/garage walks and F6. Real moving-base cabin travels among all five levels, rejects transit save and saves a clear static roof landing. No rider/cabin pose writes after fixture. Cold restore separate."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("ResidenceLift.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandResidenceLift* Lift=nullptr;AIslandLandDoor* Door=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 double Started=0;float At=0,MaximumSupportError=0;int32 Stage=0,FloorBefore=0,GoalFloor=0,Rides=0;bool HeldW=false,UnsafeSaveChecked=false,RoofSaved=false;FNeonCityVisualEvidence Evidence{TEXT("ResidenceLift")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResidenceLiftPIE,"NeonCity.Phase205c.ResidenceLiftPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FResidenceLiftPIE::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(ResidenceLiftTest::FRide(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
