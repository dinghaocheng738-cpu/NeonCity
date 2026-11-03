#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/CoastalOceanSurface.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"

namespace MainlandYacht921
{
// Stable mainland identities retained by C920. The streamed island yacht is
// deliberately excluded; no actor is substituted or respawned for this test.
static FGuid YachtID(int32 Index)
{
    return Index == 0 ? FGuid(0xA5620B90,0x39085C58,0xA507A26E,0x1EFD1981)
        : FGuid(0x3A8B376B,0x91695E5C,0xB164FD5E,0xEA9E8457);
}

enum class EStage : uint8 { Initialize, Settle, Approach, Prompt, Board, Occupied, Cruise, Brake, Return, Exit, ExitSettle, WalkClear, SternExitSettle, ShoreSave };

class FFlow : public IAutomationLatentCommand
{
public:
    FFlow(FAutomationTestBase* InTest,int32 InIndex,bool InDrive,bool InSternOnly=false,bool InBerthOnly=false)
        : Test(InTest),Index(InIndex),bDrive(InDrive),bSternOnly(InSternOnly),bBerthOnly(InBerthOnly),
          Evidence(FString::Printf(TEXT("C921-Yacht%d-%s"),InIndex,InDrive?TEXT("Drive"):TEXT("Access"))),
          Metrics(FString::Printf(TEXT("C921-Yacht%d-%s"),InIndex,InDrive?TEXT("Drive"):TEXT("Access"))) {}
    ~FFlow() { ReleaseKeys(); }
    bool Update() override
    {
        if (!Started) Started=FPlatformTime::Seconds();
        W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
        if(bFailureCleanupInitiated && !bFailureCleanupFinished)return CleanupFailedStern();
        if (FPlatformTime::Seconds()-Started>240) return End(TEXT("Two-yacht focused flow exceeded its bounded timeout"));
        if (!W || !PC || W->GetTimeSeconds()<5) return false;
        const float Age=W->GetTimeSeconds()-At;
        if(Stage==EStage::ShoreSave)
        {
            if(Save->IsOperationInProgress())return false;
            if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Actual returned-quay save failed: ")+Save->GetLastDetail());
            const auto* Model=Save->ReadSaveForInspection();
            if(!Model || !Model->HasPlayer || !Model->HasMarine || !Grounded()
                || FVector::Dist(Model->Player.Transform.GetLocation(),Player->GetActorLocation())>10)
                return End(TEXT("Actual shore save does not contain the grounded returned player and marine records"));
            bShoreSavePassed=true;return End(TEXT(""));
        }
        if (Boat)
        {
            const auto T=Boat->GetMarineMovement()->GetTelemetry();
            PeakKmh=FMath::Max(PeakKmh,T.ForwardSpeedKmh);
            const FVector Position=Boat->GetActorLocation();
            if (bHadBoatPosition) ActualDistanceCm+=FVector::Dist2D(Position,PreviousBoatPosition);
            PreviousBoatPosition=Position;bHadBoatPosition=true;
            const auto* FX=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
            CSV+=FString::Printf(TEXT("%.6f,%.6f,%d,%s,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%d,%d,%d\n"),
                FPlatformTime::Seconds()-Started,W->GetTimeSeconds(),int32(Stage),*Boat->VehicleID.ToString(EGuidFormats::Digits),
                T.ForwardSpeedKmh,T.SpeedKmh,Position.X,Position.Y,Position.Z,Boat->GetActorRotation().Yaw,
                Boat->GetHealthComponent()->GetCurrentHealth(),T.SignedThrust,T.Steering,
                FX?FX->GetActiveWakeCount():-1,FX?FX->GetActiveSprayCount():-1,Ocean && Ocean->HasPhysicalWakeFor(Boat)?1:0);
            Metrics.Sample(W,FString::Printf(TEXT("yacht%d-stage%02d"),Index,int32(Stage)));
            Evidence.Record(W,PC,int32(Stage));
            if (Boat->GetHealthComponent()->GetCurrentHealth()<Health-.1f)
                return End(TEXT("Actual authored yacht lost health during the native flow"));
            if (FMath::Abs(Boat->GetActorRotation().Roll)>22 || FMath::Abs(Boat->GetActorRotation().Pitch)>22)
                return End(TEXT("Actual authored yacht exceeded the supported boarding/drive attitude envelope"));
        }
        if (Stage==EStage::Initialize)
        {
            Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            int32 Matches=0;
            for (TActorIterator<ANeonCityMotorYacht> I(W);I;++I) if (I->VehicleID==YachtID(Index)) { Boat=*I;++Matches; }
            for (TActorIterator<ACoastalOceanSurface> I(W);I;++I) { Ocean=*I;break; }
            if (!Player || Matches!=1 || !Boat->YachtBody->GetStaticMesh() || !Boat->GetHull()->IsSimulatingPhysics())
                return End(TEXT("Exactly one expected mainland yacht and the normal on-foot player are required"));
            const auto Defaults=W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot(true);
            const auto* Default=Defaults.FindByPredicate([&](const auto& D){return D.VehicleID==YachtID(Index);});
            if (!Default) return End(TEXT("Authored yacht identity is missing from the production marine registry"));
            Berth=Default->Transform;Forward=Berth.GetRotation().GetForwardVector().GetSafeNormal2D();
            const bool bReference86=Index==1 && Boat->ActorHasTag(TEXT("NeonCity.MainlandC923.Reference86.RelocatedYacht"));
            const FVector ExpectedBerth= bReference86?FVector(-9500,-3796.9651413,0):FVector(Index==0?-2000:-4000,Index==0?-3280:-3420,0);
            if (FVector::Dist2D(Berth.GetLocation(),ExpectedBerth)>1)
                return End(TEXT("This focused route requires the actual C923 stern-in authored yacht berth"));
            Health=Boat->GetHealthComponent()->GetCurrentHealth();
            const float PierX=Berth.GetLocation().X;
            // The short stern passerelle replaces the long side finger. Read
            // its real quay end; a fixed old approach can now lie in water.
            UStaticMeshComponent* Pier=nullptr;
            for(TActorIterator<AStaticMeshActor> I(W);I;++I)
            {
                auto* C=I->GetStaticMeshComponent();const UStaticMesh* Mesh=C->GetStaticMesh().Get();
                if(!Mesh || !I->ActorHasTag(TEXT("NeonCity.MainlandC923.SternPasserelles13"))
                    || (Mesh->GetName()!=FString::Printf(TEXT("SM_C923_SternPasserelle13_%d"),Index)
                     && !(I->ActorHasTag(*FString::Printf(TEXT("NeonCity.MainlandC923.Reference86.Stern%d"),Index))
                       && Mesh->GetName()==FString::Printf(TEXT("SM_C923_SternPasserelle86_%d"),Index))))continue;
                FVector Low,High;C->GetLocalBounds(Low,High);
                const auto Pose=C->GetComponentTransform();
                const FVector Center=Pose.TransformPosition((Low+High)*.5f);
                if(FMath::Abs(Center.X-PierX)>10)continue;
                if(Pier)return End(TEXT("Current yacht boarding route has ambiguous native finger geometry"));
                Pier=C;PierEntrance=Pose.TransformPosition(FVector((Low.X+High.X)*.5f,High.Y,High.Z));
            }
            if(!Pier)return End(TEXT("The exact yacht has no current connected C923 stern passerelle at its berth"));
            Goals={FVector(PierX,PierEntrance.Y+50,0),FVector(PierX,Berth.GetLocation().Y+555,0)};
            FHitResult Floor;
            if (!StaticFloor(FVector(PierX,PierEntrance.Y+350,500),Floor)) return End(TEXT("Initial quay fixture lacks real static walkable support"));
            const FVector Stand=Floor.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(C921YachtInitialCapsule),false,Player);
            if (W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()),Query))
                return End(TEXT("Initial quay fixture capsule is blocked"));
            Player->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            InitialPlayer=Stand;ConfigureOutput();return Next(EStage::Settle);
        }
        if (Stage==EStage::Settle && Age>.8f)
        {
            if (!Grounded()) return End(TEXT("Normal player did not settle on the actual quay"));
            return Next(EStage::Approach);
        }
        if (Stage==EStage::Approach)
        {
            const int32 Result=Walk();
            if (Result<0) return End(Failure);
            if (Result>0) return Next(EStage::Prompt);
        }
        if (Stage==EStage::Prompt && Age>.4f)
        {
            if (!Grounded()) return End(TEXT("Physical quay/ramp/finger approach has no native grounded support"));
            if (!Focus()) return End(TEXT("No normal 250cm visible yacht interaction from the connected finger: ")+FocusDetails);
            if (!Boat->IsEntryPromptVisible() && Age<1) return false;
            if (!Boat->IsEntryPromptVisible()) return End(TEXT("Yacht focused through the normal trace but did not show its entry prompt"));
            bPromptPassed=true;Capture(TEXT("pier-prompt"));Key(EKeys::F,true);return Next(EStage::Board);
        }
        if (Stage==EStage::Board && Age>.25f)
        {
            Key(EKeys::F,false);
            if (PC->GetPawn()!=Boat || Boat->GetMarineInteraction()->GetDriver()!=Player || !Boat->GetMarineMovement()->IsEngineRunning())
                return End(TEXT("Ordinary F did not possess the actual yacht and start its engine"));
            bBoardPassed=true;Start=Boat->GetActorLocation();return Next(EStage::Occupied);
        }
        if (Stage==EStage::Occupied && Age>.5f)
        {
            if (!Boat->IsDrivingStatusVisible()) return End(TEXT("The occupied yacht did not show normal driving status"));
            Capture(TEXT("native-helm-entry"));return Next(bDrive?EStage::Cruise:EStage::Exit);
        }
        if (Stage==EStage::Cruise)
        {
            const FVector Position=Boat->GetActorLocation();
            const FVector Centerline=Start+Forward*60000;
            const float Heading=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,(Centerline-Position).Rotation().Yaw);
            if (!Boat->SubmitDriverInput_Implementation(PC,1,FMath::Clamp(Heading/25.f,-.35f,.35f),0))
                return End(TEXT("Production marine input rejected the actual driver"));
            if(bSternOnly && SternCaptures<2 && Age>=(SternCaptures==0?6.f:12.f))
            {
                if(!InspectStern())return End(Failure);
                Capture(FString::Printf(TEXT("stern-visual-%02d"),SternCaptures));++SternCaptures;
            }
            if (Age>=(bBerthOnly?8.f:20.f))
            {
                CruiseKmh=Boat->GetMarineMovement()->GetTelemetry().ForwardSpeedKmh;
                if(CruiseKmh<(bBerthOnly?10.f:40.f) || PeakKmh>Boat->GetProfile()->MaxSpeed+2)
                    return End(FString::Printf(TEXT("Yacht native undocking/cruise speed envelope failed: current%.3f peak%.3f"),CruiseKmh,PeakKmh));
                const auto* FX=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
                if(!FX || !FX->HasTransomWash() || FX->GetTransomWashEmissionCount()<20
                    || FX->GetActiveWakeCount()==0 || FX->GetActiveSprayCount()!=0)
                    return End(TEXT("Native yacht has no bounded surface transom wash, or bow/smoke spray returned"));
                if(bSternOnly && !InspectStern())return End(Failure);
                Capture(bBerthOnly?TEXT("short-native-undock"):bSternOnly?TEXT("stern-visual-20s"):TEXT("full-throttle-stern"));return Next(EStage::Brake);
            }
        }
        if (Stage==EStage::Brake)
        {
            Boat->SubmitDriverInput_Implementation(PC,0,0,1);
            if(bSternOnly)
            {
                if(Age>18)return End(TEXT("Focused stern observation could not stop the yacht for normal deck exit"));
                if(Boat->GetSpeed_Implementation()>65 || Age<2)return false;
                const auto* FX=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
                if(FX && FX->GetActiveWakeCount()>0)return false;
                FTransform Landing;
                if(!Boat->GetMarineInteraction()->FindExitTransform(Landing))
                    return End(TEXT("Stopped stern fixture lacks a normal supported deck exit"));
                Key(EKeys::F,true);return Next(EStage::SternExitSettle);
            }
            if (Boat->GetSpeed_Implementation()<40) return Next(EStage::Return);
            if (Age>12) return End(TEXT("Native braking did not reduce actual yacht speed"));
        }
        if (Stage==EStage::Return)
        {
            if(bBerthOnly)
            {
                // The current120cm clear stern bridge leaves only25cm for the
                // real35cm-radius player centre. Line up offshore before the
                // final slow reverse; every motion still uses production input.
                const FVector Offset=Boat->GetActorLocation()-Berth.GetLocation();
                const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
                const float Along=FVector::DotProduct(Offset,Forward),Side=FVector::DotProduct(Offset,Right);
                const float BerthYaw=Berth.Rotator().Yaw;
                const float YawError=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,BerthYaw);
                const float LandingHalfWidth=60.f-Player->GetSimpleCollisionRadius()-5.f;
                if(LandingHalfWidth<=0)return End(TEXT("Actual player capsule no longer fits the source120cm stern bridge"));
                const float SideTolerance=FMath::Min(8.f,LandingHalfWidth*.4f);
                const float FarthestAft=Boat->GetProfile()->HullExtent.X+Player->GetSimpleCollisionRadius()+175.f;
                const float YawTolerance=FMath::Min(1.f,FMath::RadiansToDegrees(FMath::Asin(
                    FMath::Clamp((LandingHalfWidth-SideTolerance)/FarthestAft,0.f,1.f))));
                if(Along<800.f && (FMath::Abs(Side)>SideTolerance || FMath::Abs(YawError)>YawTolerance))bBerthLineup=true;
                if(bBerthLineup && Along>=900.f && FMath::Abs(Side)<SideTolerance && FMath::Abs(YawError)<YawTolerance)bBerthLineup=false;
                const auto T=Boat->GetMarineMovement()->GetTelemetry();
                const float ActualSpeed=T.ForwardSpeedKmh/.036f;
                const float LookAhead=bBerthLineup?250.f:250.f+FMath::Max(Along,0.f);
                const float Correction=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Side,LookAhead)),-8.f,8.f);
                const float DesiredYaw=BerthYaw+(bBerthLineup?-Correction:Correction);
                const float Heading=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,DesiredYaw);
                const float DesiredSpeed=bBerthLineup?130.f:FMath::Clamp(-Along*.35f,-280.f,80.f);
                const float Throttle=FMath::Clamp((DesiredSpeed-ActualSpeed)/150.f,-.8f,.25f);
                // The production rudder reverses with actual forward speed.
                // Keep the proper sign while braking through a direction change.
                const float MotionSign=FMath::Abs(ActualSpeed)>10.f?FMath::Sign(ActualSpeed):FMath::Sign(DesiredSpeed);
                if(!Boat->SubmitDriverInput_Implementation(PC,Throttle,FMath::Clamp(MotionSign*Heading/22.f,-.4f,.4f),0))
                    return End(TEXT("Production input rejected the stern berth return driver"));
                if(!bBerthLineup && FMath::Abs(Along)<30.f && FMath::Abs(Side)<SideTolerance
                    && FMath::Abs(YawError)<YawTolerance && Boat->GetSpeed_Implementation()<20.f)
                {
                    FTransform Landing;
                    if(Boat->GetMarineMovement()->IsAtMooringBerth() && Boat->GetMarineInteraction()->FindExitTransform(Landing,true)
                        && FMath::Abs(FVector::DotProduct(Landing.GetLocation()-Berth.GetLocation(),Right))<LandingHalfWidth)
                    {Boat->SubmitDriverInput_Implementation(PC,0,0,1);bReturnPassed=true;Capture(TEXT("native-berth-return"));return Next(EStage::Exit);}
                }
                if(Age>125)return End(FString::Printf(TEXT("Native stern reverse return stalled: along%.1f side%.1f yaw%.2f speed%.2f lineup%d"),Along,Side,YawError,T.ForwardSpeedKmh,bBerthLineup?1:0));
                return false;
            }
            const FVector Offset=Boat->GetActorLocation()-Start;
            const float Along=FVector::DotProduct(Offset,Forward),Side=FVector::CrossProduct(Forward,Offset).Z;
            const auto T=Boat->GetMarineMovement()->GetTelemetry();
            const float DesiredSpeed=-FMath::Clamp(Along*.35f,0.f,280.f);
            const float ActualSpeed=T.ForwardSpeedKmh/.036f;
            const float Throttle=FMath::Clamp((DesiredSpeed-ActualSpeed)/150.f,-.8f,.25f);
            const float DesiredYaw=Along>200?(Offset+Forward*250).Rotation().Yaw:Berth.Rotator().Yaw;
            const float Heading=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,DesiredYaw);
            Boat->SubmitDriverInput_Implementation(PC,Throttle,FMath::Clamp(-Heading/22.f,-.4f,.4f),0);
            if (FMath::Abs(Along)<90 && FMath::Abs(Side)<90 && Boat->GetSpeed_Implementation()<65
                && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw))<15)
            { Boat->SubmitDriverInput_Implementation(PC,0,0,1);bReturnPassed=true;Capture(TEXT("native-berth-return"));return Next(EStage::Exit); }
            if (Age>125) return End(FString::Printf(TEXT("Native reverse return stalled: along%.1f side%.1f speed%.2f"),Along,Side,T.ForwardSpeedKmh));
        }
        if (Stage==EStage::Exit && Age>.4f)
        {
            Boat->SubmitDriverInput_Implementation(PC,0,0,1);
            if (Boat->GetSpeed_Implementation()>65) return false;
            FTransform Landing;
            if (!Boat->GetMarineMovement()->IsAtMooringBerth() || !Boat->GetMarineInteraction()->FindExitTransform(Landing,true))
                return End(TEXT("Actual yacht berth cannot provide a collision-safe shore exit"));
            Key(EKeys::F,true);return Next(EStage::ExitSettle);
        }
        if (Stage==EStage::ExitSettle && Age>.25f)
        {
            Key(EKeys::F,false);
            if (PC->GetPawn()!=Player || Boat->GetMarineInteraction()->GetDriver()) return End(TEXT("Ordinary F yacht disembarkation failed"));
            if (Age<1) return false;
            const auto* Base=Player->GetMovementBase();
            if (!Grounded() || !Base || Base->GetOwner()==Boat) return End(TEXT("Yacht F exit did not leave the visible player on the actual static finger pier"));
            bExitPassed=true;Goals={FVector(Berth.GetLocation().X,Player->GetActorLocation().Y,0),FVector(Berth.GetLocation().X,PierEntrance.Y+50,0),FVector(InitialPlayer.X,InitialPlayer.Y,0)};
            Goal=0;Capture(TEXT("native-F-pier-exit"));return Next(EStage::WalkClear);
        }
        if (Stage==EStage::WalkClear)
        {
            const int32 Result=Walk();if(Result<0)return End(Failure);
            if (Result>0)
            {
                bWalkClearPassed=true;Capture(TEXT("native-walk-back-quay"));
                FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
                // Explicit evidence-run opt-in; ordinary access tests and human slots stay untouched.
                if(Index==0 && bDrive && bBerthOnly && Label.EndsWith(TEXT("RefBoat106Producer")))
                {
                    ShoreSaveSlot=TEXT("NeonCity_Test_C923_Ref106_")+Label;
                    Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
                    if(!Save || !Save->ConfigureSlot(ShoreSaveSlot,false) || Save->DoesSaveExist())return End(TEXT("Fresh dedicated returned-quay save slot required"));
                    if(!Save->SaveGame())return End(TEXT("Returned-quay SaveGame rejected: ")+Save->GetLastDetail());
                    return Next(EStage::ShoreSave);
                }
                return End(TEXT(""));
            }
        }
        if(Stage==EStage::SternExitSettle && Age>.25f)
        {
            Key(EKeys::F,false);
            if(PC->GetPawn()!=Player || Boat->GetMarineInteraction()->GetDriver())
                return End(TEXT("Focused stern observation did not restore the player through normal F exit"));
            if(Age<1)return false;
            if(Player->IsHidden() || !Player->GetActorEnableCollision()
                || !Player->GetCharacterMovement()->IsMovingOnGround() || !Player->GetMovementBase()
                || Player->GetMovementBase()->GetOwner()!=Boat)
                return End(TEXT("Focused stern observation normal F exit did not settle the player on the real yacht deck"));
            bSternDeckExitPassed=true;return End(TEXT(""));
        }
        return false;
    }
