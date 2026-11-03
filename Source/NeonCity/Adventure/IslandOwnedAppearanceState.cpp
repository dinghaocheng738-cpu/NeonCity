#include "Adventure/IslandOwnedAppearanceState.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UIslandOwnedAppearanceState::UIslandOwnedAppearanceState()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickInterval=.25f;}
UIslandOwnedAppearanceState* UIslandOwnedAppearanceState::FindOrCreate(AActor* Owner)
{
 if(!IsValid(Owner) || !Owner->HasAuthority())return nullptr;
 if(auto* Existing=Owner->FindComponentByClass<UIslandOwnedAppearanceState>())return Existing;
 auto* State=NewObject<UIslandOwnedAppearanceState>(Owner);Owner->AddInstanceComponent(State);State->RegisterComponent();return State;
}
FText UIslandOwnedAppearanceState::PaletteName(int32 Index)
{
 const TCHAR* Names[]={TEXT("Coastal teal"),TEXT("Terracotta"),TEXT("Olive"),TEXT("Midnight"),TEXT("Sand")};
 return FText::FromString(Index>=0 && Index<5?Names[Index]:TEXT("Unavailable outfit"));
}
bool UIslandOwnedAppearanceState::CanChange()const
{
 const auto* P=Cast<ACharacter>(GetOwner());if(!P || !P->HasAuthority() || P->IsHidden() || P->GetAttachParentActor() || !P->GetCharacterMovement()->IsMovingOnGround())return false;
 const auto* Dive=P->FindComponentByClass<UDivingComponent>();const auto* Wing=P->FindComponentByClass<UWingsuitComponent>();
 if((Dive && (Dive->IsEquipped() || Dive->IsChangingGear())) || (Wing && Wing->IsEquipped()))return false;
 const auto* Skin=P->GetMesh()->GetSkeletalMeshAsset();
 return Skin && Skin->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/")) && P->FindComponentByClass<UCharacterAppearanceComponent>();
}
bool UIslandOwnedAppearanceState::ApplySelected()
{
 if(!CanChange() || !OwnsPalette(SelectedPalette))return false;
 auto* Character=Cast<ACharacter>(GetOwner());auto* Mesh=Character->GetMesh();
 // The retained textured Rocketbox skin does not expose the old generic
 // vehicle Paint Tint contract. This separate native derivative preserves
 // its original albedo/normal/ORM and masks only blue/teal textile texels;
 // head and skin material slots remain untouched.
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/OwnedAppearance/M_WoodMaleOwnedTextile.M_WoodMaleOwnedTextile"));if(!Parent)return false;
 auto* Material=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
 if(!Material || Material->Parent!=Parent){Material=UMaterialInstanceDynamic::Create(Parent,GetOwner());if(!Material)return false;Mesh->SetMaterial(0,Material);}
 const FLinearColor Palette[]={FLinearColor(.08f,.22f,.24f),FLinearColor(.42f,.23f,.15f),FLinearColor(.32f,.36f,.23f),FLinearColor(.19f,.22f,.31f),FLinearColor(.52f,.46f,.32f)};
 Material->SetVectorParameterValue(TEXT("Owned Textile Tint"),Palette[SelectedPalette]);SetComponentTickEnabled(false);return true;
}
bool UIslandOwnedAppearanceState::SelectOwned(int32 Index)
{
 if(!OwnsPalette(Index) || !CanChange())return false;
 const int32 Before=SelectedPalette;SelectedPalette=Index;if(ApplySelected())return true;SelectedPalette=Before;return false;
}
bool UIslandOwnedAppearanceState::SelectNextOwned()
{
 for(int32 Step=1;Step<=5;++Step){const int32 Index=(SelectedPalette+Step)%5;if(OwnsPalette(Index))return SelectOwned(Index);}return false;
}
void UIslandOwnedAppearanceState::RestoreState(int32 Selected,uint8 Mask)
{
 // Legacy saves receive the original five starting palettes. Unknown bits and
 // unowned selection cannot inject a new outfit or influence gear ownership.
 OwnedPaletteMask=Mask&0x1f;if(!OwnedPaletteMask)OwnedPaletteMask=1;
 SelectedPalette=OwnsPalette(Selected)?Selected:0;
 if(!OwnsPalette(SelectedPalette))for(int32 I=0;I<5;++I)if(OwnsPalette(I)){SelectedPalette=I;break;}
 if(!ApplySelected())SetComponentTickEnabled(true);
}
void UIslandOwnedAppearanceState::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{Super::TickComponent(Delta,Type,Function);ApplySelected();}
