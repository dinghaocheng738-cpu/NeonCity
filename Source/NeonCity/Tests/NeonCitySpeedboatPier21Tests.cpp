#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace SpeedboatPier21
{
enum class EStage:uint8 { Initialize,Settle,WalkOut,Prompt,Board,Occupied,Exit,ExitSettle,SavePress,SaveVerify,WalkBack,Cleanup };

class FFlow:public IAutomationLatentCommand
{
public:
    FFlow(FAutomationTestBase* InTest,const FString& InSlot):Test(InTest),Slot(InSlot) {}
    ~FFlow(){ReleaseKeys();}
    bool Update()override
    {
        if(!Started)Started=FPlatformTime::Seconds();
        W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
        if(FPlatformTime::Seconds()-Started>150)return Finish(TEXT("Focused speedboat pier flow exceeded150s"));
        if(!W || !PC || W->GetTimeSeconds()<5)return false;
        const float Age=W->GetTimeSeconds()-At;
        if(Stage==EStage::Cleanup)return Cleanup();
        if(Player && Boat)
        {
            if(Boat->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.1f)return Fail(TEXT("Unchanged speedboat lost health during pier-only access"));
            const FVector P=Player->GetActorLocation(),B=Boat->GetActorLocation();
            if(PC->GetPawn()==Player && bHadPlayer)
            {
                const float Step=FVector::Dist(P,PreviousPlayer);
                const double Dt=FMath::Max(0.,double(W->GetTimeSeconds())-PreviousSampleGameSeconds);
                const auto* Move=Player->GetCharacterMovement();
                const double Allowed=FMath::Max(150.,Move->GetMaxSpeed()*Dt+.5*Move->GetMaxAcceleration()*Dt*Dt+20.);
                if(Step>Allowed)return Fail(TEXT("Continuous on-foot route exceeded the actual movement/time kinematic bound"));
                WalkDistanceCm+=FVector::Dist2D(P,PreviousPlayer);
            }
            PreviousPlayer=P;bHadPlayer=PC->GetPawn()==Player;PreviousSampleGameSeconds=W->GetTimeSeconds();
            CSV+=FString::Printf(TEXT("%.6f,%.6f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n"),
                FPlatformTime::Seconds()-Started,W->GetTimeSeconds(),int32(Stage),P.X,P.Y,P.Z,B.X,B.Y,B.Z,Boat->GetActorRotation().Yaw,
                Player->GetCharacterMovement()->IsMovingOnGround()?1:0);
        }
        if(Stage==EStage::Initialize)
        {
            Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());int32 Boats=0,Piers=0;
            for(TActorIterator<ANeonCityMarineVehicleBase> I(W);I;++I)if(I->VehicleID==UMarineWorldSubsystem::SpeedboatID()){Boat=*I;++Boats;}
            for(TActorIterator<AStaticMeshActor> I(W);I;++I)
            {
                auto* C=I->GetStaticMeshComponent();const UStaticMesh* Mesh=C->GetStaticMesh().Get();
                if(Mesh && Mesh->GetName()==TEXT("SM_C923_SpeedboatPier21")){Pier=*I;PierComponent=C;++Piers;}
            }
            if(!Player || Boats!=1 || Piers!=1 || !Boat->GetHull()->IsSimulatingPhysics())return Fail(TEXT("Normal player, exact canonical speedboat and one actual Pier21 required"));
            auto* Capsule=Player->GetCapsuleComponent();Radius=Capsule->GetScaledCapsuleRadius();Half=Capsule->GetScaledCapsuleHalfHeight();
            if(!FMath::IsNearlyEqual(Radius,35.f,.01f) || !FMath::IsNearlyEqual(Half,90.f,.01f))return Fail(TEXT("Use the actual unchanged35/90cm player capsule; do not resize for access"));
            if(Boat->GetProfile()->bWalkableDeck || Boat->GetProfile()->bPreferDeckExit)return Fail(TEXT("This is the actual base speedboat, not a substituted yacht deck profile"));
            InitialHealth=Boat->GetHealthComponent()->GetCurrentHealth();InitialBoatPose=Boat->GetActorTransform();
            FVector Low,High;PierComponent->GetLocalBounds(Low,High);
            const FTransform Pose=PierComponent->GetComponentTransform();
            if(!Pose.Equals(FTransform::Identity))return Fail(TEXT("Current Pier21 must retain its guarded native identity transform"));
            const float CenterX=Pose.TransformPosition((Low+High)*.5f).X;
            ShoreGoal=Pose.TransformPosition(FVector((Low.X+High.X)*.5f,High.Y+250,0));
            // One explicitly disclosed supported starting-quay fixture only.
            // Every subsequent approach/return uses normal movement input.
            FCollisionQueryParams Q(SCENE_QUERY_STAT(Pier21InitialCapsule),false,Player);FHitResult Support;
            if(!W->SweepSingleByChannel(Support,FVector(ShoreGoal.X,ShoreGoal.Y,500),FVector(ShoreGoal.X,ShoreGoal.Y,-200),
                FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q)
                || Support.bStartPenetrating || !Support.GetComponent() || Support.GetComponent()->GetCollisionObjectType()!=ECC_WorldStatic
                || Support.ImpactNormal.Z<Player->GetCharacterMovement()->GetWalkableFloorZ())return Fail(TEXT("Starting quay has no genuine full-capsule static support"));
            InitialPlayer=Support.Location+FVector(0,0,.5);
            if(W->OverlapBlockingTestByChannel(InitialPlayer,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))return Fail(TEXT("Starting full capsule is blocked"));
            Player->SetActorLocation(InitialPlayer,false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            PreviousPlayer=InitialPlayer;bHadPlayer=true;PreviousSampleGameSeconds=W->GetTimeSeconds();
            // Reach the actual ship-facing side using normal walking. At the
            // middle of this3.2m deck the eye-to-entry-box ray exceeds250cm,
            // even though the boat's separate420cm CanEnter gate is true.
            // Keep the unchanged full capsule12cm inside the native bounds.
            const float BoardingX=Low.X+Radius+12.f;
            Goals={FVector(CenterX,High.Y+25,0),FVector(CenterX,-3900,0),FVector(CenterX,Boat->GetActorLocation().Y,0),FVector(BoardingX,Boat->GetActorLocation().Y,0)};Goal=0;
            Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
            if(Save)OriginalSlot=Save->GetSlotName();
            if(!Save || !Save->ConfigureSlot(Slot,false) || Save->DoesSaveExist())return Fail(TEXT("A unique dedicated untouched Pier21 save slot is required"));
            ConfigureOutput();return Next(EStage::Settle);
        }
        if(Stage==EStage::Settle && Age>.8f)
        {if(!Grounded(false))return Fail(TEXT("Normal player failed to settle on starting quay"));return Next(EStage::WalkOut);}
        if(Stage==EStage::WalkOut || Stage==EStage::WalkBack)
        {
            const int32 Result=Walk();if(Result<0)return Fail(Failure);
            if(Result>0)
            {
                if(Stage==EStage::WalkBack)
                {
                    if(!Grounded(false) || FVector::Dist2D(Player->GetActorLocation(),ShoreGoal)>15)return Fail(TEXT("Continuous return did not reach the original actual quay"));
                    bWalkBack=true;return Finish(FString());
                }
                return Next(EStage::Prompt);
            }
        }
        if(Stage==EStage::Prompt && Age>.4f)
        {
            if(!Grounded(true))return Fail(TEXT("Approach ended without actual native pier support"));
            if(!Focus())return Fail(TEXT("Actual250cm normal focus cannot reach the base speedboat: ")+FocusDetails);
            if(!Boat->IsEntryPromptVisible() && Age<1)return false;
            if(!Boat->IsEntryPromptVisible())return Fail(TEXT("Normal focus has no actual F entry prompt"));
            bPrompt=true;Capture(TEXT("prompt"));Key(EKeys::F,true);return Next(EStage::Board);
        }
        if(Stage==EStage::Board && Age>.25f)
        {
            Key(EKeys::F,false);
            if(PC->GetPawn()!=Boat || Boat->GetMarineInteraction()->GetDriver()!=Player || !Boat->GetMarineMovement()->IsEngineRunning())return Fail(TEXT("Normal F did not board the exact speedboat/start its normal engine"));
            bBoard=true;return Next(EStage::Occupied);
        }
        if(Stage==EStage::Occupied && Age>.5f)
        {
            if(!Boat->IsDrivingStatusVisible())return Fail(TEXT("Normal occupied speedboat status is missing"));
            // No sea cruise, throttle or boat pose fixture: isolate new access.
            bExitPlan=Boat->GetMarineInteraction()->FindExitTransform(ExitPlan);
            const FVector OldStart=Boat->GetActorLocation()+FVector(0,0,Boat->GetProfile()->HullExtent.Z+Half+6);
            FVector BoundsStart=OldStart;BoundsStart.Z=FMath::Max(OldStart.Z,Boat->GetHull()->Bounds.GetBox().Max.Z+Half+6);
            FCollisionQueryParams Q(SCENE_QUERY_STAT(Pier21ExitStartDiagnosis),false,Player);FHitResult OldHit,BoundsHit;
            const auto Shape=FCollisionShape::MakeCapsule(Radius,Half);
            const bool OldBlocked=W->SweepSingleByChannel(OldHit,OldStart,OldStart+FVector(0,0,.01),FQuat::Identity,ECC_Pawn,Shape,Q);
            const bool BoundsBlocked=W->SweepSingleByChannel(BoundsHit,BoundsStart,BoundsStart+FVector(0,0,.01),FQuat::Identity,ECC_Pawn,Shape,Q);
            ExitStartDiagnosis=FString::Printf(TEXT("boat%s oldStart%s actualBoundsStart%s oldBlocked%d oldPenetrating%d oldActor%s boundsBlocked%d boundsActor%s"),
                *Boat->GetActorTransform().ToHumanReadableString(),*OldStart.ToString(),*BoundsStart.ToString(),OldBlocked?1:0,OldHit.bStartPenetrating?1:0,
                *GetPathNameSafe(OldHit.GetActor()),BoundsBlocked?1:0,*GetPathNameSafe(BoundsHit.GetActor()));
            Key(EKeys::F,true);return Next(EStage::Exit);
        }
        if(Stage==EStage::Exit && Age>.25f)
        {
            Key(EKeys::F,false);
            if(PC->GetPawn()!=Player || Boat->GetMarineInteraction()->GetDriver())return Fail(TEXT("Normal F could not leave the speedboat at its actual live berth"));
            return Next(EStage::ExitSettle);
        }
        if(Stage==EStage::ExitSettle && Age>.8f)
        {
            if(!Grounded(true))return Fail(TEXT("F exit landed on Hull/water/unsupported floor instead of actual Pier21"));
            bExit=true;ActualLanding=Player->GetActorTransform();Capture(TEXT("shore-exit"));
            Key(EKeys::F6,true);return Next(EStage::SavePress);
        }
        if(Stage==EStage::SavePress && Age>.25f){Key(EKeys::F6,false);return Next(EStage::SaveVerify);}
        if(Stage==EStage::SaveVerify && Age>.2f)
        {
            if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Save->DoesSaveExist())return Fail(TEXT("Actual normal F6 did not write a safe genuine pier-standing save"));
            UNeonCitySaveGame* Model=Save->ReadSaveForInspection();
            if(!Model || !Model->HasPlayer || !Model->HasMarine || FVector::Dist(Model->Player.Transform.GetLocation(),ActualLanding.GetLocation())>15)return Fail(TEXT("Native saved player location differs from the genuinely landed pier state"));
            const auto* Record=Model->Marine.FindByPredicate([](const FNeonCityMarineSaveData& S){return S.VehicleID==UMarineWorldSubsystem::SpeedboatID();});
            if(!Record || !Record->Owned || Record->Condition<=0 || Record->DataAssetID!=Boat->GetProfile()->VehicleType)return Fail(TEXT("Genuine saved canonical speedboat ID/profile/ownership/condition is invalid"));
            SavedPlayer=Model->Player.Transform;SavedBoat=Record->Transform;SavedID=Model->SaveID;SavedCondition=Record->Condition;
            SavedProfile=Record->DataAssetID;SavedBerthVersion=Record->AuthoredBerthVersion;bSave=true;
            Goals={FVector(5000,-3900,0),FVector(5000,ShoreGoal.Y-225,0),ShoreGoal};Goal=0;
            return Next(EStage::WalkBack);
        }
        return false;
    }
private:
    bool Next(EStage In){Stage=In;At=W->GetTimeSeconds();return false;}
    bool Grounded(bool RequirePier)const
    {
        if(!Player || PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision() || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
        const FFindFloorResult& Floor=Player->GetCharacterMovement()->CurrentFloor;
        return Floor.IsWalkableFloor() && Floor.HitResult.GetComponent() && Floor.HitResult.GetComponent()->GetCollisionObjectType()==ECC_WorldStatic
            && Floor.HitResult.GetActor()!=Boat && (!RequirePier || Floor.HitResult.GetActor()==Pier);
    }
    int32 Walk()
    {
        if(PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision() || Player->GetCharacterMovement()->IsSwimming())
        {Failure=TEXT("Continuous pier traversal lost visible collidable on-foot movement");return -1;}
        if(!Goals.IsValidIndex(Goal))return 1;
        const FVector Delta=Goals[Goal]-Player->GetActorLocation();
        if(Delta.Size2D()<10){Player->GetCharacterMovement()->StopMovementImmediately();++Goal;At=W->GetTimeSeconds();return Goal==Goals.Num()?1:0;}
        if(W->GetTimeSeconds()-At>20){Failure=FString::Printf(TEXT("Normal pier walk blocked at goal%d position%s target%s"),Goal,*Player->GetActorLocation().ToString(),*Goals[Goal].ToString());return -1;}
        Player->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/120),.15f,1.f));PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));return 0;
    }
    bool Focus()
    {
        FVector Eye;FRotator Rotation;Player->GetActorEyesViewPoint(Eye,Rotation);
        for(float X:{-100.f,0.f,100.f})for(float Y:{80.f,100.f})for(float Z:{130.f,170.f,210.f,240.f})
        {
            const FVector Aim=Boat->GetActorTransform().TransformPosition(FVector(X,Y,Z));PC->SetControlRotation((Aim-Eye).Rotation());
            if(Player->GetInteractionComponent()->RefreshFocusedInteractable()==Boat)return true;
        }
        FocusDetails=FString::Printf(TEXT("player%s boat%s velocity%.3f canEnter%d interactionDistance%.3f"),*Player->GetActorLocation().ToString(),*Boat->GetActorLocation().ToString(),Boat->GetVelocity().Size(),Boat->GetMarineInteraction()->CanEnter(Player)?1:0,Player->GetInteractionComponent()->InteractionDistance);return false;
    }
    void Key(FKey K,bool Down)
    {bool& Old=Keys.FindOrAdd(K);if(Old==Down || !IsValid(PC))return;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}
    void ReleaseKeys(){for(auto& Pair:Keys)if(Pair.Value)Key(Pair.Key,false);}
    bool Fail(const FString& Error)
    {
        if(Boat && Player && PC && PC->GetPawn()==Boat && Boat->GetMarineInteraction()->GetDriver()==Player)
        {PendingFailure=Error;ReleaseKeys();CleanupStarted=FPlatformTime::Seconds();return Next(EStage::Cleanup);}
        return Finish(Error);
    }
    bool Cleanup()
    {
        if(PC->GetPawn()==Player){ReleaseKeys();return Finish(PendingFailure);}
        if(FPlatformTime::Seconds()-CleanupStarted>10)return Finish(PendingFailure+TEXT("; normal F failure cleanup could not restore player; run remaining fixtures in a fresh process"));
        if(CleanupKeyAt>0)
        {if(FPlatformTime::Seconds()-CleanupKeyAt>.25)Key(EKeys::F,false);if(FPlatformTime::Seconds()-CleanupKeyAt>.75)CleanupKeyAt=0;return false;}
        FTransform Exit;
        if(Boat->GetSpeed_Implementation()<=150 && Boat->GetMarineInteraction()->FindExitTransform(Exit)){Key(EKeys::F,true);CleanupKeyAt=FPlatformTime::Seconds();}
        return false;
    }
    void ConfigureOutput()
    {
        FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
        Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC923")/FPaths::MakeValidFileName(Label)/TEXT("SpeedboatPier21");
        IFileManager::Get().MakeDirectory(*Directory,true);CSV=TEXT("wall_seconds,game_seconds,stage,player_x_cm,player_y_cm,player_z_cm,boat_x_cm,boat_y_cm,boat_z_cm,boat_yaw,grounded\n");
    }
    void Capture(const FString& Name)
    {if(!FNeonCityVisualEvidence::CaptureHUD(W,TEXT("C923-SpeedboatPier21-")+Name+TEXT(".png")))bCaptureFailed=true;}
    bool Finish(const FString& Error)
    {
        ReleaseKeys();if(!Error.IsEmpty())Test->AddError(Error);if(Directory.IsEmpty())ConfigureOutput();
        bool RestoredSlot=true;
        if(Save && !OriginalSlot.IsEmpty())RestoredSlot=Save->ConfigureSlot(OriginalSlot,false) && Save->GetSlotName()==OriginalSlot;
        if(!RestoredSlot)Test->AddError(TEXT("Original save-slot selection could not be restored; no save written to it"));
        const bool Passed=Error.IsEmpty() && bPrompt && bBoard && bExit && bSave && bWalkBack && !bCaptureFailed && RestoredSlot;
        if(Error.IsEmpty() && !Passed)Test->AddError(TEXT("Pier21 final objective flags are incomplete"));
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Error);
        J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("save_slot"),Slot);
        J->SetBoolField(TEXT("normal_prompt"),bPrompt);J->SetBoolField(TEXT("normal_F_board"),bBoard);J->SetBoolField(TEXT("normal_F_actual_pier_exit"),bExit);
        J->SetBoolField(TEXT("normal_F6_genuine_landing_save"),bSave);J->SetBoolField(TEXT("continuous_walk_back_quay"),bWalkBack);J->SetNumberField(TEXT("actual_walk_cm"),WalkDistanceCm);
        J->SetNumberField(TEXT("actual_capsule_radius_cm"),Radius);J->SetNumberField(TEXT("actual_capsule_half_height_cm"),Half);J->SetBoolField(TEXT("capture_failure"),bCaptureFailed);
        J->SetStringField(TEXT("initial_supported_quay_fixture_cm"),InitialPlayer.ToString());J->SetStringField(TEXT("initial_boat_pose"),InitialBoatPose.ToHumanReadableString());
        J->SetBoolField(TEXT("production_exit_plan_available"),bExitPlan);J->SetStringField(TEXT("production_exit_plan"),ExitPlan.ToHumanReadableString());
        J->SetStringField(TEXT("actual_pier_landing"),ActualLanding.ToHumanReadableString());J->SetStringField(TEXT("saved_player_pose"),SavedPlayer.ToHumanReadableString());
        J->SetStringField(TEXT("saved_speedboat_pose"),SavedBoat.ToHumanReadableString());J->SetStringField(TEXT("save_id"),SavedID.ToString());J->SetNumberField(TEXT("saved_speedboat_condition"),SavedCondition);
        J->SetStringField(TEXT("saved_speedboat_id"),UMarineWorldSubsystem::SpeedboatID().ToString());J->SetStringField(TEXT("saved_speedboat_profile"),SavedProfile.ToString());
        J->SetNumberField(TEXT("saved_speedboat_authored_berth_version"),SavedBerthVersion);
        J->SetStringField(TEXT("focus_diagnostics"),FocusDetails);J->SetNumberField(TEXT("boat_pose_velocity_collision_writes"),0);J->SetNumberField(TEXT("player_start_pose_fixtures"),1);
        J->SetStringField(TEXT("actual_exit_route_start_diagnosis"),ExitStartDiagnosis);
        J->SetStringField(TEXT("cross_process_restore"),TEXT("NOT_RUN_REQUIRED_SEPARATE_CONSUMER_PROCESS"));
        J->SetBoolField(TEXT("original_slot_selection_restored"),RestoredSlot);
        J->SetStringField(TEXT("scope"),TEXT("Actual35/90 capsule, one disclosed supported quay starting fixture, continuous normal movement over current pier, normal250cm focus/prompt/F board and F exit to this actual static pier, genuine F6 dedicated-slot save, continuous return to original quay. No sea cruise/yacht rerun, production input/save/profile changes, boat transform/velocity writes, hull-only exit PASS or cold-restore claim."));
        FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));
        if(IFileManager::Get().FileExists(*(Directory/TEXT("flow.json"))) || IFileManager::Get().FileExists(*(Directory/TEXT("trajectory.csv"))))
        {Test->AddError(TEXT("Preserve existing Pier21 evidence; use a new explicit run label"));return true;}
        if(!FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("flow.json"))) || !FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv"))))Test->AddError(TEXT("Pier21 evidence write failed"));
        return true;
    }
    FAutomationTestBase* Test;FString Slot,OriginalSlot,Directory,CSV,Failure,PendingFailure,FocusDetails,ExitStartDiagnosis;
    UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityMarineVehicleBase* Boat=nullptr;
    AStaticMeshActor* Pier=nullptr;UStaticMeshComponent* PierComponent=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
    EStage Stage=EStage::Initialize;double Started=0,CleanupStarted=0,CleanupKeyAt=0;float At=0,InitialHealth=0,Radius=0,Half=0,SavedCondition=0;
    double WalkDistanceCm=0,PreviousSampleGameSeconds=0;FVector ShoreGoal=FVector::ZeroVector,InitialPlayer=FVector::ZeroVector,PreviousPlayer=FVector::ZeroVector;
    FTransform InitialBoatPose=FTransform::Identity,ExitPlan=FTransform::Identity,ActualLanding=FTransform::Identity,SavedPlayer=FTransform::Identity,SavedBoat=FTransform::Identity;FGuid SavedID;FName SavedProfile;int32 SavedBerthVersion=-1;
    TArray<FVector> Goals;int32 Goal=0;TMap<FKey,bool> Keys;bool bHadPlayer=false,bPrompt=false,bBoard=false,bExit=false,bSave=false,bWalkBack=false,bExitPlan=false,bCaptureFailed=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpeedboatPier21Flow923,"NeonCity.Phase205c.MainlandC923.Standalone.SpeedboatPier21Flow",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSpeedboatPier21Flow923::RunTest(const FString&)
{
    FString Slot;
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))
        || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityC923Pier21SaveSlot="),Slot) || !Slot.StartsWith(TEXT("NeonCity_Test_C923_Pier21_"))
        || Slot.Len()<=FCString::Strlen(TEXT("NeonCity_Test_C923_Pier21_")) || Slot.Len()>64)
    {AddError(TEXT("Requires isolated standalone, automatic persistence disabled and unique -NeonCityC923Pier21SaveSlot=NeonCity_Test_C923_Pier21_<run>; never use human saves"));return false;}
    for(TCHAR C:Slot)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-')){AddError(TEXT("Invalid dedicated Pier21 slot"));return false;}
    if(Slot.EndsWith(TEXT("_Backup")) || Slot.EndsWith(TEXT("_Pending"))){AddError(TEXT("Reserved save generation suffix"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(SpeedboatPier21::FFlow(this,Slot));return true;
}
#endif
