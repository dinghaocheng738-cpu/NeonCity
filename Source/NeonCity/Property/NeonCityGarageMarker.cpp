#include "Property/NeonCityGarageMarker.h"
#include "Property/PropertySubsystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
ANeonCityGarageMarker::ANeonCityGarageMarker()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.2f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Post=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Post")); Post->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal"));
 Post->SetStaticMesh(Cube.Object); Post->SetMaterial(0,Paint.Object); Post->SetRelativeScale3D(FVector(.5,.4,1.2));
 Post->SetRelativeLocation(FVector(0,0,60)); Post->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(RootComponent);
 Label->SetRelativeLocation(FVector(0,0,145)); Label->SetWorldSize(16); Label->SetHorizontalAlignment(EHTA_Center);
}
void ANeonCityGarageMarker::ApplyAuthoredPresentation()
{
 if(!Post || !PostMesh)return;
 // Keep the original interaction receiver and all its body-instance collision
 // settings. The actor-level configuration survives native component recreation.
 Post->SetStaticMesh(PostMesh);
 if(Post->GetStaticMesh()!=PostMesh)return;
 Post->SetRelativeTransform(PostLocalTransform);
 Post->EmptyOverrideMaterials();
}
void ANeonCityGarageMarker::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);ApplyAuthoredPresentation();
}
void ANeonCityGarageMarker::BeginPlay()
{
 ApplyAuthoredPresentation();Super::BeginPlay();
}
bool ANeonCityGarageMarker::CanInteract_Implementation(AActor* Visitor) const
{
 auto* Character=Cast<ACharacter>(Visitor); auto* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
 if(!HasAuthority() || !IsValid(Character) || Character->GetWorld()!=GetWorld() || !PC || !PC->IsLocalController()
  || PC->GetPawn()!=Character || FVector::Dist(Visitor->GetActorLocation(),GetActorLocation())>280) return false;
 if(auto* Body=Character->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 auto* Registry=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>(); FNeonCityPropertyRecord Record;
 return Registry->GetProperty(PropertyID,Record) && Record.Owned && Record.GarageSlots.IsValidIndex(SlotIndex);
}
FText ANeonCityGarageMarker::GetInteractionName_Implementation() const { return FText::FromString(TEXT("Register parked vehicle")); }
void ANeonCityGarageMarker::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor)) return;
 FVector Eye; FRotator R; Visitor->GetActorEyesViewPoint(Eye,R); FHitResult Hit;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(GarageMarker),false,Visitor);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,GetActorLocation()+FVector(0,0,70),ECC_Visibility,Query) || Hit.GetActor()!=this) return;
 auto* Registry=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
 Registry->StoreVehicle(PropertyID,SlotIndex,Registry->FindParkedVehicle(PropertyID,SlotIndex));
}
void ANeonCityGarageMarker::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime); auto* PC=GetWorld()->GetFirstPlayerController(); APawn* Pawn=PC ? PC->GetPawn() : nullptr;
 const bool Near=Pawn && FVector::Dist(Pawn->GetActorLocation(),GetActorLocation())<700; Label->SetVisibility(Near);
 if(!Near) return;
 FNeonCityPropertyRecord Record; auto* Registry=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
 const bool Stored=Registry->GetProperty(PropertyID,Record) && Record.GarageSlots.IsValidIndex(SlotIndex) && Record.GarageSlots[SlotIndex].VehicleID.IsValid();
 Label->SetText(FText::FromString(FString::Printf(TEXT("HOME PARKING / %02d\n%s"),SlotIndex+1,Stored ? TEXT("Vehicle registered") : TEXT("Park owned car here\nF  Register at post"))));
 FVector Eye; FRotator R; PC->GetPlayerViewPoint(Eye,R); Label->SetWorldRotation((Eye-Label->GetComponentLocation()).Rotation());
}
