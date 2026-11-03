#include "Water/SwimmingComponent.h"
#include "Water/SwimmingData.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "InputCoreTypes.h"
USwimmingComponent::USwimmingComponent() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
const USwimmingData* USwimmingComponent::GetProfile() const { return SwimmingData && SwimmingData->IsValidProfile()?SwimmingData.Get():GetDefault<USwimmingData>(); }
void USwimmingComponent::BeginPlay()
{
 Super::BeginPlay();
 if(auto* C=Cast<ACharacter>(GetOwner()))
 {
  auto* M=C->GetCharacterMovement(); M->GetNavAgentPropertiesRef().bCanSwim=true; M->AddTickPrerequisiteComponent(this);
  if(auto* Water=C->FindComponentByClass<UWaterInteractionComponent>()) AddTickPrerequisiteComponent(Water);
 }
}
void USwimmingComponent::RestoreLandSettings()
{
 RestoreSeafloorSettings();
 bDiveIntent=false;bSurfaceRecovery=false;bSeafloorPushOff=false;
 if(!bApplied) return;
 if(auto* C=Cast<ACharacter>(GetOwner())) { auto* M=C->GetCharacterMovement(); M->MaxAcceleration=LandAcceleration; M->BrakingDecelerationSwimming=LandBraking; M->Buoyancy=LandBuoyancy; M->MaxSwimSpeed=LandSwimSpeed; }
 bApplied=false;
}
void USwimmingComponent::RestoreSeafloorSettings()
{
 bSeafloorStanding=false;
 if(!bFloorSettings)return;
 if(auto* C=Cast<ACharacter>(GetOwner()))
 {
  auto* M=C->GetCharacterMovement();M->MaxWalkSpeed=LandWalkSpeed;M->GroundFriction=LandGroundFriction;
  M->BrakingDecelerationWalking=LandWalkBraking;M->MaxAcceleration=FloorAcceleration;M->MaxStepHeight=LandStepHeight;
 }
 bFloorSettings=false;
}
bool USwimmingComponent::PushOffSeafloor()
{
 if(!bSeafloorStanding)return false;
 auto* C=Cast<ACharacter>(GetOwner());if(!C)return false;
 RestoreSeafloorSettings();PushOffStarted=GetWorld()->GetTimeSeconds();SeafloorReleaseUntil=PushOffStarted+1.05;
 PushOffFloorZ=C->GetActorLocation().Z;
 bSeafloorPushOff=true;bDiveIntent=true;bSurfaceRecovery=false;
 C->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
 // Break contact gently, then integrate the fin/leg extension over a finite
 // stroke. This is deliberately independent of land JumpZVelocity.
 C->GetCharacterMovement()->AddImpulse(FVector(0,0,24),true);
 return true;
}
float USwimmingComponent::GetPushOffPoseWeight() const
{
 if(!bSeafloorPushOff || GetWorld()->GetTimeSeconds()-PushOffStarted>1.3)return 0;
 const float Rise=GetOwner()->GetActorLocation().Z-PushOffFloorZ;
 return 1-FMath::SmoothStep(40.f,100.f,Rise);
}
void USwimmingComponent::ResetForLoad()
{
 RestoreLandSettings();
 if(auto* C=Cast<ACharacter>(GetOwner()))
 {
  C->GetCapsuleComponent()->UpdatePhysicsVolume(true);
  if(C->GetCharacterMovement()->IsSwimming())
  {
   if(auto* Body=C->FindComponentByClass<UDamageableCharacterComponent>()) Body->RestoreLocomotionMode(MOVE_Falling,0);
   else C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  }
 }
 if(auto* Water=GetOwner()->FindComponentByClass<UWaterInteractionComponent>()) Water->ResetForLoad();
 if(auto* Oxygen=GetOwner()->FindComponentByClass<UOxygenComponent>()) Oxygen->ResetOxygen();
}
void USwimmingComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* C=Cast<ACharacter>(GetOwner()); if(!C) return;
 auto* M=C->GetCharacterMovement(); auto* Water=C->FindComponentByClass<UWaterInteractionComponent>();
 if(C->IsHidden() || !C->GetActorEnableCollision()) { RestoreLandSettings(); return; }
 if(auto* Body=C->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { RestoreLandSettings(); return; }
 const auto* PC=Cast<APlayerController>(C->GetController());
 const bool Ascending=PC && PC->IsInputKeyDown(EKeys::SpaceBar);
 const auto* Kit=C->FindComponentByClass<UDivingComponent>();
 const auto* Device=C->FindComponentByClass<UDiveEquipmentComponent>();
 const bool Propelled=Device && Device->IsDPVDeployed();
 const bool FloorReady=Kit && Kit->HasBallast() && Water && Water->IsFullySubmerged()
  && Water->HasSeafloorSupport() && !Ascending && !(Propelled && Device->GetMotorSpool()>.08f) && M->Velocity.Z<=3
  && GetWorld()->GetTimeSeconds()>=SeafloorReleaseUntil;
 if(FloorReady && (M->IsSwimming() || M->IsMovingOnGround()))
 {
  if(!bFloorSettings)
  {
   // Restore swimming's saved acceleration before preserving the ground profile.
   RestoreLandSettings();LandWalkSpeed=M->MaxWalkSpeed;LandGroundFriction=M->GroundFriction;
   LandWalkBraking=M->BrakingDecelerationWalking;FloorAcceleration=M->MaxAcceleration;LandStepHeight=M->MaxStepHeight;bFloorSettings=true;
  }
  bSeafloorStanding=true;bSeafloorPushOff=false;bDiveIntent=true;bSurfaceRecovery=false;
  M->MaxWalkSpeed=105;M->MaxAcceleration=160;M->GroundFriction=1.8f;M->BrakingDecelerationWalking=95;M->MaxStepHeight=14;
  if(!M->IsMovingOnGround())M->SetMovementMode(MOVE_Walking);
  if(auto* Equipment=C->FindComponentByClass<UDiveEquipmentComponent>())Equipment->ApplyPropulsion(M,Delta);
  return;
 }
 if(bSeafloorStanding)
 {
  if(Ascending)PushOffSeafloor();
  else { RestoreSeafloorSettings();if(Water && Water->IsFullySubmerged())M->SetMovementMode(MOVE_Swimming); }
 }
 if(Water && Water->HasShallowFloor() && M->IsSwimming()) M->SetMovementMode(MOVE_Walking);
 // Walking down a continuous seabed can remain inside the same physics volume.
 // CMC's volume-entry callback therefore does not run a second time after wading.
 // Resume native swimming when support is too deep, using the existing local
 // water sample. Keep dry decks, disabled/downed movement and custom modes alone.
 else if(Water && M->IsMovingOnGround() && !Water->HasShallowFloor()
  && Water->GetWaterState()!=EWaterInteractionState::Dry
  && C->GetPhysicsVolume() && C->GetPhysicsVolume()->bWaterVolume
  && Water->GetSample().DepthAt(C->GetActorLocation())>0)
  M->SetMovementMode(MOVE_Swimming);
 if(!M->IsSwimming()) { RestoreLandSettings(); return; }
 if(!bApplied) { LandAcceleration=M->MaxAcceleration; LandBraking=M->BrakingDecelerationSwimming; LandBuoyancy=M->Buoyancy; LandSwimSpeed=M->MaxSwimSpeed; bApplied=true; }
 const auto* D=GetProfile(); const bool Under=Water && Water->IsFullySubmerged();
 const bool EquippedDive=Under && Kit && Kit->IsEquipped();
 M->MaxAcceleration=EquippedDive?FMath::Min(D->Acceleration,260.f):D->Acceleration;
 M->MaxSwimSpeed=Under?(EquippedDive?FMath::Min(D->UnderwaterSpeed,165.f):D->UnderwaterSpeed):D->SwimSpeed;
 M->BrakingDecelerationSwimming=D->WaterDrag*(EquippedDive?40.f:100.f);
 float Vertical=0;
 if(PC && PC->IsLocalController())
  Vertical=(PC->IsInputKeyDown(EKeys::SpaceBar)?1.f:0.f)-((PC->IsInputKeyDown(EKeys::C) || PC->IsInputKeyDown(EKeys::LeftControl))?1.f:0.f);
 const float Immersion=M->ImmersionDepth();
 if(bSeafloorPushOff)
 {
  const float PushAge=GetWorld()->GetTimeSeconds()-PushOffStarted;
  const bool HeldAscent=Vertical>0 && PushAge>.55f;
  if(!EquippedDive || !Kit->HasBallast() || Vertical<0 || HeldAscent || (Propelled && PushAge>.55f))bSeafloorPushOff=false;
  else
  {
   // A half-second leg extension, followed by a free buoyant arc. Input
   // release does not cancel the stroke or snap momentum to a sink speed.
   // 3.3% effective weight and speed-dependent resistance keep the return
   // slow; CMC retains all collision and native fluid integration.
   M->Buoyancy=FMath::Max(0.f,1.f-32.f/FMath::Max(1.f,FMath::Abs(M->GetGravityZ())))/FMath::Max(.1f,Immersion);
   M->BrakingDecelerationSwimming=0;
   const float Stroke=PushAge<.5f?440.f*FMath::Sin(PI*FMath::Clamp(PushAge/.5f,0.f,1.f)):0;
   const float Drag=.14f+.0012f*FMath::Abs(M->Velocity.Z);
   M->AddForce(FVector(0,0,Stroke-M->Velocity.Z*Drag)*M->Mass);
   if(auto* Equipment=C->FindComponentByClass<UDiveEquipmentComponent>())Equipment->ApplyPropulsion(M,Delta);
   return;
  }
 }
 const float SurfaceBand=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.2f;
 if(Vertical<0) { bDiveIntent=true;bSurfaceRecovery=false; }
 else if(Vertical>0 && Water && Water->GetSample().DepthAt(C->GetActorLocation())<=SurfaceBand)
 { bDiveIntent=false;bSurfaceRecovery=true; }
 if(!bDiveIntent && Water && Water->GetSample().DepthAt(C->GetActorLocation())<=SurfaceBand)bSurfaceRecovery=true;
 // Neutral buoyancy uses CMC's native immersion trace. No position/velocity assignment or custom PhysSwimming.
 M->Buoyancy=Immersion>.1f?1.f/Immersion:1.f;
 float DesiredVertical=(Vertical>0?D->AscendSpeed:Vertical<0?-D->DescendSpeed:0)*(EquippedDive?.72f:1.f);
 if(Vertical==0 && Under && bDiveIntent && Kit && Kit->HasBallast() && !Propelled)DesiredVertical=-18;
 // A rising wave must not revoke a previously acquired surface intent just
 // because it temporarily exceeds the shallow capture band. Explicit diving
 // releases it; entering deep water alone does not acquire it.
 if(Vertical==0 && !bDiveIntent && bSurfaceRecovery && Water)
 {
  const float DesiredDepth=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.55f;
  // Feed forward the shared water's vertical motion, then correct depth using
  // ordinary CMC input. No actor/velocity assignment or replacement physics.
  DesiredVertical=FMath::Clamp(Water->GetSample().Flow.Z+(Water->GetSample().DepthAt(C->GetActorLocation())-DesiredDepth)*4.f,-D->DescendSpeed,D->AscendSpeed);
 }
 // Integrate the existing velocity response over this frame. An explicit
 // gain*Delta update reverses past the target below 8 FPS and makes surface
 // recovery oscillate; exponential response retains the same time constant.
 const float Response=EquippedDive?3.f:8.f;
 const float Gain=Delta>UE_SMALL_NUMBER?(1-FMath::Exp(-Response*Delta))/Delta:Response;
 const float Control=FMath::Clamp((DesiredVertical-M->Velocity.Z)*Gain/FMath::Max(1.f,M->MaxAcceleration),-1.f,1.f);
 // With the DPV, the view-directed motor supplies the vertical component.
 // A zero-input buoyancy controller must not cancel that propulsion force.
 if(!Propelled || Vertical!=0)C->AddMovementInput(FVector::UpVector,Control);
 if(auto* Equipment=C->FindComponentByClass<UDiveEquipmentComponent>())Equipment->ApplyPropulsion(M,Delta);
}
