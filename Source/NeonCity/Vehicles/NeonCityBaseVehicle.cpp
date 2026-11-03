#include "Vehicles/NeonCityBaseVehicle.h"
#include "Wanted/SearchSystem.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Explosion/ExplosionEvent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleStatusWidget.h"
#include "Engine/GameInstance.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Adventure/NacreRoofComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/NeonCityVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Interaction/InteractionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

ANeonCityBaseVehicle::ANeonCityBaseVehicle()
{
 CreateDefaultSubobject<UTransportOccupantComponent>(TEXT("OccupantPresentation"));
	DamageHealth=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 DamageState=CreateDefaultSubobject<UVehicleDamageComponent>(TEXT("VehicleDamageState"));
 CreateDefaultSubobject<UPhysicalImpactComponent>(TEXT("PhysicalImpact"));
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(TEXT("Vehicle"));
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UBoxComponent* InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(GetMesh());
	InteractionBounds->SetRelativeLocation(FVector(0.f,0.f,100.f));
	InteractionBounds->SetBoxExtent(FVector(235.f,100.f,90.f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Driving = CreateDefaultSubobject<UVehicleComponent>(TEXT("VehicleComponent"));
 RoadCruise=CreateDefaultSubobject<URoadCruiseComponent>(TEXT("RoadCruise"));
 Driving->AddTickPrerequisiteComponent(RoadCruise);
	VehicleInteraction = CreateDefaultSubobject<UVehicleInteraction>(TEXT("VehicleInteraction"));
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("VehicleSpringArm"));
	SpringArm->SetupAttachment(GetMesh());
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 550.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 5.f;
	SpringArm->CameraLagMaxDistance = 80.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCamera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	CameraExtension = CreateDefaultSubobject<UNeonCityCameraExtensionComponent>(TEXT("VehicleCameraExtension"));
 // Driver's-eye camera: placed at the driver's head from the seat each time the view is chosen.
 CockpitCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCockpitCamera"));CockpitCamera->SetupAttachment(GetMesh());
 CockpitCamera->SetFieldOfView(82.f);CockpitCamera->SetAutoActivate(false);
	EntryPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EntryPrompt"));
	EntryPrompt->SetupAttachment(GetMesh());
	EntryPrompt->SetRelativeLocation(FVector(0.f, 0.f, 190.f));
	EntryPrompt->SetText(NSLOCTEXT("NeonCity", "EnterSedanPrompt", "F - Enter Starter Sedan"));
	EntryPrompt->SetWorldSize(22.f);
	EntryPrompt->SetHorizontalAlignment(EHTA_Center);
	EntryPrompt->SetTextRenderColor(FColor(245, 235, 185));
	EntryPrompt->SetVisibility(false);

	UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement();
	Movement->Mass = 1450.f;
	Movement->bReverseAsBrake = false;
	Movement->TransmissionSetup.bUseAutoReverse = false;
	Movement->TransmissionSetup.bUseAutomaticGears = true;
	Movement->TransmissionSetup.FinalRatio = 3.5f;
	Movement->TransmissionSetup.ForwardGearRatios = {3.2f, 2.1f, 1.4f, 1.f, 0.8f};
	Movement->TransmissionSetup.ReverseGearRatios = {3.f};
	Movement->TransmissionSetup.ChangeUpRPM = 4500.f;
	Movement->TransmissionSetup.ChangeDownRPM = 1800.f;
	Movement->EngineSetup.MaxRPM = 5500.f;
	Movement->EngineSetup.EngineIdleRPM = 800.f;
	Movement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.f, 0.6f);
	Movement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(2000.f, 1.f);
	Movement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(4500.f, 0.85f);
	Movement->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5500.f, 0.5f);
	Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive;
	Movement->SteeringSetup.SteeringCurve.GetRichCurve()->Reset();
	Movement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(0.f, 1.f);
	Movement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(30.f, 0.65f);
	Movement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(60.f, 0.35f);
	Movement->DragCoefficient = 0.32f;
	Movement->DownforceCoefficient = 0.05f;
	Movement->ChassisWidth = 180.f;
	Movement->ChassisHeight = 140.f;
	Movement->bEnableCenterOfMassOverride = true;
	Movement->CenterOfMassOverride = FVector(0.f, 0.f, 45.f);
	Movement->WheelSetups.SetNum(4);
	const FName Bones[] = {TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"), TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Movement->WheelSetups[Index].WheelClass = Index < 2 ? UNeonCityVehicleFrontWheel::StaticClass() : UNeonCityVehicleWheel::StaticClass();
		Movement->WheelSetups[Index].BoneName = Bones[Index];
	}
	// Parked cars still receive a handbrake. Actual driving is gated by VehicleInteraction/possession.
	Movement->SetRequiresControllerForInputs(false);
	Movement->AddTickPrerequisiteComponent(Driving);
}

