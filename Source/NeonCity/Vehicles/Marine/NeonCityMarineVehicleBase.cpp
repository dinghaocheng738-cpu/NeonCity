#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineFeedbackComponent.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/VehicleStatusWidget.h"
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
#include "Components/SkeletalMeshComponent.h"
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
ANeonCityMarineVehicleBase::ANeonCityMarineVehicleBase()
{
 PrimaryActorTick.bCanEverTick=true;
 Hull=CreateDefaultSubobject<UBoxComponent>(TEXT("Hull")); SetRootComponent(Hull);
 Hull->InitBoxExtent(FVector(250,100,40)); Hull->SetCollisionProfileName(TEXT("PhysicsActor"));
 Hull->SetCollisionObjectType(ECC_Vehicle); Hull->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
 Hull->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Hull->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
 Hull->SetSimulatePhysics(true); Hull->SetNotifyRigidBodyCollision(true); Hull->CanCharacterStepUpOn=ECB_Yes;
 auto* EntryBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds")); EntryBounds->SetupAttachment(Hull);
 EntryBounds->InitBoxExtent(FVector(250,105,65)); EntryBounds->SetRelativeLocation(FVector(0,0,85));
 EntryBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly); EntryBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
 EntryBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); EntryBounds->CanCharacterStepUpOn=ECB_No;
 auto* Occupant=CreateDefaultSubobject<UTransportOccupantComponent>(TEXT("OccupantPresentation"));
 Occupant->SeatHipLocation=FVector(-22,-47,82);
 Occupant->LeftHandTarget=FVector(-1,-55,110);Occupant->RightHandTarget=FVector(-1,-29,110);
 Occupant->LeftFootTarget=FVector(20,-62,48);Occupant->RightFootTarget=FVector(20,-32,48);
 Movement=CreateDefaultSubobject<UMarineVehicleComponent>(TEXT("MarineMovement"));
 Feedback=CreateDefaultSubobject<UMarineFeedbackComponent>(TEXT("MarineFeedback"));
 WaterEffects=CreateDefaultSubobject<UMarineWaterEffectsComponent>(TEXT("MarineWaterEffects"));
 Interaction=CreateDefaultSubobject<UMarineInteractionComponent>(TEXT("MarineInteraction"));
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 CreateDefaultSubobject<UPhysicalImpactComponent>(TEXT("PhysicalImpact"));
 CameraExtension=CreateDefaultSubobject<UNeonCityCameraExtensionComponent>(TEXT("MarineCameraPolicy"));
 Arm=CreateDefaultSubobject<USpringArmComponent>(TEXT("MarineArm")); Arm->SetupAttachment(Hull);
 Arm->TargetArmLength=700; Arm->SetRelativeLocation(FVector(0,0,150)); Arm->SetRelativeRotation(FRotator(-15,0,0));
 Arm->bInheritPitch=false; Arm->bInheritRoll=false; Arm->bEnableCameraRotationLag=true; Arm->CameraRotationLagSpeed=4;
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("MarineCamera")); Camera->SetupAttachment(Arm,USpringArmComponent::SocketName);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("EntryPrompt")); Prompt->SetupAttachment(Hull);
 Prompt->SetRelativeLocation(FVector(0,0,180)); Prompt->SetText(NSLOCTEXT("NeonCity","EnterBoat","Press F Enter Boat"));
 Prompt->SetWorldSize(24); Prompt->SetHorizontalAlignment(EHTA_Center); Prompt->SetVisibility(false);
 // Primitive prototype body; visual parts never become separate floors or physics bodies.
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Part=[&](const TCHAR* Name,FVector Location,FVector Scale)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name); Mesh->SetupAttachment(Hull); Mesh->SetStaticMesh(Cube.Object);
  Mesh->SetRelativeLocation(Location); Mesh->SetRelativeScale3D(Scale); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 };
 Part(TEXT("Deck"),FVector::ZeroVector,FVector(5,2,.8));
 Part(TEXT("Bow"),FVector(195,0,45),FVector(1,1.8,.1));
 Part(TEXT("PortTrim"),FVector(0,-98,45),FVector(4.8,.06,.12));
 Part(TEXT("StarboardTrim"),FVector(0,98,45),FVector(4.8,.06,.12));
 Part(TEXT("Console"),FVector(60,0,64),FVector(.3,.8,.5));
 Part(TEXT("Seat"),FVector(-20,0,50),FVector(.6,.6,.2));
 NeonCityVisuals::Create(*this,Hull,TEXT("Marine"),TEXT("Speedboat"),
  {TEXT("Deck"),TEXT("Bow"),TEXT("PortTrim"),TEXT("StarboardTrim"),TEXT("Console"),TEXT("Seat")});
}
float ANeonCityMarineVehicleBase::GetAirDraft() const { return GetProfile()->HullExtent.Z*2; }
const UMarineVehicleData* ANeonCityMarineVehicleBase::GetProfile() const { return VehicleData && VehicleData->IsValidProfile()?VehicleData.Get():GetDefault<UMarineVehicleData>(); }
void ANeonCityMarineVehicleBase::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 if(!VehicleID.IsValid() && GetWorld() && !GetWorld()->IsGameWorld()) VehicleID=FGuid::NewGuid();
}
#if WITH_EDITOR
void ANeonCityMarineVehicleBase::PostEditImport() { Super::PostEditImport(); VehicleID=FGuid::NewGuid(); }
#endif
void ANeonCityMarineVehicleBase::BeginPlay()
{
 Super::BeginPlay(); const auto* D=GetProfile(); Hull->SetBoxExtent(D->HullExtent);
 Movement->Configure(Hull,D); Health->ApplyPersistenceHealth(D->Durability,D->Durability);
 CameraExtension->SetCameraContext(ENeonCityCameraContext::Marine);
 GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->RegisterBoat(this);
}
void ANeonCityMarineVehicleBase::EndPlay(EEndPlayReason::Type Reason)
{
 RemoveMapping(); if(auto* Registry=GetWorld()->GetSubsystem<UMarineWorldSubsystem>()) Registry->UnregisterBoat(this);
 Super::EndPlay(Reason);
}
void ANeonCityMarineVehicleBase::Tick(float Delta)
{
 Super::Tick(Delta);
 Feedback->UpdateFeedback(Movement->GetTelemetry(),GetProfile()->MaxSpeed);
 WaterEffects->UpdateEffects(Movement->GetTelemetry(),GetProfile()->HullExtent,Delta);
 UpdateDrivingStatus(Delta);
 if(IsLocallyControlled()) CameraExtension->ApplyMarineCamera(Camera,Arm,GetSpeed_Implementation(),GetProfile()->MaxSpeed/.036f,Movement->GetSteering(),Delta,GetProfile()->CameraRig,bHelmCamera,CameraLook);
 auto* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC?PC->GetPawn():nullptr;
 const auto* Focus=Pawn?Pawn->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this && CanInteract_Implementation(Pawn);
 Prompt->SetVisibility(Visible);
 if(Visible) { Prompt->SetText(FText::Format(NSLOCTEXT("NeonCity","MarinePrompt","F - {0}"),GetInteractionName_Implementation())); FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation); Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation()); }
}
bool ANeonCityMarineVehicleBase::IsEntryPromptVisible() const { return Prompt->IsVisible(); }
bool ANeonCityMarineVehicleBase::IsDrivingStatusVisible() const { return StatusWidget && StatusWidget->IsInViewport(); }
void ANeonCityMarineVehicleBase::UpdateDrivingStatus(float Delta)
{
 ExitFeedbackRemaining=FMath::Max(0.f,ExitFeedbackRemaining-Delta);
 if(ExitFeedbackRemaining<=0) ExitFeedback=FText::GetEmpty();
 auto* PC=Cast<APlayerController>(GetController());
 if(!PC || !PC->IsLocalController()) return;
 if(!StatusWidget)
 {
  StatusWidget=CreateWidget<UVehicleStatusWidget>(PC);
  if(!StatusWidget) return;
  StatusWidget->SetVisibility(ESlateVisibility::HitTestInvisible);StatusWidget->AddToViewport(3);StatusRefresh=0;
 }
 StatusRefresh-=Delta;if(StatusRefresh>0)return;StatusRefresh=.1f;
 const auto T=Movement->GetTelemetry(); FText State;
 switch(T.State)
 {
  case EMarineDriveState::EngineOff: State=NSLOCTEXT("NeonCity","MarineOff","Engine off");break;
  case EMarineDriveState::Idle: State=NSLOCTEXT("NeonCity","MarineIdle","Idle");break;
  case EMarineDriveState::Accelerating: State=NSLOCTEXT("NeonCity","MarineAccelerating","Accelerating");break;
  case EMarineDriveState::Cruise: State=NSLOCTEXT("NeonCity","MarineCruise","Cruise");break;
  case EMarineDriveState::Coasting: State=NSLOCTEXT("NeonCity","MarineCoasting","Coasting");break;
  case EMarineDriveState::Braking: State=NSLOCTEXT("NeonCity","MarineBraking","Slowing down");break;
  case EMarineDriveState::Reverse: State=NSLOCTEXT("NeonCity","MarineReverse","Reverse");break;
 }
 if(!ExitFeedback.IsEmpty()) State=ExitFeedback;
 StatusWidget->UpdateStatus(BuildStatusText(T,State),Health->IsDead());
}
FName ANeonCityMarineVehicleBase::GetVehicleId_Implementation() const { return VehicleID.IsValid()?FName(*VehicleID.ToString(EGuidFormats::Digits)):NAME_None; }
bool ANeonCityMarineVehicleBase::CanInteract_Implementation(AActor* Driver) const { return Interaction->CanEnter(Driver); }
FText ANeonCityMarineVehicleBase::GetInteractionName_Implementation() const { return NSLOCTEXT("NeonCity","BoatInteraction","Enter Boat"); }
void ANeonCityMarineVehicleBase::Interact_Implementation(AActor* Driver) { Interaction->Enter(Driver); }
bool ANeonCityMarineVehicleBase::EnterVehicle_Implementation(AActor* Driver) { return Interaction->Enter(Driver); }
bool ANeonCityMarineVehicleBase::ExitVehicle_Implementation() { return Interaction->Exit(); }
bool ANeonCityMarineVehicleBase::StartEngine_Implementation() { return Movement->StartEngine(); }
void ANeonCityMarineVehicleBase::StopEngine_Implementation() { Movement->StopEngine(); }
float ANeonCityMarineVehicleBase::GetSpeed_Implementation() const { return Hull->GetPhysicsLinearVelocity().Size2D(); }
FVector ANeonCityMarineVehicleBase::GetWaterVelocity_Implementation() const { return Movement->GetWaterVelocity(); }
bool ANeonCityMarineVehicleBase::SubmitDriverInput_Implementation(AController* Driver,float ThrottleValue,float SteeringValue,float BrakeValue)
{
 if(!HasAuthority() || !Driver || Driver!=GetController()) return false;
 ApplyDriverInput(ThrottleValue,SteeringValue,BrakeValue); return true;
}
void ANeonCityMarineVehicleBase::ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue) { Movement->SetInput(ThrottleValue,SteeringValue,BrakeValue); }
UTransportOccupantComponent* ANeonCityMarineVehicleBase::GetOccupantPresentation() const { return FindComponentByClass<UTransportOccupantComponent>(); }
FText ANeonCityMarineVehicleBase::BuildStatusText(const FMarineDriveTelemetry& T,const FText& State) const
{
 return FText::Format(NSLOCTEXT("NeonCity","MarineStatus","{0}  |  {1} km/h  |  Throttle {2}%\n{3}\nW / S Thrust   A / D Steer   Space Slow   F Leave   V View   Mouse Look"),
  GetProfile()->DisplayName,FText::AsNumber(FMath::RoundToInt(T.SpeedKmh)),FText::AsNumber(FMath::RoundToInt(T.Throttle*100)),State);
}
bool ANeonCityMarineVehicleBase::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(!Character || Hit.GetComponent()!=Hull || !Hit.BoneName.IsNone()) return false;
 const FVector Normal=Hull->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal);
 const FVector Point=Hull->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
 if(Normal.Z<.95f || Point.Z<GetProfile()->HullExtent.Z-5 || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ()) return false;
 Surface=FDynamicStandingSurface(); Surface.PointVelocity=Hull->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint); return true;
}
bool ANeonCityMarineVehicleBase::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ANeonCityMarineVehicleBase::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityMarineVehicleBase::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
void ANeonCityMarineVehicleBase::PossessedBy(AController* NewController)
{
 Super::PossessedBy(NewController); bHelmCamera=false; OnHelmCameraChanged(false); CameraLook=FRotator::ZeroRotator; Arm->TargetArmLength=GetProfile()->CameraRig.ChaseDistance; Movement->StartEngine(); GetWorld()->GetSubsystem<USearchSystem>()->NotifyVehiclePossessed(this);
}
void ANeonCityMarineVehicleBase::UnPossessed() { RemoveMapping(); Movement->StopEngine(); Super::UnPossessed(); }
void ANeonCityMarineVehicleBase::RemoveMapping()
{
 if(StatusWidget) { StatusWidget->RemoveFromParent();StatusWidget=nullptr; }
 ExitFeedback=FText::GetEmpty();ExitFeedbackRemaining=0;
 if(InputSubsystem.IsValid()) InputSubsystem->RemoveMappingContext(Mapping);
 InputSubsystem.Reset(); InputThrottle=InputSteer=InputBrake=0;
}
void ANeonCityMarineVehicleBase::SetupPlayerInputComponent(UInputComponent* Input)
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
  ThrottleAction=Axis(EKeys::W,EKeys::S); SteerAction=Axis(EKeys::D,EKeys::A);
  BrakeAction=NewObject<UInputAction>(this); BrakeAction->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(BrakeAction,EKeys::SpaceBar);
  ExitAction=NewObject<UInputAction>(this); Mapping->MapKey(ExitAction,EKeys::F);
  CameraAction=NewObject<UInputAction>(this); Mapping->MapKey(CameraAction,EKeys::V);
  LookYawAction=NewObject<UInputAction>(this); LookYawAction->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(LookYawAction,EKeys::MouseX);
  LookPitchAction=NewObject<UInputAction>(this); LookPitchAction->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(LookPitchAction,EKeys::MouseY);
 }
 for(auto Event:{ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
 {
  Enhanced->BindAction(ThrottleAction,Event,this,&ANeonCityMarineVehicleBase::Throttle);
  Enhanced->BindAction(SteerAction,Event,this,&ANeonCityMarineVehicleBase::Steering);
  Enhanced->BindAction(BrakeAction,Event,this,&ANeonCityMarineVehicleBase::Brake);
 }
 Enhanced->BindAction(ExitAction,ETriggerEvent::Started,this,&ANeonCityMarineVehicleBase::Exit);
 Enhanced->BindAction(CameraAction,ETriggerEvent::Started,this,&ANeonCityMarineVehicleBase::ToggleMarineCamera);
 Enhanced->BindAction(LookYawAction,ETriggerEvent::Triggered,this,&ANeonCityMarineVehicleBase::LookYaw);
 Enhanced->BindAction(LookPitchAction,ETriggerEvent::Triggered,this,&ANeonCityMarineVehicleBase::LookPitch);
 BindExtraHelmKeys(Enhanced,Mapping);
 FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true; Sub->AddMappingContext(Mapping,100,Options); InputSubsystem=Sub;
}
void ANeonCityMarineVehicleBase::Throttle(const FInputActionValue& V) { InputThrottle=V.Get<float>(); ApplyDriverInput(InputThrottle,InputSteer,InputBrake); }
void ANeonCityMarineVehicleBase::Steering(const FInputActionValue& V) { InputSteer=V.Get<float>(); ApplyDriverInput(InputThrottle,InputSteer,InputBrake); }
void ANeonCityMarineVehicleBase::Brake(const FInputActionValue& V) { InputBrake=V.Get<float>(); ApplyDriverInput(InputThrottle,InputSteer,InputBrake); }
bool ANeonCityMarineVehicleBase::RequestDriverExit() { return Interaction->Exit(); }
void ANeonCityMarineVehicleBase::Exit()
{
 if(RequestDriverExit()) return;
 ExitFeedback=!GetProfile()->bWalkableDeck && GetSpeed_Implementation()>150
  ?NSLOCTEXT("NeonCity","MarineExitSlow","Slow below 5 km/h to step onto the deck")
  :NSLOCTEXT("NeonCity","MarineExitClear","Keep the boat level and the deck clear before leaving");
 ExitFeedbackRemaining=3;StatusRefresh=0;
}

void ANeonCityMarineVehicleBase::ToggleMarineCamera()
{
 if(!IsLocallyControlled()) return;
 bHelmCamera=!bHelmCamera; CameraLook=FRotator(bHelmCamera?GetProfile()->CameraRig.HelmPitch:0,0,0);
 Arm->TargetArmLength=bHelmCamera?0:GetProfile()->CameraRig.ChaseDistance;
 if(auto* Occupant=FindComponentByClass<UTransportOccupantComponent>())
  if(auto* Mesh=Occupant->GetDriverMesh()) Mesh->SetOwnerNoSee(bHelmCamera);
 OnHelmCameraChanged(bHelmCamera);
 CameraExtension->ApplyMarineCamera(Camera,Arm,GetSpeed_Implementation(),GetProfile()->MaxSpeed/.036f,Movement->GetSteering(),.016f,GetProfile()->CameraRig,bHelmCamera,CameraLook);
}
void ANeonCityMarineVehicleBase::LookYaw(const FInputActionValue& Value)
{
 const float Axis=Value.Get<float>(); if(!FMath::IsFinite(Axis)) return;
 CameraLook.Yaw=FMath::UnwindDegrees(CameraLook.Yaw+Axis*.75f);
 if(bHelmCamera) CameraLook.Yaw=FMath::Clamp(CameraLook.Yaw,-150.f,150.f);
}
void ANeonCityMarineVehicleBase::LookPitch(const FInputActionValue& Value)
{
 const float Axis=Value.Get<float>(); if(FMath::IsFinite(Axis)) CameraLook.Pitch=FMath::Clamp(CameraLook.Pitch+Axis*.75f,-60.f,50.f);
}
