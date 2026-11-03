#include "Fishing/FishingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Fishing/FishingFightModel.h"
#include "Fishing/FishingSessionModel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
TSharedRef<SWidget> UFishingWidget::RebuildWidget()
{
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
 auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.015,.03,.045,.88));Panel->SetPadding(FMargin(16));
 auto* PanelSlot=Canvas->AddChildToCanvas(Panel);PanelSlot->SetAnchors(FAnchors(0,1));PanelSlot->SetAlignment(FVector2D(0,1));PanelSlot->SetPosition(FVector2D(28,-36));PanelSlot->SetAutoSize(true);
 auto* Width=WidgetTree->ConstructWidget<USizeBox>();Width->SetWidthOverride(320);Panel->SetContent(Width);
 auto* Stack=WidgetTree->ConstructWidget<UVerticalBox>();Width->SetContent(Stack);
 Body=WidgetTree->ConstructWidget<UTextBlock>();Body->SetAutoWrapText(true);Body->SetColorAndOpacity(FSlateColor(FLinearColor(.9,.97,1)));
 auto Font=Body->GetFont();Font.Size=16;Body->SetFont(Font);Stack->AddChildToVerticalBox(Body)->SetPadding(FMargin(0,0,0,10));
 CastMeter=WidgetTree->ConstructWidget<USizeBox>();CastMeter->SetHeightOverride(38);Stack->AddChildToVerticalBox(CastMeter);
 auto* Track=WidgetTree->ConstructWidget<UCanvasPanel>();CastMeter->SetContent(Track);
 const auto Zone=[&](float X,float W,FLinearColor Color)
 {
  auto* Z=WidgetTree->ConstructWidget<UBorder>();Z->SetBrushColor(Color);auto* Slot=Track->AddChildToCanvas(Z);Slot->SetPosition(FVector2D(X,6));Slot->SetSize(FVector2D(W,8));return Slot;
 };
 Zone(0,300,FLinearColor(.17,.23,.25));Zone(150,96,FLinearColor(.12,.48,.39));
 Zone(FNeonCityFishingSession::PerfectLow*300,(FNeonCityFishingSession::PerfectHigh-FNeonCityFishingSession::PerfectLow)*300,FLinearColor(.92,.65,.19));
 CastCursor=Zone(0,3,FLinearColor(.96,.99,1));CastCursor->SetSize(FVector2D(3,18));CastCursor->SetPosition(FVector2D(0,1));
 auto* Labels=WidgetTree->ConstructWidget<UTextBlock>();auto Small=Font;Small.Size=11;Labels->SetFont(Small);Labels->SetText(FText::FromString(TEXT("WEAK                        GOOD                 PERFECT")));auto* LabelSlot=Track->AddChildToCanvas(Labels);LabelSlot->SetPosition(FVector2D(0,21));LabelSlot->SetAutoSize(true);
 TensionLabel=WidgetTree->ConstructWidget<UTextBlock>();TensionLabel->SetText(NSLOCTEXT("NeonCity","FishingTension","LINE TENSION"));Font.Size=16;TensionLabel->SetFont(Font);Stack->AddChildToVerticalBox(TensionLabel);
 TensionMeter=WidgetTree->ConstructWidget<USizeBox>();TensionMeter->SetHeightOverride(24);Stack->AddChildToVerticalBox(TensionMeter)->SetPadding(FMargin(0,7,0,8));
 Track=WidgetTree->ConstructWidget<UCanvasPanel>();TensionMeter->SetContent(Track);
 Zone(0,300,FLinearColor(.47,.17,.09));Zone(0,FNeonCityFishFight::BandLow*300,FLinearColor(.5,.36,.14));
 Zone(FNeonCityFishFight::BandLow*300,(FNeonCityFishFight::BandHigh-FNeonCityFishFight::BandLow)*300,FLinearColor(.09,.58,.32));
 TensionCursor=Zone(0,3,FLinearColor(.97,.99,1));TensionCursor->SetSize(FVector2D(3,19));TensionCursor->SetPosition(FVector2D(0,0));
 ProgressLabel=WidgetTree->ConstructWidget<UTextBlock>();ProgressLabel->SetText(NSLOCTEXT("NeonCity","FishingReelProgress","REELED IN"));ProgressLabel->SetFont(Font);Stack->AddChildToVerticalBox(ProgressLabel);
 ProgressBar=WidgetTree->ConstructWidget<UProgressBar>();ProgressBar->SetFillColorAndOpacity(FLinearColor(.1,.75,.68));Stack->AddChildToVerticalBox(ProgressBar)->SetPadding(FMargin(0,7,0,0));
 return Super::RebuildWidget();
}
void UFishingWidget::ShowState(const FText& Text,bool Fighting,float Tension,float Progress,bool Charging,float Power)
{
 if(!Body)return;Body->SetText(Text);
 CastMeter->SetVisibility(Charging?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 CastCursor->SetPosition(FVector2D(FMath::Clamp(Power,0.f,1.f)*297,1));
 const auto BarVisibility=Fighting?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed;
 TensionLabel->SetVisibility(BarVisibility);ProgressLabel->SetVisibility(BarVisibility);TensionMeter->SetVisibility(BarVisibility);ProgressBar->SetVisibility(BarVisibility);
 TensionCursor->SetPosition(FVector2D(FMath::Clamp(Tension,0.f,1.f)*297,0));ProgressBar->SetPercent(FMath::Clamp(Progress,0.f,1.f));
}
