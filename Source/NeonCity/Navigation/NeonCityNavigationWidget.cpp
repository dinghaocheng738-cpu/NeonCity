#include "Navigation/NeonCityNavigationWidget.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Navigation/NeonCityWorldMapWidget.h"
#include "Navigation/NeonCityMapData.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "GameFramework/Pawn.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

TSharedRef<SWidget> UNeonCityNavigationWidget::RebuildWidget() { return SNew(SSpacer); }
int32 UNeonCityNavigationWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
 FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const
{
 Layer=Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,ParentEnabled);
 if(!Source.IsValid() || GetRenderOpacity()<=0.f)return Layer;
 const auto& Display=Source->GetDisplay();
 const FVector2D PanelSize(340,294),Panel=Geometry.GetLocalSize()-PanelSize-FVector2D(24,30);
 const FVector2D MapOrigin=Panel+FVector2D(12,34),MapSize(316,196),Center=MapOrigin+MapSize*.5;
 constexpr double Scale=.018; // 1 px / 55.6 cm; north is authored world +Y.
 const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 const FLinearColor Text(.9f,.96f,1),Muted(.54f,.67f,.73f),Amber(1,.64f,.16f),Teal(.1f,.9f,.8f);
 auto Box=[&](FVector2D P,FVector2D Size,FLinearColor Color,int32 Depth)
 { FSlateDrawElement::MakeBox(Elements,Layer+Depth,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint()); };
 auto Label=[&](const FString& Value,FVector2D P,int32 Size,FLinearColor Color)
 { FSlateDrawElement::MakeText(Elements,Layer+7,Geometry.ToPaintGeometry(FVector2D(316,28),FSlateLayoutTransform(P)),Value,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size),ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint()); };
 auto Project=[&](const FVector& P){const FVector D=P-Display.Origin;return Center+FVector2D(D.X,-D.Y)*Scale;};
 auto Line=[&](const TArray<FVector2D>& P,FLinearColor Color,float Width,int32 Depth)
 { FSlateDrawElement::MakeLines(Elements,Layer+Depth,Geometry.ToPaintGeometry(),P,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint(),true,Width); };
 Box(Panel,PanelSize,FLinearColor(.012f,.025f,.04f,.94f),1);
 const UNeonCityMapData* Map=Source->GetMapData();
 if(Map && !Map->Contains(Display.Origin))Map=nullptr;
 if(Map)
 {
  // painted map, north up, ~300 m across, centred on the player
  constexpr double MetresAcross=300.;
  const double Mpp=MetresAcross/MapSize.X,SideM=(Map->WorldMax.X-Map->WorldMin.X)/100.;
  const FVector2D UV=Map->ToUV(Display.Origin),Half(MapSize.X*.5*Mpp/SideM,MapSize.Y*.5*Mpp/SideM);
  Label(Map->Title.IsEmpty()?TEXT("MAP"):Map->Title.ToString().ToUpper(),Panel+FVector2D(12,8),16,Muted);
  Label(TEXT("N"),Panel+FVector2D(312,8),16,Text);
  Box(MapOrigin,MapSize,FLinearColor(.83f,.84f,.8f),2);
  NeonCityMapPaint::Texture(Elements,Layer+3,Geometry,MapBrush,*Map,MapOrigin,MapSize,UV-Half,UV+Half,FLinearColor::White);
  Elements.PushClip(FSlateClippingZone(Geometry.MakeChild(MapSize,FSlateLayoutTransform(MapOrigin))));
  auto ToScreen=[&](const FVector& P){return Center+FVector2D(Display.Origin.X-P.X,Display.Origin.Y-P.Y)/(Mpp*100.);};
  const FLinearColor Route(.13f,.47f,.95f),Accent(1.f,.55f,.08f);
  if(Display.Active)NeonCityMapPaint::Route(Elements,Layer+4,Geometry,Display.Route,ToScreen,4,Route);
  for(const auto& P:Map->Places)
  {
   if(P.Kind==TEXT("area"))continue;
   const FVector2D At=ToScreen(FVector(P.Location,0));
   if(P.Kind==TEXT("home"))NeonCityMapPaint::Home(Elements,Layer+5,Geometry,At,12,NeonCityMapPaint::PlaceColor(P.Kind));
   else NeonCityMapPaint::Dot(Elements,Layer+5,Geometry,At,7,NeonCityMapPaint::PlaceColor(P.Kind),FLinearColor::White);
  }
  if(Display.Active)
  {
   // the pin, or an arrow on the rim pointing to it
   FVector2D G=ToScreen(Display.Goal)-Center;const FVector2D Bound=MapSize*.5-FVector2D(14,14);
   const double K=FMath::Max(1.,FMath::Max(FMath::Abs(G.X)/Bound.X,FMath::Abs(G.Y)/Bound.Y));
   if(K<=1.)NeonCityMapPaint::Pin(Elements,Layer+6,Geometry,Center+G,16,Accent);
   else
   {
    const FVector2D At=Center+G/K,F=G.GetSafeNormal(),R(-F.Y,F.X);
    Line({At+F*9,At-F*6+R*7,At-F*6-R*7,At+F*9},Accent,3,7);
   }
  }
  const double Yaw=FMath::DegreesToRadians(Display.Heading);
  NeonCityMapPaint::Player(Elements,Layer+7,Geometry,Center,FVector2D(-FMath::Cos(Yaw),-FMath::Sin(Yaw)),11,Route);
  Elements.PopClip();
 }
 else
 {
 Label(TEXT("SOUTH BEACH"),Panel+FVector2D(12,8),16,Muted);
 Label(TEXT("N"),Panel+FVector2D(312,8),16,Text);
 Box(MapOrigin,MapSize,FLinearColor(.035f,.075f,.09f,.97f),2);
 Elements.PushClip(FSlateClippingZone(Geometry.MakeChild(MapSize,FSlateLayoutTransform(MapOrigin))));
 for(const auto& Road:Source->GetRoadGuide().GetRoads())
 {
  TArray<FVector2D> Points;Points.Reserve(Road.Points.Num()+1);
  for(const auto& P:Road.Points)Points.Add(Project(P));
  if(Road.Closed && !Points.IsEmpty()) { const FVector2D First=Points[0];Points.Add(First); }
  Line(Points,FLinearColor(.25f,.37f,.42f),6,3);
 }
 if(Display.Active)
 {
  TArray<FVector2D> Points;Points.Reserve(Display.Route.Num());
  for(const auto& P:Display.Route)Points.Add(Project(P));
  Line(Points,Display.UsingRoad?Amber:Teal,3,4);
  FVector2D D=Project(Display.Goal)-Center;
  const FVector2D Bound=MapSize*.5-FVector2D(10,10);
  D/=FMath::Max(1.,FMath::Max(FMath::Abs(D.X)/Bound.X,FMath::Abs(D.Y)/Bound.Y));
  const FVector2D Goal=Center+D;
  Line({Goal+FVector2D(0,-7),Goal+FVector2D(7,0),Goal+FVector2D(0,7),Goal+FVector2D(-7,0),Goal+FVector2D(0,-7)},Teal,2.5f,5);
 }
 const double R=FMath::DegreesToRadians(Display.Heading);
 const FVector2D Forward(FMath::Cos(R),-FMath::Sin(R)),Right(-Forward.Y,Forward.X);
 Line({Center+Forward*10,Center-Forward*7+Right*6,Center-Forward*3,Center-Forward*7-Right*6,Center+Forward*10},Text,2.5f,6);
 Elements.PopClip();
 }
 if(Display.Active)PaintWorldMarker(Elements,Layer+8,Geometry);
 Label(Display.Active?Display.Label.ToString().Left(32):TEXT("Explore the neighborhood"),Panel+FVector2D(12,238),18,Text);
 if(Display.Active)
 {
  const FVector D=Display.Goal-Display.Origin;
  const TCHAR* Directions[]={TEXT("N"),TEXT("NE"),TEXT("E"),TEXT("SE"),TEXT("S"),TEXT("SW"),TEXT("W"),TEXT("NW")};
  const int32 Index=(FMath::RoundToInt(FMath::RadiansToDegrees(FMath::Atan2(D.X,D.Y))/45)+8)%8;
  Label(FString::Printf(TEXT("%.0f m %s | %s"),Display.DistanceMeters,Directions[Index],*Display.RouteHint.ToString()),Panel+FVector2D(12,266),14,Muted);
 }
 else
 {
  const auto* Pawn=GetOwningPlayerPawn();
  const auto* Light=Pawn?Pawn->FindComponentByClass<UNeonCityFlashlightComponent>():nullptr;
  const FString MapHint=Source->GetMapData()?TEXT("  |  M  Map"):TEXT("");
  Label((Light?(Light->IsEquipped()?TEXT("P  Phone  |  L  Stow light"):TEXT("P  Phone  |  L  Flashlight")):TEXT("P  Phone and opportunities"))+MapHint,Panel+FVector2D(12,266),14,Muted);
 }
 if(!Display.Notice.IsEmpty())
 {
  const auto Font=FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),16);
  const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
  TArray<FString> Lines,Paragraphs;Display.Notice.ToString().ParseIntoArray(Paragraphs,TEXT("\n"),false);
  for(const FString& Paragraph:Paragraphs)
  {
   TArray<FString> Words;Paragraph.ParseIntoArrayWS(Words);FString Current;
   for(const FString& Word:Words)
   {
    const FString Candidate=Current.IsEmpty()?Word:Current+TEXT(" ")+Word;
    if(!Current.IsEmpty() && Measure->Measure(Candidate,Font).X>316){Lines.Add(Current);Current.Reset();}
    // Split an unusually long single word as well; never let a notice run
    // beyond the panel or discard the end of the player's instructions.
    for(TCHAR Character:Word)
    {
     if(!Current.IsEmpty() && Measure->Measure(Current+FString::Chr(Character),Font).X>316){Lines.Add(Current);Current.Reset();}
     Current.AppendChar(Character);
    }
    Current+=TEXT(" ");
   }
   Current.TrimEndInline();Lines.Add(Current);
  }
  const double Height=24+Lines.Num()*22;
  const FVector2D Origin=Panel-FVector2D(0,Height+8);
  Box(Origin,FVector2D(340,Height),FLinearColor(.012f,.025f,.04f,.96f),1);
  for(int32 I=0;I<Lines.Num();++I)Label(Lines[I],Origin+FVector2D(12,12+I*22),16,Teal);
 }
 return Layer+12;
}
void UNeonCityNavigationWidget::PaintWorldMarker(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry) const
{
 // A marker over the destination in the world view (or on the screen edge pointing to it): pin, name and distance.
 const auto& Display=Source->GetDisplay();APlayerController* PC=GetOwningPlayer();
 if(!PC || !PC->PlayerCameraManager)return;
 const FVector Goal=Display.Goal+FVector(0,0,250);
 const FVector Cam=PC->PlayerCameraManager->GetCameraLocation();const FRotator Rot=PC->PlayerCameraManager->GetCameraRotation();
 const FVector Local=Rot.UnrotateVector(Goal-Cam);
 const FVector2D Size=Geometry.GetLocalSize(),Mid=Size*.5,Inset(60,70);
 FVector2D At;bool OnScreen=false;
 if(Local.X>10 && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Goal,At,false))
  OnScreen=At.X>Inset.X && At.Y>Inset.Y && At.X<Size.X-Inset.X && At.Y<Size.Y-Inset.Y;
 FVector2D Dir;
 if(OnScreen)Dir=FVector2D::ZeroVector;
 else
 {
  Dir=Local.X>10?(At-Mid):FVector2D(Local.Y,-Local.Z);                    // behind: its side, pushed to the edge
  if(Local.X<=10 && FMath::Abs(Dir.X)<1)Dir.X=1;
  if(Local.X<=10)Dir.Y=FMath::Max(Dir.Y,FMath::Abs(Dir.X)*.6);
  Dir=Dir.GetSafeNormal();
  const FVector2D B=Mid-Inset;const double K=FMath::Min(B.X/FMath::Max(1e-3,FMath::Abs(Dir.X)),B.Y/FMath::Max(1e-3,FMath::Abs(Dir.Y)));
  At=Mid+Dir*K;
 }
 const FLinearColor Accent(1.f,.55f,.08f);
 if(OnScreen)NeonCityMapPaint::Pin(Elements,Layer,Geometry,At,26,Accent);
 else
 {
  const FVector2D R(-Dir.Y,Dir.X);
  FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),{At+Dir*16,At-Dir*8+R*12,At-Dir*8-R*12,At+Dir*16},ESlateDrawEffect::None,FLinearColor(0,0,0,.5f),true,6);
  FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),{At+Dir*16,At-Dir*8+R*12,At-Dir*8-R*12,At+Dir*16},ESlateDrawEffect::None,Accent,true,3);
 }
 const FString Txt=FString::Printf(TEXT("%s  %.0f m"),*Display.Label.ToString().Left(28),Display.DistanceMeters);
 const auto Font=FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),14);
 const FVector2D TS=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Txt,Font);
 FVector2D TP=At+FVector2D(-TS.X*.5,OnScreen?-62:(Dir.Y>0?-38:22));
 TP.X=FMath::Clamp(TP.X,8.,Size.X-TS.X-8);
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 FSlateDrawElement::MakeBox(Elements,Layer+2,Geometry.ToPaintGeometry(TS+FVector2D(14,6),FSlateLayoutTransform(TP-FVector2D(7,3))),White,ESlateDrawEffect::None,FLinearColor(.02f,.03f,.05f,.72f));
 FSlateDrawElement::MakeText(Elements,Layer+3,Geometry.ToPaintGeometry(TS+FVector2D(2,2),FSlateLayoutTransform(TP)),Txt,Font,ESlateDrawEffect::None,FLinearColor::White);
}
