#include "Navigation/NeonCityWorldMapWidget.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Navigation/NeonCityMapData.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace NeonCityMapPaint
{
FLinearColor PlaceColor(FName Kind)
{
 if(Kind==TEXT("home"))return FLinearColor(1.f,.62f,.12f);
 if(Kind==TEXT("temple"))return FLinearColor(.86f,.3f,.2f);
 if(Kind==TEXT("landmark"))return FLinearColor(.72f,.24f,.2f);
 if(Kind==TEXT("scenic"))return FLinearColor(.12f,.58f,.5f);
 if(Kind==TEXT("boat"))return FLinearColor(.16f,.42f,.86f);                 // rowboats you can take out
 return FLinearColor(.25f,.32f,.38f);
}
void Texture(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FSlateBrush& Brush,const UNeonCityMapData& Map,
 FVector2D Position,FVector2D Size,FVector2D UVMin,FVector2D UVMax,const FLinearColor& Tint)
{
 if(!Map.Texture)return;
 if(Brush.GetResourceObject()!=Map.Texture){Brush.SetResourceObject(Map.Texture);Brush.DrawAs=ESlateBrushDrawType::Image;Brush.Tiling=ESlateBrushTileType::NoTile;}
 // the visible window may run past the map: draw only the part inside it, at the matching place
 const FVector2D A(FMath::Max(UVMin.X,0.),FMath::Max(UVMin.Y,0.)),B(FMath::Min(UVMax.X,1.),FMath::Min(UVMax.Y,1.));
 if(B.X<=A.X || B.Y<=A.Y)return;
 const FVector2D Span=UVMax-UVMin;
 const FVector2D P=Position+(A-UVMin)/Span*Size,S=(B-A)/Span*Size;
 Brush.ImageSize=S;Brush.SetUVRegion(FBox2d(A,B));
 FSlateDrawElement::MakeBox(Elements,Layer,Geometry.ToPaintGeometry(S,FSlateLayoutTransform(P)),&Brush,ESlateDrawEffect::None,Tint);
}
void Route(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,const TArray<FVector>& Route,FToScreen ToScreen,float Width,const FLinearColor& Tint)
{
 if(Route.Num()<2)return;
 TArray<FVector2D> P;P.Reserve(Route.Num());for(const FVector& V:Route)P.Add(ToScreen(V));
 FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),P,ESlateDrawEffect::None,FLinearColor(0,0,0,.45f)*Tint.A,true,Width+3);
 FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),P,ESlateDrawEffect::None,Tint,true,Width);
}
void Pin(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float S,const FLinearColor& Tint)
{
 // a map pin: round head with a white centre on a tapered point, the point at At
 const FVector2D Head=At+FVector2D(0,-S*1.05);
 for(int32 K=0;K<6;++K)                                                  // the point: a few tapering strokes
 {
  const float T=K/6.f;
  FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),{At,Head+FVector2D((T-.5f)*S*.7f,0)},ESlateDrawEffect::None,Tint,true,2.f);
 }
 Dot(Elements,Layer+1,Geometry,Head,S*.62f,Tint,FLinearColor(1,1,1,Tint.A));
 Dot(Elements,Layer+3,Geometry,Head,S*.22f,FLinearColor(1,1,1,Tint.A),FLinearColor(0,0,0,0));
}
void Home(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float S,const FLinearColor& Tint)
{
 Dot(Elements,Layer,Geometry,At,S,Tint,FLinearColor(1,1,1,Tint.A));
 const FLinearColor W(1,1,1,Tint.A);
 FSlateDrawElement::MakeLines(Elements,Layer+2,Geometry.ToPaintGeometry(),
  {At+FVector2D(-S*.5,-S*.05),At+FVector2D(0,-S*.55),At+FVector2D(S*.5,-S*.05)},ESlateDrawEffect::None,W,true,2);
 FSlateDrawElement::MakeLines(Elements,Layer+2,Geometry.ToPaintGeometry(),
  {At+FVector2D(-S*.32,-S*.15),At+FVector2D(-S*.32,S*.45),At+FVector2D(S*.32,S*.45),At+FVector2D(S*.32,-S*.15)},ESlateDrawEffect::None,W,true,2);
}
void Player(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,FVector2D F,float S,const FLinearColor& Tint)
{
 const FVector2D R(-F.Y,F.X);
 Dot(Elements,Layer,Geometry,At,S*.95f,FLinearColor(.05f,.4f,.85f,.35f),FLinearColor(0,0,0,0));
 const TArray<FVector2D> Arrow={At+F*S,At-F*S*.7+R*S*.62,At-F*S*.3,At-F*S*.7-R*S*.62,At+F*S};
 FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,FLinearColor(1,1,1,Tint.A),true,5);
 FSlateDrawElement::MakeLines(Elements,Layer+2,Geometry.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,Tint,true,2.5f);
}
void Dot(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float R,const FLinearColor& Fill,const FLinearColor& Ring)
{
 TArray<FVector2D> C;for(int32 I=0;I<=20;++I){const double A=2*PI*I/20;C.Add(At+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*.5);}
 FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),C,ESlateDrawEffect::None,Fill,true,R);   // a filled disc: a ring as thick as its radius
 if(Ring.A>0)
 {
  TArray<FVector2D> O;for(int32 I=0;I<=24;++I){const double A=2*PI*I/24;O.Add(At+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}
  FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),O,ESlateDrawEffect::None,Ring,true,1.5f);
 }
}
}

