#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UObject/UObjectIterator.h"
#include "InputKeyEventArgs.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"

namespace NeonCityVehicleTests
{
class FVerifyDrivingPIE : public IAutomationLatentCommand
{
public:
	explicit FVerifyDrivingPIE(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartTime == 0.0) { StartTime = Now; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (Now - StartTime > 120.0)
		{
			Test->AddError(FString::Printf(TEXT("Vehicle PIE timed out at stage %d."), Stage));
			return Finish(PC);
		}
		if (!World || !PC || !World->HasBegunPlay()) { return false; }
		++StageFrames;
		const double SimTime = World->GetTimeSeconds();
		auto Wait = [&](double Duration) { return SimTime - PhaseStart < Duration || StageFrames < 3; };
		auto Advance = [&](int32 Next) { Stage = Next; PhaseStart = SimTime; StageFrames = 0; return false; };
		if (Stage == 0)
		{
			Player = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
			if (!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			int32 Sedans = 0;
			for (TActorIterator<ANeonCityStarterSedan> It(World); It; ++It) { if(It->GetClass()==ANeonCityStarterSedan::StaticClass()) { Vehicle = *It; ++Sedans; } }
			if (!Test->TestEqual(TEXT("Exactly one Starter Sedan placed"), Sedans, 1)) { return Finish(PC); }
			Test->TestTrue(TEXT("Vehicle test uses South Beach"), World->GetMapName().Contains(TEXT("NeonCity_SouthBeach_Test")));
			OriginalCameraFOV = Player->GetFollowCamera()->FieldOfView;
			OriginalCameraArm = Player->GetCameraBoom()->TargetArmLength;
			OriginalData = Player->GetPlayerDataComponent();
			CharacterContextCount = MappingCount(PC);
			if (!Test->TestTrue(TEXT("Enhanced Input contexts are available"), CharacterContextCount > 0)) { return Finish(PC); }
			return Advance(1);
		}
		if (!Player.IsValid() || !Vehicle.IsValid())
		{
			Test->AddError(TEXT("Original player or placed sedan was destroyed during driving test."));
			return Finish(PC);
		}
		UChaosWheeledVehicleMovementComponent* Chaos = Vehicle->GetChaosMovement();
		if (Stage == 1)
		{
			if (Wait(1.0)) { return false; }
			Test->TestEqual(TEXT("Chaos has four physical wheels"), Chaos->Wheels.Num(), 4);
			LogRigGeometry();
			if (!PlacePlayerAtDoor(World, PC)) { return Finish(PC); }
			return Advance(2);
		}
		if (Stage == 2 || Stage == 16)
		{
			if (Wait(0.4)) { return false; }
			LogInteractionProbe(PC);
			if (!Test->TestTrue(TEXT("Generic interaction detects the sedan"),
				Player->GetInteractionComponent()->RefreshFocusedInteractable() == Vehicle.Get())) { return Finish(PC); }
			Test->TestTrue(TEXT("Nearby focused sedan displays F entry prompt"), Vehicle->IsEntryPromptVisible());
			Key(PC, EKeys::F, IE_Pressed);
			return Advance(Stage == 2 ? 3 : 17);
		}
		if (Stage == 3)
		{
			if (Wait(0.25)) { return false; }
			Key(PC, EKeys::F, IE_Released);
			if (!VerifyEntered(PC)) { return Finish(PC); }
			VehicleContextCount = MappingCount(PC);
			return Advance(32);
		}
		if (Stage == 32)
		{
			// The populated map now has traffic: wait for a safe gap before leaving parking.
			const FVector Exit(Vehicle->GetActorLocation().X,0.f,89.f);
			for (TActorIterator<ANeonCityTrafficVehicle> It(World); It; ++It)
			{
				if (FVector::Dist2D(It->GetActorLocation(),Exit) < 1200.f)
				{
					if (!Wait(20.0)) { Test->AddError(TEXT("No traffic gap at parking exit.")); return Finish(PC); }
					return false;
				}
			}
			ParkingOrigin = Vehicle->GetActorLocation();
			Key(PC,EKeys::W,IE_Pressed);
			return Advance(30);
		}
		if (Stage == 30)
		{
			if (Wait(3.0)) { return false; }
			Test->AddInfo(FString::Printf(TEXT("Parking departure: %s -> %s, %.2f km/h"), *ParkingOrigin.ToString(), *Vehicle->GetActorLocation().ToString(), Vehicle->GetDrivingComponent()->GetSpeedKmh()));
			Test->TestTrue(TEXT("Sedan physically drives from parking across driveway onto road"), Vehicle->GetActorLocation().Y < 580.f && GroundedWheels() >= 2);
			Key(PC,EKeys::W,IE_Released);
			Key(PC,EKeys::SpaceBar,IE_Pressed);
			return Advance(31);
		}
		if (Stage == 31)
		{
			if (Wait(1.5)) { return false; }
			Key(PC,EKeys::SpaceBar,IE_Released);
			// Test-only placement isolates driving from parking obstacles; subsequent motion is all Chaos.
			Vehicle->SetActorLocationAndRotation(FVector(3500.f, -280.f, 160.f),
				FRotator(0.f, 180.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			Vehicle->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Vehicle->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Vehicle->GetMesh()->WakeAllRigidBodies();
			return Advance(4);
		}
		if (Stage == 4)
		{
			if (Wait(1.0)) { return false; }
			if (!Test->TestTrue(TEXT("Sedan settles onto road through wheel contacts"), GroundedWheels() >= 2)) { return Finish(PC); }
			DrivingOrigin = Vehicle->GetActorLocation();
			InitialVehicleArm = Vehicle->GetVehicleSpringArm()->TargetArmLength;
			InitialVehicleFOV = Vehicle->GetVehicleCamera()->FieldOfView;
			Key(PC, EKeys::W, IE_Pressed);
			return Advance(5);
		}
		if (Stage == 5)
		{
			if (Wait(3.0)) { return false; }
			const float Speed = Vehicle->GetDrivingComponent()->GetSpeedKmh();
			Test->AddInfo(FString::Printf(TEXT("After W: speed %.2f km/h, travel %.1f cm, grounded wheels %d."),
				Speed, FVector::Dist2D(DrivingOrigin, Vehicle->GetActorLocation()), GroundedWheels()));
			if (!Test->TestTrue(TEXT("W accelerates actual Chaos vehicle"), Speed > 3.f)
				|| !Test->TestTrue(TEXT("Vehicle travels under wheel physics"), FVector::Dist2D(DrivingOrigin, Vehicle->GetActorLocation()) > 100.f))
			{
				return Finish(PC);
			}
			Test->TestTrue(TEXT("Tires remain in road contact while accelerating"), GroundedWheels() >= 2);
			Test->TestTrue(TEXT("Speed extends vehicle camera distance"), Vehicle->GetVehicleSpringArm()->TargetArmLength > InitialVehicleArm + 2.f);
			Test->TestTrue(TEXT("Speed modestly increases vehicle FOV"), Vehicle->GetVehicleCamera()->FieldOfView > InitialVehicleFOV + 0.05f
				&& Vehicle->GetVehicleCamera()->FieldOfView <= 96.1f);
			Key(PC, EKeys::W, IE_Released);
			Key(PC, EKeys::F, IE_Pressed);
			return Advance(6);
		}
		if (Stage == 6)
		{
			if (Wait(0.2)) { return false; }
			Key(PC, EKeys::F, IE_Released);
			if (!Test->TestTrue(TEXT("F refuses unsafe exit while car is moving"), PC->GetPawn() == Vehicle.Get())) { return Finish(PC); }
			SteeringYaw = Vehicle->GetActorRotation().Yaw;
			Key(PC, EKeys::D, IE_Pressed);
			return Advance(7);
		}
		if (Stage == 7)
		{
			if (Wait(0.6)) { return false; }
			const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(SteeringYaw, Vehicle->GetActorRotation().Yaw));
			Test->AddInfo(FString::Printf(TEXT("D steering changes chassis heading %.2f degrees."), YawDelta));
			Test->TestTrue(TEXT("D turns chassis through Chaos steering"), YawDelta > 0.5f);
			Test->TestTrue(TEXT("D produces positive front-wheel steering"), Chaos->Wheels.Num() >= 2 && Chaos->Wheels[0]->GetSteerAngle() > 0.5f);
			Key(PC, EKeys::D, IE_Released);
			Key(PC, EKeys::A, IE_Pressed);
			return Advance(8);
		}
		if (Stage == 8)
		{
			if (Wait(0.6)) { return false; }
			Test->TestTrue(TEXT("A produces opposite front-wheel steering"), Chaos->Wheels.Num() >= 2 && Chaos->Wheels[0]->GetSteerAngle() < -0.5f);
			Key(PC, EKeys::A, IE_Released);
			SpeedBeforeBrake = FMath::Abs(Vehicle->GetDrivingComponent()->GetSpeedKmh());
			Key(PC, EKeys::SpaceBar, IE_Pressed);
			return Advance(9);
		}
		if (Stage == 9)
		{
			if (Wait(2.0)) { return false; }
			const float Speed = FMath::Abs(Vehicle->GetDrivingComponent()->GetSpeedKmh());
			Test->AddInfo(FString::Printf(TEXT("Space brake: %.2f -> %.2f km/h."), SpeedBeforeBrake, Speed));
			if (!Test->TestTrue(TEXT("Space service brake slows the vehicle"), Speed < FMath::Max(2.f, SpeedBeforeBrake * 0.35f)))
			{
				return Finish(PC);
			}
			Key(PC, EKeys::SpaceBar, IE_Released);
			ReverseOrigin = Vehicle->GetActorLocation();
			ReverseForward = Vehicle->GetActorForwardVector();
			Key(PC, EKeys::S, IE_Pressed);
			return Advance(10);
		}
		if (Stage == 10)
		{
			// Preserve the original assertion while diagnosing intermittent map contact.
			if (SimTime - LastReverseProbe > 0.2)
			{
				LastReverseProbe = SimTime;
				Test->AddInfo(FString::Printf(TEXT("Reverse probe t=%.3f position=%s speed=%.3f gear=%d target=%d throttle=%.2f brake=%.2f health=%.1f wheels=%d"),
					SimTime-PhaseStart,*Vehicle->GetActorLocation().ToString(),Vehicle->GetDrivingComponent()->GetSpeedKmh(),
					Chaos->GetCurrentGear(),Chaos->GetTargetGear(),Chaos->GetThrottleInput(),Chaos->GetBrakeInput(),
					Vehicle->GetHealthComponent()->GetCurrentHealth(),GroundedWheels()));
				FHitResult Rear; FCollisionQueryParams Probe(SCENE_QUERY_STAT(DrivingReverseProbe),false,Vehicle.Get());
				Probe.AddIgnoredActor(Player.Get());
				const FVector From=Vehicle->GetActorLocation()+FVector(0,0,65);
				if (World->LineTraceSingleByChannel(Rear,From,From-Vehicle->GetActorForwardVector()*700,ECC_Visibility,Probe))
					Test->AddInfo(FString::Printf(TEXT("Reverse rear clearance actor=%s component=%s distance=%.1f"),
						*GetNameSafe(Rear.GetActor()),*GetNameSafe(Rear.GetComponent()),Rear.Distance));
			}
			if (Wait(1.8)) { return false; }
			if (auto* Impacts=World->GetSubsystem<UPhysicalImpactSubsystem>())
				for (const auto& E:Impacts->GetRecentEvents())
					if (E.SourceActor.Get()==Vehicle.Get() || E.TargetActor.Get()==Vehicle.Get())
						Test->AddInfo(FString::Printf(TEXT("Driving contact time=%.3f source=%s target=%s speed=%.2f damage=%.2f location=%s"),
							E.Timestamp,*GetNameSafe(E.SourceActor.Get()),*GetNameSafe(E.TargetActor.Get()),E.ImpactSpeed,E.AppliedDamage,*E.ImpactLocation.ToString()));
			const float ReverseTravel = FVector::DotProduct(Vehicle->GetActorLocation() - ReverseOrigin, ReverseForward);
			Test->AddInfo(FString::Printf(TEXT("S reverse: %.2f km/h, longitudinal travel %.1f cm."),
				Vehicle->GetDrivingComponent()->GetSpeedKmh(), ReverseTravel));
			Test->TestTrue(TEXT("S selects reverse and drives backwards"), Vehicle->GetDrivingComponent()->GetSpeedKmh() < -1.f);
			Test->TestTrue(TEXT("Reverse produces physical rearward travel"), ReverseTravel < -30.f);
			Key(PC, EKeys::S, IE_Released);
			Key(PC, EKeys::SpaceBar, IE_Pressed);
			return Advance(11);
		}
		if (Stage == 11)
		{
			if (Wait(2.0)) { return false; }
			if (!Test->TestTrue(TEXT("Car stops before exit"), FMath::Abs(Vehicle->GetDrivingComponent()->GetSpeedKmh()) < 1.f)) { return Finish(PC); }
			Key(PC, EKeys::SpaceBar, IE_Released);
			Key(PC, EKeys::F, IE_Pressed);
			return Advance(12);
		}
		if (Stage == 12 || Stage == 19)
		{
			if (Wait(0.3)) { return false; }
			Key(PC, EKeys::F, IE_Released);
			if (!VerifyExited(PC)) { return Finish(PC); }
			if (Stage == 19)
			{
				Test->AddInfo(TEXT("Vehicle PIE completed: generic F entry, possession/camera/input switch, Chaos acceleration/contact, D/A steering, service brake, reverse, moving-exit rejection, restored character, repeated entry/exit and mapping cleanup."));
				if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityVehicleCapture"))) { return Finish(PC); }
				FActorSpawnParameters Spawn;
				Spawn.ObjectFlags |= RF_Transient;
				CaptureCamera = World->SpawnActor<ACameraActor>(Spawn);
				if (!Test->TestNotNull(TEXT("Optional vehicle capture camera created"), CaptureCamera.Get())) { return Finish(PC); }
				const FVector Focus = Vehicle->GetActorLocation() + FVector(0.f, 0.f, 90.f);
				const FVector CameraLocation = Vehicle->GetActorLocation()
					+ Vehicle->GetActorForwardVector() * 800.f - Vehicle->GetActorRightVector() * 950.f + FVector(0.f, 0.f, 480.f);
				CaptureCamera->SetActorLocation(CameraLocation);
				CaptureCamera->SetActorRotation((Focus - CameraLocation).Rotation());
				CaptureCamera->GetCameraComponent()->SetFieldOfView(55.f);
				PC->SetViewTarget(CaptureCamera.Get());
				return Advance(20);
			}
			return Advance(13);
		}
		if (Stage == 13)
		{
			if (Wait(0.3)) { return false; }
			CharacterMoveOrigin = Player->GetActorLocation();
			// Face away from the car so restored movement is not measured against its body.
			PC->SetControlRotation((Player->GetActorLocation() - Vehicle->GetActorLocation()).Rotation());
			Key(PC, EKeys::W, IE_Pressed);
			return Advance(14);
		}
		if (Stage == 14)
		{
			if (Wait(0.5)) { return false; }
			Key(PC, EKeys::W, IE_Released);
			Test->TestTrue(TEXT("W controls original character again after exit"),
				FVector::Dist2D(CharacterMoveOrigin, Player->GetActorLocation()) > 20.f);
			return Advance(15);
		}
		if (Stage == 15)
		{
			if (Wait(0.3)) { return false; }
			if (!PlacePlayerAtDoor(World, PC)) { return Finish(PC); }
			return Advance(16);
		}
		if (Stage == 17)
		{
			if (Wait(0.25)) { return false; }
			Key(PC, EKeys::F, IE_Released);
			if (!VerifyEntered(PC)) { return Finish(PC); }
			Test->TestEqual(TEXT("Second entry does not accumulate input contexts"), MappingCount(PC), VehicleContextCount);
			return Advance(18);
		}
		if (Stage == 18)
		{
			if (Wait(0.3)) { return false; }
			Key(PC, EKeys::F, IE_Pressed);
			return Advance(19);
		}
		if (Stage == 20)
		{
			if (Wait(10.0)) { return false; }
			const FString CaptureDirectory = FPaths::ProjectSavedDir() / TEXT("Phase5");
			IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
			FScreenshotRequest::RequestScreenshot(CaptureDirectory / TEXT("StarterSedan.png"), false, false);
			return Advance(21);
		}
		if (Stage == 21)
		{
			if (Wait(0.5) || FScreenshotRequest::IsScreenshotRequested()) { return false; }
			Test->TestTrue(TEXT("Optional rendered Starter Sedan screenshot saved"),
				IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir() / TEXT("Phase5/StarterSedan.png"))));
			return Finish(PC);
		}
		return false;
	}

private:
	static void Key(APlayerController* PC, FKey InKey, EInputEvent Event)
	{
		if (PC)
		{
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0),
				InKey, Event, FPlatformTime::Cycles64()));
		}
	}

	bool Finish(APlayerController* PC)
	{
		for (FKey Input : {EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::SpaceBar, EKeys::F}) { Key(PC, Input, IE_Released); }
		if (CaptureCamera.IsValid())
		{
			if (PC && Player.IsValid()) { PC->SetViewTarget(Player.Get()); }
			CaptureCamera->Destroy();
			CaptureCamera.Reset();
		}
		return true;
	}

	void LogInteractionProbe(APlayerController* PC) const
	{
		FVector Eye;
		FRotator EyeRotation;
		Player->GetActorEyesViewPoint(Eye, EyeRotation);
		FHitResult Hit;
		AActor* Candidate = Player->GetInteractionComponent()->DetectInteractable(
			Eye, Eye + PC->GetControlRotation().Vector() * 250.f, Hit);
		Test->AddInfo(FString::Printf(TEXT("Interaction probe stage %d: car location %s rotation %s velocity %s; player location %s eye %s control rotation %s; CanEnter=%d; hit actor=%s component=%s point=%s distance=%.1f candidate=%s."),
			Stage, *Vehicle->GetActorLocation().ToString(), *Vehicle->GetActorRotation().ToString(), *Vehicle->GetVelocity().ToString(),
			*Player->GetActorLocation().ToString(), *Eye.ToString(), *PC->GetControlRotation().ToString(),
			Vehicle->GetVehicleInteraction()->CanEnter(Player.Get()) ? 1 : 0,
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToString(), Hit.Distance, *GetNameSafe(Candidate)));
	}

	void LogRigGeometry() const
	{
		USkeletalMeshComponent* Mesh = Vehicle->GetMesh();
		USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
		if (!Asset) { return; }
		const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
		for (int32 Index = 0; Index < Vehicle->GetChaosMovement()->WheelSetups.Num(); ++Index)
		{
			const FName Bone = Vehicle->GetChaosMovement()->WheelSetups[Index].BoneName;
			const int32 BoneIndex = Skeleton.FindBoneIndex(Bone);
			if (BoneIndex == INDEX_NONE)
			{
				Test->AddError(FString::Printf(TEXT("Configured wheel bone %s does not exist in official rig."), *Bone.ToString()));
				continue;
			}
			FTransform Reference = Skeleton.GetRefBonePose()[BoneIndex];
			for (int32 Parent = Skeleton.GetParentIndex(BoneIndex); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
			{
				Reference *= Skeleton.GetRefBonePose()[Parent];
			}
			const float Radius = Vehicle->GetChaosMovement()->Wheels.IsValidIndex(Index)
				? Vehicle->GetChaosMovement()->Wheels[Index]->WheelRadius : 0.f;
			Test->AddInfo(FString::Printf(TEXT("Official rig %s wheel %d bone %s: reference position %s cm, Chaos radius %.1f cm."),
				*Asset->GetPathName(), Index, *Bone.ToString(), *Reference.GetLocation().ToString(), Radius));
		}
		if (const UPhysicsAsset* Physics = Mesh->GetPhysicsAsset())
		{
			const FBox Bounds = Physics->CalcAABB(Mesh, FTransform::Identity);
			Test->AddInfo(FString::Printf(TEXT("PhysicsAsset %s component-space bounds: min %s max %s size %s cm."),
				*Physics->GetPathName(), *Bounds.Min.ToString(), *Bounds.Max.ToString(), *Bounds.GetSize().ToString()));
		}
		if (const FBodyInstance* Body = Mesh->GetBodyInstance())
		{
			const FBox Bounds = Body->GetBodyBoundsLocal();
			Test->AddInfo(FString::Printf(TEXT("Simulating chassis body-local bounds: min %s max %s size %s cm; body mass %.1f kg."),
				*Bounds.Min.ToString(), *Bounds.Max.ToString(), *Bounds.GetSize().ToString(), Body->GetBodyMass()));
		}
	}

	int32 MappingCount(APlayerController* PC) const
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
			? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		if (!Subsystem) { return -1; }
		int32 Count = 0;
		for (TObjectIterator<UInputMappingContext> It; It; ++It)
		{
			Count += Subsystem->HasMappingContext(*It) ? 1 : 0;
		}
		return Count;
	}

	int32 GroundedWheels() const
	{
		int32 Count = 0;
		UChaosWheeledVehicleMovementComponent* Chaos = Vehicle->GetChaosMovement();
		for (int32 Index = 0; Index < Chaos->GetNumWheels(); ++Index) { Count += Chaos->GetWheelState(Index).bInContact ? 1 : 0; }
		return Count;
	}

	bool PlacePlayerAtDoor(UWorld* World, APlayerController* PC)
	{
		const FVector Side = Vehicle->GetActorLocation() + Vehicle->GetActorRightVector() * -210.f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NeonCityVehicleTestGround), false);
		Params.AddIgnoredActor(Player.Get());
		Params.AddIgnoredActor(Vehicle.Get());
		FHitResult Ground;
		if (!Test->TestTrue(TEXT("Ground exists alongside parked sedan"),
			World->LineTraceSingleByChannel(Ground, Side + FVector(0.f, 0.f, 400.f),
				Side - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))) { return false; }
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->SetActorLocation(Ground.ImpactPoint + FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f),
			false, nullptr, ETeleportType::TeleportPhysics);
		FVector Eye;
		FRotator View;
		Player->GetActorEyesViewPoint(Eye, View);
		PC->SetControlRotation((Vehicle->GetActorLocation() + FVector(0.f, 0.f, 90.f) - Eye).Rotation());
		return true;
	}

	bool VerifyEntered(APlayerController* PC)
	{
		if (!Test->TestTrue(TEXT("F possesses vehicle pawn"), PC->GetPawn() == Vehicle.Get())) { return false; }
		Test->TestTrue(TEXT("Vehicle remembers original driver"), Vehicle->GetVehicleInteraction()->GetDriver() == Player.Get());
		Test->TestTrue(TEXT("Vehicle becomes camera view target"), PC->GetViewTarget() == Vehicle.Get());
		Test->TestTrue(TEXT("Original character is hidden and collision disabled while seated"), Player->IsHidden() && !Player->GetActorEnableCollision());
		Test->TestFalse(TEXT("Entry prompt hidden while driving"), Vehicle->IsEntryPromptVisible());
		return true;
	}

	bool VerifyExited(APlayerController* PC)
	{
		if (!Test->TestTrue(TEXT("F restores the same character pawn"), PC->GetPawn() == Player.Get())) { return false; }
		Test->TestTrue(TEXT("Character visible with collision restored"), !Player->IsHidden() && Player->GetActorEnableCollision());
		Test->TestTrue(TEXT("Character camera view target restored"), PC->GetViewTarget() == Player.Get());
		Test->TestTrue(TEXT("Original player data component is preserved"), Player->GetPlayerDataComponent() == OriginalData);
		Test->TestNull(TEXT("Vehicle clears driver reference"), Vehicle->GetVehicleInteraction()->GetDriver());
		Test->TestEqual(TEXT("Character camera FOV unchanged"), Player->GetFollowCamera()->FieldOfView, OriginalCameraFOV);
		Test->TestEqual(TEXT("Character camera arm unchanged"), Player->GetCameraBoom()->TargetArmLength, OriginalCameraArm);
		Test->TestEqual(TEXT("Exit restores input context count"), MappingCount(PC), CharacterContextCount);
		return true;
	}

	FAutomationTestBase* Test;
	int32 Stage = 0;
	int32 StageFrames = 0;
	int32 CharacterContextCount = 0;
	int32 VehicleContextCount = 0;
	double StartTime = 0.0;
	double PhaseStart = 0.0;
	double LastReverseProbe = 0.0;
	float OriginalCameraFOV = 0.f;
	float OriginalCameraArm = 0.f;
	float InitialVehicleArm = 0.f;
	float InitialVehicleFOV = 0.f;
	float SpeedBeforeBrake = 0.f;
	float SteeringYaw = 0.f;
	FVector ParkingOrigin = FVector::ZeroVector;
	FVector DrivingOrigin = FVector::ZeroVector;
	FVector ReverseOrigin = FVector::ZeroVector;
	FVector ReverseForward = FVector::ForwardVector;
	FVector CharacterMoveOrigin = FVector::ZeroVector;
	UNeonCityPlayerDataComponent* OriginalData = nullptr;
	TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
	TWeakObjectPtr<ANeonCityStarterSedan> Vehicle;
	TWeakObjectPtr<ACameraActor> CaptureCamera;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityVehicleDriving, "NeonCity.Vehicle.DrivingPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNeonCityVehicleDriving::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityVehicleTests::FVerifyDrivingPIE>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
