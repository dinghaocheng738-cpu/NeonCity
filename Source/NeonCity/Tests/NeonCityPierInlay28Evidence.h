#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Adventure/IslandLandLight.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"

namespace C923Pier28
{
inline bool Validate(UWorld* World,const FString& Output,bool Night)
{
 if(!World || IFileManager::Get().FileExists(*Output))return false;
 const auto* Budget=World->GetSubsystem<UWorldLightingSubsystem>();if(!Budget || Budget->GetActiveLightCount()>24 || Budget->GetShadowLightCount()>6)return false;
 int32 Meshes=0,Lights=0;TArray<TSharedPtr<FJsonValue>> Rows;
 for(TActorIterator<AActor> I(World);I;++I)
 {
  if(I->GetLevel()!=World->PersistentLevel || !I->ActorHasTag(TEXT("NeonCity.MainlandC923.PierInlays28")))continue;
  if(I->IsActorTickEnabled())return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),I->GetPathName());
  if(auto* Actor=Cast<AStaticMeshActor>(*I))
  {
   auto* C=Actor->GetStaticMeshComponent();if(!C || GetNameSafe(C->GetStaticMesh())!=TEXT("SM_C923_PierInlay28") || C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || !C->IsVisible() || C->bHiddenInGame || C->CastShadow)return false;
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(C923InlayDeck),false,*I);const FVector P=I->GetActorLocation();
   if(!World->LineTraceSingleByObjectType(H,P+FVector(0,0,30),P-FVector(0,0,30),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
    || !H.GetActor() || H.GetActor()->GetFName()!=TEXT("StaticMeshActor_1656") || FMath::Abs(P.Z-H.ImpactPoint.Z)>.1f || FVector::DotProduct(I->GetActorUpVector(),H.ImpactNormal)<.999f)return false;
   ++Meshes;J->SetStringField(TEXT("real_floor"),H.GetActor()->GetPathName());J->SetNumberField(TEXT("floor_error_cm"),P.Z-H.ImpactPoint.Z);J->SetNumberField(TEXT("collision_enabled"),int32(C->GetCollisionEnabled()));
  }
  else if(auto* LightActor=Cast<AIslandLandLight>(*I))
  {
   auto* C=Cast<UPointLightComponent>(LightActor->GetLightComponent());if(!C || LightActor->Lumens!=12 || LightActor->DayMultiplier!=0 || LightActor->bBudgetShadows || LightActor->CullDistance!=6500 || C->IntensityUnits!=ELightUnits::Lumens || C->CastShadows || C->AttenuationRadius!=400)return false;
   if(Night?(!C->IsVisible() || !FMath::IsNearlyEqual(C->Intensity,12.f,.01f)):!FMath::IsNearlyZero(C->Intensity,.001f))return false;
   ++Lights;J->SetNumberField(TEXT("actual_lumens"),C->Intensity);J->SetBoolField(TEXT("actual_visible"),C->IsVisible());J->SetBoolField(TEXT("actual_shadow"),C->CastShadows);
  }
  else return false;
  Rows.Add(MakeShared<FJsonValueObject>(J));
 }
 if(Meshes!=6 || Lights!=6)return false;auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),true);J->SetBoolField(TEXT("night"),Night);J->SetArrayField(TEXT("actual_fixtures"),Rows);
 J->SetNumberField(TEXT("global_active_lights"),Budget->GetActiveLightCount());J->SetNumberField(TEXT("global_shadow_lights"),Budget->GetShadowLightCount());J->SetNumberField(TEXT("fixture_or_parameter_writes"),0);
 J->SetStringField(TEXT("scope"),TEXT("Read actual native fixture support, collision, Tick, day/night intensity and shared budget. Normal fixed-camera art evidence; no continuous route or beauty PASS claim."));
 FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Output);
}
}
#endif
