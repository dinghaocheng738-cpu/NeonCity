// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/SlateWrapperTypes.h"
#include "NeonCityPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UPhoneComponent;
class UFishingComponent;
class UNeonCityNavigationComponent;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ANeonCityPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ANeonCityPlayerController();
	UFishingComponent* GetFishingComponent() const { return Fishing; }
	UPhoneComponent* GetPhoneComponent() const { return Phone; }
	UNeonCityNavigationComponent* GetNavigationComponent() const { return Navigation; }
	/** F9 returns to the authored marina reference camera; press again to resume. */
	UFUNCTION(Exec) void MarinaReference();
	/** F10 hides/restores existing HUD widgets without changing gameplay input. */
	UFUNCTION(Exec) void PhotoHUD();
	void SetPhotographyHUDHidden(bool bHideRequested);
	bool IsPhotographyHUDHidden() const { return bPhotographyHUDHidden; }
private:
	TWeakObjectPtr<AActor> PhotographyPreviousView;
	TMap<TWeakObjectPtr<UUserWidget>,float> PhotographyWidgetOpacity;
	bool bPhotographyHUDHidden=false;
	bool bPhotographyPreviousHUD=true;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Fishing") TObjectPtr<UFishingComponent> Fishing;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Phone") TObjectPtr<UPhoneComponent> Phone;
	UPROPERTY(VisibleAnywhere, Category="NeonCity|Navigation") TObjectPtr<UNeonCityNavigationComponent> Navigation;
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
