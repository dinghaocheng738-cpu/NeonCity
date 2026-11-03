#include "VisualArtLibrary.h"
#include "Engine/SkeletalMesh.h"

bool UVisualArtLibrary::ConfigureHeroGarmentLODSettings(USkeletalMesh* Mesh)
{
 if(!Mesh || Mesh->GetLODNum()!=3)return false;
 Mesh->Modify();
 const float Fractions[]={1,.55f,.25f};const float Screens[]={1,.16f,.055f};
 for(int32 I=0;I<3;++I)
 {
  auto* LOD=Mesh->GetLODInfo(I);LOD->ScreenSize.Default=Screens[I];
  LOD->ReductionSettings.NumOfTrianglesPercentage=Fractions[I];
  LOD->ReductionSettings.bRemapMorphTargets=true;
 }
 return true;
}