void UNeonCityWorldMapWidget::SetSource(UNeonCityNavigationComponent* Value)
{
 // A UserWidget defaults to SelfHitTestInvisible: this self-painted map has no child to catch the pointer, so wheel
 // zoom, drag pan and clicks never arrived. The open map takes the mouse itself.
 Source=Value;SetIsFocusable(true);SetVisibility(ESlateVisibility::Visible);
 const UNeonCityMapData* Map=Value?Value->GetMapData():nullptr;
 const APawn* Pawn=GetOwningPlayerPawn();
 if(Map && Pawn){Center=Map->Contains(Pawn->GetActorLocation())?Map->ToUV(Pawn->GetActorLocation()):FVector2D(.5,.5);Zoom=3;ClampView();}
}
TSharedRef<SWidget> UNeonCityWorldMapWidget::RebuildWidget() { return SNew(SSpacer); }

void UNeonCityWorldMapWidget::Layout(const FGeometry& Geometry,FVector2D& Origin,double& Side) const
{
 const FVector2D S=Geometry.GetLocalSize();
 Side=FMath::Max(100.,FMath::Min(S.X-360,S.Y-110));
 Origin=FVector2D(40,(S.Y-Side)*.5+10);
}
FVector2D UNeonCityWorldMapWidget::UVAt(const FGeometry& Geometry,FVector2D Local) const
{
 FVector2D O;double Side;Layout(Geometry,O,Side);
 return Center+(Local-O-FVector2D(Side,Side)*.5)/Side/Zoom;
}
FVector2D UNeonCityWorldMapWidget::LocalAt(const FGeometry& Geometry,FVector2D UV) const
{
 FVector2D O;double Side;Layout(Geometry,O,Side);
 return O+FVector2D(Side,Side)*.5+(UV-Center)*Zoom*Side;
}
void UNeonCityWorldMapWidget::ClampView()
{
 Zoom=FMath::Clamp(Zoom,1.,10.);const double H=.5/Zoom;
 Center.X=FMath::Clamp(Center.X,H,1-H);Center.Y=FMath::Clamp(Center.Y,H,1-H);
}
const FNeonCityMapPlace* UNeonCityWorldMapWidget::PlaceNear(const FGeometry& Geometry,FVector2D Local) const
{
 const UNeonCityMapData* Map=Source.IsValid()?Source->GetMapData():nullptr;if(!Map)return nullptr;
 const FNeonCityMapPlace* Best=nullptr;double BestD=18;
 for(const auto& P:Map->Places)
 {
  const double D=FVector2D::Distance(LocalAt(Geometry,Map->ToUV(FVector(P.Location,0))),Local);
  if(D<BestD){BestD=D;Best=&P;}
 }
 return Best;
}
void UNeonCityWorldMapWidget::Click(const FGeometry& Geometry,FVector2D Local,bool Right)
{
 auto* Nav=Source.Get();const UNeonCityMapData* Map=Nav?Nav->GetMapData():nullptr;if(!Map)return;
 if(Right){Nav->ClearWaypoint();return;}
 FVector2D O;double Side;Layout(Geometry,O,Side);
 if(Local.X<O.X || Local.Y<O.Y || Local.X>O.X+Side || Local.Y>O.Y+Side)return;
 FText Label;FName ID;FVector World;
 if(const FNeonCityMapPlace* P=PlaceNear(Geometry,Local)){World=FVector(P->Location,0);Label=P->Name;ID=P->PlaceID;}
 else World=Map->ToWorld(UVAt(Geometry,Local));
 // ground height under the pin, for the marker in the world view
 FHitResult Hit;const UWorld* W=GetWorld();
 World.Z=W && W->LineTraceSingleByChannel(Hit,FVector(World.X,World.Y,60000),FVector(World.X,World.Y,-20000),ECC_Visibility)?Hit.ImpactPoint.Z:0;
 Nav->SetWaypoint(World,Label,ID);
}