private:
    bool OwnsYachtDriver() const
    {
        // An unrelated test's possessed car/boat is not this fixture's driver.
        // Keep the independent normal-on-foot precondition strict.
        return Boat && Player && PC && PC->GetPawn()==Boat
            && Boat->GetMarineInteraction()->GetDriver()==Player;
    }
    bool CleanupFailedStern()
    {
        const double Now=FPlatformTime::Seconds();
        if(!W || !PC || Now-FailureCleanupStarted>20)
        {
            ReleaseKeys();bFailureCleanupFinished=true;
            return End(PendingFailure+TEXT("; stern failure cleanup could not restore a normal player through a supported F deck exit"));
        }
        if(PC->GetPawn()==Player)
        {
            Key(EKeys::F,false);
            if(Player->IsHidden() || !Player->GetActorEnableCollision()
                || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
            bFailureCleanupFinished=bFailureCleanupRestoredPlayer=true;
            return End(PendingFailure);
        }
        if(!OwnsYachtDriver())
        {
            ReleaseKeys();bFailureCleanupFinished=true;
            return End(PendingFailure+TEXT("; stern failure cleanup lost its owned yacht driver"));
        }
        Boat->SubmitDriverInput_Implementation(PC,0,0,1);
        if(FailureExitPressedAt>0)
        {
            if(Now-FailureExitPressedAt<.25)return false;
            Key(EKeys::F,false);
            if(Now-FailureExitPressedAt<.75)return false;
            FailureExitPressedAt=0;
        }
        FTransform Landing;
        if(Boat->GetSpeed_Implementation()<=65 && Boat->GetMarineInteraction()->FindExitTransform(Landing))
        {
            Key(EKeys::F,true);FailureExitPressedAt=Now;
        }
        return false;
    }
    bool InspectStern()
    {
        TArray<UInstancedStaticMeshComponent*> Components;Boat->GetComponents(Components);
        UInstancedStaticMeshComponent* Instances=nullptr;
        for(auto* C:Components)if(C->GetFName()==TEXT("MarineWaterParticles"))Instances=C;
        const auto* FX=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
        if(!FX || !Instances || Instances->GetInstanceCount()!=72 || FX->GetActiveWakeCount()>48
            || FX->GetActiveSprayCount()!=0 || !Ocean || !Ocean->HasPhysicalWakeFor(Boat))
        {Failure=TEXT("Stern observation lost the existing bounded72-instance/48-foam pool or production physical wake");return false;}
        const FVector Anchor=Boat->GetActorTransform().TransformPosition(FX->GetTransomWashLocalAnchor());
        const FVector Direction=Boat->GetActorForwardVector().GetSafeNormal2D();
        auto Sample=MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());
        Sample->SetNumberField(TEXT("speed_kmh"),Boat->GetMarineMovement()->GetTelemetry().ForwardSpeedKmh);
        Sample->SetNumberField(TEXT("active_foam"),FX->GetActiveWakeCount());
        Sample->SetNumberField(TEXT("pool_instances"),Instances->GetInstanceCount());
        double Nearest=DBL_MAX,NearWidth=0,MaxCenterError=0;int32 Supported=0;
        auto* Water=W->GetSubsystem<UMarineWorldSubsystem>();
        for(int32 I=0;I<48;++I)
        {
            FTransform T;
            if(!Instances->GetInstanceTransform(I,T,true) || T.GetScale3D().X<=0
                || !Instances->PerInstanceSMCustomData.IsValidIndex(I) || Instances->PerInstanceSMCustomData[I]<.2f)continue;
            FWaterSurfaceSample Surface;
            if(!Water->QueryWater(T.GetLocation(),Surface)){Failure=TEXT("Actual stern foam has no production water support");return false;}
            ++Supported;MaxCenterError=FMath::Max(MaxCenterError,FMath::Abs(T.GetLocation().Z-Surface.SurfacePoint.Z-3));
            const double Behind=-FVector::DotProduct(T.GetLocation()-Anchor,Direction);
            if(Behind>=-30 && Behind<Nearest){Nearest=Behind;NearWidth=100*T.GetScale3D().Y;}
        }
        Sample->SetNumberField(TEXT("supported_visible_centres"),Supported);
        Sample->SetNumberField(TEXT("nearest_visible_centre_aft_cm"),Nearest==DBL_MAX?-1:Nearest);
        Sample->SetNumberField(TEXT("nearest_visible_quad_width_cm"),NearWidth);
        Sample->SetNumberField(TEXT("max_centre_surface_offset_error_cm"),MaxCenterError);
        SternSamples.Add(MakeShared<FJsonValueObject>(Sample));
        // The old45cm stamps fail this measured footprint gate. This is a
        // technical check of real render instances, not subjective image approval.
        if(Supported<12 || Nearest>130 || NearWidth<60 || MaxCenterError>1)
        {Failure=FString::Printf(TEXT("Stern feedback footprint/support failed: visible%d nearest%.2fcm width%.2fcm surfaceError%.3fcm"),Supported,Nearest,NearWidth,MaxCenterError);return false;}
        return true;
    }
    bool Next(EStage In) { Stage=In;At=W->GetTimeSeconds();return false; }
    bool StaticFloor(const FVector& Point,FHitResult& Hit) const
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C921YachtWalkFloor),false,Player);Query.AddIgnoredActor(Boat);
        return W->LineTraceSingleByObjectType(Hit,Point+FVector(0,0,100),Point-FVector(0,0,800),
            FCollisionObjectQueryParams(ECC_WorldStatic),Query) && Hit.IsValidBlockingHit()
            && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics()
            && Hit.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ();
    }
    bool Grounded() const
    {
        FHitResult Floor;
        return PC->GetPawn()==Player && !Player->IsHidden() && Player->GetActorEnableCollision()
            && Player->GetCharacterMovement()->IsMovingOnGround() && StaticFloor(Player->GetActorLocation(),Floor)
            && FMath::Abs(Player->GetActorLocation().Z-Floor.ImpactPoint.Z-Player->GetSimpleCollisionHalfHeight())<12;
    }
    int32 Walk()
    {
        if (PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision() || Player->GetCharacterMovement()->IsSwimming())
        { Failure=TEXT("Normal yacht approach lost visible, collidable on-foot movement");return -1; }
        if (!Goals.IsValidIndex(Goal)) return 1;
        const FVector Delta=Goals[Goal]-Player->GetActorLocation();
        if (Delta.Size2D()<10) { Player->GetCharacterMovement()->StopMovementImmediately();++Goal;At=W->GetTimeSeconds();return Goal==Goals.Num()?1:0; }
        if (W->GetTimeSeconds()-At>15) { Failure=FString::Printf(TEXT("Yacht%d physical approach blocked at goal%d pose%s target%s"),Index,Goal,*Player->GetActorLocation().ToString(),*Goals[Goal].ToString());return -1; }
        Player->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/120),.15f,1.f));
        PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));return 0;
    }
    bool Focus()
    {
        FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);
        const auto* Interaction=Player->GetInteractionComponent();FocusDetails.Reset();
        for (float X:{-520.f,-500.f,-480.f,-455.f}) for (float Y:{-90.f,-30.f,0.f,30.f,90.f}) for(float Z:{50.f,80.f,100.f,130.f})
        {
            const FVector Aim=Boat->GetActorTransform().TransformPosition(FVector(X,Y,Z));
            PC->SetControlRotation((Aim-Eye).Rotation());
            if (Player->GetInteractionComponent()->RefreshFocusedInteractable()==Boat) return true;
            FHitResult Hit;Interaction->DetectInteractable(Eye,Eye+(Aim-Eye).GetSafeNormal()*Interaction->InteractionDistance,Hit);
            if (FocusDetails.Len()<7000) FocusDetails+=FString::Printf(TEXT("[aim%s hit%s/%s dist%.1f canEnter%d vel%.1f]"),
                *Aim.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.Distance,
                Boat->GetMarineInteraction()->CanEnter(Player)?1:0,Boat->GetVelocity().Size());
        }
        return false;
    }
    void Key(FKey K,bool Down)
    {
        bool& Previous=Keys.FindOrAdd(K);if (Previous==Down || !PC)return;
        PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Previous=Down;
    }
    void ReleaseKeys() { for(auto& Pair:Keys)if(Pair.Value)Key(Pair.Key,false); }
    void Capture(const FString& Name)
    {
        const FString File=FString::Printf(TEXT("C921-Yacht%d-%s.png"),Index,*Name);
        if (!FNeonCityVisualEvidence::CaptureHUD(W,File)) { bCaptureFailed=true;Test->AddError(TEXT("Native yacht visual evidence capture failed: ")+File); }
    }
    void ConfigureOutput()
    {
        FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
        Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/FPaths::MakeValidFileName(Label);
        IFileManager::Get().MakeDirectory(*Directory,true);
        CSV=TEXT("wall_seconds,game_seconds,stage,vehicle_id,forward_kmh,speed_kmh,x_cm,y_cm,z_cm,yaw,health,thrust,steering,foam_count,spray_count,physical_wake\n");
    }
    bool End(const FString& Error)
    {
        if((bSternOnly || bBerthOnly) && !Error.IsEmpty() && !bFailureCleanupInitiated && OwnsYachtDriver())
        {
            // Finish a failed visual assertion only after releasing this
            // fixture's real input/possession, using production brake and F.
            PendingFailure=Error;bFailureCleanupInitiated=true;
            FailureCleanupStarted=FPlatformTime::Seconds();ReleaseKeys();
            Boat->SubmitDriverInput_Implementation(PC,0,0,1);return false;
        }
        ReleaseKeys();Metrics.Flush();if(OwnsYachtDriver())Boat->SubmitDriverInput_Implementation(PC,0,0,1);
        if(!Error.IsEmpty())Test->AddError(Error);
        if(Directory.IsEmpty())ConfigureOutput();
        auto J=MakeShared<FJsonObject>();
        J->SetBoolField(TEXT("passed"),Error.IsEmpty() && !bCaptureFailed);J->SetStringField(TEXT("failure"),Error);
        J->SetBoolField(TEXT("capture_failure"),bCaptureFailed);
        J->SetNumberField(TEXT("yacht_index"),Index);J->SetStringField(TEXT("vehicle_id"),YachtID(Index).ToString(EGuidFormats::Digits));
        J->SetNumberField(TEXT("stage"),int32(Stage));J->SetBoolField(TEXT("driving_requested"),bDrive);
        J->SetBoolField(TEXT("normal_prompt"),bPromptPassed);J->SetBoolField(TEXT("normal_F_board"),bBoardPassed);
        J->SetBoolField(TEXT("native_return"),bReturnPassed);J->SetBoolField(TEXT("normal_F_shore_exit"),bExitPassed);
        J->SetBoolField(TEXT("native_walk_back_quay"),bWalkClearPassed);J->SetNumberField(TEXT("peak_forward_kmh"),PeakKmh);
        J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("shore_save_slot"),ShoreSaveSlot);J->SetBoolField(TEXT("actual_shore_save_passed"),bShoreSavePassed);
        J->SetBoolField(TEXT("stern_only"),bSternOnly);J->SetBoolField(TEXT("stern_normal_F_deck_exit"),bSternDeckExitPassed);
        J->SetBoolField(TEXT("stern_failure_cleanup_attempted"),bFailureCleanupInitiated);
        J->SetBoolField(TEXT("stern_failure_cleanup_restored_player"),bFailureCleanupRestoredPlayer);
        J->SetArrayField(TEXT("stern_instance_samples"),SternSamples);
        J->SetBoolField(TEXT("berth_fix_only"),bBerthOnly);J->SetNumberField(TEXT("full_throttle_observation_seconds"),bDrive?(bBerthOnly?8:20):0);
        J->SetNumberField(bBerthOnly?TEXT("short_undock_kmh"):TEXT("full_throttle_20s_kmh"),CruiseKmh);J->SetNumberField(TEXT("actual_travel_cm"),ActualDistanceCm);
        J->SetStringField(TEXT("focus_diagnostics"),FocusDetails);J->SetStringField(TEXT("initial_player_cm"),InitialPlayer.ToString());
        J->SetStringField(TEXT("current_native_pier_entrance_cm"),PierEntrance.ToString());
        J->SetStringField(TEXT("scope"),TEXT("Exact retained mainland yacht. One initial supported on-foot quay fixture derived from the current native C921 ramp bounds; native movement down real ramp/finger, normal 250cm occluded focus/prompt/F, engine/status/F shore exit and native walk back. Optional 20s full public throttle requires40km/h minimum and profile governor envelope, bounded transom surface wash with no bow spray, production brake and reverse return. Zero boat pose/velocity/collision writes. Rendered readbacks remain excluded from clean performance claims. Each yacht has its own automation result so one failure cannot suppress the other."));
        if(Boat){const auto* D=Boat->GetProfile();J->SetNumberField(TEXT("mass_kg"),D->Mass);J->SetNumberField(TEXT("engine_kw"),D->EnginePower);J->SetNumberField(TEXT("acceleration_mps2"),D->Acceleration);J->SetNumberField(TEXT("water_drag"),D->WaterDrag);J->SetNumberField(TEXT("speed_limit_kmh"),D->MaxSpeed);
            if(const auto* FX=Boat->FindComponentByClass<UMarineWaterEffectsComponent>())
            {J->SetBoolField(TEXT("transom_wash_enabled"),FX->HasTransomWash());J->SetStringField(TEXT("transom_wash_anchor_cm"),FX->GetTransomWashLocalAnchor().ToString());J->SetNumberField(TEXT("transom_emissions"),FX->GetTransomWashEmissionCount());}}
        if(bSternOnly)J->SetStringField(TEXT("scope"),TEXT("Stern visual fix only. Reuses the real connected pier, normal focus/F possession and production throttle for20s. Captures6/12/20s and reads existing world-space ISM transforms/opacity to check the48-foam bound, near-stern footprint and displaced water support. Brakes, waits for foam decay and uses ordinary F to restore player on the actual deck. No return/shore-route rerun, boat pose/velocity/collision write or new VFX assets. Images require separate visual review; rendered readbacks are not performance evidence."));
        if(bBerthOnly)J->SetStringField(TEXT("scope"),TEXT("C923 stern berth fix only: one supported quay fixture, normal continuous capsule walk on the real short passerelle, normal250cm occluded focus/prompt/F, optional8s public-throttle undock and native brake/reverse return, normal F to actual static stern landing and walk back to quay. Boat pose/velocity/collision/scale unchanged by fixture. Cached20s governor/sea cruise and stern VFX qualification are not rerun or relabelled as new evidence. Rendered captures are not clean performance samples."));
        FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));
        const FString Stem=FString::Printf(TEXT("Yacht%d-%s"),Index,bBerthOnly?TEXT("SternBerth13"):bSternOnly?TEXT("SternVisual"):bDrive?TEXT("Drive"):TEXT("Access"));
        if(!FFileHelper::SaveStringToFile(Json,*(Directory/(Stem+TEXT(".json")))) || !FFileHelper::SaveStringToFile(CSV,*(Directory/(Stem+TEXT("-trajectory.csv"))))) Test->AddError(TEXT("Yacht focused evidence files could not be written"));
        return true;
    }
    FAutomationTestBase* Test;int32 Index;bool bDrive,bSternOnly,bBerthOnly;
    FNeonCityVisualEvidence Evidence;FNeonCityLandFrameMetrics Metrics;
    UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityMotorYacht* Boat=nullptr;ACoastalOceanSurface* Ocean=nullptr;
    EStage Stage=EStage::Initialize;double Started=0,ActualDistanceCm=0;float At=0,Health=0,PeakKmh=0,CruiseKmh=0;
    FTransform Berth;FVector InitialPlayer,Start,Forward,PreviousBoatPosition,PierEntrance;TArray<FVector> Goals;int32 Goal=0;
    TMap<FKey,bool> Keys;FString Directory,CSV,Failure,FocusDetails;
    TArray<TSharedPtr<FJsonValue>> SternSamples;int32 SternCaptures=0;bool bSternDeckExitPassed=false;
    FString PendingFailure;double FailureCleanupStarted=0,FailureExitPressedAt=0;
    bool bFailureCleanupInitiated=false,bFailureCleanupFinished=false,bFailureCleanupRestoredPlayer=false;
    bool bHadBoatPosition=false,bPromptPassed=false,bBoardPassed=false,bReturnPassed=false,bExitPassed=false,bWalkClearPassed=false,bCaptureFailed=false;
    bool bBerthLineup=false;
    UNeonCitySaveSubsystem* Save=nullptr;FString ShoreSaveSlot;bool bShoreSavePassed=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtAccess0_921,"NeonCity.Phase205c.MainlandC921.Standalone.YachtAccess0",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtAccess0_921::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Yacht focus test requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,0,false));return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtAccess1_921,"NeonCity.Phase205c.MainlandC921.Standalone.YachtAccess1",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtAccess1_921::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Yacht focus test requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,1,false));return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtDrive921,"NeonCity.Phase205c.MainlandC921.Standalone.YachtDrive",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtDrive921::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Yacht focus test requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,0,true));return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtSternVisual921,"NeonCity.Phase205c.MainlandC921.Standalone.YachtSternVisual",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtSternVisual921::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Yacht stern observation requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,0,true,true));return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtSternBerth0_923,"NeonCity.Phase205c.MainlandC923.Standalone.YachtSternBerth0",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtSternBerth0_923::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Stern berth test requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,0,true,false,true));return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandYachtSternAccess1_923,"NeonCity.Phase205c.MainlandC923.Standalone.YachtSternAccess1",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandYachtSternAccess1_923::RunTest(const FString&)
{ if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Stern berth test requires preserved human save slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MainlandYacht921::FFlow(this,1,false,false,true));return true; }
#endif
