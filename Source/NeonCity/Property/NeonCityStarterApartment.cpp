#include "Property/NeonCityStarterApartment.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
ANeonCityStarterApartment::ANeonCityStarterApartment()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.1f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Property=CreateDefaultSubobject<UPropertyComponent>(TEXT("Property")); Property->GarageSlotsLocal.Add(FVector(375,-600,26));
 Door=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door")); Door->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal"));
 Door->SetStaticMesh(Cube.Object); Door->SetMaterial(0,Paint.Object); Door->SetRelativeLocation(FVector(0,0,130));
 Door->SetRelativeScale3D(FVector(2.0,.4,2.6)); Door->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 InteriorBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("InteriorBounds")); InteriorBounds->SetupAttachment(RootComponent);
 InteriorBounds->SetRelativeLocation(FVector(0,420,150)); InteriorBounds->SetBoxExtent(FVector(390,390,148)); InteriorBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Prompt")); Prompt->SetupAttachment(RootComponent);
 Prompt->SetRelativeLocation(FVector(0,0,220)); Prompt->SetWorldSize(18); Prompt->SetHorizontalAlignment(EHTA_Center); Prompt->SetVisibility(false);
}
bool ANeonCityStarterApartment::IsResident(AActor* Visitor) const
{
 const auto* Character=Cast<ACharacter>(Visitor);
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
 if(!HasAuthority() || !IsValid(Character) || Character->GetWorld()!=GetWorld() || !PC || !PC->IsLocalController()
  || PC->GetPawn()!=Character || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
 if(const auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 const auto* GI=GetWorld()->GetGameInstance();
 return GI && GI->GetSubsystem<UPropertySubsystem>()->IsOwned(Property->GetPropertyID());
}
bool ANeonCityStarterApartment::IsAtDoor(AActor* Visitor) const
{
 if(!IsResident(Visitor) || FVector::Dist2D(Visitor->GetActorLocation(),GetActorLocation())>285 || FMath::Abs(Visitor->GetActorLocation().Z-GetActorLocation().Z)>200) return false;
 FVector Eye; FRotator Rotation; Visitor->GetActorEyesViewPoint(Eye,Rotation);
 FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(ApartmentDoor),false,Visitor);
 return GetWorld()->LineTraceSingleByChannel(Hit,Eye,Door->GetComponentLocation(),ECC_Visibility,Query) && Hit.GetActor()==this;
}
bool ANeonCityStarterApartment::IsInsideInterior_Implementation(AActor* Visitor) const
{
 if(!IsValid(Visitor) || Visitor->GetWorld()!=GetWorld()) return false;
 const FVector Local=InteriorBounds->GetComponentTransform().InverseTransformPosition(Visitor->GetActorLocation());
 const FVector Extent=InteriorBounds->GetUnscaledBoxExtent();
 return FMath::Abs(Local.X)<Extent.X && FMath::Abs(Local.Y)<Extent.Y && FMath::Abs(Local.Z)<Extent.Z;
}
bool ANeonCityStarterApartment::CanEnterInterior_Implementation(AActor* Visitor) const
{ return IsAtDoor(Visitor) && !IsInsideInterior_Implementation(Visitor); }
bool ANeonCityStarterApartment::CanInteract_Implementation(AActor* Visitor) const { return IsAtDoor(Visitor); }
FText ANeonCityStarterApartment::GetInteractionName_Implementation() const
{
 auto* PC=GetWorld()->GetFirstPlayerController();
 return FText::FromString(PC && IsInsideInterior_Implementation(PC->GetPawn()) ? TEXT("Leave home") : TEXT("Enter home"));
}
void ANeonCityStarterApartment::Interact_Implementation(AActor* Visitor)
{ if(IsInsideInterior_Implementation(Visitor)) ExitInterior_Implementation(Visitor); else EnterInterior_Implementation(Visitor); }
bool ANeonCityStarterApartment::MoveThroughDoor(ACharacter* Visitor,const FTransform& LocalDestination)
{
 const FTransform Destination=LocalDestination*GetActorTransform();
 if(Destination.ContainsNaN()) return false;
 const auto* Capsule=Visitor->GetCapsuleComponent();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(ApartmentDestination),false,Visitor);
 if(GetWorld()->OverlapBlockingTestByProfile(Destination.GetLocation(),Destination.GetRotation(),Capsule->GetCollisionProfileName(),
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query)) return false;
 if(!Visitor->TeleportTo(Destination.GetLocation(),Destination.Rotator(),false,false)) return false;
 Visitor->GetCharacterMovement()->StopMovementImmediately();
 CastChecked<APlayerController>(Visitor->GetController())->SetControlRotation(Destination.Rotator());
 const bool Inside=IsInsideInterior_Implementation(Visitor);
 OnInteriorChanged.Broadcast(Visitor,Inside);
 if(Inside) GetWorld()->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>()->RequestAutosave(ENeonCityAutosaveReason::EnteredHome);
 return true;
}
bool ANeonCityStarterApartment::EnterInterior_Implementation(AActor* Visitor)
{ return CanEnterInterior_Implementation(Visitor) && MoveThroughDoor(CastChecked<ACharacter>(Visitor),EntryLocal); }
bool ANeonCityStarterApartment::ExitInterior_Implementation(AActor* Visitor)
{ return IsAtDoor(Visitor) && IsInsideInterior_Implementation(Visitor) && MoveThroughDoor(CastChecked<ACharacter>(Visitor),ExitLocal); }
bool ANeonCityStarterApartment::RequestHomeService_Implementation(AActor* Visitor,ENeonCityHomeService Service)
{
 if(!IsResident(Visitor) || !IsInsideInterior_Implementation(Visitor) || uint8(Service)>uint8(ENeonCityHomeService::SavePoint)) return false;
 OnHomeServiceRequested.Broadcast(Visitor,Service);
 if(Service==ENeonCityHomeService::SavePoint) GetWorld()->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>()->RequestAutosave(ENeonCityAutosaveReason::SavePoint);
 return true;
}
void ANeonCityStarterApartment::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime); auto* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC ? PC->GetPawn() : nullptr;
 auto* Interaction=Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr;
 const bool Focused=Interaction && Interaction->GetFocusedInteractable()==this;
 Prompt->SetVisibility(Focused);
 if(Focused)
 {
  Prompt->SetText(FText::FromString(TEXT("F  ")+GetInteractionName_Implementation().ToString()));
  FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation);
  Prompt->SetWorldLocation(GetActorLocation()+FVector(0,0,210)+(Eye-Door->GetComponentLocation()).GetSafeNormal2D()*35);
  Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());
 }
}
