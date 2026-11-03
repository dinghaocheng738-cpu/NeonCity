#include "Combat/WeaponStoreActor.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
AWeaponStoreActor::AWeaponStoreActor()
{
 auto* Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoreTerminal")); SetRootComponent(Body);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Body->SetStaticMesh(Cube.Object); Body->SetRelativeScale3D(FVector(.4f,1,.9f)); Body->SetCollisionProfileName(TEXT("BlockAll"));
 auto* Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("StoreLabel")); Label->SetupAttachment(Body);
 Label->SetRelativeLocation(FVector(0,0,100)); Label->SetWorldSize(20); Label->SetHorizontalAlignment(EHTA_Center);
 Label->SetText(FText::FromString(TEXT("WEAPON TEST STORE\nF: Browse weapons / ammo")));
}
void AWeaponStoreActor::ApplyAuthoredPresentation(UStaticMesh* Mesh,const FText& Title)
{
 if(!Mesh)return;
 auto* Body=Cast<UStaticMeshComponent>(GetRootComponent());if(!Body)return;
 Body->SetStaticMesh(Mesh);Body->SetRelativeScale3D(FVector::OneVector);Body->EmptyOverrideMaterials();
 const FBox Box=Mesh->GetBoundingBox();const FVector Centre=Box.GetCenter();
 SetActorLocation(GetActorLocation()-GetActorQuat().RotateVector(FVector(Centre.X,Centre.Y,Box.Min.Z)));
 TInlineComponentArray<UTextRenderComponent*> Labels(this);
 for(auto* Label:Labels)Label->SetVisibility(false);
 StoreTitle=Title;
}
bool AWeaponStoreActor::CanInteract_Implementation(AActor* A) const
{
 const auto* Pawn=Cast<APawn>(A);
 if(!HasAuthority() || !Pawn || !Pawn->IsPlayerControlled() || !A->FindComponentByClass<UWeaponInventoryComponent>() || FVector::Dist(A->GetActorLocation(),GetActorLocation())>350) return false;
 if(const auto* Body=A->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(WeaponStoreAccess),false,A); Q.AddIgnoredActor(this);
 FVector Eye;FRotator View;A->GetActorEyesViewPoint(Eye,View);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,GetActorLocation(),ECC_Visibility,Q);
}
FText AWeaponStoreActor::GetInteractionName_Implementation() const { return FText::FromString(TEXT("Browse weapons and ammo")); }
void AWeaponStoreActor::Interact_Implementation(AActor* A)
{ if(CanInteract_Implementation(A)) if(auto* Adapter=A->FindComponentByClass<UPlayerCombatAdapter>()) Adapter->OpenStore(this); }
bool AWeaponStoreActor::Buy(AActor* Buyer,int32 Slot,bool Ammo)
{
 if(!CanInteract_Implementation(Buyer) || Slot<1 || Slot>4) return false;
 auto* Inventory=Buyer->FindComponentByClass<UWeaponInventoryComponent>();
 return Ammo?Inventory->PurchaseAmmo(ENeonCityAmmoType(Slot-1)):Inventory->PurchaseWeapon(NeonCityWeapons::Slot(Slot));
}
FText AWeaponStoreActor::Describe(AActor* Buyer) const
{
 auto* Inventory=IsValid(Buyer)?Buyer->FindComponentByClass<UWeaponInventoryComponent>():nullptr;
 auto* Wallet=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 FString Text=FString::Printf(TEXT("%s     Cash: $%lld\n"),*StoreTitle.ToString(),Wallet->GetBalance());
 for(int32 Slot=1;Slot<=4;++Slot)
 {
  const auto* P=NeonCityWeapons::Profile(NeonCityWeapons::Slot(Slot));
  Text+=FString::Printf(TEXT("%d: %s $%lld %s     %d: Ammo +%d $%lld\n"),Slot,*P->DisplayName.ToString(),P->Price,Inventory && Inventory->Owns(P->WeaponId)?TEXT("[OWNED]"):TEXT(""),Slot+4,UWeaponInventoryComponent::AmmoPackSize(P->AmmoType),UWeaponInventoryComponent::AmmoPackPrice(P->AmmoType));
 }
 return FText::FromString(Text+TEXT("F / Esc: Close   |   Owned weapons: 1-4 equip / stow"));
}
