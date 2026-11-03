#include "Phone/PhoneWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
TSharedRef<SWidget> UPhoneWidget::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree=NewObject<UWidgetTree>(this); }
	UCanvasPanel* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget=Canvas;
	UBorder* Panel=WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.015f,0.025f,0.045f,0.96f));
	Panel->SetPadding(FMargin(26));
	auto* PanelSlot=Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1,0,1,1));PanelSlot->SetAlignment(FVector2D::ZeroVector);
	PanelSlot->SetOffsets(FMargin(-584,36,560,36));
 auto* Stack=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Stack);
 Scroll=WidgetTree->ConstructWidget<UScrollBox>();
 auto* ScrollSlot=Stack->AddChildToVerticalBox(Scroll);ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* PhotoFrame=WidgetTree->ConstructWidget<USizeBox>();PhotoFrame->SetWidthOverride(480);PhotoFrame->SetHeightOverride(270);
 Preview=WidgetTree->ConstructWidget<UImage>();PhotoFrame->SetContent(Preview);PhotoFrame->SetVisibility(ESlateVisibility::Collapsed);
 Scroll->AddChild(PhotoFrame);
	Body=WidgetTree->ConstructWidget<UTextBlock>();
	Body->SetAutoWrapText(true);
	Body->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f,0.96f,1.f)));
	FSlateFontInfo Font=Body->GetFont(); Font.Size=22; Body->SetFont(Font);
	Scroll->AddChild(Body);
 auto* Hint=WidgetTree->ConstructWidget<UTextBlock>();
 Hint->SetText(NSLOCTEXT("NeonCity","PhoneScrollHint","PG UP / PG DN  Scroll    P  Close"));
 Hint->SetColorAndOpacity(FSlateColor(FLinearColor(.55f,.7f,.76f)));
 Font.Size=18;Hint->SetFont(Font);Stack->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0,16,0,0));
	return Super::RebuildWidget();
}
void UPhoneWidget::SetPhoneText(const FText& Text)
{
 SetPhotoPreview(TEXT(""));
 if(Body)Body->SetText(Text);
 if(Scroll && !CurrentText.EqualTo(Text))Scroll->ScrollToStart();CurrentText=Text;
}
void UPhoneWidget::SetPhotoPreview(const FString& SavedPhotograph)
{
 if(!Preview)return;
 const bool Allowed=!SavedPhotograph.IsEmpty() && FPaths::IsUnderDirectory(SavedPhotograph,FPaths::ProjectSavedDir()/TEXT("IslandActivities/Photos"));
 if(Allowed && PreviewFile!=SavedPhotograph)
 {
  PreviewTexture=FImageUtils::ImportFileAsTexture2D(SavedPhotograph);PreviewFile=PreviewTexture?SavedPhotograph:TEXT("");
  Preview->SetBrushFromTexture(PreviewTexture,false);
 }
 if(auto* Frame=Preview->GetParent())Frame->SetVisibility(Allowed && PreviewTexture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
void UPhoneWidget::ScrollPage(float Direction)
{ if(Scroll && FMath::IsFinite(Direction))Scroll->SetScrollOffset(FMath::Max(0.f,Scroll->GetScrollOffset()+Direction*350)); }
float UPhoneWidget::GetScrollOffset() const { return Scroll?Scroll->GetScrollOffset():0; }
