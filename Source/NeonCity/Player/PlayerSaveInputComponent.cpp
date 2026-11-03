#include "Player/PlayerSaveInputComponent.h"
#include "Adventure/IslandLandGrill.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Adventure/DivingComponent.h"
#include "Phone/PhoneComponent.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UPlayerSaveInputComponent::SetupInput(UInputComponent* Component)
{
 auto* PC=Cast<APlayerController>(GetOwner());auto* Input=Cast<UEnhancedInputComponent>(Component);
 auto* S=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Input || !S || Mapping)return;
 Mapping=NewObject<UInputMappingContext>(this,TEXT("IMC_PlayerSave"));
 SaveAction=NewObject<UInputAction>(this,TEXT("IA_QuickSave"));LoadAction=NewObject<UInputAction>(this,TEXT("IA_QuickLoad"));
 SaveAction->ValueType=LoadAction->ValueType=EInputActionValueType::Boolean;
 Mapping->MapKey(SaveAction,EKeys::F6);Mapping->MapKey(LoadAction,EKeys::F7);
 Input->BindAction(SaveAction,ETriggerEvent::Started,this,&UPlayerSaveInputComponent::Save);
 Input->BindAction(LoadAction,ETriggerEvent::Started,this,&UPlayerSaveInputComponent::Load);S->AddMappingContext(Mapping,1);
 Notify(TEXT("F6 Save   |   F7 Load\nSave on dry land or in a parked vehicle."));
}
bool UPlayerSaveInputComponent::CanOperate(bool AllowGrillSave) const
{
 const auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->GetPawn())return false;
 const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();const auto* Fishing=PC->FindComponentByClass<UFishingComponent>();
 const auto* Diving=PC->GetPawn()->FindComponentByClass<UDivingComponent>();
 const bool SavedGrill=AllowGrillSave && Fishing && Cast<AIslandLandGrill>(Fishing->GetActiveStation());
 return (!Phone || !Phone->IsPhoneOpen()) && (!Fishing || !Fishing->IsActive() || SavedGrill) && (!Diving || !Diving->IsChangingGear());
}
void UPlayerSaveInputComponent::Save()
{
 if(!CanOperate(true))return;auto* S=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
 Notify(S->SaveGame()?TEXT("Game saved. Diving gear ownership retained."):TEXT("Unable to save here. Stand safely on dry land and try again."));
}
void UPlayerSaveInputComponent::Load()
{
 if(!CanOperate())return;auto* S=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
 Notify(S->LoadGame()?TEXT("Game loaded. Hand-held equipment restored safely stowed."):TEXT("Unable to load. ")+S->GetLastDetail());
}
void UPlayerSaveInputComponent::Notify(const FString& Message)
{
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC)return;
 if(!Notice)
 {
  Notice=CreateWidget<UPlayerSaveNotice>(PC);Notice->WidgetTree=NewObject<UWidgetTree>(Notice);
  auto* Canvas=Notice->WidgetTree->ConstructWidget<UCanvasPanel>();Notice->WidgetTree->RootWidget=Canvas;
  auto* Panel=Notice->WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.01f,.025f,.035f,.85f));Panel->SetPadding(FMargin(14,10));
  auto* Slot=Canvas->AddChildToCanvas(Panel);Slot->SetAnchors(FAnchors(1,0));Slot->SetAlignment(FVector2D(1,0));Slot->SetPosition(FVector2D(-28,28));Slot->SetAutoSize(true);
  Label=Notice->WidgetTree->ConstructWidget<UTextBlock>();auto Font=Label->GetFont();Font.Size=14;Label->SetFont(Font);Panel->SetContent(Label);
 }
 Label->SetText(FText::FromString(Message));if(!Notice->IsInViewport())Notice->AddToViewport(40);Notice->SetVisibility(ESlateVisibility::HitTestInvisible);
 GetWorld()->GetTimerManager().SetTimer(HideTimer,this,&UPlayerSaveInputComponent::Hide,7,false);
}
void UPlayerSaveInputComponent::Hide(){if(Notice)Notice->RemoveFromParent();}
void UPlayerSaveInputComponent::EndPlay(EEndPlayReason::Type Reason)
{
 Hide();GetWorld()->GetTimerManager().ClearTimer(HideTimer);
 if(auto* PC=Cast<APlayerController>(GetOwner());PC && PC->GetLocalPlayer())if(auto* S=PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())S->RemoveMappingContext(Mapping);
 Super::EndPlay(Reason);
}
