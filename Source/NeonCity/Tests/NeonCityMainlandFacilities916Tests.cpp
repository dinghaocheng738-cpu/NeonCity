#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Property/NeonCityStarterApartment.h"
#include "Interaction/InteractiveBuildingDoor.h"
#include "Interaction/InteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandFacilities916
{
// Independent, disclosed initial approaches to the two existing facilities.
// Production F owns each doorway transition; interior travel retains physics.
class FLegacyAccess:public IAutomationLatentCommand
{
public:explicit FLegacyAccess(FAutomationTestBase* T,bool InVisitorOnly=false):Test(T),VisitorOnly(InVisitorOnly){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>130)return End(TEXT("Legacy facility access timeout"));
  const float Age=W->GetTimeSeconds()-At;
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal walking player absent"));Original=P->GetActorTransform();
   for(TActorIterator<ANeonCityStarterApartment> I(W);I;++I)Home=*I;
   for(TActorIterator<AInteractiveBuildingDoor> I(W);I;++I)Door=*I;
   if(Door){OriginalDoorOpen=Door->IsOpen();DoorStateCaptured=true;}
   if(!Home || !Door || Door->IsOpen())return End(TEXT("Expected native home and closed visitor entrance"));
   if(VisitorOnly){if(!Fixture(Door,FVector(145,80,0)))return End(TEXT("Visitor approach blocked"));return Next(6);}
   if(!Fixture(Home,FVector(0,-145,0)))return End(TEXT("Upgraded home has no clear supported entrance approach"));return Next(1);
  }
  if(Stage==1 && Age>1.5)
  {if(Home->IsInsideInterior_Implementation(P) || !Focus(Home,FVector(0,0,130)))return End(TEXT("Upgraded home facade blocks its normal F prompt"));Key(true);return Next(2);}
  if(Stage==2 && Age>.3)
  {Key(false);if(!Home->IsInsideInterior_Implementation(P))return End(TEXT("Normal F home entry rejected its fitted destination"));Goals={Home->GetActorTransform().TransformPosition(FVector(0,360,99)),Home->GetActorTransform().TransformPosition(FVector(-130,540,99)),Home->GetActorTransform().TransformPosition(FVector(180,520,99)),Home->GetActorTransform().TransformPosition(FVector(0,240,99))};return Next(3);}
  if(Stage==3)
  {
   if(!Walk(Goals[Point],Age))return false;
   if(Failed)return End(TEXT("Home interior walking clearance failed"));
   if(++Point==Goals.Num())return Next(4);At=W->GetTimeSeconds();return false;
  }
  if(Stage==4 && Age>.4)
  {
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Mainland916-LegacyHomeInterior.jpg"));
#endif
   if(!Focus(Home,FVector(0,0,130)))return End(TEXT("Home normal exit prompt occluded"));Key(true);return Next(5);
  }
  if(Stage==5 && Age>.3)
  {Key(false);if(Home->IsInsideInterior_Implementation(P) || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal F home exit did not return supported walking"));Test->AddInfo(TEXT("Existing property: normal F entry, physical interior walking, normal F exit passed"));if(!Fixture(Door,FVector(145,80,0)))return End(TEXT("Visitor entrance lacks a supported clear approach"));return Next(6);}
  if(Stage==6 && Age>1.5)
  {if(!Focus(Door,FVector(0,80,120)))return End(TEXT("Visitor normal F prompt occluded"));Key(true);return Next(7);}
  if(Stage==7 && Age>.3)
  {Key(false);if(!Door->IsOpen())return End(TEXT("Visitor F did not open the actual hinge"));return Next(8);}
  if(Stage==8)
  {if(!WalkDoor(Door->GetActorTransform().TransformPosition(FVector(-170,80,P->GetSimpleCollisionHalfHeight()+3)),Age))return false;if(Failed)return End(TEXT("Visitor open doorway blocks the original capsule"));return Next(9);}
  if(Stage==9 && Age>.3)
  {if(!Focus(Door,FVector(-80,0,120)))return End(TEXT("Visitor interior cannot focus the open door"));Key(true);return Next(10);}
  if(Stage==10 && Age>.3)
  {Key(false);if(Door->IsOpen())return End(TEXT("Visitor normal F close failed"));return Next(15);}
  if(Stage==15 && Age>.15){if(!Focus(Door,FVector(0,80,120)))return End(TEXT("Visitor closed door prompt absent inside"));Key(true);return Next(11);}
  if(Stage==11 && Age>.3)
  {Key(false);if(!Door->IsOpen())return End(TEXT("Visitor F reopening failed"));return Next(12);}
  if(Stage==12)
  {if(!WalkDoor(Door->GetActorTransform().TransformPosition(FVector(145,80,P->GetSimpleCollisionHalfHeight()+3)),Age))return false;if(Failed)return End(TEXT("Visitor exit has an invisible blocking surface"));return Next(13);}
  if(Stage==13 && Age>.3)
  {if(!Focus(Door,FVector(-80,0,120)))return End(TEXT("Visitor exterior cannot close open door"));Key(true);return Next(14);}
  if(Stage==14 && Age>.3)
  {Key(false);if(Door->IsOpen() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Visitor final close/walking state failed"));Test->AddInfo(TEXT("Visitor hall: normal F open, physical entry, close/reopen, physical exit, close passed"));return End(TEXT(""));}
  return false;
 }
private:
 bool Fixture(AActor* Facility,const FVector& Local)
 {
  FVector Stand=Facility->GetActorTransform().TransformPosition(Local);FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(FacilityInitialApproach916),false,P);
  if(!W->LineTraceSingleByChannel(Floor,Stand+FVector(0,0,200),Stand-FVector(0,0,150),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return false;
  Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return false;
  P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return true;
 }
 bool Focus(AActor* Facility,FVector Local)
 {FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);PC->SetControlRotation((Facility->GetActorTransform().TransformPosition(Local)-Eye).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Facility;}
 bool Walk(FVector Goal,float Age)
 {
  if(Path.IsEmpty()){FPhase205cWalkSurvey Survey;if(!Phase205cSupportedWalkPath(W,P,Goal,Path,110,25,&Survey,5000)){Test->AddError(FString::Printf(TEXT("Facility physical path: %s; from%s to%s"),*Survey.Failure,*P->GetActorLocation().ToString(),*Goal.ToString()));Failed=true;return true;}PathPoint=0;}
  FVector D=Path[PathPoint]-P->GetActorLocation();
  if(D.Size2D()<18 && FMath::Abs(D.Z)<45){if(++PathPoint>=Path.Num()){Path.Reset();return true;}return false;}
  if(Age>25){Test->AddError(TEXT("Actual facility walking blocked"));Failed=true;return true;}
  P->AddMovementInput(D.GetSafeNormal2D(),FMath::Clamp(float(D.Size2D()/100),.08f,1.f));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));return false;
 }
 bool WalkDoor(FVector Goal,float Age)
 {
  const FVector D=Goal-P->GetActorLocation();
  if(D.Size2D()<18 && FMath::Abs(D.Z)<45)return true;
  if(Age>20)
  {
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(VisitorDoorActual916),false,P);
   W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+D.GetSafeNormal2D()*100,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
   Test->AddError(FString::Printf(TEXT("Actual visitor crossing blocked at%s by %s/%s normal%s impact%s"),*P->GetActorLocation().ToString(),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactNormal.ToString(),*H.ImpactPoint.ToString()));Failed=true;return true;
  }
  P->AddMovementInput(D.GetSafeNormal2D(),FMath::Clamp(float(D.Size2D()/100),.08f,1.f));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));return false;
 }
 void Key(bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool Next(int N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error){Key(false);if(DoorStateCaptured && Door && Door->IsOpen()!=OriginalDoorOpen){Door->Interact_Implementation(P);if(Door->IsOpen()!=OriginalDoorOpen)Test->AddError(TEXT("Visitor cleanup could not restore the captured door state"));}if(!Error.IsEmpty())Test->AddError(Error);if(P){P->GetCharacterMovement()->StopMovementImmediately();P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);}return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityStarterApartment* Home=nullptr;AInteractiveBuildingDoor* Door=nullptr;FTransform Original;TArray<FVector> Goals,Path;int Stage=0,Point=0,PathPoint=0;bool Failed=false,VisitorOnly=false,DoorStateCaptured=false,OriginalDoorOpen=false;float At=0;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandLegacyFacilities916,"NeonCity.Phase205c.Mainland916.Standalone.LegacyFacilities",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandLegacyFacilities916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandFacilities916::FLegacyAccess(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandVisitorDoor916,"NeonCity.Phase205c.Mainland916.Standalone.VisitorDoor",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandVisitorDoor916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandFacilities916::FLegacyAccess(this,true));return true;}
#endif
