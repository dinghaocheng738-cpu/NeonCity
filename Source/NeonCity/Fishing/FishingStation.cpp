#include "Fishing/FishingStation.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

AFishingStation::AFishingStation()
{
 PrimaryActorTick.bCanEverTick=false;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationVisual"));Visual->SetupAttachment(RootComponent);Visual->SetCollisionProfileName(TEXT("BlockAll"));
 InteractionBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("Interaction"));InteractionBounds->SetupAttachment(RootComponent);
 InteractionBounds->SetRelativeLocation(FVector(0,0,95));InteractionBounds->SetBoxExtent(FVector(32,42,85));
 InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("StationLabel"));Label->SetupAttachment(RootComponent);
 Label->SetRelativeLocation(FVector(-8,0,135));Label->SetRelativeRotation(FRotator(0,180,0));Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(9);
 Label->SetTextRenderColor(FColor(224,246,246));
}
void AFishingStation::OnConstruction(const FTransform& Transform)
{ Super::OnConstruction(Transform);Label->SetText(DisplayName); }
void AFishingStation::BeginPlay()
{ Super::BeginPlay();GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->RegisterStation(this); }
void AFishingStation::EndPlay(EEndPlayReason::Type Reason)
{ if(GetGameInstance())GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->UnregisterStation(this);Super::EndPlay(Reason); }
bool AFishingStation::IsInReach(const AActor* Visitor) const
{
 if(!HasAuthority() || StationID.IsNone() || !IsValid(Visitor) || Visitor->GetWorld()!=GetWorld()
  || FVector::Dist2D(Visitor->GetActorLocation(),GetActorLocation())>270 || FMath::Abs(Visitor->GetActorLocation().Z-GetActorLocation().Z)>210)return false;
 if(const AActor* Carrier=GetAttachParentActor();Carrier)
 {
  const auto* Character=Cast<ACharacter>(Visitor);const auto* Base=Character?Cast<UPrimitiveComponent>(Character->GetMovementBaseObject()):nullptr;
  // A based character is already moving with the deck. World speed is not walking speed.
  if(!Character || !Base || Base->GetOwner()!=Carrier || !Character->GetCharacterMovement()->IsMovingOnGround())return false;
 }
 FVector Eye;FRotator View;Visitor->GetActorEyesViewPoint(Eye,View);FHitResult Hit;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(FishingStationAccess),false,Visitor);
 return GetWorld()->LineTraceSingleByChannel(Hit,Eye,InteractionBounds->GetComponentLocation(),ECC_Visibility,Query) && Hit.GetActor()==this;
}
bool AFishingStation::ResolveWaterTarget(FVector& Target) const
{
 if(Kind!=ENeonCityFishingStation::CastingPoint)return false;
 FWaterSurfaceSample Sample;Target=GetActorTransform().TransformPosition(CastTargetLocal);
 if(!GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Target,Sample))return false;
 Target=Sample.SurfacePoint+Sample.Normal*3;
 if(Target.ContainsNaN())return false;
 // The broad marine volume can extend below the beach or a deck. Only open
 // water is a valid casting destination; query this point, never all surfaces.
 FHitResult Obstruction;FCollisionQueryParams Query(SCENE_QUERY_STAT(FishingOpenWater),false,this);
 return !GetWorld()->LineTraceSingleByChannel(Obstruction,Sample.SurfacePoint+FVector(0,0,300),Sample.SurfacePoint-FVector(0,0,30),ECC_Visibility,Query);
}
bool AFishingStation::IsFacingWater(const AActor* Visitor) const
{
 if(Kind!=ENeonCityFishingStation::CastingPoint)return true;
 if(!IsValid(Visitor))return false;
 const FVector Direction=(GetActorTransform().TransformPosition(CastTargetLocal)-Visitor->GetActorLocation()).GetSafeNormal2D();
 return FVector::DotProduct(Visitor->GetActorForwardVector().GetSafeNormal2D(),Direction)>.8f;
}
bool AFishingStation::CanInteract_Implementation(AActor* Visitor) const
{
 auto* Pawn=Cast<APawn>(Visitor);auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 auto* Fishing=PC?PC->FindComponentByClass<UFishingComponent>():nullptr;
 return Fishing && Fishing->CanStart(this);
}
FText AFishingStation::GetInteractionName_Implementation() const
{
 if(Kind==ENeonCityFishingStation::FishMarket)return NSLOCTEXT("NeonCity","FishMarketPrompt","Sell a fresh catch");
 if(Kind==ENeonCityFishingStation::Grill)return NSLOCTEXT("NeonCity","FishGrillPrompt","Grill a fish");
 return GetAttachParentActor()?NSLOCTEXT("NeonCity","FishDeckPrompt","Cast from the deck"):NSLOCTEXT("NeonCity","FishCastPrompt","Fish the coast");
}
void AFishingStation::Interact_Implementation(AActor* Visitor)
{
 auto* Pawn=Cast<APawn>(Visitor);auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 if(auto* Fishing=PC?PC->FindComponentByClass<UFishingComponent>():nullptr)Fishing->Start(this);
}
