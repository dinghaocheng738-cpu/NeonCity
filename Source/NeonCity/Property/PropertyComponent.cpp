#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
UPropertyComponent::UPropertyComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.5f; }
FName UPropertyComponent::GetPropertyID() const { return Data ? Data->PropertyID : NAME_None; }
FTransform UPropertyComponent::GetHomeSpawnTransform() const { return HomeSpawnLocal*GetOwner()->GetActorTransform(); }
FVector UPropertyComponent::GetGarageLocation(int32 Index) const
{ return GarageSlotsLocal.IsValidIndex(Index) ? GetOwner()->GetActorTransform().TransformPosition(GarageSlotsLocal[Index]) : FVector::ZeroVector; }
void UPropertyComponent::BeginPlay()
{
 Super::BeginPlay();
 auto* GI=GetWorld()->GetGameInstance();
 if(!GI || !GI->GetSubsystem<UPropertySubsystem>()->RegisterLiveProperty(this))
 { UE_LOG(LogTemp,Error,TEXT("Invalid/duplicate live property on %s"),*GetOwner()->GetName()); SetComponentTickEnabled(false); }
}
void UPropertyComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(DeltaTime,TickType,TickFunction);
 if(auto* GI=GetWorld()->GetGameInstance()) GI->GetSubsystem<UPropertySubsystem>()->ReconcileGarage(GetPropertyID());
}
void UPropertyComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* GI=GetWorld()->GetGameInstance())
  if(auto* Registry=GI->GetSubsystem<UPropertySubsystem>()) Registry->UnregisterLiveProperty(this);
 Super::EndPlay(Reason);
}
