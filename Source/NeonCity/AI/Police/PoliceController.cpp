#include "AI/Police/PoliceController.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Police/PoliceSearchCoordinator.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "Wanted/SearchSystem.h"
#include "Wanted/WantedSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
APoliceController::APoliceController() { PrimaryActorTick.bCanEverTick=true; }
void APoliceController::OnPossess(APawn* InPawn) { Super::OnPossess(InPawn); Home=InPawn->GetActorLocation(); NavigationTarget=Home; }
void APoliceController::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 APoliceCharacter* Officer=Cast<APoliceCharacter>(GetPawn());
 if (!Officer || !Officer->CanRespond()) return;
 if(Officer->GetCombatAI()->ControlsLocomotion()) { Officer->GetCombatAI()->UpdateLocomotion(DeltaSeconds); return; }
 USearchSystem* Search=GetWorld()->GetSubsystem<USearchSystem>();
 UWantedSubsystem* Wanted=GetWorld()->GetSubsystem<UWantedSubsystem>();
 const FVector Position=Officer->GetActorLocation();
 if (!Officer->IsResponseEnabled() || !Search->HasLocation())
 {
  State=ENeonCityPoliceState::Patrol;
  NavigationTarget=Home+FVector((Waypoint%2)?350:-350,0,0);
  if (FVector::DistSquared2D(Position,NavigationTarget)<FMath::Square(70.f)) ++Waypoint;
 }
 else
 {
  NavigationTarget=GetWorld()->GetSubsystem<UPoliceSearchCoordinator>()->GetAssignment(Officer->GetDispatchSlot(),Wanted->GetSearchRadius(),EPoliceSearchPattern::InvestigatePoint);
  if (Search->HasVisualContact()) State=ENeonCityPoliceState::Chase;
  else if (FVector::Dist2D(Position,NavigationTarget)>Wanted->GetSearchRadius()) State=ENeonCityPoliceState::Investigate;
  else
  {
   State=ENeonCityPoliceState::Search;
   SearchStepTime+=DeltaSeconds;
   if (SearchStepTime>3) { SearchStepTime=0; ++Waypoint; }
   NavigationTarget=GetWorld()->GetSubsystem<UPoliceSearchCoordinator>()->GetAssignment(Officer->GetDispatchSlot()+Waypoint,Wanted->GetSearchRadius(),Officer->GetDispatchSlot()%2?EPoliceSearchPattern::Perimeter:EPoliceSearchPattern::SearchArea);
  }
 }
 Officer->GetSensor()->SetScanning(State==ENeonCityPoliceState::Search);
 Officer->GetCharacterMovement()->MaxWalkSpeed=State==ENeonCityPoliceState::Chase?360:State==ENeonCityPoliceState::Patrol?150:240;
 FVector Direction=NavigationTarget-Position; Direction.Z=0;
 if (Direction.Size2D()<100) return;
 // Ground probe and CharacterMovement sweeps keep the prototype out of water and solid buildings.
 const FVector Ahead=Position+Direction.GetSafeNormal()*100;
 FHitResult Ground; FCollisionQueryParams Query(SCENE_QUERY_STAT(PoliceGround),false,Officer);
 if (GetWorld()->LineTraceSingleByChannel(Ground,Ahead,Ahead-FVector(0,0,210),ECC_Visibility,Query) && Ground.ImpactNormal.Z>0.65f)
  Officer->AddMovementInput(Direction.GetSafeNormal(),1.f);
}
