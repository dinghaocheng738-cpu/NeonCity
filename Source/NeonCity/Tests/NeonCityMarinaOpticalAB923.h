#pragma once
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Water/CoastalOceanSurface.h"
#include "Water/CoastalWaveData.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace C923MarinaOptical
{
/** Read actual cold-start native binding; no MID parameter or actor writes. */
inline bool ValidateOwnedMaterial(UWorld* World,const FString& Output)
{
 if(!World || IFileManager::Get().FileExists(*Output))return false;
 ACoastalOceanSurface* Ocean=nullptr;ANeonCityMainlandSite* Site=nullptr;int32 Providers=0,Sites=0;
 for(TActorIterator<ACoastalOceanSurface> I(World);I;++I){Ocean=*I;++Providers;}
 for(TActorIterator<ANeonCityMainlandSite> I(World);I;++I)if(I->GetLevel()==World->PersistentLevel){Site=*I;++Sites;}
 if(Providers!=1 || Sites!=1 || !Ocean || !Ocean->IsReady() || !Site || !Site->OceanRenderingMaterial
  || Ocean->GetField()!=Site->OceanField.Get() || GetNameSafe(Ocean->GetField())!=TEXT("DA_C923_HarborWaves26"))return false;
 const auto* Surface=Ocean->FindComponentByClass<UInstancedStaticMeshComponent>();
 const auto* Material=Surface?Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0)):nullptr;
 const FString Expected=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/M_C923_MarinaOceanOptics27.M_C923_MarinaOceanOptics27");
 if(!Material || GetPathNameSafe(Site->OceanRenderingMaterial.Get())!=Expected || Material->GetMaterial()!=Site->OceanRenderingMaterial->GetMaterial())return false;
 FLinearColor Actual;if(!Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanAbsorption")),Actual)
  || !Actual.Equals(FLinearColor(.0042f,.00115f,.00075f,0),1.e-8f))return false;
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("actual_cold_start_binding_pass"),true);
 J->SetStringField(TEXT("material_parent"),GetPathNameSafe(Material->GetMaterial()));J->SetStringField(TEXT("site_rendering_material"),GetPathNameSafe(Site->OceanRenderingMaterial.Get()));
 J->SetStringField(TEXT("field"),GetPathNameSafe(Ocean->GetField()));J->SetStringField(TEXT("depth"),GetPathNameSafe(Ocean->GetField()->DepthTexture.Get()));
 J->SetNumberField(TEXT("ocean_providers"),Providers);J->SetNumberField(TEXT("material_parameter_writes"),0);
 J->SetStringField(TEXT("unchanged_base_absorption_inverse_cm"),Actual.ToString());
 J->SetStringField(TEXT("scope"),TEXT("Normal native material and production bootstrap, no diagnostic parameter writes. Graph/source proof defines smooth local mask; outside view is ordinary evidence, not a pixel equality claim."));
 FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Output);
}
/** One temporary runtime parameter, never an asset/physics change or beauty PASS. */
class FAbsorptionAB
{
public:
 ~FAbsorptionAB(){Restore();}
 bool Begin(UWorld* World,const FString& Output)
 {
  if(bBegan || !World || IFileManager::Get().FileExists(*(Output/TEXT("optical-before.json"))))return false;
  Directory=Output;int32 Providers=0;
  for(TActorIterator<ACoastalOceanSurface> I(World);I;++I){Ocean=*I;++Providers;}
  if(Providers!=1 || !Ocean.IsValid() || !Ocean->IsReady() || !Ocean->GetField())return false;
  Field=Ocean->GetField();Depth=Field->DepthTexture.Get();
  if(GetNameSafe(Field)!=TEXT("DA_C923_HarborWaves26") || GetNameSafe(Depth)!=TEXT("T_C923_HarborDepth26"))return false;
  Surface=Ocean->FindComponentByClass<UInstancedStaticMeshComponent>();
  Material=Surface.IsValid()?Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0)):nullptr;
  if(!Material.IsValid() || !Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanAbsorption")),Original))return false;
  Parent=Material->GetMaterial();
  if(!Save(TEXT("optical-before.json"),false))return false;
  // Coefficients are inverse centimetres. Candidate is an art hypothesis,
  // not a measured harbor medium. Original alpha channel remains exact.
  Candidate=FLinearColor(.0042f,.0024f,.0015f,Original.A);
  bBegan=true;Material->SetVectorParameterValue(TEXT("OceanAbsorption"),Candidate);++Writes;
  FLinearColor Actual;
  return UnchangedProvider() && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanAbsorption")),Actual)
   && Actual.Equals(Candidate,1.e-8f) && Save(TEXT("optical-candidate.json"),false);
 }
 bool Restore()
 {
  if(!bBegan || bRestored)return !bBegan || bRestorePass;
  bRestored=true;
  if(Material.IsValid())
  {
   Material->SetVectorParameterValue(TEXT("OceanAbsorption"),Original);++Restores;
   FLinearColor Actual;bRestorePass=UnchangedProvider() && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanAbsorption")),Actual)
    && Actual==Original && Save(TEXT("optical-restored.json"),true);
  }
  return bRestorePass;
 }
 int32 GetWrites()const{return Writes;}
 int32 GetRestores()const{return Restores;}
private:
 bool UnchangedProvider()const
 {
  return Ocean.IsValid() && Ocean->GetField()==Field && Field && Field->DepthTexture.Get()==Depth
   && Surface.IsValid() && Material.IsValid() && Surface->GetMaterial(0)==Material.Get() && Material->GetMaterial()==Parent;
 }
 bool Save(const TCHAR* Name,bool Restored)
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("Diagnostic runtime MID absorption only; all geometry, material parent, field/depth, physics, controls, quality and native assets preserved. No production beauty/acceptance/performance claim."));
  J->SetBoolField(TEXT("eligible_as_visual_acceptance"),false);J->SetBoolField(TEXT("restore_attempt"),Restored);
  J->SetStringField(TEXT("ocean"),GetPathNameSafe(Ocean.Get()));J->SetStringField(TEXT("field"),GetPathNameSafe(Field));J->SetStringField(TEXT("depth"),GetPathNameSafe(Depth));
  J->SetStringField(TEXT("material"),GetPathNameSafe(Material.Get()));J->SetStringField(TEXT("parent"),GetPathNameSafe(Parent));
  J->SetStringField(TEXT("original_inverse_cm"),Original.ToString());J->SetStringField(TEXT("candidate_inverse_cm"),Candidate.ToString());
  FLinearColor Actual;const bool Read=Material.IsValid() && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("OceanAbsorption")),Actual);
  J->SetBoolField(TEXT("actual_parameter_read"),Read);J->SetStringField(TEXT("actual_inverse_cm"),Actual.ToString());
  J->SetNumberField(TEXT("temporary_parameter_writes"),Writes);J->SetNumberField(TEXT("restore_parameter_writes"),Restores);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Directory,true);
  const FString File=Directory/Name;return !IFileManager::Get().FileExists(*File) && FFileHelper::SaveStringToFile(Text,*File);
 }
 FString Directory;TWeakObjectPtr<ACoastalOceanSurface> Ocean;TWeakObjectPtr<UInstancedStaticMeshComponent> Surface;TWeakObjectPtr<UMaterialInstanceDynamic> Material;
 const UCoastalWaveData* Field=nullptr;UTexture2D* Depth=nullptr;UMaterial* Parent=nullptr;
 FLinearColor Original=FLinearColor::Black,Candidate=FLinearColor::Black;int32 Writes=0,Restores=0;
 bool bBegan=false,bRestored=false,bRestorePass=false;
};
}
#endif
