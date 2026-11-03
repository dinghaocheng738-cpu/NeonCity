#pragma once
// DIAGNOSTIC B ONLY. Never use its captures as normal production/beauty evidence.
// Test-only helper for the existing fixed-camera view test. No actor transform,
// visibility, material, collision, physics, input, CVar, asset or save writes.
// Its one intentional runtime change is SetCastShadow(false) on actual current
// main-map marina boat mesh shadow casters; every original CastShadow is restored.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace C923MarinaWaterAttribution
{
class FBoatShadowAB1
{
    struct FState
    {
        TWeakObjectPtr<UMeshComponent> Component;
        FString Path;
        bool CastShadow=false;
    };
    TArray<FState> States;
    TSharedPtr<FJsonObject> Receipt=MakeShared<FJsonObject>();
    FString ReceiptFile;
    bool Applied=false,Restored=false;
    int32 DisableWrites=0,RestoreWrites=0;
    bool Persist()const
    {
        if(ReceiptFile.IsEmpty())return false;FString Json;
        if(FJsonSerializer::Serialize(Receipt.ToSharedRef(),TJsonWriterFactory<>::Create(&Json)))
            return FFileHelper::SaveStringToFile(Json,*ReceiptFile);
        return false;
    }
public:
    FBoatShadowAB1()=default;
    FBoatShadowAB1(const FBoatShadowAB1&)=delete;
    FBoatShadowAB1& operator=(const FBoatShadowAB1&)=delete;
    ~FBoatShadowAB1(){Restore();}
    int32 GetDisableWrites()const{return DisableWrites;}
    int32 GetRestoreWrites()const{return RestoreWrites;}
    bool Begin(UWorld* W,const FString& UniqueDirectory)
    {
        if(Applied || !W || !States.IsEmpty())return false;
        ReceiptFile=UniqueDirectory/TEXT("diagnostic-boat-shadow-ab.json");
        if(IFileManager::Get().FileExists(*ReceiptFile)){ReceiptFile.Reset();return false;}
        const FBox Region(FVector(-9000,-9000,-5000),FVector(6000,3000,5000));
        int32 Boats=0;TArray<TSharedPtr<FJsonValue>> Casters;
        for(TActorIterator<ANeonCityMotorYacht> I(W);I;++I)
        {
            if(I->GetLevel()!=W->PersistentLevel || !Region.IsInsideOrOn(I->GetActorLocation()))continue;
            ++Boats;TInlineComponentArray<UMeshComponent*> Meshes(*I);
            for(UMeshComponent* C:Meshes)
            {
                if(!C || !C->CastShadow || !C->IsRegistered())continue;
                if((I->IsHidden() || !C->IsVisible() || C->bHiddenInGame) && !C->bCastHiddenShadow)continue;
                States.Add({C,C->GetPathName(),bool(C->CastShadow)});
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),I->GetPathName());
                J->SetStringField(TEXT("component"),C->GetPathName());J->SetBoolField(TEXT("original_cast_shadow"),C->CastShadow);
                J->SetBoolField(TEXT("original_cast_hidden_shadow_unchanged"),C->bCastHiddenShadow);
                J->SetStringField(TEXT("original_pose_observation_only"),I->GetActorTransform().ToString());
                Casters.Add(MakeShared<FJsonValueObject>(J));
            }
        }
        Receipt->SetStringField(TEXT("scope"),TEXT("DIAGNOSTIC B ONLY: same camera/hour/normal quality, only actual two main marina boats' runtime mesh CastShadow disabled. Never production or visual PASS. No actor pose/physics/input/material/visibility/collision/CVar/asset/save writes. Destructor and explicit Restore restore original shadow flags."));
        Receipt->SetNumberField(TEXT("actual_marina_boats"),Boats);Receipt->SetArrayField(TEXT("actual_casters"),Casters);
        if(Boats!=2 || States.IsEmpty())
        {
            Receipt->SetStringField(TEXT("status"),TEXT("REFUSED_NO_MUTATION_EXPECT_EXACT_TWO_ACTUAL_MARINA_BOATS_AND_SHADOW_CASTER"));
            IFileManager::Get().MakeDirectory(*UniqueDirectory,true);Persist();States.Reset();return false;
        }
        if(!IFileManager::Get().MakeDirectory(*UniqueDirectory,true)){States.Reset();return false;}
        Receipt->SetStringField(TEXT("status"),TEXT("DIAGNOSTIC_PREPARED_NOT_YET_APPLIED"));
        if(!Persist()){States.Reset();return false;}
        Applied=true;
        for(const FState& S:States)if(S.Component.IsValid()){S.Component->SetCastShadow(false);++DisableWrites;}
        Receipt->SetNumberField(TEXT("actual_disable_writes"),DisableWrites);
        Receipt->SetStringField(TEXT("status"),TEXT("DIAGNOSTIC_B_RUNTIME_BOAT_SHADOWS_DISABLED_RESTORE_REQUIRED"));
        if(!Persist()){Restore();return false;}return true;
    }
    bool Restore()
    {
        if(!Applied)return true;if(Restored)return Receipt->GetBoolField(TEXT("all_original_cast_shadow_restored"));
        bool Valid=true;TArray<TSharedPtr<FJsonValue>> Results;
        for(const FState& S:States)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("component"),S.Path);
            const bool Exists=S.Component.IsValid();J->SetBoolField(TEXT("component_still_exists"),Exists);
            if(Exists){S.Component->SetCastShadow(S.CastShadow);++RestoreWrites;J->SetBoolField(TEXT("restored_cast_shadow"),S.Component->CastShadow);Valid&=bool(S.Component->CastShadow)==S.CastShadow;}
            else Valid=false;Results.Add(MakeShared<FJsonValueObject>(J));
        }
        Restored=true;Receipt->SetArrayField(TEXT("restore_results"),Results);Receipt->SetBoolField(TEXT("all_original_cast_shadow_restored"),Valid);
        Receipt->SetNumberField(TEXT("actual_restore_writes"),RestoreWrites);
        Receipt->SetStringField(TEXT("status"),Valid?TEXT("DIAGNOSTIC_COMPLETE_OR_ABORTED_ALL_BOAT_SHADOW_FLAGS_RESTORED"):TEXT("DIAGNOSTIC_WORLD_OR_COMPONENT_ENDED_BEFORE_RESTORE"));return Persist() && Valid;
    }
};
}
#endif