FReply UNeonCityWorldMapWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 const FKey K=Event.GetKey();
 if(K==EKeys::M || K==EKeys::Escape || K==EKeys::Tab){if(Source.IsValid())Source->ToggleWorldMap();return FReply::Handled();}
 if(K==EKeys::Delete || K==EKeys::BackSpace){if(Source.IsValid())Source->ClearWaypoint();return FReply::Handled();}
 if(K==EKeys::Add || K==EKeys::Equals){Zoom*=1.25;ClampView();return FReply::Handled();}
 if(K==EKeys::Subtract || K==EKeys::Hyphen){Zoom/=1.25;ClampView();return FReply::Handled();}
 // keyboard pan: a tenth of the visible map per press
 const double Step=.1/Zoom;
 if(K==EKeys::W || K==EKeys::Up){Center.Y-=Step;ClampView();return FReply::Handled();}
 if(K==EKeys::S || K==EKeys::Down){Center.Y+=Step;ClampView();return FReply::Handled();}
 if(K==EKeys::A || K==EKeys::Left){Center.X-=Step;ClampView();return FReply::Handled();}
 if(K==EKeys::D || K==EKeys::Right){Center.X+=Step;ClampView();return FReply::Handled();}
 if(K==EKeys::H)                                                         // jump to home
 {
  const UNeonCityMapData* Map=Source.IsValid()?Source->GetMapData():nullptr;
  if(Map)for(const auto& P:Map->Places)if(P.Kind==TEXT("home")){Center=Map->ToUV(FVector(P.Location,0));ClampView();}
  return FReply::Handled();
 }
 return FReply::Handled();                                               // the map has the keyboard while open
}
FReply UNeonCityWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
 const FVector2D L=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
 PressAt=LastMouse=L;bDragged=false;
 if(Event.GetEffectingButton()==EKeys::LeftMouseButton)bPressed=true;
 if(Event.GetEffectingButton()==EKeys::RightMouseButton)bRightPressed=true;
 return FReply::Handled().CaptureMouse(TakeWidget());
}
FReply UNeonCityWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{
 const FVector2D L=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
 if(Event.GetEffectingButton()==EKeys::LeftMouseButton && bPressed){bPressed=false;if(!bDragged)Click(Geometry,L,false);}
 if(Event.GetEffectingButton()==EKeys::RightMouseButton && bRightPressed){bRightPressed=false;if(!bDragged)Click(Geometry,L,true);}
 return FReply::Handled().ReleaseMouseCapture();
}
FReply UNeonCityWorldMapWidget::NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event)
{
 const FVector2D L=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());Hover=L;
 if(bPressed || bRightPressed)
 {
  if(FVector2D::Distance(L,PressAt)>5)bDragged=true;
  if(bDragged){FVector2D O;double Side;Layout(Geometry,O,Side);Center-=(L-LastMouse)/Side/Zoom;ClampView();}
 }
 LastMouse=L;return FReply::Handled();
}
FReply UNeonCityWorldMapWidget::NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event)
{
 const FVector2D L=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
 const FVector2D Before=UVAt(Geometry,L);
 Zoom*=FMath::Pow(1.2,Event.GetWheelDelta());ClampView();
 Center+=Before-UVAt(Geometry,L);ClampView();                            // zoom about the cursor
 return FReply::Handled();
}

