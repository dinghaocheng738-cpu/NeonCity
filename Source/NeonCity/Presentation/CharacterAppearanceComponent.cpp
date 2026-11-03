#include "Presentation/CharacterAppearanceComponent.h"
#include "AI/NPCInterface.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
UCharacterAppearanceComponent::UCharacterAppearanceComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 ReferenceMale=TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/SK_NCTW_Wood_Male_01_Gameplay.SK_NCTW_Wood_Male_01_Gameplay")));
 ReferenceFemale=TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/SK_NCTW_Female_Adult_04_Gameplay.SK_NCTW_Female_Adult_04_Gameplay")));
}
void UCharacterAppearanceComponent::BeginPlay()
{
 Super::BeginPlay(); RefreshAppearance();
}
void UCharacterAppearanceComponent::RefreshAppearance()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(NeonCityRefreshCharacterAppearance);
 uint32 Palette=0;
 if(GetOwner()->Implements<UNPCInterface>()) Palette=FCrc::StrCrc32(*INPCInterface::Execute_GetNPCId(GetOwner()).ToString());
 auto* Character=Cast<ACharacter>(GetOwner());
 auto* Mesh=Character?Character->GetMesh():nullptr;
 const auto* Existing=Mesh?Mesh->GetSkeletalMeshAsset():nullptr;
 // Only the template placeholders and our managed skins opt in. User-authored
 // characters keep their appearance. All bones, animation, collision and seats
 // continue to use the same skeletal component and existing project rig.
 if(bUseReferenceCharacter && Existing && !Mesh->IsSimulatingPhysics())
 {
  const FString Path=Existing->GetPathName();
  const bool bManaged=Path.StartsWith(TEXT("/Game/Characters/Mannequins/Meshes/SKM_"))
   || Path.StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/"));
  if(bManaged)
  {
   USkeletalMesh* Skin=(Palette%2?ReferenceFemale:ReferenceMale).LoadSynchronous();
   if(Skin && Skin!=Existing && Skin->GetSkeleton()==Existing->GetSkeleton())
   {
    Mesh->EmptyOverrideMaterials();
    Mesh->SetSkeletalMesh(Skin);
   }
  }
 }
 if(bUseCoastalPalette) ApplyPalette(int32(Palette%5));
}
void UCharacterAppearanceComponent::ApplyPalette(int32 PaletteIndex)
{
 auto* Character=Cast<ACharacter>(GetOwner()); if(!Character) return;
 auto* CharacterMesh=Character->GetMesh(); if(!CharacterMesh) return;
 const FLinearColor Palette[]={FLinearColor(.08f,.22f,.24f),FLinearColor(.42f,.23f,.15f),FLinearColor(.32f,.36f,.23f),FLinearColor(.19f,.22f,.31f),FLinearColor(.52f,.46f,.32f)};
 const int32 Index=FMath::Clamp(PaletteIndex,0,4);
 for(int32 Slot=0;Slot<CharacterMesh->GetNumMaterials();++Slot)
 {
  auto* Existing=CharacterMesh->GetMaterial(Slot); if(!Existing) continue;
  FLinearColor Value;
  // Future custom characters are left alone unless they expose this material contract.
  if(!Existing->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Paint Tint")),Value)) continue;
  auto* Material=CharacterMesh->CreateAndSetMaterialInstanceDynamic(Slot); if(!Material) continue;
  Material->SetVectorParameterValue(TEXT("Paint Tint"),Slot==0?Palette[Index]:FLinearColor(.055f,.064f,.068f));
  Material->SetScalarParameterValue(TEXT("MetalPaintMetallic"),.05f);
  Material->SetScalarParameterValue(TEXT("MetalPaintRoughness"),.62f);
  Material->SetScalarParameterValue(TEXT("EmissivePower"),0);
 }
}
