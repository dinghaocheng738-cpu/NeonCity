#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterAppearanceComponent.generated.h"

class USkeletalMesh;

/** Reviewed external skins on the existing animation/physics skeleton. No movement logic. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCharacterAppearanceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCharacterAppearanceComponent();
 UFUNCTION(BlueprintCallable,Category="Appearance") void RefreshAppearance();
 UFUNCTION(BlueprintCallable,Category="Appearance") void ApplyPalette(int32 PaletteIndex);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Appearance") bool bUseReferenceCharacter=true;
 UPROPERTY(EditDefaultsOnly,Category="Appearance") TSoftObjectPtr<USkeletalMesh> ReferenceMale;
 UPROPERTY(EditDefaultsOnly,Category="Appearance") TSoftObjectPtr<USkeletalMesh> ReferenceFemale;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Appearance") bool bUseCoastalPalette=true;
protected:
 virtual void BeginPlay() override;
};
