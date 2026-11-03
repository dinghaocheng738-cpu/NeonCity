#include "Leisure/NeonCityActivityHUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

void UNeonCityActivityHUDSubsystem::EnsureWidget()
{
 if(Widget || !GetWorld() || !GetWorld()->IsGameWorld())return;
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->IsLocalController())return;
 Widget=CreateWidget<UNeonCityActivityHUDWidget>(PC);if(!Widget)return;
 Widget->SetSource(this);Widget->AddToViewport(6);
}
void UNeonCityActivityHUDSubsystem::Show(FName Owner,const FNeonCityActivityHUDState& Value)
{
 if(Owner.IsNone())return;
 Current=Owner;State=Value;EnsureWidget();
}
void UNeonCityActivityHUDSubsystem::Clear(FName Owner)
{
 if(Owner!=Current)return;
 Current=NAME_None;State=FNeonCityActivityHUDState();
}
void UNeonCityActivityHUDSubsystem::Banner(const FText& Text,const FText& Detail,FLinearColor Color,float Seconds)
{
 if(!GetWorld())return;
 BannerText=Text;BannerDetail=Detail;BannerColor=Color;BannerLength=FMath::Max(.3f,Seconds);
 BannerUntil=GetWorld()->GetRealTimeSeconds()+BannerLength;EnsureWidget();
}
bool UNeonCityActivityHUDSubsystem::GetBanner(FText& Text,FText& Detail,FLinearColor& Color,float& Alpha) const
{
 if(!GetWorld())return false;
 const double Left=BannerUntil-GetWorld()->GetRealTimeSeconds();if(Left<=0 || BannerText.IsEmpty())return false;
 Text=BannerText;Detail=BannerDetail;Color=BannerColor;
 Alpha=float(FMath::Min(1.,Left/.5))*float(FMath::Min(1.,(BannerLength-Left)/.12+.2));return true;
}
void UNeonCityActivityHUDSubsystem::Deinitialize()
{
 if(Widget)Widget->RemoveFromParent();Widget=nullptr;
 Super::Deinitialize();
}

TSharedRef<SWidget> UNeonCityActivityHUDWidget::RebuildWidget() { return SNew(SSpacer); }
int32 UNeonCityActivityHUDWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
 FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const
{
 Layer=Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,ParentEnabled);
 if(!Source.IsValid())return Layer;
 const FVector2D Size=Geometry.GetLocalSize();
 const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 auto Box=[&](FVector2D P,FVector2D S,FLinearColor Color,int32 Depth)
 { FSlateDrawElement::MakeBox(Elements,Layer+Depth,Geometry.ToPaintGeometry(S,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint()); };
 // text centred on X at top Y
 auto Centred=[&](const FString& Value,double X,double Y,int32 Points,FLinearColor Color,const TCHAR* Face=TEXT("Regular"))
 {
  const FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle(Face,Points);const FVector2D W=Measure->Measure(Value,Font);
  FSlateDrawElement::MakeText(Elements,Layer+6,Geometry.ToPaintGeometry(W,FSlateLayoutTransform(FVector2D(X-W.X*.5,Y))),Value,Font,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());
  return W;
 };
 const FLinearColor Text(.93f,.96f,1),Muted(.6f,.68f,.74f);
 const UNeonCityActivityHUDSubsystem& S=*Source;
 if(!S.GetOwnerID().IsNone())
 {
  const FNeonCityActivityHUDState& St=S.GetState();
  const double W=420,X=(Size.X-W)*.5,Y=26;
  const double H=96+St.Lines.Num()*22+(St.Progress>=0?14:0);
  Box(FVector2D(X,Y),FVector2D(W,H),FLinearColor(.01f,.018f,.03f,.82f),1);
  Box(FVector2D(X,Y),FVector2D(W,3),St.Accent,2);
  Centred(St.Title.ToString().ToUpper(),Size.X*.5,Y+10,12,Muted,TEXT("Bold"));
  Centred(St.Clock.ToString(),Size.X*.5,Y+26,30,Text,TEXT("Bold"));
  Centred(St.Sub.ToString(),Size.X*.5,Y+68,13,St.Accent);
  double LY=Y+92;
  for(const FText& L:St.Lines){Centred(L.ToString(),Size.X*.5,LY,11,Muted);LY+=22;}
  if(St.Progress>=0)
  {
   Box(FVector2D(X+16,Y+H-12),FVector2D(W-32,4),FLinearColor(1,1,1,.12f),2);
   Box(FVector2D(X+16,Y+H-12),FVector2D((W-32)*FMath::Clamp(St.Progress,0.f,1.f),4),St.Accent,3);
  }
  if(St.bFraming)
  {
   // viewfinder corners round the centre of the view; they close in and turn the accent colour as the hold fills
   const float F=FMath::Clamp(St.FramingProgress,0.f,1.f);
   const FVector2D C=Size*.5,Half(FMath::Lerp(Size.X*.24,Size.X*.17,F),FMath::Lerp(Size.Y*.24,Size.Y*.17,F));
   const FLinearColor Col=FMath::Lerp(FLinearColor(1,1,1,.75f),St.Accent,F);
   const double Arm=38;
   for(int32 SX=-1;SX<=1;SX+=2)for(int32 SY=-1;SY<=1;SY+=2)
   {
    const FVector2D P=C+FVector2D(SX*Half.X,SY*Half.Y);
    const TArray<FVector2D> Corner={P-FVector2D(SX*Arm,0),P,P-FVector2D(0,SY*Arm)};
    FSlateDrawElement::MakeLines(Elements,Layer+5,Geometry.ToPaintGeometry(),Corner,ESlateDrawEffect::None,Col*Style.GetColorAndOpacityTint(),true,3);
   }
   Centred(St.FramingLabel.ToString(),C.X,C.Y+Half.Y+14,13,Col);
  }
 }
 FText BText,BDetail;FLinearColor BColor;float Alpha=0;
 if(S.GetBanner(BText,BDetail,BColor,Alpha))
 {
  BColor.A*=Alpha;FLinearColor D=Text;D.A*=Alpha;
  const double Y=Size.Y*.34;
  Box(FVector2D(0,Y-12),FVector2D(Size.X,BDetail.IsEmpty()?92:124),FLinearColor(0,0,0,.35f*Alpha),1);
  Centred(BText.ToString(),Size.X*.5,Y,48,BColor,TEXT("Bold"));
  if(!BDetail.IsEmpty())Centred(BDetail.ToString(),Size.X*.5,Y+72,16,D);
 }
 return Layer+7;
}
