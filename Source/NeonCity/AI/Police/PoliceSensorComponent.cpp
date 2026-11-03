#include "AI/Police/PoliceSensorComponent.h"
#include "Wanted/SearchSystem.h"
#include "Combat/HealthComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
UPoliceSensorComponent::UPoliceSensorComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UPoliceSensorComponent::BeginPlay() { Super::BeginPlay(); if (GetOwner()->HasAuthority()) { GetWorld()->GetSubsystem<USearchSystem>()->RegisterSensor(this); } }
void UPoliceSensorComponent::EndPlay(EEndPlayReason::Type Reason)
{ if (auto* S=GetWorld()->GetSubsystem<USearchSystem>()) { S->UnregisterSensor(this); } Super::EndPlay(Reason); }
FVector UPoliceSensorComponent::GetEyeLocation() const { return GetOwner()->GetActorTransform().TransformPosition(EyeOffset); }
FVector UPoliceSensorComponent::GetViewDirection() const
{
 if(bSurveillance) return GetOwner()->GetActorForwardVector();
 FRotator View=bFocus ? (FocusLocation-GetEyeLocation()).Rotation() : GetOwner()->GetActorRotation(); if (bScanning) { View.Yaw+=GetWorld()->GetTimeSeconds()*75.f; } return View.Vector();
}
bool UPoliceSensorComponent::CanObserve() const
{
 if (!IsValid(GetOwner()) || GetOwner()->IsHidden()) { return false; }
 if(auto* P=GetOwner()->FindComponentByClass<UCharacterImpactComponent>(); P && (P->GetState()==ECharacterImpactState::Knockdown || P->GetState()==ECharacterImpactState::Downed || P->GetState()==ECharacterImpactState::Dead)) return false;
 if (auto* H=GetOwner()->FindComponentByClass<UHealthComponent>(); H && H->IsDead()) { return false; }
 if(auto* Vehicle=GetOwner()->FindComponentByClass<UVehicleDamageComponent>(); Vehicle && Vehicle->GetDrivePowerMultiplier()==0) return false;
 if (auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body
  && (Body->GetBodyState()==ENeonCityBodyState::Downed || Body->GetBodyState()==ENeonCityBodyState::Dead)) { return false; }
 return true;
}
