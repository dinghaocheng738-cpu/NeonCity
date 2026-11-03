#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "NeonCityCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerController.h"
#include "Core/NeonCityGameInstance.h"
#include "World/NeonCityWorldManager.h"
#include "Interaction/InteractionComponent.h"

#include "Interaction/Examples/NeonCityInteractionTestDoor.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Components/BoxComponent.h"
#include "InputKeyEventArgs.h"

namespace NeonCityTests
{
class FVerifyThirdPersonPIE : public IAutomationLatentCommand
{
public:
	explicit FVerifyThirdPersonPIE(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartTime == 0.0) { StartTime = Now; }
		if (Now - StartTime > 30.0)
		{
			Test->AddError(TEXT("Timed out waiting for Third Person PIE/movement."));
			return true;
		}

		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ANeonCityCharacter* Character = PC ? Cast<ANeonCityCharacter>(PC->GetPawn()) : nullptr;
		if (!Character || !World->HasBegunPlay()) { return false; }

		if (Stage == 0)
		{
			if (!Character->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			Test->TestEqual(TEXT("Original default pawn remains active"),
				Character->GetClass()->GetPathName(),
				FString(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C")));
			Test->TestEqual(TEXT("Original blueprint GameMode remains active"),
				World->GetAuthGameMode()->GetClass()->GetPathName(),
				FString(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")));
			Test->TestNotNull(TEXT("Original controller"), Cast<ANeonCityPlayerController>(PC));
			Test->TestNotNull(TEXT("Active pawn uses NeonCity extension"), Cast<ANeonCityPlayerCharacter>(Character));
			if (ANeonCityPlayerCharacter* Player = Cast<ANeonCityPlayerCharacter>(Character))
			{
				Test->TestNotNull(TEXT("Player data is attached"), Player->GetPlayerDataComponent());
				Test->TestNotNull(TEXT("Camera extension is attached"), Player->GetCameraExtensionComponent());
			}

			Test->TestNotNull(TEXT("Enhanced Input component"), Cast<UEnhancedInputComponent>(Character->InputComponent));
			Test->TestNotNull(TEXT("Animation instance"), Character->GetMesh()->GetAnimInstance());
			Test->TestNotNull(TEXT("Template camera"), Character->GetFollowCamera());
			Test->TestNotNull(TEXT("Template spring arm"), Character->GetCameraBoom());
			UNeonCityGameInstance* GI = Cast<UNeonCityGameInstance>(World->GetGameInstance());
			Test->TestTrue(TEXT("Configured GameInstance initialized"), GI && GI->IsFrameworkReady());
			UNeonCityWorldManager* Manager = World->GetSubsystem<UNeonCityWorldManager>();
			Test->TestTrue(TEXT("World manager initialized"), Manager && Manager->IsSystemReady_Implementation());

			UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer()
				? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			UInputMappingContext* Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
			Test->TestTrue(TEXT("Default input mapping applied"), Input && Mapping && Input->HasMappingContext(Mapping));

			const ANeonCityPlayerCharacter* Extension = GetDefault<ANeonCityPlayerCharacter>();
			Test->TestNotNull(TEXT("New subclass has opt-in interaction component"), Extension->GetInteractionComponent());
			Test->TestEqual(TEXT("Inherited walk speed unchanged"),
				Extension->GetCharacterMovement()->MaxWalkSpeed, GetDefault<ANeonCityCharacter>()->GetCharacterMovement()->MaxWalkSpeed);
			Test->TestTrue(TEXT("Interaction supports proximity detection"), Extension->GetInteractionComponent()->PrimaryComponentTick.bCanEverTick);
			Test->TestFalse(TEXT("Null interaction target rejected"), Extension->GetInteractionComponent()->CanInteractWith(nullptr));
			FHitResult Hit;
			Test->TestNull(TEXT("Unbound detection safely returns no target"),
				Extension->GetInteractionComponent()->DetectInteractable(FVector::ZeroVector, FVector::ZeroVector, Hit));

			Origin = Character->GetActorLocation();
			PhaseStart = World->GetTimeSeconds();
			Stage = 1;
		}

		if (Stage == 1)
		{
			Character->DoMove(0.f, 1.f);
			if (++MovementFrames < 20 || World->GetTimeSeconds() - PhaseStart < 0.75) { return false; }
			Test->TestTrue(TEXT("Template movement moves the possessed pawn"),
				FVector::Dist2D(Origin, Character->GetActorLocation()) > 10.f);
			LookBefore = PC->GetControlRotation();
			Character->DoLook(10.f, 0.f);
			PhaseStart = World->GetTimeSeconds();
			Stage = 2;
			return false;
		}

		if (Stage == 2)
		{
			if (World->GetTimeSeconds() - PhaseStart < 0.15) { return false; }
			Test->TestTrue(TEXT("Template look changes control yaw"),
				FMath::Abs(FMath::FindDeltaAngleDegrees(LookBefore.Yaw, PC->GetControlRotation().Yaw)) > 0.1f);
			if (!Character->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			Character->DoJumpStart();
			Stage = 3;
			PhaseStart = World->GetTimeSeconds();
			return false;
		}

		if (Stage == 3)
		{
			if (World->GetTimeSeconds() - PhaseStart < 0.1) { return false; }
			Test->TestTrue(TEXT("Template jump becomes airborne"), Character->GetCharacterMovement()->IsFalling());
			Character->DoJumpEnd();
			Stage = 4;
			return false;
		}

		if (Stage == 4)
		{
			ANeonCityPlayerCharacter* Player = Cast<ANeonCityPlayerCharacter>(Character);
			if (!Test->TestNotNull(TEXT("Player extension available for interaction"), Player)) { return true; }
			// Isolate test geometry in the existing PIE world's empty air; nothing is saved.
			Character->GetCharacterMovement()->DisableMovement();
			Character->SetActorLocation(FVector(0.f, 0.f, 2000.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator::ZeroRotator);
			FVector Eye;
			FRotator View;
			Character->GetActorEyesViewPoint(Eye, View);
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Door = World->SpawnActor<ANeonCityInteractionTestDoor>(
				Eye + FVector(150.f, -50.f, -100.f), FRotator::ZeroRotator, Spawn);
			if (!Test->TestNotNull(TEXT("Temporary test door spawned"), Door.Get())) { return true; }
			UInteractionComponent* Interaction = Player->GetInteractionComponent();
			Test->TestTrue(TEXT("Interface detection finds nearby door"), Interaction->RefreshFocusedInteractable() == Door.Get());

			AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
			UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
			Blocker->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10.f, 60.f, 100.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->RegisterComponent();
			Blocker->SetActorLocation(Eye + FVector(70.f, 0.f, 0.f));
			Test->TestNull(TEXT("Blocking non-interactable prevents detection"), Interaction->RefreshFocusedInteractable());
			Test->TestFalse(TEXT("Cannot interact through an obstruction"), Interaction->TryInteract());
			Test->TestFalse(TEXT("Occluded door remains closed"), Door->IsOpen());
			Blocker->Destroy();

			Door->AddActorWorldOffset(FVector(600.f, 0.f, 0.f));
			Test->TestFalse(TEXT("Out-of-range interaction rejected"), Interaction->TryInteract());
			IInteractableInterface::Execute_Interact(Door.Get(), Character);
			Test->TestFalse(TEXT("Door independently rejects distant execution"), Door->IsOpen());
			Door->AddActorWorldOffset(FVector(-600.f, 0.f, 0.f));
			PhaseStart = World->GetTimeSeconds();
			Stage = 5;
			return false;
		}

		if (Stage == 5)
		{
			if (World->GetTimeSeconds() - PhaseStart < 0.25) { return false; }
			ANeonCityPlayerCharacter* Player = CastChecked<ANeonCityPlayerCharacter>(Character);
			Test->TestTrue(TEXT("Automatic proximity focus updates"), Player->GetInteractionComponent()->GetFocusedInteractable() == Door.Get());
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0),
				EKeys::F, IE_Pressed, FPlatformTime::Cycles64()));
			PhaseStart = World->GetTimeSeconds();
			Stage = 6;
			return false;
		}

		if (Stage == 6)
		{
			if (World->GetTimeSeconds() - PhaseStart < 0.2) { return false; }
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0),
				EKeys::F, IE_Released, FPlatformTime::Cycles64()));
			Test->TestTrue(TEXT("F key opens door through Enhanced Input and interface"), Door.IsValid() && Door->IsOpen());
			if (Door.IsValid()) { Door->Destroy(); }
			ANeonCityPlayerCharacter* Player = CastChecked<ANeonCityPlayerCharacter>(Character);
			Test->TestNull(TEXT("Destroyed target is cleared"), Player->GetInteractionComponent()->RefreshFocusedInteractable());
			Test->TestFalse(TEXT("No target interaction safely fails"), Player->GetInteractionComponent()->TryInteract());
			Test->AddInfo(TEXT("Player extension PIE: preserved template movement, look, jump, animation, input; automatic focus, obstruction, range and F-key door interaction checks completed."));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Stage = 0;
	int32 MovementFrames = 0;
	double StartTime = 0.0;
	double PhaseStart = 0.0;
	FVector Origin = FVector::ZeroVector;
	FRotator LookBefore = FRotator::ZeroRotator;
	TWeakObjectPtr<ANeonCityInteractionTestDoor> Door;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityThirdPersonSmoke,
	"NeonCity.Architecture.ThirdPersonPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNeonCityThirdPersonSmoke::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityTests::FVerifyThirdPersonPIE>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