UChaosWheeledVehicleMovementComponent* ANeonCityBaseVehicle::GetChaosMovement() const
{
	return CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

void ANeonCityBaseVehicle::BeginPlay()
{
	Super::BeginPlay();
 GetMesh()->CanCharacterStepUpOn=ECB_Yes;
 // Suspension samples road geometry, not the character stepping beside a wheel. Chassis still blocks Pawn.
 GetChaosMovement()->SetWheelTraceResponseToChannel(ECC_Pawn,ECR_Ignore);
	const UVehicleData* Tuning = GetVehicleProfile();
 if(!VehicleID.IsValid()) VehicleID=FGuid::NewGuid();
 DamageHealth->SetMaxHealth(Tuning->Durability); DamageHealth->SetCurrentHealth(Tuning->Durability);
 DamageState->OnCollisionFeedback.AddDynamic(this,&ANeonCityBaseVehicle::CollisionFeedback);
 auto* Registry=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
 if(Registry->RegisterLiveVehicle(this)) { if(bStartsOwned && !Registry->IsOwned(VehicleID)) Registry->GrantVehicle(this); }
 else UE_LOG(LogTemp,Warning,TEXT("Vehicle identity rejected: %s (%s)"),*GetName(),*VehicleID.ToString());
 EntryPrompt->SetText(FText::Format(NSLOCTEXT("NeonCity","EnterVehiclePrompt","F - Enter {0}"),Tuning->DisplayName));
	const TArray<float> FittedWheelRadii = GetFittedWheelRadii();
	Driving->Configure(GetChaosMovement(), Tuning, FittedWheelRadii);
	GetMesh()->SetMassOverrideInKg(NAME_None, GetChaosMovement()->Mass, true);
	Driving->SetDrivingEnabled(IsPlayerControlled());
	CameraExtension->SetCameraContext(ENeonCityCameraContext::Vehicle);
}

bool ANeonCityBaseVehicle::CanInteract_Implementation(AActor* Interactor) const { return VehicleInteraction->CanEnter(Interactor); }
FText ANeonCityBaseVehicle::GetInteractionName_Implementation() const { return FText::Format(NSLOCTEXT("NeonCity","EnterVehicle","Enter {0}"),GetVehicleProfile()->DisplayName); }
void ANeonCityBaseVehicle::Interact_Implementation(AActor* Interactor) { VehicleInteraction->Enter(Interactor); }
bool ANeonCityBaseVehicle::IsEntryPromptVisible() const { return EntryPrompt->IsVisible(); }

void ANeonCityBaseVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (VehicleInteraction->GetDriver() && !GetController()) { VehicleInteraction->RestoreDriver(true); }
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		if(!StatusWidget)
  {
   StatusWidget=CreateWidget<UVehicleStatusWidget>(Cast<APlayerController>(GetController()));
   if(StatusWidget) { StatusWidget->SetVisibility(ESlateVisibility::HitTestInvisible); StatusWidget->AddToViewport(3); }
  }
  if(StatusWidget)
  {
   const FString Condition=StaticEnum<ENeonCityVehicleCondition>()->GetNameStringByValue(int64(DamageState->GetCondition()));
   const FString Engine=StaticEnum<ENeonCityEngineState>()->GetNameStringByValue(int64(Driving->GetEngineState()));
   FString Text=FString::Printf(TEXT("%s  |  %.0f km/h\nEngine: %s  |  %s  %.0f / %.0f"),*GetVehicleProfile()->DisplayName.ToString(),FMath::Abs(Driving->GetSpeedKmh()),*Engine,*Condition,DamageHealth->GetCurrentHealth(),DamageHealth->GetMaxHealth());
   if(DamageState->GetRecentDamage()>0) Text+=FString::Printf(TEXT("  -%.0f HP"),DamageState->GetRecentDamage());
   if(RoadCruise->IsAvailable())Text+=TEXT("\n")+RoadCruise->GetStatusText().ToString();
   if(const auto* Roof=FindComponentByClass<UNacreRoofComponent>();Roof && Roof->IsEnabled())Text+=TEXT("\n")+Roof->GetStatus().ToString();
   if(Driving->GetStabilityLevel()>.25f)Text+=TEXT("  |  ESC");
   Text+=bCockpitView?TEXT("\nV  Chase view  |  Mouse  Look around"):TEXT("\nV  Driver's view");
   StatusWidget->UpdateStatus(FText::FromString(Text),DamageState->GetRecentDamage()>0);
  }
  if(bCockpitView)UpdateCockpitCamera(DeltaSeconds);
  else CameraExtension->ApplyVehicleCamera(Camera, SpringArm, GetChaosMovement()->GetForwardSpeed(),
			Driving->GetMaxSpeedCmPerSecond(), DeltaSeconds);
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr;
	const bool bFocused = Interaction && Interaction->GetFocusedInteractable() == this && CanInteract_Implementation(Pawn);
	EntryPrompt->SetVisibility(bFocused);
	if (bFocused)
	{
		const FVector ViewLocation = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Pawn->GetActorLocation();
		EntryPrompt->SetWorldRotation((ViewLocation - EntryPrompt->GetComponentLocation()).Rotation());
	}
}

void ANeonCityBaseVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
 GetWorld()->GetSubsystem<USearchSystem>()->NotifyVehiclePossessed(this);
	Driving->SetDrivingEnabled(true);
	GetMesh()->WakeAllRigidBodies();
}

void ANeonCityBaseVehicle::UnPossessed()
{
 RoadCruise->Cancel(TEXT("Driver left vehicle"));
 if(StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget=nullptr; }
 SetCockpitView(false);
	RemoveVehicleMapping();
	Driving->SetDrivingEnabled(false);
	Super::UnPossessed();
}

void ANeonCityBaseVehicle::EndPlay(EEndPlayReason::Type EndPlayReason)
{
 if(StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget=nullptr; }
 DamageState->OnCollisionFeedback.RemoveAll(this);
 if(auto* GI=GetGameInstance()) if(auto* Registry=GI->GetSubsystem<UVehicleOwnershipSubsystem>()) Registry->UnregisterLiveVehicle(this);
	RemoveVehicleMapping();
	Super::EndPlay(EndPlayReason);
}

void ANeonCityBaseVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	RemoveVehicleMapping();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Subsystem) { return; }
	if (!VehicleMapping)
	{
		VehicleMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
		auto Axis = [this](FKey Positive, FKey Negative)
		{
			UInputAction* Action = NewObject<UInputAction>(this, NAME_None, RF_Transient);
			Action->ValueType = EInputActionValueType::Axis1D;
			VehicleMapping->MapKey(Action, Positive);
			VehicleMapping->MapKey(Action, Negative).Modifiers.Add(NewObject<UInputModifierNegate>(VehicleMapping));
			return Action;
		};
		DriveAction = Axis(EKeys::W, EKeys::S);
		SteerAction = Axis(EKeys::D, EKeys::A);
		BrakeAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		BrakeAction->ValueType = EInputActionValueType::Axis1D;
		VehicleMapping->MapKey(BrakeAction, EKeys::SpaceBar);
		ExitAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		ExitAction->ValueType = EInputActionValueType::Boolean;
		VehicleMapping->MapKey(ExitAction, EKeys::F);
  CruiseAction=NewObject<UInputAction>(this,NAME_None,RF_Transient);
  CruiseAction->ValueType=EInputActionValueType::Boolean;
  if(RoadCruise->IsAvailable())VehicleMapping->MapKey(CruiseAction,EKeys::C);
  // Independent button Started events also catch A+D pressed together, whose axis sum is zero.
  ManualLeftAction=NewObject<UInputAction>(this,NAME_None,RF_Transient);
  ManualLeftAction->ValueType=EInputActionValueType::Boolean;ManualLeftAction->bConsumeInput=false;
  ManualRightAction=NewObject<UInputAction>(this,NAME_None,RF_Transient);
  ManualRightAction->ValueType=EInputActionValueType::Boolean;ManualRightAction->bConsumeInput=false;
  VehicleMapping->MapKey(ManualLeftAction,EKeys::A);VehicleMapping->MapKey(ManualRightAction,EKeys::D);
  CameraAction=NewObject<UInputAction>(this,NAME_None,RF_Transient);CameraAction->ValueType=EInputActionValueType::Boolean;
  VehicleMapping->MapKey(CameraAction,EKeys::V);
  for(auto* Look:{&LookYawAction,&LookPitchAction})
  {*Look=NewObject<UInputAction>(this,NAME_None,RF_Transient);(*Look)->ValueType=EInputActionValueType::Axis1D;(*Look)->bConsumeInput=false;}
  VehicleMapping->MapKey(LookYawAction,EKeys::MouseX);VehicleMapping->MapKey(LookPitchAction,EKeys::MouseY);
	}
	for (ETriggerEvent Event : {ETriggerEvent::Triggered, ETriggerEvent::Completed, ETriggerEvent::Canceled})
	{
		Input->BindAction(DriveAction, Event, this, &ANeonCityBaseVehicle::Drive);
		Input->BindAction(SteerAction, Event, this, &ANeonCityBaseVehicle::Steer);
		Input->BindAction(BrakeAction, Event, this, &ANeonCityBaseVehicle::Brake);
	}
	Input->BindAction(ExitAction, ETriggerEvent::Started, this, &ANeonCityBaseVehicle::ExitVehicle);
 Input->BindAction(CruiseAction,ETriggerEvent::Started,this,&ANeonCityBaseVehicle::ToggleCruise);
 Input->BindAction(CameraAction,ETriggerEvent::Started,this,&ANeonCityBaseVehicle::ToggleCockpitView);
 Input->BindAction(LookYawAction,ETriggerEvent::Triggered,this,&ANeonCityBaseVehicle::LookYaw);
 Input->BindAction(LookPitchAction,ETriggerEvent::Triggered,this,&ANeonCityBaseVehicle::LookPitch);
 Input->BindAction(ManualLeftAction,ETriggerEvent::Started,this,&ANeonCityBaseVehicle::ManualSteeringTakeover);
 Input->BindAction(ManualRightAction,ETriggerEvent::Started,this,&ANeonCityBaseVehicle::ManualSteeringTakeover);
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	Subsystem->AddMappingContext(VehicleMapping, 100, Options);
	InputSubsystem = Subsystem;
}

