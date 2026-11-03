#include "Adventure/IslandLandDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "TimerManager.h"

AIslandLandDoor::AIslandLandDoor()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge")));
 Leaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));Leaf->SetupAttachment(RootComponent);
 Leaf->SetMobility(EComponentMobility::Movable);Leaf->SetCollisionProfileName(TEXT("BlockAll"));
 Leaf->SetGenerateOverlapEvents(false);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("DoorPrompt"));Prompt->SetupAttachment(RootComponent);
 Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetWorldSize(12);Prompt->SetVisibility(false);
 Prompt->SetTextRenderColor(FColor(241,227,196));
}
void AIslandLandDoor::BeginPlay()
{
 Super::BeginPlay();GetWorldTimerManager().SetTimer(PromptTimer,this,&AIslandLandDoor::RefreshPrompt,.1f,true);
}
void AIslandLandDoor::RefreshPrompt()
{
 auto* PC=GetWorld()->GetFirstPlayerController();APawn* P=PC?PC->GetPawn():nullptr;
 auto* Focus=P?P->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this;
 Prompt->SetVisibility(Visible);
 if(Visible)
 {
  FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
  Prompt->SetText(FText::FromString(TEXT("F  ")+GetInteractionName_Implementation().ToString()));
  Prompt->SetWorldLocation(GetActorLocation()+FVector(0,0,Height+14)+(Eye-GetActorLocation()).GetSafeNormal2D()*25);
  Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());
 }
}
bool AIslandLandDoor::CanInteract_Implementation(AActor* Visitor) const
{
 return HasAuthority() && IsValid(Visitor) && Visitor->GetWorld()==GetWorld()
  && Cast<APawn>(Visitor) && FVector::DistSquared(Visitor->GetActorLocation(),Leaf->GetComponentLocation())<FMath::Square(270.f);
}
FText AIslandLandDoor::GetInteractionName_Implementation() const
{
 return FText::Format(bOpening?NSLOCTEXT("NeonCity","LandCloseDoor","Close {0} door"):
  NSLOCTEXT("NeonCity","LandOpenDoor","Open {0} door"),RoomName);
}
void AIslandLandDoor::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 bOpening=!bOpening;SetActorTickEnabled(true);
}
void AIslandLandDoor::Tick(float Delta)
{
 Super::Tick(Delta);
 const float Next=FMath::FInterpConstantTo(Angle,bOpening?OpenAngle:0,FMath::Clamp(Delta,0.f,.05f),100);
 const FTransform T=FTransform(FRotator(0,Next,0))*GetActorTransform();
 // Rotation sweeps are not supported by SetRelativeRotation. Query the next
 // actual leaf volume instead; pause while a person occupies its swing.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(IslandDoorSwing),false,this);
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);
 const FVector Centre=T.TransformPosition(FVector(Width*.5f,0,Height*.5f));
 if(GetWorld()->OverlapAnyTestByObjectType(Centre,T.GetRotation(),Objects,
    FCollisionShape::MakeBox(FVector(Width*.5f,6,Height*.5f)),Q))return;
 Angle=Next;Leaf->SetRelativeRotation(FRotator(0,Angle,0));
 if(FMath::IsNearlyEqual(Angle,bOpening?OpenAngle:0,.01f))SetActorTickEnabled(false);
}
