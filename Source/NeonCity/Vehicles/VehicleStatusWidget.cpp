#include "Vehicles/VehicleStatusWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
TSharedRef<SWidget> UVehicleStatusWidget::RebuildWidget()
{
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
 auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetPadding(FMargin(12,8));
 Panel->SetBrushColor(FLinearColor(.015f,.025f,.04f,.8f));
 Status=WidgetTree->ConstructWidget<UTextBlock>();Panel->SetContent(Status);
 auto* StatusSlot=Canvas->AddChildToCanvas(Panel);
 StatusSlot->SetAnchors(FAnchors(0,1)); StatusSlot->SetAlignment(FVector2D(0,1)); StatusSlot->SetPosition(FVector2D(24,-30)); StatusSlot->SetAutoSize(true);
 auto Font=Status->GetFont(); Font.Size=18; Status->SetFont(Font);
 Status->SetShadowColorAndOpacity(FLinearColor(0,0,0,.9f));Status->SetShadowOffset(FVector2D(1,1));return Super::RebuildWidget();
}
void UVehicleStatusWidget::UpdateStatus(const FText& Text,bool bDamage)
{ if(Status) { Status->SetText(Text); Status->SetColorAndOpacity(FSlateColor(bDamage?FLinearColor(1,.3f,.15f):FLinearColor::White)); } }