int32 UNeonCityWorldMapWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
 FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const
{
 Layer=Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,ParentEnabled);
 const auto* Nav=Source.Get();const UNeonCityMapData* Map=Nav?Nav->GetMapData():nullptr;if(!Map)return Layer;
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 const FVector2D S=Geometry.GetLocalSize();
 auto Box=[&](FVector2D P,FVector2D Z,FLinearColor C,int32 D){FSlateDrawElement::MakeBox(Elements,Layer+D,Geometry.ToPaintGeometry(Z,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,C);};
 auto Text=[&](const FString& V,FVector2D P,int32 Size,FLinearColor C,int32 D,const TCHAR* Face=TEXT("Regular"))
 {FSlateDrawElement::MakeText(Elements,Layer+D,Geometry.ToPaintGeometry(FVector2D(600,40),FSlateLayoutTransform(P)),V,FCoreStyle::GetDefaultFontStyle(Face,Size),ESlateDrawEffect::None,C);};
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const FLinearColor Ink(.1f,.13f,.16f),Muted(.36f,.42f,.47f),Panel(.97f,.97f,.95f,.97f),Accent(1.f,.55f,.08f),Route(.13f,.47f,.95f);
 Box(FVector2D::ZeroVector,S,FLinearColor(.04f,.06f,.08f,.86f),1);
 FVector2D O;double Side;Layout(Geometry,O,Side);
 Box(O-FVector2D(6,6),FVector2D(Side+12,Side+12),FLinearColor(.95f,.95f,.93f),2);
 const double H=.5/Zoom;
 NeonCityMapPaint::Texture(Elements,Layer+3,Geometry,Brush,*Map,O,FVector2D(Side,Side),Center-FVector2D(H,H),Center+FVector2D(H,H),FLinearColor::White);
 Elements.PushClip(FSlateClippingZone(Geometry.MakeChild(FVector2D(Side,Side),FSlateLayoutTransform(O))));
 auto ToScreen=[&](const FVector& W){return LocalAt(Geometry,Map->ToUV(W));};
 const auto& D=Nav->GetDisplay();
 if(D.Active)NeonCityMapPaint::Route(Elements,Layer+4,Geometry,D.Route,ToScreen,5,Route);
 // places: kind colour dots, names (areas only when zoomed in)
 for(const auto& P:Map->Places)
 {
  const FVector2D At=ToScreen(FVector(P.Location,0));
  if(P.Kind==TEXT("home")){NeonCityMapPaint::Home(Elements,Layer+6,Geometry,At,15,NeonCityMapPaint::PlaceColor(P.Kind));}
  else if(P.Kind!=TEXT("area"))NeonCityMapPaint::Dot(Elements,Layer+6,Geometry,At,9,NeonCityMapPaint::PlaceColor(P.Kind),FLinearColor::White);
  if(P.Kind==TEXT("area") && Zoom<1.6)continue;
  const FString Name=P.Name.ToString();const int32 Size=P.Kind==TEXT("area")?15:13;
  const auto Font=FCoreStyle::GetDefaultFontStyle(P.Kind==TEXT("area")?TEXT("Italic"):TEXT("Bold"),Size);
  const FVector2D TS=Measure->Measure(Name,Font);
  const FVector2D TP=At+FVector2D(-TS.X*.5,P.Kind==TEXT("area")?-TS.Y*.5:12);
  if(P.Kind!=TEXT("area"))Box(TP-FVector2D(5,2),TS+FVector2D(10,4),FLinearColor(1,1,1,.82f),7);
  FSlateDrawElement::MakeText(Elements,Layer+8,Geometry.ToPaintGeometry(TS+FVector2D(4,2),FSlateLayoutTransform(TP)),Name,Font,ESlateDrawEffect::None,
   P.Kind==TEXT("area")?FLinearColor(.18f,.24f,.28f,.85f):Ink);
 }
 if(D.Active)NeonCityMapPaint::Pin(Elements,Layer+9,Geometry,ToScreen(D.Goal),22,Accent);
 if(const APawn* Pawn=GetOwningPlayerPawn())
 {
  const double R=FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);
  NeonCityMapPaint::Player(Elements,Layer+10,Geometry,ToScreen(Pawn->GetActorLocation()),FVector2D(-FMath::Cos(R),-FMath::Sin(R)),14,Route);
 }
 // hover hint on a place
 if(const FNeonCityMapPlace* P=PlaceNear(Geometry,Hover))
  Text(FString::Printf(TEXT("Click to go to %s"),*P->Name.ToString()),Hover+FVector2D(16,10),13,FLinearColor::White,12);
 Elements.PopClip();
 // side panel: title, route card, places list, controls
 const FVector2D PP(O.X+Side+30,O.Y-6);const double PW=S.X-PP.X-30;
 Box(PP,FVector2D(PW,Side+12),Panel,2);
 Text(Map->Title.IsEmpty()?TEXT("MAP"):Map->Title.ToString().ToUpper(),PP+FVector2D(18,16),22,Ink,3,TEXT("Bold"));
 double Y=PP.Y+58;
 if(D.Active)
 {
  Box(FVector2D(PP.X+12,Y),FVector2D(PW-24,96),FLinearColor(1.f,.95f,.86f),3);
  Text(TEXT("DESTINATION"),FVector2D(PP.X+24,Y+8),11,Accent*.8f,4,TEXT("Bold"));
  Text(D.Label.ToString().Left(28),FVector2D(PP.X+24,Y+26),17,Ink,4,TEXT("Bold"));
  Text(FString::Printf(TEXT("%.0f m  |  %s"),D.DistanceMeters,*D.RouteHint.ToString()),FVector2D(PP.X+24,Y+54),13,Muted,4);
  Text(TEXT("Right click / Del to clear"),FVector2D(PP.X+24,Y+74),11,Muted,4);
  Y+=112;
 }
 else
 {
  Text(TEXT("Click anywhere to set a waypoint,"),FVector2D(PP.X+18,Y),13,Muted,3);
  Text(TEXT("or pick a place below."),FVector2D(PP.X+18,Y+20),13,Muted,3);
  Y+=52;
 }
 Text(TEXT("PLACES"),FVector2D(PP.X+18,Y),11,Muted,3,TEXT("Bold"));Y+=22;
 for(const auto& P:Map->Places)
 {
  if(P.Kind==TEXT("area"))continue;
  if(Y>PP.Y+Side-120)break;
  NeonCityMapPaint::Dot(Elements,Layer+3,Geometry,FVector2D(PP.X+26,Y+10),8,NeonCityMapPaint::PlaceColor(P.Kind),FLinearColor::White);
  Text(P.Name.ToString(),FVector2D(PP.X+40,Y+1),13,Ink,4);Y+=24;
 }
 const double HY=PP.Y+Side-86;
 Box(FVector2D(PP.X+12,HY),FVector2D(PW-24,86),FLinearColor(.92f,.93f,.92f),3);
 Text(TEXT("Left click   Set waypoint"),FVector2D(PP.X+22,HY+8),12,Ink,4);
 Text(TEXT("Wheel / + -   Zoom      Drag / WASD   Pan"),FVector2D(PP.X+22,HY+26),12,Ink,4);
 Text(TEXT("H   Find home"),FVector2D(PP.X+22,HY+44),12,Ink,4);
 Text(TEXT("M / Esc   Close"),FVector2D(PP.X+22,HY+62),12,Ink,4);
 // scale bar (bottom-left of the map)
 const double MetresPerPx=(Map->WorldMax.X-Map->WorldMin.X)/100./(Side*Zoom);
 double Bar=100;for(double C:{50.,100.,200.,500.,1000.})if(C/MetresPerPx<=160)Bar=C;
 const FVector2D B0=O+FVector2D(16,Side-26);const double BL=Bar/MetresPerPx;
 Box(B0-FVector2D(6,18),FVector2D(BL+12,30),FLinearColor(1,1,1,.85f),11);
 Box(B0,FVector2D(BL,4),Ink,12);
 Text(FString::Printf(TEXT("%.0f m"),Bar),B0+FVector2D(0,-17),11,Ink,12);
 // north arrow
 const FVector2D N=O+FVector2D(Side-30,30);
 FSlateDrawElement::MakeLines(Elements,Layer+12,Geometry.ToPaintGeometry(),{N+FVector2D(-9,10),N+FVector2D(0,-14),N+FVector2D(9,10),N+FVector2D(0,4),N+FVector2D(-9,10)},ESlateDrawEffect::None,Ink,true,2.5f);
 Text(TEXT("N"),N+FVector2D(-5,12),12,Ink,12,TEXT("Bold"));
 return Layer+13;
}
