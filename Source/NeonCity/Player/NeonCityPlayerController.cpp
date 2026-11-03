// Copyright Epic Games, Inc. All Rights Reserved.


#include "NeonCityPlayerController.h"
#include "Player/PlayerSaveInputComponent.h"
#include "Phone/PhoneComponent.h"
#include "Fishing/FishingComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "NeonCity.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

ANeonCityPlayerController::ANeonCityPlayerController()
{
 CreateDefaultSubobject<UPlayerSaveInputComponent>(TEXT("PlayerSaveInput"));
	Phone=CreateDefaultSubobject<UPhoneComponent>(TEXT("Phone"));
	Fishing=CreateDefaultSubobject<UFishingComponent>(TEXT("Fishing"));
	Navigation=CreateDefaultSubobject<UNeonCityNavigationComponent>(TEXT("Navigation"));
}

void ANeonCityPlayerController::MarinaReference()
{
	if(PhotographyPreviousView.IsValid())
	{
		SetViewTarget(PhotographyPreviousView.Get());PhotographyPreviousView.Reset();return;
	}
	for(TActorIterator<ACameraActor> I(GetWorld());I;++I)
		if(I->GetLevel()==GetWorld()->PersistentLevel && I->ActorHasTag(TEXT("NeonCity.MainlandC923.ReferenceTarget")))
		{PhotographyPreviousView=GetViewTarget();SetViewTarget(*I);return;}
}

void ANeonCityPlayerController::PhotoHUD(){SetPhotographyHUDHidden(!bPhotographyHUDHidden);}

void ANeonCityPlayerController::SetPhotographyHUDHidden(bool bHideRequested)
{
	if(bHideRequested==bPhotographyHUDHidden)return;
	bPhotographyHUDHidden=bHideRequested;
	if(bHideRequested)
	{
		TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UUserWidget::StaticClass(),true);
		for(auto* Widget:Widgets)if(Widget && Widget->GetOwningPlayer()==this)
		{PhotographyWidgetOpacity.Add(Widget,Widget->GetRenderOpacity());Widget->SetRenderOpacity(0.f);}
		if(auto* HUD=GetHUD()){bPhotographyPreviousHUD=HUD->bShowHUD;HUD->bShowHUD=false;}
	}
	else
	{
		for(const auto& Item:PhotographyWidgetOpacity)if(Item.Key.IsValid())Item.Key->SetRenderOpacity(Item.Value);
		PhotographyWidgetOpacity.Empty();if(auto* HUD=GetHUD())HUD->bShowHUD=bPhotographyPreviousHUD;
	}
}

void ANeonCityPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	SetPhotographyHUDHidden(false);PhotographyPreviousView.Reset();Super::EndPlay(Reason);
}

void ANeonCityPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogNeonCity, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ANeonCityPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
 FindComponentByClass<UPlayerSaveInputComponent>()->SetupInput(InputComponent);
	Phone->SetupInput(InputComponent);
	Fishing->SetupInput(InputComponent);
 Navigation->SetupInput(InputComponent);
	if(IsLocalPlayerController())
	{
		InputComponent->BindKey(EKeys::F9,IE_Pressed,this,&ANeonCityPlayerController::MarinaReference);
		InputComponent->BindKey(EKeys::F10,IE_Pressed,this,&ANeonCityPlayerController::PhotoHUD);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ANeonCityPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
