#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PhoneWidget.generated.h"
class UTextBlock;
class UScrollBox;
class UImage;
class UTexture2D;

/** Minimal keyboard-operated inbox and vehicle catalogue; no purchase or inventory UI. */
UCLASS()
class NEONCITY_API UPhoneWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetPhoneText(const FText& Text);
 void SetPhotoPreview(const FString& SavedPhotograph);
 void ScrollPage(float Direction);
 float GetScrollOffset() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Body;
 UPROPERTY() TObjectPtr<UScrollBox> Scroll;
 UPROPERTY() TObjectPtr<UImage> Preview;
 UPROPERTY() TObjectPtr<UTexture2D> PreviewTexture;
 FString PreviewFile;
 FText CurrentText;
};
