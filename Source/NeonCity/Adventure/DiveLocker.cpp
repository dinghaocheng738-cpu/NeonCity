#include "Adventure/DiveLocker.h"
#include "Adventure/DivingComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
ADiveLocker::ADiveLocker()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiveEquipment"));SetRootComponent(Visual);Visual->SetCollisionProfileName(TEXT("BlockAll"));
 // Broad, waist-height service area: the tiny cylinder valves are not an aiming puzzle.
 InteractionTarget=CreateDefaultSubobject<UBoxComponent>(TEXT("EquipmentInteraction"));InteractionTarget->SetupAttachment(Visual);
 InteractionTarget->SetBoxExtent(FVector(50,50,65));InteractionTarget->SetRelativeLocation(FVector(0,0,65));
 InteractionTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);InteractionTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
 InteractionTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);InteractionTarget->SetCanEverAffectNavigation(false);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("GearStationPrompt"));Prompt->SetupAttachment(Visual);
 Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetWorldSize(8);Prompt->SetTextRenderColor(FColor(205,244,235));Prompt->SetVisibility(false);
}
bool ADiveLocker::CanInteract_Implementation(AActor* V) const
{
 const auto* Kit=V?V->FindComponentByClass<UDivingComponent>():nullptr;
 return Kit && V->GetWorld()==GetWorld() && FVector::Dist(V->GetActorLocation(),GetActorLocation())<300 && Kit->CanChangeGear();
}
FText ADiveLocker::GetInteractionName_Implementation() const
{
 const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;const APawn* P=PC?PC->GetPawn():nullptr;
 const auto* Kit=P?P->FindComponentByClass<UDivingComponent>():nullptr;
 return FText::FromString(Kit && Kit->IsEquipped()?TEXT("Remove Diving Gear"):TEXT("Equip Diving Gear"));
}
void ADiveLocker::Interact_Implementation(AActor* V){if(CanInteract_Implementation(V))V->FindComponentByClass<UDivingComponent>()->BeginGearChange();}
void ADiveLocker::BeginPlay()
{
 Super::BeginPlay();if(GetClass()!=StaticClass()){SetActorTickEnabled(false);return;}
 InteractionTarget->SetBoxExtent(FVector(104,48,102));InteractionTarget->SetRelativeLocation(FVector(0,0,103));
 Prompt->SetRelativeLocation(FVector(0,48,218));
}
void ADiveLocker::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);if(GetClass()!=StaticClass())return;
 auto* PC=GetWorld()->GetFirstPlayerController();APawn* P=PC?PC->GetPawn():nullptr;
 const auto* Kit=P?P->FindComponentByClass<UDivingComponent>():nullptr;
 const auto* Interaction=P?P->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Near=Kit && FVector::Dist(P->GetActorLocation(),GetActorLocation())<550;
 // World lettering washes out against the sunlit sea. Keep the nearby action
 // in a compact, consistently legible panel, clear of the left diving HUD.
 Prompt->SetVisibility(false);
 if(!Near){if(ScreenPrompt)ScreenPrompt->SetVisibility(ESlateVisibility::Collapsed);return;}
 if(!ScreenPrompt && PC->IsLocalController())
 {
  ScreenPrompt=CreateWidget<UDivingStationPrompt>(PC);ScreenPrompt->WidgetTree=NewObject<UWidgetTree>(ScreenPrompt);
  auto* Canvas=ScreenPrompt->WidgetTree->ConstructWidget<UCanvasPanel>();ScreenPrompt->WidgetTree->RootWidget=Canvas;
  auto* Panel=ScreenPrompt->WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.008f,.025f,.033f,.92f));Panel->SetPadding(FMargin(20,12));
  auto* Slot=Canvas->AddChildToCanvas(Panel);Slot->SetAnchors(FAnchors(.5f,1));Slot->SetAlignment(FVector2D(.5f,1));Slot->SetPosition(FVector2D(0,-32));Slot->SetAutoSize(true);
  PromptLabel=ScreenPrompt->WidgetTree->ConstructWidget<UTextBlock>();auto Font=PromptLabel->GetFont();Font.Size=18;PromptLabel->SetFont(Font);
  PromptLabel->SetJustification(ETextJustify::Center);PromptLabel->SetColorAndOpacity(FSlateColor(FLinearColor(.82f,.97f,.94f)));Panel->SetContent(PromptLabel);ScreenPrompt->AddToViewport(35);
 }
 const bool Focused=Interaction && Interaction->GetFocusedInteractable()==this && CanInteract_Implementation(P);
 const FString Caption=Kit->IsChangingGear()?TEXT("Checking diving equipment..."):
  Focused?TEXT("F - ")+GetInteractionName_Implementation().ToString():TEXT("Diving Gear\nApproach the service bay");
 if(ScreenPrompt){ScreenPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);PromptLabel->SetText(FText::FromString(Caption));}
}
void ADiveLocker::EndPlay(EEndPlayReason::Type Reason)
{
 if(ScreenPrompt)ScreenPrompt->RemoveFromParent();ScreenPrompt=nullptr;PromptLabel=nullptr;
 Super::EndPlay(Reason);
}
