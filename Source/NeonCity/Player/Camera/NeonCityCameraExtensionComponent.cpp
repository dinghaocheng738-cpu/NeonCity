#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/SkinnedMeshComponent.h"

UNeonCityCameraExtensionComponent::UNeonCityCameraExtensionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNeonCityCameraExtensionComponent::ApplyInteriorCamera(AActor* Room,ANeonCityPlayerCharacter* Player,float ArmLength,float Delta)
{
 if(!IsValid(Room) || !IsValid(Player) || CameraContext!=ENeonCityCameraContext::Default || !FMath::IsFinite(ArmLength))return;
 if(!InteriorPlayer.IsValid())
 {
  InteriorPlayer=Player;SavedInteriorLength=Player->GetCameraBoom()->TargetArmLength;
  TInlineComponentArray<USkinnedMeshComponent*> Meshes(Player);
  for(auto* Mesh:Meshes)InteriorVisibility.Emplace(Mesh,bool(Mesh->bOwnerNoSee));
 }
 if(InteriorPlayer.Get()!=Player)return;
 InteriorRooms.FindOrAdd(Room)=FMath::Clamp(ArmLength,100.f,300.f);float Limit=SavedInteriorLength;
 for(auto I=InteriorRooms.CreateIterator();I;++I){if(!I.Key().IsValid())I.RemoveCurrent();else Limit=FMath::Min(Limit,I.Value());}
 Player->GetCameraBoom()->TargetArmLength=FMath::FInterpTo(Player->GetCameraBoom()->TargetArmLength,Limit,Delta,10);
 const bool Near=FVector::DistSquared(Player->GetFollowCamera()->GetComponentLocation(),Player->GetActorLocation())<FMath::Square(125.f);
 for(const auto& State:InteriorVisibility)if(auto* Mesh=State.Key.Get())Mesh->SetOwnerNoSee(State.Value || Near);
}
bool UNeonCityCameraExtensionComponent::ReleaseInteriorCamera(AActor* Room)
{
 InteriorRooms.Remove(Room);
 for(auto I=InteriorRooms.CreateIterator();I;++I)if(!I.Key().IsValid())I.RemoveCurrent();
 if(!InteriorRooms.IsEmpty())return true;
 if(auto* Player=InteriorPlayer.Get())
 {
  // Aim/fishing restore their own snapshot before the final room release.
  if(CameraContext!=ENeonCityCameraContext::Default && Player->IsLocallyControlled())return false;
  Player->GetCameraBoom()->TargetArmLength=SavedInteriorLength;
 }
 for(const auto& State:InteriorVisibility)if(auto* Mesh=State.Key.Get())Mesh->SetOwnerNoSee(State.Value);
 InteriorVisibility.Reset();InteriorPlayer.Reset();return true;
}

void UNeonCityCameraExtensionComponent::SetCameraContext(ENeonCityCameraContext NewContext)
{
	if (!StaticEnum<ENeonCityCameraContext>()->IsValidEnumValue(static_cast<int64>(NewContext))
		|| NewContext == CameraContext)
	{
		return;
	}

	const ENeonCityCameraContext PreviousContext = CameraContext;
	CameraContext = NewContext;
	if(NewContext==ENeonCityCameraContext::Default && InteriorRooms.IsEmpty() && InteriorPlayer.IsValid())ReleaseInteriorCamera(nullptr);
	OnCameraContextChanged(PreviousContext, NewContext);
}

void UNeonCityCameraExtensionComponent::ApplyVehicleCamera(UCameraComponent* Camera,
	USpringArmComponent* SpringArm, float SpeedCmPerSecond, float MaxSpeedCmPerSecond, float DeltaSeconds)
{
	if (CameraContext != ENeonCityCameraContext::Vehicle || !IsValid(Camera) || !IsValid(SpringArm)
		|| !FMath::IsFinite(SpeedCmPerSecond) || !FMath::IsFinite(MaxSpeedCmPerSecond)
		|| !FMath::IsFinite(DeltaSeconds) || MaxSpeedCmPerSecond <= UE_SMALL_NUMBER || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float SpeedAlpha = FMath::Clamp(FMath::Abs(SpeedCmPerSecond) / MaxSpeedCmPerSecond, 0.0f, 1.0f);
	VehicleImpact=FMath::FInterpTo(VehicleImpact,0.f,DeltaSeconds,10.f);
	const float TargetArmLength = VehicleArmLength + VehicleSpeedArmExtension * SpeedAlpha - VehicleImpact*30.f;
	const float TargetFieldOfView = VehicleFieldOfView + VehicleSpeedFieldOfViewIncrease * SpeedAlpha;

	// The caller supplies a vehicle-owned spring arm; the template's character arm is untouched.
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = VehicleCameraLagSpeed;
	SpringArm->bUseCameraLagSubstepping = true;
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength,
		TargetArmLength, DeltaSeconds, VehicleCameraInterpolationSpeed);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,
		TargetFieldOfView, DeltaSeconds, VehicleCameraInterpolationSpeed));
}

