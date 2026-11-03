#include "Wanted/WantedStatusWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
TSharedRef<SWidget> UWantedStatusWidget::RebuildWidget()
{
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
 Status=WidgetTree->ConstructWidget<UTextBlock>(); auto* StatusSlot=Canvas->AddChildToCanvas(Status);
 StatusSlot->SetAnchors(FAnchors(.5f,0)); StatusSlot->SetAlignment(FVector2D(.5f,0)); StatusSlot->SetPosition(FVector2D(0,28)); StatusSlot->SetAutoSize(true);
 auto Font=Status->GetFont(); Font.Size=22; Status->SetFont(Font); Status->SetShadowOffset(FVector2D(1,2));
 return Super::RebuildWidget();
}
void UWantedStatusWidget::UpdateStatus(int32 Level,bool bVisibleSuspect)
{
 if(!Status) return;
 Status->SetText(Level>0?FText::FromString(FString::Printf(TEXT("POLICE RESPONSE  %d / 5  |  %s"),Level,bVisibleSuspect?TEXT("IN SIGHT"):TEXT("SEARCHING"))):FText::GetEmpty());
 Status->SetColorAndOpacity(FSlateColor(bVisibleSuspect?FLinearColor(1,.35f,.2f):FLinearColor(.95f,.8f,.3f)));
}
