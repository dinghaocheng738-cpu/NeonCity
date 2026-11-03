#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandGarageDoor.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
AIslandGarageControl::AIslandGarageControl()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.15f;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GarageControlOrigin")));
 Face=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlFace"));Face->SetupAttachment(RootComponent);Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("NormalFControl"));Interaction->SetupAttachment(RootComponent);Interaction->SetBoxExtent(FVector(3,13,18));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Interaction->SetGenerateOverlapEvents(false);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("FocusedGaragePrompt"));Prompt->SetupAttachment(RootComponent);Prompt->SetRelativeLocation(FVector(4,0,36));Prompt->SetWorldSize(10);Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetTextRenderColor(FColor(241,227,196));Prompt->SetVisibility(false);
}
bool AIslandGarageControl::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* P=Cast<ACharacter>(Visitor);const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!HasAuthority() || !P || P->GetWorld()!=GetWorld() || !PC || !PC->IsLocalController() || PC->GetPawn()!=P || !P->GetCharacterMovement()->IsMovingOnGround() || P->GetAttachParentActor() || FVector::DistSquared(P->GetActorLocation(),GetActorLocation())>FMath::Square(240.f))return false;
 if(const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();Body && !Body->CanAct())return false;
 return GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID);
}
FGuid AIslandGarageControl::FindStoredOwnedVehicle()const
{
 auto* V=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
 for(const auto& Record:V->BuildSaveSnapshot())if(V->IsLegacyFleetStored(Record.VehicleID) && !V->FindLiveVehicle(Record.VehicleID))return Record.VehicleID;
 return FGuid();
}
FText AIslandGarageControl::GetInteractionName_Implementation()const
{
 const TCHAR* Labels[]={TEXT("Register owned car parked in this bay"),TEXT("Release this parking registration"),TEXT("Retrieve an owned stored car into the clear bay"),TEXT("Switch vehicle display lighting"),TEXT("Operate articulated garage door")};
 return FText::FromString(Labels[static_cast<uint8>(Action)]);
}
void AIslandGarageControl::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;auto* Properties=GetGameInstance()->GetSubsystem<UPropertySubsystem>();bool Done=false;FString Notice;
 if(Action==EIslandGarageAction::RegisterParked){Done=Properties->StoreVehicle(PropertyID,SlotIndex,Properties->FindParkedVehicle(PropertyID,SlotIndex));Notice=Done?TEXT("Owned vehicle registered in this physical bay. F6 saves its original identity."):TEXT("Park an owned car fully inside this bay, stop and get out first.");}
 else if(Action==EIslandGarageAction::ReleaseParked){Done=Properties->ReleaseVehicle(PropertyID,SlotIndex);Notice=Done?TEXT("Parking registration released. The car remains here and stays owned."):TEXT("This bay has no registered car.");}
 else if(Action==EIslandGarageAction::RetrieveStored)
 {
  FNeonCityPropertyRecord Property;auto* Vehicles=GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();const FGuid ID=FindStoredOwnedVehicle();
  if(Properties->GetProperty(PropertyID,Property) && Property.GarageSlots.IsValidIndex(SlotIndex) && ID.IsValid())
  {FTransform Bay=RetrievalBayLocal*GetActorTransform();Bay.SetLocation(Property.GarageSlots[SlotIndex].Location);Done=Vehicles->DeployStoredVehicle(ID,Bay)!=nullptr;}
  Notice=Done?TEXT("Your stored car is back in the bay with its original identity and condition. F6 saves it."):(ID.IsValid()?TEXT("Retrieval bay obstructed. Clear people, cars and objects before retrieving."):TEXT("No owned retired island car is currently stored. Live cars are never copied or moved here."));
 }
 else if(Action==EIslandGarageAction::DisplayLighting){bDisplayLit=!bDisplayLit;for(const auto& L:DisplayLights)if(IsValid(L))L->SetVisibility(bDisplayLit);Done=true;Notice=bDisplayLit?TEXT("Vehicle display lights on."):TEXT("Vehicle display lights off.");}
 else if(Action==EIslandGarageAction::Door && Door){Door->Toggle();Done=true;Notice=Door->IsTargetOpen()?TEXT("Garage door opening. Keep the track clear."):TEXT("Garage door closing. Obstructions stop and reverse it.");}
 auto* P=Cast<ACharacter>(Visitor);if(auto* N=P->GetController()->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(Notice.IsEmpty()?TEXT("This control is unavailable."):Notice),5);
}
void AIslandGarageControl::Tick(float Delta)
{
 Super::Tick(Delta);auto* PC=GetWorld()->GetFirstPlayerController();APawn* P=PC?PC->GetPawn():nullptr;auto* Focus=P?P->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this;Prompt->SetVisibility(Visible);if(!Visible)return;
 Prompt->SetText(FText::FromString(TEXT("F  ")+GetInteractionName_Implementation().ToString()));FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());
}
