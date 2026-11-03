#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UCharacterWeaponPresentationComponent::UCharacterWeaponPresentationComponent()
{ PrimaryComponentTick.bCanEverTick=false; }
USceneComponent* UCharacterWeaponPresentationComponent::GetGrip()
{
 if(Grip) return Grip;
 auto* Character=Cast<ACharacter>(GetOwner());
 if(!Character || !Character->GetMesh() || !Character->GetMesh()->DoesSocketExist(HandSocket)) return nullptr;
 Grip=NewObject<USceneComponent>(Character,TEXT("CharacterWeaponGrip"));
 Character->AddInstanceComponent(Grip);
 Grip->SetupAttachment(Character->GetMesh(),HandSocket);
 Grip->SetRelativeTransform(GripOffset); Grip->RegisterComponent();
 return Grip;
}
