#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftFlightStatusComponent.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/CombatCollision.h"
#include "Interaction/InteractionComponent.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Wanted/SearchSystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
ANeonCityAircraftVehicleBase::ANeonCityAircraftVehicleBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 PrimaryActorTick.bCanEverTick=true;
 // Like ground vehicles: an aircraft placed in a level stays unpossessed until the player boards it
 // (a default AI controller would hold it and refuse entry).
 AutoPossessAI=EAutoPossessAI::Disabled; AIControllerClass=nullptr;
 Hull=CreateDefaultSubobject<UBoxComponent>(TEXT("Hull")); SetRootComponent(Hull);
 Hull->InitBoxExtent(FVector(200,90,75)); Hull->SetCollisionProfileName(TEXT("PhysicsActor"));
 Hull->SetCollisionObjectType(ECC_Vehicle); Hull->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
 Hull->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Hull->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
 Hull->SetSimulatePhysics(true); Hull->SetNotifyRigidBodyCollision(true); Hull->CanCharacterStepUpOn=ECB_Yes;
 auto* EntryBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds")); EntryBounds->SetupAttachment(Hull);
 EntryBounds->InitBoxExtent(FVector(205,110,70)); EntryBounds->SetRelativeLocation(FVector(0,0,25));
 EntryBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly); EntryBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
 EntryBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); EntryBounds->CanCharacterStepUpOn=ECB_No;
 auto* Occupant=CreateDefaultSubobject<UTransportOccupantComponent>(TEXT("OccupantPresentation"));
 Occupant->SeatHipLocation=FVector(72,-38,-20);
 Occupant->LeftHandTarget=FVector(64,-61,-15);Occupant->RightHandTarget=FVector(87,-38,0);
 Occupant->LeftFootTarget=FVector(122,-53,-43);Occupant->RightFootTarget=FVector(122,-23,-43);
 Movement=CreateDefaultSubobject<UAircraftVehicleComponent>(TEXT("AircraftMovement"));
 FlightStatus=CreateDefaultSubobject<UAircraftFlightStatusComponent>(TEXT("FlightStatus"));
 Interaction=CreateDefaultSubobject<UAircraftInteractionComponent>(TEXT("AircraftInteraction"));
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 CreateDefaultSubobject<UPhysicalImpactComponent>(TEXT("PhysicalImpact"));
 CreateDefaultSubobject<UAircraftWaterInteraction>(TEXT("AircraftWaterInteraction"));
 CameraExtension=CreateDefaultSubobject<UNeonCityCameraExtensionComponent>(TEXT("AircraftCameraPolicy"));
 Arm=CreateDefaultSubobject<USpringArmComponent>(TEXT("AircraftArm")); Arm->SetupAttachment(Hull);
 Arm->TargetArmLength=900; Arm->SetRelativeLocation(FVector(0,0,180)); Arm->SetRelativeRotation(FRotator(-15,0,0));
 Arm->bUsePawnControlRotation=true; Arm->bInheritRoll=false; Arm->bEnableCameraRotationLag=true; Arm->CameraRotationLagSpeed=4;
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("AircraftCamera")); Camera->SetupAttachment(Arm,USpringArmComponent::SocketName);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("EntryPrompt")); Prompt->SetupAttachment(Hull);
 Prompt->SetRelativeLocation(FVector(0,0,270)); Prompt->SetText(NSLOCTEXT("NeonCity","EnterAircraft","Press F Enter Helicopter"));
 Prompt->SetWorldSize(24); Prompt->SetHorizontalAlignment(EHTA_Center); Prompt->SetVisibility(false);

}
const UAircraftVehicleData* ANeonCityAircraftVehicleBase::GetProfile() const { return VehicleData && VehicleData->IsValidProfile()?VehicleData.Get():GetDefault<UAircraftVehicleData>(); }
void ANeonCityAircraftVehicleBase::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 if(!AircraftID.IsValid() && GetWorld() && !GetWorld()->IsGameWorld()) AircraftID=FGuid::NewGuid();
}
#if WITH_EDITOR
void ANeonCityAircraftVehicleBase::PostEditImport() { Super::PostEditImport(); AircraftID=FGuid::NewGuid(); }
#endif
void ANeonCityAircraftVehicleBase::BeginPlay()
{
 Super::BeginPlay(); const auto* D=GetProfile(); Hull->SetBoxExtent(D->BodyExtent);
 Movement->Configure(Hull,D); Health->ApplyPersistenceHealth(D->Durability,D->Durability);
 FindComponentByClass<UPhysicalImpactComponent>()->MinimumFloorImpactSpeed=D->SafeGroundImpactSpeed;
 CameraExtension->SetCameraContext(ENeonCityCameraContext::Aircraft);
 GetWorld()->GetSubsystem<UAircraftWorldSubsystem>()->RegisterAircraft(this);
}
void ANeonCityAircraftVehicleBase::EndPlay(EEndPlayReason::Type Reason)
{
 RemoveMapping(); if(auto* Registry=GetWorld()->GetSubsystem<UAircraftWorldSubsystem>()) Registry->UnregisterAircraft(this);
 Super::EndPlay(Reason);
}
void ANeonCityAircraftVehicleBase::Tick(float Delta)
{
 Super::Tick(Delta);
 FlightStatus->UpdateStatus(Delta);
 if(IsLocallyControlled()) UpdateAircraftCamera(Delta);
 auto* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC?PC->GetPawn():nullptr;
 const auto* Focus=Pawn?Pawn->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this && Interaction->CanEnter(Pawn);
 Prompt->SetVisibility(Visible);
 if(Visible) { FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation); Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation()); }
}
void ANeonCityAircraftVehicleBase::UpdateAircraftCamera(float Delta)
{ CameraExtension->ApplyAircraftCamera(Camera,Arm,GetVelocity().Size(),GetProfile()->MaxSpeed/.036f,Movement->GetAltitude(),Delta); }
bool ANeonCityAircraftVehicleBase::IsEntryPromptVisible() const { return Prompt->IsVisible(); }
FName ANeonCityAircraftVehicleBase::GetVehicleId_Implementation() const { return AircraftID.IsValid()?FName(*AircraftID.ToString(EGuidFormats::Digits)):NAME_None; }
bool ANeonCityAircraftVehicleBase::CanInteract_Implementation(AActor* Driver) const { return Interaction->CanEnter(Driver); }
FText ANeonCityAircraftVehicleBase::GetInteractionName_Implementation() const { return NSLOCTEXT("NeonCity","AircraftInteraction","Enter Helicopter"); }
void ANeonCityAircraftVehicleBase::Interact_Implementation(AActor* Driver) { Interaction->Enter(Driver); }
bool ANeonCityAircraftVehicleBase::EnterAircraft_Implementation(AActor* Driver) { return Interaction->Enter(Driver); }
bool ANeonCityAircraftVehicleBase::ExitAircraft_Implementation() { return Interaction->Exit(); }
bool ANeonCityAircraftVehicleBase::StartEngine_Implementation() { return Movement->StartEngine(); }
void ANeonCityAircraftVehicleBase::StopEngine_Implementation() { Movement->StopEngine(); }
FVector ANeonCityAircraftVehicleBase::GetVelocity() const { return Hull?Hull->GetPhysicsLinearVelocity():FVector::ZeroVector; }
float ANeonCityAircraftVehicleBase::GetAltitude_Implementation() const { return Movement->GetAltitude(); }
EAircraftFlightState ANeonCityAircraftVehicleBase::GetFlightState_Implementation() const { return Movement->GetFlightState(); }
bool ANeonCityAircraftVehicleBase::SubmitFlightInput_Implementation(AController* Driver,FVector Translation,float YawValue)
{
 if(!HasAuthority() || !Driver || Driver!=GetController() || Translation.ContainsNaN() || !FMath::IsFinite(YawValue)) return false;
 Movement->SetInput(Translation,YawValue); return true;
}
bool ANeonCityAircraftVehicleBase::SubmitPowerInput(AController* Driver,float Adjustment)
{
 return HasAuthority() && Driver && Driver==GetController() && Movement->SetPowerInput(Adjustment);
}
void ANeonCityAircraftVehicleBase::ReceivePhysicalImpact_Implementation(const FPhysicalImpactEvent& Event)
{
 if(!HasAuthority() || Event.TargetActor.Get()!=this || Event.AppliedDamage<=0 || Event.ImpactSpeed<GetProfile()->SeriousImpactSpeed) return;
 // Aircraft structural failure is additional unified damage, never an ad-hoc health write.
 const float Remaining=Health->GetMaxHealth()*(Event.Severity==EImpactSeverity::Extreme?GetProfile()->DisabledFraction:GetProfile()->CriticalFraction)*.8f;
 FNeonCityDamageEvent Damage; Damage.DamageAmount=FMath::Max(0.f,Health->GetCurrentHealth()-Remaining);
 Damage.DamageType=ENeonCityDamageType::Impact; Damage.SourceActor=Event.SourceActor.Get(); Damage.Instigator=Event.Instigator.Get(); Damage.HitLocation=Event.ImpactLocation;
 if(Damage.DamageAmount>0) GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Damage);
 if(Movement->GetCondition()==EAircraftCondition::Disabled) Movement->StopEngine();
}
bool ANeonCityAircraftVehicleBase::CanSupportStandingCharacter() const
{ return Movement->IsGrounded() && !Movement->IsEngineRunning(); }
bool ANeonCityAircraftVehicleBase::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 // This prototype exposes only its stationary cabin roof, never the rotor disc.
 if(!CanSupportStandingCharacter() || !Character || Hit.GetComponent()!=Hull || !Hit.BoneName.IsNone()) return false;
 const FVector Normal=Hull->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal);
 const FVector Point=Hull->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
 if(Normal.Z<.95f || Point.Z<GetProfile()->BodyExtent.Z-5 || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ()) return false;
 Surface=FDynamicStandingSurface(); Surface.PointVelocity=Hull->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint); return true;
}
bool ANeonCityAircraftVehicleBase::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ANeonCityAircraftVehicleBase::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityAircraftVehicleBase::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
void ANeonCityAircraftVehicleBase::PossessedBy(AController* NewController)
{
 Super::PossessedBy(NewController); NewController->SetControlRotation(FRotator(-15,GetActorRotation().Yaw,0)); Movement->StartEngine(); GetWorld()->GetSubsystem<USearchSystem>()->NotifyVehiclePossessed(this);
}
void ANeonCityAircraftVehicleBase::UnPossessed() { RemoveMapping(); Movement->StopEngine(); Super::UnPossessed(); }
void ANeonCityAircraftVehicleBase::RemoveMapping()
{
 if(InputSubsystem.IsValid()) InputSubsystem->RemoveMappingContext(Mapping);
 InputSubsystem.Reset(); TranslationInput=FVector::ZeroVector; YawInput=0;
 Movement->SetPowerInput(0);FlightStatus->Clear();
}
void ANeonCityAircraftVehicleBase::SetupPlayerInputComponent(UInputComponent* Input)
{
 Super::SetupPlayerInputComponent(Input); RemoveMapping();
 auto* Enhanced=Cast<UEnhancedInputComponent>(Input); auto* PC=Cast<APlayerController>(GetController());
 auto* Sub=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Enhanced || !Sub) return;
 if(!Mapping)
 {
  Mapping=NewObject<UInputMappingContext>(this);
  auto Axis=[&](FKey Positive,FKey Negative)
  {
   auto* Action=NewObject<UInputAction>(this); Action->ValueType=EInputActionValueType::Axis1D;
   Mapping->MapKey(Action,Positive); Mapping->MapKey(Action,Negative).Modifiers.Add(NewObject<UInputModifierNegate>(Mapping)); return Action;
  };
  ForwardAction=Axis(EKeys::W,EKeys::S); StrafeAction=Axis(EKeys::E,EKeys::Q);
  VerticalAction=Axis(EKeys::SpaceBar,EKeys::C); YawAction=Axis(EKeys::D,EKeys::A);
  PowerAction=Axis(EKeys::LeftShift,EKeys::LeftControl);
  LookYawAction=NewObject<UInputAction>(this); LookYawAction->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(LookYawAction,EKeys::MouseX);
  LookPitchAction=NewObject<UInputAction>(this); LookPitchAction->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(LookPitchAction,EKeys::MouseY);
  ExitAction=NewObject<UInputAction>(this); Mapping->MapKey(ExitAction,EKeys::F);
 }
 for(auto Event:{ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
 {
  Enhanced->BindAction(ForwardAction,Event,this,&ANeonCityAircraftVehicleBase::Forward);
  Enhanced->BindAction(StrafeAction,Event,this,&ANeonCityAircraftVehicleBase::Strafe);
  Enhanced->BindAction(VerticalAction,Event,this,&ANeonCityAircraftVehicleBase::Vertical);
  Enhanced->BindAction(YawAction,Event,this,&ANeonCityAircraftVehicleBase::Yaw);
  Enhanced->BindAction(PowerAction,Event,this,&ANeonCityAircraftVehicleBase::Power);
 }
 Enhanced->BindAction(LookYawAction,ETriggerEvent::Triggered,this,&ANeonCityAircraftVehicleBase::LookYaw);
 Enhanced->BindAction(LookPitchAction,ETriggerEvent::Triggered,this,&ANeonCityAircraftVehicleBase::LookPitch);
 Enhanced->BindAction(ExitAction,ETriggerEvent::Started,this,&ANeonCityAircraftVehicleBase::Exit);
 FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true; Sub->AddMappingContext(Mapping,100,Options); InputSubsystem=Sub;
}
void ANeonCityAircraftVehicleBase::UpdateFlightInput() { SubmitFlightInput_Implementation(GetController(),TranslationInput,YawInput); }
void ANeonCityAircraftVehicleBase::Forward(const FInputActionValue& V) { TranslationInput.X=V.Get<float>(); UpdateFlightInput(); }
void ANeonCityAircraftVehicleBase::Strafe(const FInputActionValue& V) { TranslationInput.Y=V.Get<float>(); UpdateFlightInput(); }
void ANeonCityAircraftVehicleBase::Vertical(const FInputActionValue& V) { TranslationInput.Z=V.Get<float>(); UpdateFlightInput(); }
void ANeonCityAircraftVehicleBase::Yaw(const FInputActionValue& V) { YawInput=V.Get<float>(); UpdateFlightInput(); }
void ANeonCityAircraftVehicleBase::Power(const FInputActionValue& V) { SubmitPowerInput(GetController(),V.Get<float>()); }
void ANeonCityAircraftVehicleBase::LookYaw(const FInputActionValue& V) { AddControllerYawInput(V.Get<float>()); }
void ANeonCityAircraftVehicleBase::LookPitch(const FInputActionValue& V) { AddControllerPitchInput(-V.Get<float>()); }
void ANeonCityAircraftVehicleBase::Exit() { if(!Interaction->Exit())FlightStatus->NotifyExitDenied(!Movement->IsGrounded()); }
