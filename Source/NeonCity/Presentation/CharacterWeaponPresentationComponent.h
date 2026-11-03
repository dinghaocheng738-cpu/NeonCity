#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterWeaponPresentationComponent.generated.h"
class USceneComponent;

/** Skeletal grip adapter. Weapons continue to accept any scene-component host,
 * including vehicle hardpoints; only characters opt into this presentation. */
UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCharacterWeaponPresentationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCharacterWeaponPresentationComponent();
 USceneComponent* GetGrip();
 UPROPERTY(EditDefaultsOnly, Category="Presentation") FName HandSocket=TEXT("hand_r");
 UPROPERTY(EditDefaultsOnly, Category="Presentation") FTransform GripOffset=FTransform(FRotator(0,180,0));
private:
 UPROPERTY() TObjectPtr<USceneComponent> Grip;
};
