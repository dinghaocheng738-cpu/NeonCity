#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicles/Marine/NeonCityRowboat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"

namespace NeonCityWestLakeRowboatLanding
{
void Key(APlayerController* PC,FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
/** Stepping ashore from the home rowboat onto the open lake bank (no pier): with the boat nosed into the bank,
 * and again lying alongside it, the boat reports a landing in reach and F takes the rower over the gunwale onto
 * the lawn above the water. Before 2026-10-22 only a strip beside the stern seat was searched, so a boat run up
 * to the bank bow-first could not be left. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150){Test->AddError(FString::Printf(TEXT("Rowboat landing check timed out at stage %d"),Stage));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  const double E=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=Cast<ACharacter>(PC->GetPawn());if(!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityRowboat> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.HomeBoat")))Boat=*It;
   if(!Test->TestNotNull(TEXT("home rowboat"),Boat.Get()))return true;
   if(!Test->TestTrue(TEXT("open bank found along the residence lakefront"),FindBank()))return true;
   if(!Test->TestTrue(TEXT("dry footing beside the moored boat"),StandBeside()))return true;
   return Next(10);
  }
  if(Stage==10 || Stage==11)
  {
   if(E<.8 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   if(!Boat->CanInteract_Implementation(Player.Get()))
   {
    const auto* Body=Player->FindComponentByClass<UDamageableCharacterComponent>();
    Test->AddInfo(FString::Printf(TEXT("cannot board: phase %d ground %d boat v %.0f ctrl %s dead %d attach %s canact %d dist %.0f reach %.0f"),int32(Boat->GetPhase()),
     Player->GetCharacterMovement()->IsMovingOnGround(),Boat->GetVelocity().Size(),Boat->GetController()?*Boat->GetController()->GetName():TEXT("-"),
     Boat->GetHealthComponent()->IsDead(),Player->GetAttachParentActor()?*Player->GetAttachParentActor()->GetName():TEXT("-"),Body?Body->CanAct():-1,
     FVector::Dist(Boat->GetActorLocation(),Player->GetActorLocation()),Boat->GetProfile()->InteractionReachCm));
   }
   if(!Test->TestTrue(TEXT("boat can be boarded"),Boat->CanInteract_Implementation(Player.Get())))return true;
   Boat->Interact_Implementation(Player.Get());return Next(Stage==10?1:4);
  }
  if(Stage==1)
  {
   if(Boat->GetPhase()!=ANeonCityRowboat::EPhase::Seated){if(E>8){Test->AddError(TEXT("not seated"));return true;}return false;}
   // nose into the bank: bow 40 cm off the bank edge, square to it
   Place(Bank-Dir*(Boat->GetHull()->GetScaledBoxExtent().X+40),Dir.Rotation().Yaw);
   return Next(2);
  }
  if(Stage==2 || Stage==5)
  {
   Hold();
   if(E<1.5)return false;
   const TCHAR* Case=Stage==2?TEXT("nosed into the bank"):TEXT("alongside the bank");
   Test->TestTrue(FString::Printf(TEXT("landing in reach, %s"),Case),Boat->HasLandingInReach());
   if(!Test->TestTrue(FString::Printf(TEXT("seated before stepping ashore, %s"),Case),Boat->GetMarineInteraction()->GetDriver()!=nullptr))return true;
   Key(PC,EKeys::F,IE_Pressed);Key(PC,EKeys::F,IE_Released);                         // F: stand up and step ashore
   return Next(Stage+1);
  }
  if(Stage==3 || Stage==6)
  {
   Hold();
   if(Boat->GetMarineInteraction()->GetDriver() || Boat->GetPhase()!=ANeonCityRowboat::EPhase::Idle){if(E>10){Test->AddError(FString::Printf(TEXT("stepping ashore never finished (phase %d)"),int32(Boat->GetPhase())));return true;}return false;}
   if(E<1.0 || (!Player->GetCharacterMovement()->IsMovingOnGround() && E<4))return false;   // landed from the step
   const float Z=Player->GetActorLocation().Z-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   Test->TestTrue(FString::Printf(TEXT("ashore on the bank (feet %.0f cm above the water, walking %d)"),Z-Water,Player->GetCharacterMovement()->IsMovingOnGround()),
    Player->GetCharacterMovement()->IsMovingOnGround() && Z>Water+50 && FVector::Dist2D(Player->GetActorLocation(),Boat->GetActorLocation())<900);
   if(Stage==6)return true;
   return Next(11);                                                                   // board again from the bank
  }
  if(Stage==4)
  {
   if(Boat->GetPhase()!=ANeonCityRowboat::EPhase::Seated){if(E>8){Test->AddError(TEXT("not seated again"));return true;}return false;}
   // alongside: starboard / port side 60 cm off the bank edge, parallel to it
   Place(Bank-Dir*(Boat->GetHull()->GetScaledBoxExtent().Y+60),Dir.Rotation().Yaw+90);
   return Next(5);
  }
  return true;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool StandBeside()
 {
  // the jetty deck (or bank) beside the moored boat
  const float Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  FCollisionQueryParams Q(TEXT("RowboatLandingBoard"),false,Player.Get());Q.AddIgnoredActor(Boat.Get());
  for(float Side:{1.f,-1.f})for(float Out:{150.f,190.f})
  {
   const FVector Spot=Boat->GetActorTransform().TransformPosition(FVector(-60,Side*Out,0));FHitResult Floor;
   if(W->LineTraceSingleByChannel(Floor,Spot+FVector(0,0,300),Spot-FVector(0,0,200),ECC_Pawn,Q) && Floor.ImpactPoint.Z>Water+20)
   {Player->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Half+2),false,nullptr,ETeleportType::TeleportPhysics);return true;}
  }
  return false;
 }
 void Place(const FVector& Where,float Yaw)
 {
  Boat->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);Boat->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
  Boat->SetActorLocationAndRotation(FVector(Where.X,Where.Y,Water),FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
  Pose=FTransform(FRotator(0,Yaw,0),FVector(Where.X,Where.Y,Water));
 }
 void Hold()
 {
  // a rower holding the boat against the bank with a hand: no drift while the check runs
  if(Pose.GetLocation().IsZero())return;
  Boat->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);
  if(FVector::Dist2D(Boat->GetActorLocation(),Pose.GetLocation())>30)Boat->SetActorLocationAndRotation(Pose.GetLocation(),Pose.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
 }
 bool FindBank()
 {
  // the residence lakefront: from the moored boat, the first ground at least 50 cm above the water, 12 m or more
  // from the home jetty (the mooring holds the boat within 8 m of it)
  float Surface=0;FVector Current;
  auto* Lake=W->GetSubsystem<UMarineWorldSubsystem>();
  if(!Lake || !Lake->SampleWater(Boat->GetActorLocation(),Surface,Current))return false;
  Water=Surface;
  FCollisionQueryParams Q(TEXT("RowboatLandingBank"),false,Boat.Get());Q.AddIgnoredActor(Player.Get());
  const FVector From=Boat->GetActorLocation();float Best=MAX_flt;
  for(float Yaw=0;Yaw<360;Yaw+=10)
  {
   const FVector D=FRotator(0,Yaw,0).Vector();FVector Prev=From;
   for(float R=200;R<4000;R+=40)
   {
    const FVector P=From+D*R;FHitResult G;
    if(!W->LineTraceSingleByChannel(G,P+FVector(0,0,500),P-FVector(0,0,600),ECC_Pawn,Q))continue;
    if(G.GetActor() && (G.GetActor()->ActorHasTag(TEXT("WL.HomeBoat")) || Cast<ANeonCityRowboat>(G.GetActor())))break;   // the jetty
    if(G.ImpactPoint.Z<Water+50){Prev=P;continue;}
    if(FVector::Dist2D(P,From)>1200 && R<Best && G.ImpactNormal.Z>.8f){Best=R;Bank=FVector(P.X,P.Y,Water);Dir=D;}
    break;
   }
  }
  Test->AddInfo(FString::Printf(TEXT("bank %.0f m from the moored boat at %s"),Best/100,*Bank.ToCompactString()));
  return Best<MAX_flt;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;double Started=0,At=0;int32 Stage=0;float Water=0;
 FVector Bank=FVector::ZeroVector,Dir=FVector::ForwardVector;FTransform Pose=FTransform(FVector::ZeroVector);
 TWeakObjectPtr<ANeonCityRowboat> Boat;TWeakObjectPtr<ACharacter> Player;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeRowboatLandingPIE,"NeonCity.WestLake.RowboatLandingPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeRowboatLandingPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeRowboatLanding::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
