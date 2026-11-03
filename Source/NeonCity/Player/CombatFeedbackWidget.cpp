#include "Player/CombatFeedbackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
TSharedRef<SWidget> UCombatFeedbackWidget::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree=NewObject<UWidgetTree>(this); }
	auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
	Crosshair=WidgetTree->ConstructWidget<UTextBlock>();
	auto* Center=Canvas->AddChildToCanvas(Crosshair);
	Center->SetAnchors(FAnchors(.5,.5)); Center->SetAlignment(FVector2D(.5,.5)); Center->SetAutoSize(true);
	auto Font=Crosshair->GetFont(); Font.Size=28; Crosshair->SetFont(Font);
	Status=WidgetTree->ConstructWidget<UTextBlock>();
	auto* Bottom=Canvas->AddChildToCanvas(Status);
	Bottom->SetAnchors(FAnchors(.5,1)); Bottom->SetAlignment(FVector2D(.5,1)); Bottom->SetPosition(FVector2D(0,-38)); Bottom->SetAutoSize(true);
	Font.Size=22; Status->SetFont(Font);
	Damage=WidgetTree->ConstructWidget<UTextBlock>();
	auto* Above=Canvas->AddChildToCanvas(Damage);
	Above->SetAnchors(FAnchors(.5,.5)); Above->SetAlignment(FVector2D(.5,1)); Above->SetPosition(FVector2D(0,-46)); Above->SetAutoSize(true);
	Font.Size=24; Damage->SetFont(Font); Damage->SetColorAndOpacity(FSlateColor(FLinearColor(1,.25f,.2f))); Damage->SetVisibility(ESlateVisibility::Collapsed);
	Vitals=WidgetTree->ConstructWidget<UTextBlock>();
	auto* Top=Canvas->AddChildToCanvas(Vitals);
	Top->SetAnchors(FAnchors(.5,0)); Top->SetAlignment(FVector2D(.5,0)); Top->SetPosition(FVector2D(0,64)); Top->SetAutoSize(true);
	Font.Size=30; Vitals->SetFont(Font); Vitals->SetVisibility(ESlateVisibility::Collapsed);
	return Super::RebuildWidget();
}
void UCombatFeedbackWidget::UpdateDamage(const FText& Text,bool bActive)
{
	if (!Damage) { return; }
	Damage->SetText(Text); Damage->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
void UCombatFeedbackWidget::UpdateVitals(const FText& Text,const FLinearColor& Color,bool bActive)
{
	if (!Vitals) { return; }
	Vitals->SetText(Text); Vitals->SetColorAndOpacity(FSlateColor(Color));
	Vitals->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
void UCombatFeedbackWidget::UpdateStatus(const FText& Text,bool Hit,bool Aim)
{
	if (Status) { Status->SetText(Text); }
	if (Crosshair)
	{
		Crosshair->SetText(FText::FromString(Hit ? TEXT("X") : Aim ? TEXT("+") : TEXT(".")));
		Crosshair->SetColorAndOpacity(FSlateColor(Hit ? FLinearColor::Red : FLinearColor::White));
	}
}
