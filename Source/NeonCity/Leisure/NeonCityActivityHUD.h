#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "NeonCityActivityHUD.generated.h"
class UNeonCityActivityHUDWidget;

/** What a timed leisure activity shows while it runs: a top-centre panel (title, big clock, sub line, a few
 * detail lines, an optional progress bar), a centre banner (countdown digits, GO, results) and an optional
 * camera framing bracket that fills while a photograph is being lined up. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityActivityHUDState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FText Title;
 UPROPERTY(BlueprintReadWrite) FText Clock;
 UPROPERTY(BlueprintReadWrite) FText Sub;
 UPROPERTY(BlueprintReadWrite) TArray<FText> Lines;
 /** 0..1 fills the bar under the panel; negative hides it. */
 UPROPERTY(BlueprintReadWrite) float Progress=-1;
 UPROPERTY(BlueprintReadWrite) FLinearColor Accent=FLinearColor(1.f,.36f,.18f);
 UPROPERTY(BlueprintReadWrite) bool bFraming=false;
 UPROPERTY(BlueprintReadWrite) float FramingProgress=0;
 UPROPERTY(BlueprintReadWrite) FText FramingLabel;
};

/** One owner at a time (the running activity); banners are queued by any owner. */
UCLASS()
class NEONCITY_API UNeonCityActivityHUDSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 void Show(FName Owner,const FNeonCityActivityHUDState& State);
 void Clear(FName Owner);
 /** Large centre text for Seconds (fades in its last half second). */
 void Banner(const FText& Text,const FText& Detail,FLinearColor Color,float Seconds=2.5f);
 bool IsShowing(FName Owner) const { return Owner==Current && !Owner.IsNone(); }
 const FNeonCityActivityHUDState& GetState() const { return State; }
 FName GetOwnerID() const { return Current; }
 bool GetBanner(FText& Text,FText& Detail,FLinearColor& Color,float& Alpha) const;
 virtual void Deinitialize() override;
private:
 void EnsureWidget();
 FName Current;
 FNeonCityActivityHUDState State;
 FText BannerText,BannerDetail;FLinearColor BannerColor=FLinearColor::White;double BannerUntil=0,BannerLength=1;
 UPROPERTY(Transient) TObjectPtr<UNeonCityActivityHUDWidget> Widget;
};

UCLASS()
class NEONCITY_API UNeonCityActivityHUDWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetSource(UNeonCityActivityHUDSubsystem* Value) { Source=Value; }
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
  FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override;
private:
 TWeakObjectPtr<UNeonCityActivityHUDSubsystem> Source;
};
