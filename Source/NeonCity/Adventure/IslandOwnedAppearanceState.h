#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IslandOwnedAppearanceState.generated.h"

/** The five existing licensed coastal clothing palettes are the starting owned
 * wardrobe. This state never grants, replaces or removes adventure equipment. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UIslandOwnedAppearanceState : public UActorComponent
{
 GENERATED_BODY()
public:
 UIslandOwnedAppearanceState();
 static UIslandOwnedAppearanceState* FindOrCreate(AActor* Owner);
 static FText PaletteName(int32 Index);
 bool CanChange() const;
 bool SelectNextOwned();
 bool SelectOwned(int32 Index);
 void RestoreState(int32 Selected,uint8 OwnedMask);
 int32 GetSelectedPalette() const { return SelectedPalette; }
 uint8 GetOwnedPaletteMask() const { return OwnedPaletteMask; }
 bool OwnsPalette(int32 Index) const { return Index>=0 && Index<5 && (OwnedPaletteMask&(1<<Index)); }
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 bool ApplySelected();
 int32 SelectedPalette=0;
 uint8 OwnedPaletteMask=0x1f;
};
