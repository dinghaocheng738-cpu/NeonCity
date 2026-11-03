#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/PointLight.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/NeonCityWorldManager.h"
#include "World/NeonCityDayNightCycle.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Interaction/InteractiveBuildingDoor.h"
#include "Interaction/InteractionComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace NeonCityWorldTests
{
class FVerifySouthBeach : public IAutomationLatentCommand
{
public:
	explicit FVerifySouthBeach(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Start == 0.0) { Start = Now; }
		if (Now - Start > 90.0) { Test->AddError(TEXT("South Beach PIE timed out.")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ANeonCityPlayerCharacter* Player = PC ? Cast<ANeonCityPlayerCharacter>(PC->GetPawn()) : nullptr;
		if (!Player || !World->HasBegunPlay()) { return false; }

		if (Stage == 0)
		{
			if (!Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			Test->TestTrue(TEXT("Entered South Beach map"), World->GetMapName().Contains(TEXT("NeonCity_SouthBeach_Test")));
			UNeonCityWorldManager* Manager = World->GetSubsystem<UNeonCityWorldManager>();
			if (!Test->TestNotNull(TEXT("World manager exists"), Manager)) { return true; }
			Test->TestEqual(TEXT("Region registered"), Manager->GetCurrentRegion(), FName(TEXT("SouthBeach")));
			Test->TestTrue(TEXT("World state active"), Manager->GetWorldState() == ENeonCityWorldState::Active);
			UNeonCitySaveSubsystem* Save = World->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
			Test->TestTrue(TEXT("Save subsystem is ready with disk persistence and legacy domains"),
				Save && Save->IsSystemReady_Implementation() && Save->IsPersistenceImplemented() && Save->GetPlannedDomains().Num() == 4);
			for (TActorIterator<ANeonCityDayNightCycle> It(World); It; ++It) { Cycle = *It; }
			for (TActorIterator<AInteractiveBuildingDoor> It(World); It; ++It) { Door = *It; }
			if (!Test->TestNotNull(TEXT("Day/night actor placed"), Cycle.Get())
				|| !Test->TestNotNull(TEXT("Building entrance placed"), Door.Get())) { return true; }
			Cycle->SetCycleEnabled(false);
			Cycle->SetTimeOfDay(12.f);
			const FRotator NoonRotation = Cycle->GetSun()->GetRelativeRotation();
			Test->TestTrue(TEXT("Day sunlight active"), Cycle->GetSun()->Intensity > 70000.f);
			Test->TestTrue(TEXT("Night light references assigned"), Cycle->NightLights.Num() == 9);
			for (APointLight* Light : Cycle->NightLights)
			{
				Test->TestTrue(TEXT("City light off at noon"), IsValid(Light) && Light->GetLightComponent()->Intensity == 0.f);
			}
			Cycle->SetTimeOfDay(0.f);
			Test->TestEqual(TEXT("Sunlight off at midnight"), Cycle->GetSun()->Intensity, 0.f);
			Test->TestFalse(TEXT("Sun rotates with time"), NoonRotation.Equals(Cycle->GetSun()->GetRelativeRotation(), 0.1f));
			for (APointLight* Light : Cycle->NightLights)
			{
				Test->TestTrue(TEXT("City light on at night"), IsValid(Light) && Light->GetLightComponent()->Intensity > 5900.f);
			}
			Cycle->SetTimeOfDay(-1.f);
			Test->TestEqual(TEXT("Negative hour wraps"), Cycle->GetTimeOfDay(), 23.f);
			Cycle->SetTimeOfDay(23.99f);
			Cycle->DayLengthSeconds = 2.f;
			Cycle->SetCycleEnabled(true);
			Origin = Player->GetActorLocation();
			PhaseStart = Now;
			Stage = 1;
		}
		if (Stage == 1)
		{
			Player->DoMove(0.f, 1.f);
			if (Now - PhaseStart < 0.5) { return false; }
			Test->TestTrue(TEXT("Player moves across block"), FVector::Dist2D(Origin, Player->GetActorLocation()) > 40.f);
			Test->TestTrue(TEXT("Clock ticks across midnight"), Cycle->GetTimeOfDay() > 0.1f && Cycle->GetTimeOfDay() < 18.f);
			Cycle->DayLengthSeconds = 600.f;
			Cycle->SetCycleEnabled(false);
			Cycle->SetTimeOfDay(10.f);
			Player->GetCharacterMovement()->StopMovementImmediately();
			Player->SetActorLocation(FVector(-1500.f, 1180.f, 136.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(0.f, 90.f, 0.f));
			PhaseStart = Now;
			Stage = 2;
			return false;
		}
		if (Stage == 2)
		{
			if (Now - PhaseStart < 0.3) { return false; }
			Test->TestTrue(TEXT("Placed doorway is detected through generic interface"),
				Player->GetInteractionComponent()->RefreshFocusedInteractable() == Door.Get());
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0),
				EKeys::F, IE_Pressed, FPlatformTime::Cycles64()));
			PhaseStart = Now;
			Stage = 3;
			return false;
		}
		if (Stage == 3)
		{
			if (Now - PhaseStart < 0.2) { return false; }
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0),
				EKeys::F, IE_Released, FPlatformTime::Cycles64()));
			Test->TestTrue(TEXT("F opens the placed building door"), Door->IsOpen());
			PhaseStart = Now;
			Stage = 4;
			return false;
		}
		if (Stage == 4)
		{
			Player->DoMove(0.f, 1.f);
			if (Now - PhaseStart < 1.0) { return false; }
			Test->TestTrue(TEXT("Player physically enters lobby through opening"), Player->GetActorLocation().Y > 1510.f);
			Test->TestTrue(TEXT("Lobby floor supports player"), Player->GetCharacterMovement()->IsMovingOnGround());
			Player->GetCharacterMovement()->StopMovementImmediately();
			Test->AddInfo(TEXT("South Beach: map entry, player movement, region/state, reserved save framework, clock progression, day/night lights and F-key building entry checks completed."));
			if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityCapture"))) { return true; }
			ACameraActor* Camera = World->SpawnActor<ACameraActor>();
			const FVector Location(8000.f, -10400.f, 4300.f);
			Camera->SetActorLocation(Location);
			Camera->SetActorRotation((FVector(-200.f, 700.f, 350.f) - Location).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(62.f);
			PC->SetViewTarget(Camera);
			PhaseStart = Now;
			Stage = 5;
			return false;
		}
		if (Stage == 5)
		{
			if (Now - PhaseStart < 10.0) { return false; }
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Phase4");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / TEXT("SouthBeach-Day.png"), false, false);
			PhaseStart = Now;
			Stage = 6;
			return false;
		}
		if (Stage == 6)
		{
			if (FScreenshotRequest::IsScreenshotRequested() || Now - PhaseStart < 1.0) { return false; }
			Test->TestTrue(TEXT("Day image captured"), IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir() / TEXT("Phase4/SouthBeach-Day.png"))));
			Cycle->SetTimeOfDay(22.f);
			PhaseStart = Now;
			Stage = 7;
			return false;
		}
		if (Stage == 7)
		{
			if (Now - PhaseStart < 12.0) { return false; }
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Phase4/SouthBeach-Night.png"), false, false);
			PhaseStart = Now;
			Stage = 8;
			return false;
		}
		if (Stage == 8)
		{
			if (FScreenshotRequest::IsScreenshotRequested() || Now - PhaseStart < 1.0) { return false; }
			Test->TestTrue(TEXT("Night image captured"), IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir() / TEXT("Phase4/SouthBeach-Night.png"))));
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	int32 Stage = 0;
	double Start = 0.0;
	double PhaseStart = 0.0;
	FVector Origin = FVector::ZeroVector;
	TWeakObjectPtr<ANeonCityDayNightCycle> Cycle;
	TWeakObjectPtr<AInteractiveBuildingDoor> Door;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCitySouthBeachSmoke, "NeonCity.World.SouthBeachPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNeonCitySouthBeachSmoke::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWorldTests::FVerifySouthBeach>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