bool UNeonCityCameraExtensionComponent::BeginAim(UCameraComponent* Camera,USpringArmComponent* Arm)
{
	if (CameraContext!=ENeonCityCameraContext::Default || !IsValid(Camera) || !IsValid(Arm)) { return false; }
	AimCamera=Camera; AimArm=Arm; SavedAimFOV=Camera->FieldOfView; SavedAimLength=Arm->TargetArmLength;
	SavedAimOffset=Arm->SocketOffset; SetCameraContext(ENeonCityCameraContext::Aim); return true;
}
void UNeonCityCameraExtensionComponent::ApplyAimCamera(float DeltaSeconds)
{
	if (CameraContext!=ENeonCityCameraContext::Aim || !AimCamera.IsValid() || !AimArm.IsValid()
		|| !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0) { return; }
	AimCamera->SetFieldOfView(FMath::FInterpTo(AimCamera->FieldOfView,65.f,DeltaSeconds,12.f));
	AimArm->TargetArmLength=FMath::FInterpTo(AimArm->TargetArmLength,220.f,DeltaSeconds,12.f);
	AimArm->SocketOffset=FMath::VInterpTo(AimArm->SocketOffset,FVector(0,65,15),DeltaSeconds,12.f);
}
void UNeonCityCameraExtensionComponent::EndAim()
{
	if (AimCamera.IsValid()) { AimCamera->SetFieldOfView(SavedAimFOV); }
	if (AimArm.IsValid()) { AimArm->TargetArmLength=SavedAimLength; AimArm->SocketOffset=SavedAimOffset; }
	AimCamera.Reset(); AimArm.Reset();
	if (CameraContext==ENeonCityCameraContext::Aim) { SetCameraContext(ENeonCityCameraContext::Default); }
}

void UNeonCityCameraExtensionComponent::AddVehicleImpact(float Strength)
{
 if(CameraContext==ENeonCityCameraContext::Vehicle && FMath::IsFinite(Strength)) VehicleImpact=FMath::Max(VehicleImpact,FMath::Clamp(Strength,0.f,1.f));
}

bool UNeonCityCameraExtensionComponent::BeginFishing(UCameraComponent* Camera,USpringArmComponent* Arm)
{
 if(CameraContext!=ENeonCityCameraContext::Default || !IsValid(Camera) || !IsValid(Arm))return false;
 FishingCamera=Camera;FishingArm=Arm;SavedFishingFOV=Camera->FieldOfView;SavedFishingLength=Arm->TargetArmLength;SavedFishingOffset=Arm->SocketOffset;
 SetCameraContext(ENeonCityCameraContext::Fishing);return true;
}
void UNeonCityCameraExtensionComponent::ApplyFishingCamera(float Delta)
{
 if(CameraContext!=ENeonCityCameraContext::Fishing || !FishingCamera.IsValid() || !FishingArm.IsValid() || !FMath::IsFinite(Delta) || Delta<=0)return;
 FishingCamera->SetFieldOfView(FMath::FInterpTo(FishingCamera->FieldOfView,80.f,Delta,6.f));
 FishingArm->TargetArmLength=FMath::FInterpTo(FishingArm->TargetArmLength,450.f,Delta,6.f);
 FishingArm->SocketOffset=FMath::VInterpTo(FishingArm->SocketOffset,FVector(0,120,55),Delta,6.f);
}
void UNeonCityCameraExtensionComponent::EndFishing()
{
 if(FishingCamera.IsValid())FishingCamera->SetFieldOfView(SavedFishingFOV);
 if(FishingArm.IsValid()) { FishingArm->TargetArmLength=SavedFishingLength;FishingArm->SocketOffset=SavedFishingOffset; }
 FishingCamera.Reset();FishingArm.Reset();
 if(CameraContext==ENeonCityCameraContext::Fishing)SetCameraContext(ENeonCityCameraContext::Default);
}

