#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "NeonCityWorldMapWidget.generated.h"
class UNeonCityNavigationComponent;
class UNeonCityMapData;
struct FNeonCityMapPlace;

/** Drawing shared by the minimap and the full-screen map: the painted map through a UV window, the route, places,
 * the waypoint and the player. ToScreen maps a world point (cm) to widget space. */
namespace NeonCityMapPaint
{
using FToScreen=TFunctionRef<FVector2D(const FVector&)>;
FLinearColor PlaceColor(FName Kind);
void Texture(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FSlateBrush& Brush,const UNeonCityMapData& Map,
 FVector2D Position,FVector2D Size,FVector2D UVMin,FVector2D UVMax,const FLinearColor& Tint);
void Route(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,const TArray<FVector>& Route,FToScreen ToScreen,float Width,const FLinearColor& Tint);
void Pin(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float Size,const FLinearColor& Tint);
void Home(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float Size,const FLinearColor& Tint);
void Player(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,FVector2D Forward,float Size,const FLinearColor& Tint);
void Dot(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry,FVector2D At,float Radius,const FLinearColor& Fill,const FLinearColor& Ring);
}

/** M: full-screen map. Left click sets a waypoint (on a named place: that place), right click clears it, the wheel
 * zooms about the cursor, dragging pans; M / Esc / Tab close. Game input is suspended while it is open. */
UCLASS()
class NEONCITY_API UNeonCityWorldMapWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetSource(UNeonCityNavigationComponent* Value);
 double GetZoom() const { return Zoom; }
 FVector2D GetViewCenter() const { return Center; }
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
  FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
 virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event) override;
private:
 void Layout(const FGeometry& Geometry,FVector2D& Origin,double& Side) const;
 FVector2D UVAt(const FGeometry& Geometry,FVector2D Local) const;
 FVector2D LocalAt(const FGeometry& Geometry,FVector2D UV) const;
 void ClampView();
 const FNeonCityMapPlace* PlaceNear(const FGeometry& Geometry,FVector2D Local) const;
 void Click(const FGeometry& Geometry,FVector2D Local,bool Right);
 TWeakObjectPtr<UNeonCityNavigationComponent> Source;
 mutable FSlateBrush Brush;
 FVector2D Center=FVector2D(.5,.5);
 double Zoom=3;
 FVector2D PressAt=FVector2D::ZeroVector,LastMouse=FVector2D::ZeroVector,Hover=FVector2D(-1,-1);
 bool bPressed=false,bDragged=false,bRightPressed=false;
};
