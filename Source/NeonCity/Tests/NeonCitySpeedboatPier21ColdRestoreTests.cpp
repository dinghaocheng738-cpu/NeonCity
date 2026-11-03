#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace SpeedboatPier21Cold
{
// UE5.8's GenericPlatform SHA256 entry is deliberately unimplemented on
// Windows. Use the standard OS SHA256 provider, confined to this test; no
// replacement crypto algorithm, production dependency or external process.
class FReadOnlySHA256
{
public:
    FReadOnlySHA256(const FReadOnlySHA256&)=delete;
    FReadOnlySHA256& operator=(const FReadOnlySHA256&)=delete;
    FReadOnlySHA256()
    {
#if PLATFORM_WINDOWS
        Library=FPlatformProcess::GetDllHandle(TEXT("bcrypt.dll"));
        if(Library)
        {
            Open=reinterpret_cast<FOpen>(FPlatformProcess::GetDllExport(Library,TEXT("BCryptOpenAlgorithmProvider")));
            Hash=reinterpret_cast<FHash>(FPlatformProcess::GetDllExport(Library,TEXT("BCryptHash")));
            Close=reinterpret_cast<FClose>(FPlatformProcess::GetDllExport(Library,TEXT("BCryptCloseAlgorithmProvider")));
            if(Open && Hash && Close && Open(&Provider,TEXT("SHA256"),nullptr,0)<0)Provider=nullptr;
        }
#endif
    }
    ~FReadOnlySHA256()
    {
#if PLATFORM_WINDOWS
        if(Provider && Close)Close(Provider,0);
#endif
        if(Library)FPlatformProcess::FreeDllHandle(Library);
    }
    bool File(const FString& Path,FString& Out)const
    {
#if PLATFORM_WINDOWS
        TArray64<uint8> Bytes;uint8 Digest[32];
        if(!Provider || !Hash || !FFileHelper::LoadFileToArray(Bytes,*Path) || uint64(Bytes.Num())>MAX_uint32
            || Hash(Provider,nullptr,0,Bytes.GetData(),uint32(Bytes.Num()),Digest,32)<0)return false;
        Out.Reset();for(uint8 Byte:Digest)Out+=FString::Printf(TEXT("%02x"),uint32(Byte));return true;
#else
        return false;
#endif
    }
private:
    void* Library=nullptr;void* Provider=nullptr;
#if PLATFORM_WINDOWS
    using FOpen=int32(__stdcall*)(void**,const wchar_t*,const wchar_t*,uint32);
    using FHash=int32(__stdcall*)(void*,uint8*,uint32,uint8*,uint32,uint8*,uint32);
    using FClose=int32(__stdcall*)(void*,uint32);
    FOpen Open=nullptr;FHash Hash=nullptr;FClose Close=nullptr;
#endif
};

bool ValidSHA(const FString& Hash)
{if(Hash.Len()!=64)return false;for(TCHAR C:Hash)if(!FChar::IsHexDigit(C))return false;return true;}

bool ReadJson(const FString& Path,TSharedPtr<FJsonObject>& Out)
{FString Text;return FFileHelper::LoadFileToString(Text,*Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out) && Out.IsValid();}

bool SafeProjectPath(const FString& Relative,FString& Absolute)
{
    if(Relative.IsEmpty() || !FPaths::IsRelative(Relative) || Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")))return false;
    Absolute=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Relative);FPaths::NormalizeFilename(Absolute);return true;
}

class FRestore final:public IAutomationLatentCommand
{
public:
    FRestore(FAutomationTestBase* InTest,const FString& InManifest,const FString& InSHA,const FString& Run)
        :Test(InTest),ManifestPath(FPaths::ConvertRelativePathToFull(InManifest)),ManifestSHA(InSHA.ToLower())
    {Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC923")/FPaths::MakeValidFileName(Run)/TEXT("SpeedboatPier21ColdRestore");}
    ~FRestore(){RestoreSlot();}
    bool Update()override
    {
        const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
        if(Now-Started>90)return Finish(TEXT("Focused cold restore exceeded90s"));
        W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
        if(!W || !PC || W->GetTimeSeconds()<5)return false;
        if(bLoaded && (!IsValid(Player) || !IsValid(Boat) || PC->GetPawn()!=Player))return Finish(TEXT("Cold restored normal player/canonical boat became unavailable during settle"));
        if(!bLoaded)
        {
            if(IFileManager::Get().FileExists(*(Directory/TEXT("restore.json"))))return Finish(TEXT("Preserve prior cold evidence; use a new run label"));
            if(!ValidateInputs())return Finish(Failure);
            Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
            if(!Player || !Save.IsValid() || !ResolveActors())return Finish(TEXT("Actual normal player, canonical speedboat and exactly one Pier21 are required"));
            if(!CapsuleUnchanged())return Finish(TEXT("Actual35/90cm player capsule required without test resizing"));
            OriginalSlot=Save->GetSlotName();
            if(!Save->ConfigureSlot(Slot,false) || !Save->DoesSaveExist())return Finish(TEXT("Retained genuine producer save is missing; never create a synthetic substitute"));
            TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());
            if(!CheckModel(Model.Get()))return Finish(Failure);bRead=true;
            // The only state restoration performed here is the existing
            // production LoadGame. No test pose/velocity/collision writes.
            if(!Save->LoadGame() || Save->GetLastResult()!=ENeonCitySaveResult::Success || Save->IsOperationInProgress())return Finish(TEXT("Production LoadGame was not an immediate successful native restore: ")+Save->GetLastDetail());
            bLoaded=true;LoadedAt=Now;ImmediatePlayer=Player->GetActorTransform();ImmediateBoat=Boat->GetActorTransform();
            if(!ResolveActors() || PC->GetPawn()!=Player || !LiveBoatMatches(true) || FVector::Dist2D(Player->GetActorLocation(),ExpectedPlayer.GetLocation())>3.f)
                return Finish(TEXT("Immediate production restore changed canonical saved boat state/pose or moved player XY more than3cm"));
            bImmediate=true;return false;
        }
        HeaveCSV+=FString::Printf(TEXT("%.6f,%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f\n"),
            Now-LoadedAt,W->GetTimeSeconds(),Player->GetActorLocation().X,Player->GetActorLocation().Y,Player->GetActorLocation().Z,
            Boat->GetActorLocation().X,Boat->GetActorLocation().Y,Boat->GetActorLocation().Z,Boat->GetActorRotation().Pitch,Boat->GetActorRotation().Yaw,Boat->GetActorRotation().Roll);
        if(Now-LoadedAt<2.5)return false;
        SettledPlayer=Player->GetActorTransform();SettledBoat=Boat->GetActorTransform();
        if(!ResolveActors() || !CapsuleUnchanged() || !LiveBoatMatches(false) || !ActualPierFloor())return Finish(Failure.IsEmpty()?TEXT("Settled restore lost actual canonical boat or35/90 on-foot pier support"):Failure);
        if(FVector::Dist2D(Player->GetActorLocation(),ExpectedPlayer.GetLocation())>3.f)return Finish(TEXT("Settled restored player XY exceeds3cm; do not hide safe-restore fallback by teleporting test pawn"));
        bSettled=true;return Finish(FString());
    }
private:
    bool ValidateInputs()
    {
        FString Actual;
        if(!Hash.File(ManifestPath,Actual) || Actual!=ManifestSHA || !ReadJson(ManifestPath,Manifest))return Bad(TEXT("Actual sealed manifest SHA256/JSON mismatch"));
        FString Status,ExpectedWorld,ProducerPath,ProducerSHA,PreflightPath,PreflightSHA;
        double ProducerPID=0;bool Exited=false;
        if(!Manifest->TryGetStringField(TEXT("status"),Status) || Status!=TEXT("C923_PIER21_REAL_PRODUCER_SEALED_CONSUMER_PENDING")
            || !Manifest->TryGetStringField(TEXT("world_package"),ExpectedWorld) || ExpectedWorld!=TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")
            || W->GetOutermost()->GetName()!=ExpectedWorld || !Manifest->TryGetStringField(TEXT("save_slot"),Slot)
            || !Manifest->TryGetNumberField(TEXT("producer_process_id"),ProducerPID) || ProducerPID<=0 || ProducerPID>MAX_uint32
            || uint32(ProducerPID)==FPlatformProcess::GetCurrentProcessId() || !Manifest->TryGetBoolField(TEXT("producer_process_exit_verified"),Exited) || !Exited
            || !Manifest->TryGetStringField(TEXT("producer_receipt"),ProducerPath) || !Manifest->TryGetStringField(TEXT("producer_receipt_sha256"),ProducerSHA)
            || !Manifest->TryGetStringField(TEXT("preflight"),PreflightPath) || !Manifest->TryGetStringField(TEXT("preflight_sha256"),PreflightSHA))return Bad(TEXT("Sealed real producer/new OS PID/current map inputs are incomplete"));
        ProducerProcessID=uint32(ProducerPID);
        if(!Slot.StartsWith(TEXT("NeonCity_Test_C923_Pier21_")) || Slot.Len()>64 || Slot.EndsWith(TEXT("_Backup")) || Slot.EndsWith(TEXT("_Pending")))return Bad(TEXT("Only the genuine dedicated Pier21 slot may be read"));
        for(TCHAR C:Slot)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return Bad(TEXT("Invalid isolated slot"));
        if(!SafeProjectPath(ProducerPath,ProducerAbsolute) || !ValidSHA(ProducerSHA) || !Hash.File(ProducerAbsolute,Actual) || Actual!=ProducerSHA.ToLower() || !ReadJson(ProducerAbsolute,Producer)
            || !SafeProjectPath(PreflightPath,PreflightAbsolute) || !ValidSHA(PreflightSHA) || !Hash.File(PreflightAbsolute,Actual) || Actual!=PreflightSHA.ToLower())return Bad(TEXT("Actual producer/preflight byte provenance differs from sealed SHA256"));
        bool Passed=false;double ActualPID=0;FString ProducerSlot;
        if(!Producer->TryGetBoolField(TEXT("passed"),Passed) || !Passed || !Producer->TryGetNumberField(TEXT("process_id"),ActualPID) || ActualPID!=ProducerPID
            || !Producer->TryGetStringField(TEXT("save_slot"),ProducerSlot) || ProducerSlot!=Slot)return Bad(TEXT("Actual producer receipt is not the specified successful process/slot"));
        for(const TCHAR* Flag:{TEXT("normal_prompt"),TEXT("normal_F_board"),TEXT("normal_F_actual_pier_exit"),TEXT("normal_F6_genuine_landing_save"),TEXT("continuous_walk_back_quay"),TEXT("original_slot_selection_restored")})
            if(!Producer->TryGetBoolField(Flag,Passed) || !Passed)return Bad(FString(TEXT("Producer normal gameplay flag failed: "))+Flag);
        if(!VerifyPins() || !VerifySaves())return false;bInputs=true;return true;
    }
    bool VerifyPins()
    {
        const TArray<TSharedPtr<FJsonValue>>* Pins=nullptr;
        if(!Manifest.IsValid() || !Manifest->TryGetArrayField(TEXT("file_pins"),Pins) || !Pins || Pins->Num()<8)return Bad(TEXT("Required producer map/modules/ledger/native/source pins missing"));
        TSet<FString> Seen;int32 Modules=0;bool Map=false,Ledger=false,Native=false;
        for(const auto& Value:*Pins)
        {
            const TSharedPtr<FJsonObject>* Row=nullptr;FString Relative,Expected,Absolute,Actual;
            if(!Value.IsValid() || !Value->TryGetObject(Row) || !Row || !(*Row)->TryGetStringField(TEXT("path"),Relative) || !(*Row)->TryGetStringField(TEXT("sha256"),Expected)
                || Seen.Contains(Relative) || !SafeProjectPath(Relative,Absolute) || !ValidSHA(Expected) || !Hash.File(Absolute,Actual) || Actual!=Expected.ToLower())return Bad(TEXT("Pinned actual file changed/missing: ")+Relative);
            Seen.Add(Relative);
            if(Relative.StartsWith(TEXT("Binaries/Win64/UnrealEditor-NeonCity")) && Relative.EndsWith(TEXT(".dll")))++Modules;
            Map|=Relative==TEXT("Content/NeonCity/Maps/NeonCity_SouthBeach_Test.umap");
            Ledger|=Relative.EndsWith(TEXT("/Native/packages.json"));Native|=Relative.EndsWith(TEXT("/receipt.json"));
        }
        return Modules==3 && Map && Ledger && Native?true:Bad(TEXT("Actual current map/three editor modules/native receipt/ledger are mandatory"));
    }
    bool VerifySaves()
    {
        const TSharedPtr<FJsonObject>* Expected=nullptr;
        if(!Manifest.IsValid() || !Manifest->TryGetObjectField(TEXT("saved_files_sha256"),Expected) || !Expected)return Bad(TEXT("Full saved-files byte snapshot missing"));
        TArray<FString> Files;IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")),TEXT("*"),true,false);
        if(Files.Num()!=(*Expected)->Values.Num())return Bad(TEXT("Saved-file set changed; preserve genuine generations and all human saves"));
        for(FString File:Files)
        {
            File=FPaths::ConvertRelativePathToFull(File);FPaths::NormalizeFilename(File);FString Relative=File;
            FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Root);Root+=TEXT("/");
            if(!FPaths::MakePathRelativeTo(Relative,*Root))return Bad(TEXT("Unable to resolve native save path"));
            FString ExpectedSHA,Actual;
            if(!(*Expected)->TryGetStringField(Relative,ExpectedSHA) || !Hash.File(File,Actual) || Actual!=ExpectedSHA.ToLower())return Bad(TEXT("Input generation or human save bytes changed: ")+Relative);
        }
        const TSharedPtr<FJsonObject>* Generations=nullptr;
        if(!Manifest->TryGetObjectField(TEXT("save_generations_sha256"),Generations) || !Generations || (*Generations)->Values.Num()!=3)return Bad(TEXT("All three native generation presence/hash inputs required"));
        for(const FString& Suffix:{FString(),FString(TEXT("_Backup")),FString(TEXT("_Pending"))})
        {
            const FString Relative=TEXT("Saved/SaveGames/")+Slot+Suffix+TEXT(".sav");const auto Value=(*Generations)->TryGetField(Relative);FString Absolute;
            if(!Value || !SafeProjectPath(Relative,Absolute))return Bad(TEXT("Invalid native generation path"));
            if(Value->IsNull()){if(IFileManager::Get().FileExists(*Absolute))return Bad(TEXT("Previously absent save generation was created"));}
            else{FString ExpectedSHA,Actual;if(!Value->TryGetString(ExpectedSHA) || !Hash.File(Absolute,Actual) || Actual!=ExpectedSHA.ToLower())return Bad(TEXT("Native generation bytes differ from real producer"));}
        }
        return true;
    }
    bool CheckModel(const UNeonCitySaveGame* Model)
    {
        FString SaveID,PlayerPose,BoatPose,BoatID,Profile,ManifestID;double Condition=0,Version=-1;
        if(!Model || !Model->HasPlayer || !Model->HasMarine || !Producer->TryGetStringField(TEXT("save_id"),SaveID)
            || !Manifest->TryGetStringField(TEXT("save_id"),ManifestID) || SaveID!=ManifestID || Model->SaveID.ToString()!=SaveID
            || !Producer->TryGetStringField(TEXT("saved_player_pose"),PlayerPose) || Model->Player.Transform.ToHumanReadableString()!=PlayerPose
            || !Producer->TryGetStringField(TEXT("saved_speedboat_pose"),BoatPose) || !Producer->TryGetStringField(TEXT("saved_speedboat_id"),BoatID)
            || BoatID!=UMarineWorldSubsystem::SpeedboatID().ToString() || !Producer->TryGetStringField(TEXT("saved_speedboat_profile"),Profile)
            || !Producer->TryGetNumberField(TEXT("saved_speedboat_condition"),Condition) || !Producer->TryGetNumberField(TEXT("saved_speedboat_authored_berth_version"),Version))return Bad(TEXT("Read model does not match genuine producer SaveID/player/boat metadata"));
        int32 Count=0;for(const auto& Record:Model->Marine)if(Record.VehicleID==UMarineWorldSubsystem::SpeedboatID()){ExpectedBoat=Record;++Count;}
        if(Count!=1 || !ExpectedBoat.Owned || ExpectedBoat.DataAssetID.ToString()!=Profile || ExpectedBoat.Transform.ToHumanReadableString()!=BoatPose
            || !FMath::IsFinite(ExpectedBoat.Condition) || FMath::Abs(double(ExpectedBoat.Condition)-Condition)>1.e-6 || ExpectedBoat.AuthoredBerthVersion!=Version)return Bad(TEXT("Actual saved canonical boat identity/profile/ownership/condition/version/pose differs from producer"));
        ExpectedPlayer=Model->Player.Transform;ActualSaveID=Model->SaveID.ToString();return true;
    }
    bool ResolveActors()
    {
        int32 Boats=0,Piers=0;
        for(TActorIterator<ANeonCityMarineVehicleBase> I(W);I;++I)if(I->VehicleID==UMarineWorldSubsystem::SpeedboatID()){Boat=*I;++Boats;}
        for(TActorIterator<AStaticMeshActor> I(W);I;++I)
        {const auto* C=I->GetStaticMeshComponent();const auto* Mesh=C->GetStaticMesh().Get();if(Mesh && Mesh->GetName()==TEXT("SM_C923_SpeedboatPier21")){Pier=*I;++Piers;}}
        return Boats==1 && Piers==1 && Boat && Boat->GetProfile() && Boat->GetMarineInteraction() && Boat->GetMarineMovement() && Boat->GetHealthComponent();
    }
    bool CapsuleUnchanged()const
    {const auto* C=Player?Player->GetCapsuleComponent():nullptr;return C && C->GetCollisionProfileName()==FName(TEXT("Pawn")) && FMath::IsNearlyEqual(C->GetScaledCapsuleRadius(),35.f,.01f) && FMath::IsNearlyEqual(C->GetScaledCapsuleHalfHeight(),90.f,.01f);}
    bool LiveBoatMatches(bool ExactPose)
    {
        const auto Records=W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot();int32 Count=0;bool Match=false;
        for(const auto& R:Records)if(R.VehicleID==ExpectedBoat.VehicleID)
        {++Count;Match=R.DataAssetID==ExpectedBoat.DataAssetID && R.Owned==ExpectedBoat.Owned && R.AuthoredBerthVersion==ExpectedBoat.AuthoredBerthVersion && FMath::Abs(R.Condition-ExpectedBoat.Condition)<1.e-5
            && (!ExactPose || R.Transform.Equals(ExpectedBoat.Transform,.01f));}
        return Count==1 && Match && Boat->Owned && Boat->GetProfile()->VehicleType==ExpectedBoat.DataAssetID && !Boat->GetMarineInteraction()->GetDriver() && !Boat->GetMarineMovement()->IsEngineRunning();
    }
    bool ActualPierFloor()
    {
        const auto* Move=Player->GetCharacterMovement();const auto* Capsule=Player->GetCapsuleComponent();
        if(PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision() || !Move->IsMovingOnGround()
            || !Move->CurrentFloor.IsWalkableFloor() || Move->CurrentFloor.HitResult.GetActor()!=Pier
            || !Move->CurrentFloor.HitResult.GetComponent() || Move->CurrentFloor.HitResult.GetComponent()->GetCollisionObjectType()!=ECC_WorldStatic)return Bad(TEXT("Normal visible on-foot restore is not supported by actual static Pier21; Hull/water/fallback disallowed"));
        FCollisionQueryParams Q(SCENE_QUERY_STAT(Pier21ColdRestoreCapsule),false,Player);FHitResult Floor;const FVector Center=Player->GetActorLocation();
        const FCollisionShape Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
        if(!W->SweepSingleByProfile(Floor,Center+FVector(0,0,2),Center-FVector(0,0,10),FQuat::Identity,Capsule->GetCollisionProfileName(),Shape,Q)
            || Floor.bStartPenetrating || Floor.GetActor()!=Pier || Floor.ImpactNormal.Z<Move->GetWalkableFloorZ()
            || W->OverlapBlockingTestByProfile(Center,FQuat::Identity,Capsule->GetCollisionProfileName(),Shape,Q))return Bad(TEXT("Full unchanged Pawn-profile capsule lacks clear actual Pier21 support/occupancy"));
        FloorActor=Floor.GetActor()->GetPathName();FloorComponent=Floor.GetComponent()?Floor.GetComponent()->GetPathName():FString();FloorImpact=Floor.ImpactPoint;FloorNormal=Floor.ImpactNormal;return true;
    }
    bool Bad(const FString& Error){Failure=Error;return false;}
    bool RestoreSlot()
    {
        if(bSlotRestored)return true;
        if(!Save.IsValid() || OriginalSlot.IsEmpty())return true;
        bSlotRestored=Save->ConfigureSlot(OriginalSlot,false) && Save->GetSlotName()==OriginalSlot;return bSlotRestored;
    }
    bool Finish(const FString& Error)
    {
        if(!Error.IsEmpty())Test->AddError(Error);
        const bool Pins=bInputs && VerifyPins(),Bytes=bInputs && VerifySaves(),Restored=RestoreSlot();
        if(bInputs && !Pins)Test->AddError(TEXT("Immutable map/module/ledger/native pins changed: ")+Failure);
        if(bInputs && !Bytes)Test->AddError(TEXT("Input generations/human saves changed: ")+Failure);
        if(!Restored)Test->AddError(TEXT("Original save-slot selection failed to restore without saving"));
        const bool Passed=Error.IsEmpty() && bInputs && bRead && bLoaded && bImmediate && bSettled && Pins && Bytes && Restored && !Test->HasAnyErrors();
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Error);
        J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetNumberField(TEXT("producer_process_id"),ProducerProcessID);
        J->SetStringField(TEXT("manifest"),ManifestPath);J->SetStringField(TEXT("manifest_sha256"),ManifestSHA);J->SetStringField(TEXT("producer_receipt"),ProducerAbsolute);
        J->SetStringField(TEXT("save_slot"),Slot);J->SetStringField(TEXT("save_id"),ActualSaveID);J->SetBoolField(TEXT("native_read_matches_real_producer"),bRead);
        J->SetBoolField(TEXT("production_load_success"),bLoaded);J->SetBoolField(TEXT("immediate_saved_boat_state_and_pose_match"),bImmediate);J->SetBoolField(TEXT("settled_actual_pier_full_capsule_support"),bSettled);
        J->SetBoolField(TEXT("input_and_all_human_save_bytes_unchanged"),Bytes);J->SetBoolField(TEXT("map_modules_ledger_and_native_assets_unchanged"),Pins);J->SetBoolField(TEXT("original_slot_selection_restored"),Restored);
        J->SetStringField(TEXT("saved_player_pose"),ExpectedPlayer.ToHumanReadableString());J->SetStringField(TEXT("saved_speedboat_pose"),ExpectedBoat.Transform.ToHumanReadableString());
        J->SetStringField(TEXT("immediate_player_pose"),ImmediatePlayer.ToHumanReadableString());J->SetStringField(TEXT("immediate_speedboat_pose"),ImmediateBoat.ToHumanReadableString());
        J->SetStringField(TEXT("settled_player_pose"),SettledPlayer.ToHumanReadableString());J->SetStringField(TEXT("settled_speedboat_pose"),SettledBoat.ToHumanReadableString());
        J->SetNumberField(TEXT("settled_player_xy_error_cm"),FVector::Dist2D(SettledPlayer.GetLocation(),ExpectedPlayer.GetLocation()));
        J->SetNumberField(TEXT("settled_speedboat_heave_delta_cm"),SettledBoat.GetLocation().Z-ExpectedBoat.Transform.GetLocation().Z);
        J->SetStringField(TEXT("actual_floor_actor"),FloorActor);J->SetStringField(TEXT("actual_floor_component"),FloorComponent);J->SetStringField(TEXT("actual_floor_impact_cm"),FloorImpact.ToString());J->SetStringField(TEXT("actual_floor_normal"),FloorNormal.ToString());
        J->SetNumberField(TEXT("actual_capsule_radius_cm"),Player?Player->GetCapsuleComponent()->GetScaledCapsuleRadius():0);J->SetNumberField(TEXT("actual_capsule_half_height_cm"),Player?Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():0);
        J->SetNumberField(TEXT("test_direct_pose_velocity_collision_writes"),0);J->SetNumberField(TEXT("test_save_writes"),0);J->SetNumberField(TEXT("production_load_calls"),bLoaded?1:0);
        J->SetStringField(TEXT("scope"),TEXT("One real new OS process; actual sealed successful producer/SaveID/generation SHA256; existing ConfigureSlot(false), ReadSaveForInspection and production LoadGame only. Immediate exact saved canonical boat state, settled35/90cm actual static Pier21 full-capsule floor and player XY<=3cm; natural settled heave recorded. No sea cruise, repeated walk, synthetic save, direct pose/velocity/collision writes or subjective visual PASS."));
        IFileManager::Get().MakeDirectory(*Directory,true);
        if(IFileManager::Get().FileExists(*(Directory/TEXT("restore.json"))) || IFileManager::Get().FileExists(*(Directory/TEXT("settled-heave.csv")))){Test->AddError(TEXT("Existing cold evidence must be preserved"));return true;}
        FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
        if(!FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("restore.json"))) || !FFileHelper::SaveStringToFile(HeaveCSV,*(Directory/TEXT("settled-heave.csv"))))Test->AddError(TEXT("Actual cold evidence could not be saved"));return true;
    }
    FAutomationTestBase* Test;FReadOnlySHA256 Hash;FString ManifestPath,ManifestSHA,Directory,Slot,OriginalSlot,Failure,ProducerAbsolute,PreflightAbsolute,ActualSaveID,FloorActor,FloorComponent;
    FString HeaveCSV=TEXT("settle_wall_seconds,game_seconds,player_x_cm,player_y_cm,player_z_cm,boat_x_cm,boat_y_cm,boat_z_cm,boat_pitch,boat_yaw,boat_roll\n");
    TSharedPtr<FJsonObject> Manifest,Producer;TWeakObjectPtr<UNeonCitySaveSubsystem> Save;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityMarineVehicleBase* Boat=nullptr;AStaticMeshActor* Pier=nullptr;
    FNeonCityMarineSaveData ExpectedBoat;FTransform ExpectedPlayer=FTransform::Identity,ImmediatePlayer=FTransform::Identity,ImmediateBoat=FTransform::Identity,SettledPlayer=FTransform::Identity,SettledBoat=FTransform::Identity;
    FVector FloorImpact=FVector::ZeroVector,FloorNormal=FVector::ZeroVector;double Started=0,LoadedAt=0;uint32 ProducerProcessID=0;bool bInputs=false,bRead=false,bLoaded=false,bImmediate=false,bSettled=false,bSlotRestored=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpeedboatPier21ColdRestore923,"NeonCity.Phase205c.MainlandC923.Standalone.SpeedboatPier21ColdRestore",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSpeedboatPier21ColdRestore923::RunTest(const FString&)
{
    FString Manifest,SHA,Run;
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))
        || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityC923Pier21RestoreManifest="),Manifest) || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityC923Pier21RestoreManifestSHA256="),SHA)
        || !SpeedboatPier21Cold::ValidSHA(SHA) || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run) || Run.IsEmpty())
    {AddError(TEXT("Requires isolated standalone/automatic persistence disabled, unique ArtRun and actual sealed producer manifest path+SHA256; no generated future values"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(SpeedboatPier21Cold::FRestore(this,Manifest,SHA,Run));return true;
}
#endif
