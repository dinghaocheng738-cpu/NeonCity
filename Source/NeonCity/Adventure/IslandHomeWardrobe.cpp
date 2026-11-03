#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandOwnedAppearanceState.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Property/PropertySubsystem.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
AIslandHomeWardrobe::AIslandHomeWardrobe()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.15f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WardrobeFront")));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("WardrobeHandle"));Interaction->SetupAttachment(RootComponent);Interaction->SetBoxExtent(FVector(9,42,30));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Interaction->SetGenerateOverlapEvents(false);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("WardrobePrompt"));Prompt->SetupAttachment(RootComponent);Prompt->SetRelativeLocation(FVector(12,0,65));Prompt->SetWorldSize(11);Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetTextRenderColor(FColor(241,227,196));Prompt->SetVisibility(false);
}
bool AIslandHomeWardrobe::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* P=Cast<ACharacter>(Visitor);const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!HasAuthority() || !P || P->GetWorld()!=GetWorld() || !PC || !PC->IsLocalController() || PC->GetPawn()!=P || FVector::DistSquared(P->GetActorLocation(),GetActorLocation())>FMath::Square(230.f))return false;
 if(const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();Body && !Body->CanAct())return false;
 return GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID);
}
FText AIslandHomeWardrobe::GetInteractionName_Implementation()const
{return NSLOCTEXT("NeonCity","OwnedWardrobeNext","Choose next owned coastal outfit");}
void AIslandHomeWardrobe::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 auto* State=UIslandOwnedAppearanceState::FindOrCreate(Visitor);auto* P=Cast<ACharacter>(Visitor);auto* PC=Cast<APlayerController>(P->GetController());
 const bool Changed=State && State->SelectNextOwned();
 if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(Changed?FText::Format(NSLOCTEXT("NeonCity","WardrobeWorn","Wearing {0}. F chooses the next owned outfit."),UIslandOwnedAppearanceState::PaletteName(State->GetSelectedPalette())):NSLOCTEXT("NeonCity","WardrobeGearSafe","Change clothes while standing, after removing dive or flight equipment. Owned gear is retained."),5);
}
void AIslandHomeWardrobe::Tick(float Delta)
{
 Super::Tick(Delta);auto* PC=GetWorld()->GetFirstPlayerController();APawn* P=PC?PC->GetPawn():nullptr;auto* Focus=P?P->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this;Prompt->SetVisibility(Visible);if(!Visible)return;
 const auto* State=P->FindComponentByClass<UIslandOwnedAppearanceState>();Prompt->SetText(FText::FromString(TEXT("F  Owned wardrobe / ")+UIslandOwnedAppearanceState::PaletteName(State?State->GetSelectedPalette():0).ToString()));FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());
}
