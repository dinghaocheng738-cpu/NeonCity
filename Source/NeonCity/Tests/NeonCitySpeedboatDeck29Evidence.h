#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"

namespace C923Deck29
{
inline bool Validate(UWorld* World,const FString& Output)
{
 if(!World || IFileManager::Get().FileExists(*Output))return false;
 auto* Site=ANeonCityMainlandSite::Find(World);if(!Site || !Site->SpeedboatVisualMesh)return false;
 ANeonCityMarineVehicleBase* Boat=nullptr;int32 Count=0;
 for(TActorIterator<ANeonCityMarineVehicleBase> I(World);I;++I)if(I->VehicleID==UMarineWorldSubsystem::SpeedboatID()){Boat=*I;++Count;}
 if(Count!=1 || !Boat || Boat->GetProfile()!=GetDefault<UMarineVehicleData>() || !Boat->GetHull()->IsSimulatingPhysics()
  || !Boat->GetHull()->GetUnscaledBoxExtent().Equals(Boat->GetProfile()->HullExtent,.001f))return false;
 UStaticMeshComponent* Visual=nullptr;TInlineComponentArray<UStaticMeshComponent*> Meshes(Boat);
 for(auto* C:Meshes)if(C->GetFName()==TEXT("ArtVisual"))Visual=C;
 if(!Visual || Visual->GetStaticMesh()!=Site->SpeedboatVisualMesh.Get() || GetNameSafe(Visual->GetStaticMesh())!=TEXT("SM_C923_SpeedboatDeck29")
  || Visual->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || Visual->GetAttachParent()!=Boat->GetHull()
  || !Visual->GetRelativeTransform().Equals(FTransform::Identity,.001f) || !Visual->IsVisible() || Visual->bHiddenInGame)return false;
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("actual_cold_start_appearance_pass"),true);
 J->SetStringField(TEXT("boat_id"),Boat->VehicleID.ToString());J->SetStringField(TEXT("actual_mesh"),GetPathNameSafe(Visual->GetStaticMesh()));
 J->SetStringField(TEXT("physical_profile"),GetPathNameSafe(Boat->GetProfile()));J->SetStringField(TEXT("physical_extent_cm"),Boat->GetHull()->GetUnscaledBoxExtent().ToString());
 J->SetStringField(TEXT("actual_runtime_pose"),Boat->GetActorTransform().ToString());J->SetNumberField(TEXT("visual_collision_enabled"),int32(Visual->GetCollisionEnabled()));
 J->SetNumberField(TEXT("pose_input_or_parameter_writes"),0);J->SetStringField(TEXT("scope"),TEXT("Actual production bootstrap derivative and unchanged physical profile/hull/identity. Normal near still; previous continuous gameplay/save cache remains separately attributed."));
 FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Output);
}
}
#endif
