#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "NeonCityNavigationWidget.generated.h"
class UNeonCityNavigationComponent;

/** Minimap: the painted area map around the player where the world has one (north up, the route, places, the
 * waypoint, and a waypoint marker in the world view); elsewhere a bounded vector map from authored roads. */
UCLASS()
class NEONCITY_API UNeonCityNavigationWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetSource(UNeonCityNavigationComponent* Value) { Source=Value; }
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
  FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override;
private:
 void PaintWorldMarker(FSlateWindowElementList& Elements,int32 Layer,const FGeometry& Geometry) const;
 TWeakObjectPtr<UNeonCityNavigationComponent> Source;
 mutable FSlateBrush MapBrush;
};