void ANeonCityBaseVehicle::RemoveVehicleMapping()
{
	if (InputSubsystem.IsValid()) { InputSubsystem->RemoveMappingContext(VehicleMapping); }
	InputSubsystem.Reset();
}
void ANeonCityBaseVehicle::Drive(const FInputActionValue& Value) { if(!RoadCruise->HandlePlayerDrive(Value.Get<float>()))Driving->SetDriveInput(Value.Get<float>()); }
void ANeonCityBaseVehicle::Steer(const FInputActionValue& Value) { if(FMath::Abs(Value.Get<float>())>.01f)RoadCruise->PlayerSteeringTakeover();if(!RoadCruise->IsActive())Driving->SetSteerInput(Value.Get<float>()); }
void ANeonCityBaseVehicle::Brake(const FInputActionValue& Value) { if(!RoadCruise->HandlePlayerBrake(Value.Get<float>()))Driving->SetBrakeInput(Value.Get<float>()); }
void ANeonCityBaseVehicle::ToggleCruise() {RoadCruise->Toggle();}
void ANeonCityBaseVehicle::ManualSteeringTakeover() {RoadCruise->PlayerSteeringTakeover();}
void ANeonCityBaseVehicle::ExitVehicle() { VehicleInteraction->Exit(); }
void ANeonCityBaseVehicle::ToggleCockpitView() { if(IsLocallyControlled())SetCockpitView(!bCockpitView); }
void ANeonCityBaseVehicle::LookYaw(const FInputActionValue& Value)
{ if(bCockpitView && FMath::IsFinite(Value.Get<float>())){CockpitLook.Yaw=FMath::Clamp(CockpitLook.Yaw+Value.Get<float>()*.9f,-135.f,135.f);if(FMath::Abs(Value.Get<float>())>.01f)LookIdle=0;} }
void ANeonCityBaseVehicle::LookPitch(const FInputActionValue& Value)
{ if(bCockpitView && FMath::IsFinite(Value.Get<float>())){CockpitLook.Pitch=FMath::Clamp(CockpitLook.Pitch+Value.Get<float>()*.9f,-45.f,35.f);if(FMath::Abs(Value.Get<float>())>.01f)LookIdle=0;} }