void UNeonCityCameraExtensionComponent::ApplyMarineCamera(UCameraComponent* Camera,USpringArmComponent* Arm,float Speed,float MaxSpeed,float Steering,float Delta,
 const FMarineCameraSettings& Rig,bool FirstPerson,FRotator Look)
{
 if(CameraContext!=ENeonCityCameraContext::Marine || !Camera || !Arm || !Rig.IsValid() || Look.ContainsNaN()
  || !FMath::IsFinite(Speed) || !FMath::IsFinite(MaxSpeed) || !FMath::IsFinite(Steering) || !FMath::IsFinite(Delta) || Delta<=0 || MaxSpeed<=0) return;
 Arm->bUsePawnControlRotation=false; Arm->bInheritYaw=true;
 Arm->bInheritPitch=Arm->bInheritRoll=FirstPerson;
 Arm->bEnableCameraLag=Arm->bEnableCameraRotationLag=!FirstPerson;
 Arm->bDoCollisionTest=!FirstPerson;
 if(FirstPerson)
 {
  // Keep the eye inside the authored helm, with no lag through walls on a pitching boat.
  Arm->SetRelativeLocation(Rig.HelmEye);
  const FQuat HullRotation=Arm->GetAttachParent()?Arm->GetAttachParent()->GetComponentQuat():FQuat::Identity;
  const FQuat Horizon=FRotator(0,HullRotation.Rotator().Yaw,0).Quaternion();
  Arm->SetWorldRotation(FQuat::Slerp(HullRotation,Horizon,Rig.HorizonStabilization)*Look.Quaternion());
  Arm->TargetArmLength=0; Arm->SocketOffset=FVector::ZeroVector;
  Camera->SetFieldOfView(Rig.HelmFieldOfView); return;
 }
 const float Alpha=FMath::Clamp(FMath::Abs(Speed)/MaxSpeed,0.f,1.f);
 Arm->SetRelativeLocation(Rig.ChaseAnchor); Arm->SetRelativeRotation(FRotator(-15+Look.Pitch,Look.Yaw,0));
 Arm->bUseCameraLagSubstepping=true; Arm->CameraLagSpeed=6; Arm->CameraRotationLagSpeed=4;
 Arm->TargetArmLength=FMath::FInterpTo(Arm->TargetArmLength,Rig.ChaseDistance+200*Alpha,Delta,3);
 Arm->SocketOffset.Y=FMath::FInterpTo(Arm->SocketOffset.Y,FMath::Clamp(Steering,-1.f,1.f)*80*Alpha,Delta,3);
 Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,Rig.ChaseFieldOfView+5*Alpha,Delta,3));
}

void UNeonCityCameraExtensionComponent::ApplyAircraftCamera(UCameraComponent* Camera,USpringArmComponent* Arm,float Speed,float MaxSpeed,float Altitude,float Delta)
{
 if(CameraContext!=ENeonCityCameraContext::Aircraft || !Camera || !Arm || !FMath::IsFinite(Speed) || !FMath::IsFinite(MaxSpeed)
  || !FMath::IsFinite(Altitude) || !FMath::IsFinite(Delta) || Delta<=0 || MaxSpeed<=0) return;
 const float Alpha=FMath::Clamp(Speed/MaxSpeed,0.f,1.f),Height=FMath::Clamp(Altitude/3000,0.f,1.f);
 Arm->bEnableCameraLag=true; Arm->bUseCameraLagSubstepping=true; Arm->CameraLagSpeed=5;
 Arm->TargetArmLength=FMath::FInterpTo(Arm->TargetArmLength,900+250*Alpha+150*Height,Delta,3);
 Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,90+5*Alpha,Delta,3));
}

#include "Vehicles/Aircraft/FixedWing/FixedWingAircraftData.h"
void UNeonCityCameraExtensionComponent::ApplyFixedWingCamera(UCameraComponent* Camera,USpringArmComponent* Arm,const UFixedWingAircraftData& D,float Speed,float Bank,float Delta)
{
 if(CameraContext!=ENeonCityCameraContext::Aircraft || !Camera || !Arm || !FMath::IsFinite(Speed) || !FMath::IsFinite(Bank) || !FMath::IsFinite(Delta) || Delta<=0) return;
 const float Alpha=FMath::Clamp(Speed/(D.MaxSpeed/.036f),0.f,1.f);
 Arm->bEnableCameraLag=true; Arm->bUseCameraLagSubstepping=true; Arm->CameraLagSpeed=FMath::Lerp(6.f,3.f,Alpha);
 Arm->TargetArmLength=FMath::FInterpTo(Arm->TargetArmLength,D.CameraDistance+D.CameraSpeedExtension*Alpha,Delta,3);
 Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,D.CameraFOV+D.CameraSpeedFOV*Alpha,Delta,3));
 FRotator Rotation=Camera->GetRelativeRotation(); Rotation.Roll=FMath::FInterpTo(Rotation.Roll,FMath::Clamp(Bank*D.CameraBankFraction,-15.f,15.f),Delta,3); Camera->SetRelativeRotation(Rotation);
}
