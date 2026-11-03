#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "Player/NeonCitySwimmingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Water/UnderwaterViewComponent.h"
#include "Water/DiveBreathingComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Combat/CombatCollision.h"
#include "Components/CapsuleComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "Combat/DamageableCharacterComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Economy/CurrencyComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Player/NeonCityFootstepComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

ANeonCityPlayerCharacter::ANeonCityPlayerCharacter(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer.SetDefaultSubobjectClass<UNeonCitySurfaceMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
 CreateDefaultSubobject<UCharacterWeaponPresentationComponent>(TEXT("WeaponPresentation"));
 CreateDefaultSubobject<UNeonCityFlashlightComponent>(TEXT("Flashlight"));
 CreateDefaultSubobject<UDivingComponent>(TEXT("DivingKit"));
 CreateDefaultSubobject<UDiveEquipmentComponent>(TEXT("DiveEquipment"));
 CreateDefaultSubobject<UWingsuitComponent>(TEXT("Wingsuit"));
	DamageHealth=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 CreateDefaultSubobject<UCharacterAppearanceComponent>(TEXT("Appearance"));
	Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventory"));
	BodyState=CreateDefaultSubobject<UDamageableCharacterComponent>(TEXT("BodyState"));
	DamageFeedback=CreateDefaultSubobject<UCharacterDamageFeedbackComponent>(TEXT("DamageFeedback"));
	CombatAdapter=CreateDefaultSubobject<UPlayerCombatAdapter>(TEXT("CombatAdapter"));
 CreateDefaultSubobject<UCharacterImpactComponent>(TEXT("PhysicalReaction"));
 CreateDefaultSubobject<UNeonCitySwimmingComponent>(TEXT("Swimming"));
 CreateDefaultSubobject<UWaterInteractionComponent>(TEXT("WaterInteraction"));
 CreateDefaultSubobject<UOxygenComponent>(TEXT("Oxygen"));
 CreateDefaultSubobject<UUnderwaterViewComponent>(TEXT("UnderwaterView"));
 CreateDefaultSubobject<UDiveBreathingComponent>(TEXT("DiveBreathing"));
	Currency=CreateDefaultSubobject<UCurrencyComponent>(TEXT("Currency"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	PlayerDataComponent = CreateDefaultSubobject<UNeonCityPlayerDataComponent>(TEXT("PlayerDataComponent"));
	CameraExtensionComponent = CreateDefaultSubobject<UNeonCityCameraExtensionComponent>(TEXT("CameraExtensionComponent"));
	CreateDefaultSubobject<UNeonCityFootstepComponent>(TEXT("Footsteps"));                    // surface footsteps (all maps but the island)
}

void ANeonCityPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CombatAdapter->SetupInput(PlayerInputComponent);
 FindComponentByClass<UNeonCityFlashlightComponent>()->SetupInput(PlayerInputComponent);
	RemoveInteractionMapping();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Subsystem)
	{
		return;
	}

	if (!InteractAction)
	{
		InteractAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		InteractAction->ValueType = EInputActionValueType::Boolean;
		InteractionMapping = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);
		InteractionMapping->MapKey(InteractAction, EKeys::F);
		ViewAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		ViewAction->ValueType = EInputActionValueType::Boolean;
		InteractionMapping->MapKey(ViewAction, EKeys::V);
	}

	InteractionBindingHandle = Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ANeonCityPlayerCharacter::HandleInteract).GetHandle();
	if (ViewAction) { Input->BindAction(ViewAction, ETriggerEvent::Started, this, &ANeonCityPlayerCharacter::ToggleFirstPerson); }
	BoundInteractionInput = Input;
	bHasInteractionBinding = true;
	Subsystem->AddMappingContext(InteractionMapping, 0);
	InteractionInputSubsystem = Subsystem;
}

void ANeonCityPlayerCharacter::HandleInteract()
{
	InteractionComponent->TryInteract();
}

void ANeonCityPlayerCharacter::ToggleFirstPerson()
{
	ApplyFirstPerson(!bFirstPerson);
}

void ANeonCityPlayerCharacter::ApplyFirstPerson(bool bEnable)
{
	USpringArmComponent* Boom = GetCameraBoom(); UCameraComponent* Camera = GetFollowCamera();
	if (!Boom || !Camera) { return; }
	if (bEnable && !bFirstPerson)
	{
		SavedBoomLength = Boom->TargetArmLength; SavedBoomOffset = Boom->SocketOffset; SavedBoomLocation = Boom->GetRelativeLocation();
		bSavedBoomLag = Boom->bEnableCameraLag; bSavedBoomCollision = Boom->bDoCollisionTest;
		// at the eyes, a hand's width ahead of the face so the head never fills the view; the body turns with the view
		Boom->TargetArmLength = 0.f; Boom->SocketOffset = FVector(14.f, 0.f, 0.f); Boom->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight + 6.f));
		Boom->bEnableCameraLag = false; Boom->bEnableCameraRotationLag = false; Boom->bDoCollisionTest = false;
		bUseControllerRotationYaw = true; GetCharacterMovement()->bOrientRotationToMovement = false;
		GetMesh()->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);
	}
	else if (!bEnable && bFirstPerson)
	{
		Boom->TargetArmLength = SavedBoomLength; Boom->SocketOffset = SavedBoomOffset; Boom->SetRelativeLocation(SavedBoomLocation);
		Boom->bEnableCameraLag = bSavedBoomLag; Boom->bDoCollisionTest = bSavedBoomCollision;
		bUseControllerRotationYaw = false; GetCharacterMovement()->bOrientRotationToMovement = true;
		GetMesh()->UnHideBoneByName(TEXT("head"));
	}
	bFirstPerson = bEnable;
}

void ANeonCityPlayerCharacter::RemoveInteractionMapping()
{
	if (bHasInteractionBinding && BoundInteractionInput.IsValid())
	{
		BoundInteractionInput->RemoveBindingByHandle(InteractionBindingHandle);
	}
	bHasInteractionBinding = false;
	BoundInteractionInput.Reset();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = InteractionInputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(InteractionMapping);
	}
	InteractionInputSubsystem.Reset();
}

void ANeonCityPlayerCharacter::UnPossessed()
{
 FindComponentByClass<UDivingComponent>()->Unequip();
 FindComponentByClass<UWingsuitComponent>()->Unequip();
 FindComponentByClass<UNeonCityFlashlightComponent>()->Suspend();
	CombatAdapter->Suspend();
	RemoveInteractionMapping();
	Super::UnPossessed();
}

void ANeonCityPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveInteractionMapping();
	Super::EndPlay(EndPlayReason);
}

bool ANeonCityPlayerCharacter::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ANeonCityPlayerCharacter::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityPlayerCharacter::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
