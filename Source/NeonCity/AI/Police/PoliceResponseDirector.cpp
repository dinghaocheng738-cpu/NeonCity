#include "AI/Police/PoliceResponseDirector.h"
#include "Police/PoliceDispatchSubsystem.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceVehicle.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Wanted/WantedSubsystem.h"
#include "Wanted/SearchSystem.h"
#include "Wanted/WantedStatusWidget.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
APoliceResponseDirector::APoliceResponseDirector()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.2f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void APoliceResponseDirector::BeginPlay()
{
 Super::BeginPlay(); if(!HasAuthority()) return;
 GetWorld()->GetSubsystem<UPoliceDispatchSubsystem>()->RegisterStation(this);
}
void APoliceResponseDirector::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds); if(!HasAuthority() || !IsValid(Road)) return;
 auto* Wanted=GetWorld()->GetSubsystem<UWantedSubsystem>();
 if(!StatusWidget)
 {
  auto* PC=GetWorld()->GetFirstPlayerController();
  if(PC && PC->IsLocalController())
  {
   StatusWidget=CreateWidget<UWantedStatusWidget>(PC);
   StatusWidget->SetVisibility(ESlateVisibility::HitTestInvisible); StatusWidget->AddToViewport(4);
  }
 }
 if(StatusWidget) StatusWidget->UpdateStatus(Wanted->GetWantedLevel(),GetWorld()->GetSubsystem<USearchSystem>()->HasVisualContact());

}

void APoliceResponseDirector::EndPlay(EEndPlayReason::Type Reason)
{
 if(StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget=nullptr; }
 if(Reason==EEndPlayReason::Destroyed) GetWorld()->GetSubsystem<UPoliceDispatchSubsystem>()->UnregisterStation(this);
 Super::EndPlay(Reason);
}