void ANeonCityBaseVehicle::SetCockpitView(bool bEnabled)
{
 const auto* Seat=FindComponentByClass<UTransportOccupantComponent>();
 // the eye sits a seated head's height above the hip, a little forward of it
 CockpitHead=(Seat?Seat->SeatHipLocation:FVector(-5,-40,35))+FVector(4,0,66);
 bCockpitView=bEnabled;CockpitLook=FRotator::ZeroRotator;LookIdle=0;PrevVelocity=GetVelocity();
 CockpitCamera->SetRelativeLocationAndRotation(CockpitHead,FRotator::ZeroRotator);
 CockpitCamera->SetActive(bEnabled);Camera->SetActive(!bEnabled);
 // the driver's own head would fill the view; their arms on the wheel stay
 if(auto* Driver=Seat?Seat->GetDriverMesh():nullptr)
 {
  if(bEnabled){Driver->HideBoneByName(TEXT("head"),EPhysBodyOp::PBO_None);Driver->HideBoneByName(TEXT("neck_02"),EPhysBodyOp::PBO_None);}
  else {Driver->UnHideBoneByName(TEXT("head"));Driver->UnHideBoneByName(TEXT("neck_02"));}
 }
 OnCockpitViewChanged(bEnabled);
}

void ANeonCityBaseVehicle::UpdateCockpitCamera(float DeltaSeconds)
{
 if(DeltaSeconds<=0)return;
 // the look drifts back to the road ahead when the mouse rests
 LookIdle+=DeltaSeconds;
 if(LookIdle>1.5f)CockpitLook=FMath::RInterpTo(CockpitLook,FRotator::ZeroRotator,DeltaSeconds,2.5f);
 // the head rides the car: pushed back under acceleration, sideways in a corner (smoothed, a few cm)
 const FVector V=GetVelocity();const FVector Accel=GetActorTransform().InverseTransformVectorNoScale((V-PrevVelocity)/DeltaSeconds);PrevVelocity=V;
 const FVector Lean(FMath::Clamp(-Accel.X*.004f,-4.f,4.f),FMath::Clamp(-Accel.Y*.004f,-5.f,5.f),0.f);
 const FVector Head=FMath::VInterpTo(CockpitCamera->GetRelativeLocation(),CockpitHead+Lean,DeltaSeconds,6.f);
 // the eye stays level with the horizon in roll, follows the car in pitch and yaw
 FRotator R=CockpitLook;R.Roll=-GetActorRotation().Roll*.6f;
 CockpitCamera->SetRelativeLocationAndRotation(Head,R);
 const float Kmh=FMath::Abs(Driving->GetSpeedKmh());
 CockpitCamera->SetFieldOfView(FMath::FInterpTo(CockpitCamera->FieldOfView,82.f+FMath::Clamp(Kmh/200.f,0.f,1.f)*8.f,DeltaSeconds,3.f));
}

bool ANeonCityBaseVehicle::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
bool ANeonCityBaseVehicle::ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event)
{ return DamageState && DamageState->OfferChainExplosion(Event); }
float ANeonCityBaseVehicle::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityBaseVehicle::GetCombatState_Implementation() const { return Combat->GetCombatState(); }

const UVehicleData* ANeonCityBaseVehicle::GetVehicleProfile() const { return VehicleData && VehicleData->IsValidProfile()?VehicleData.Get():GetDefault<UVehicleData>(); }
void ANeonCityBaseVehicle::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 if(!VehicleID.IsValid() && GetWorld() && !GetWorld()->IsGameWorld()) VehicleID=FGuid::NewGuid();
}
#if WITH_EDITOR
void ANeonCityBaseVehicle::PostEditImport() { Super::PostEditImport(); VehicleID=FGuid::NewGuid(); }
#endif
void ANeonCityBaseVehicle::CollisionFeedback(float DeltaSpeed,float AppliedDamage,FVector Location)
{ if(IsPlayerControlled()) CameraExtension->AddVehicleImpact(DeltaSpeed/1500.f); }


bool ANeonCityBaseVehicle::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(!Character || Hit.GetComponent()!=GetMesh() || (Hit.BoneName!=NAME_None && Hit.BoneName!=TEXT("Root"))) return false;
 const FVector LocalNormal=GetMesh()->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal);
 // Root chassis upward faces support standing at their actual fitted height.
 // Wheel bodies and vertical doors are excluded independently of model dimensions.
 if(LocalNormal.Z<.95f
  || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ()) return false;
 Surface=FDynamicStandingSurface();
 Surface.PointVelocity=GetMesh()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint,TEXT("Root"));
 return true;
}
