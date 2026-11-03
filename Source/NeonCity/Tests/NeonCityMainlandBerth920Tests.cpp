#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "World/NeonCityMainlandSite.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandBerth920
{
enum class EMode : uint8 { Speedboat, Helicopter };
enum class EStage : uint8
{
    Initialize, SettleStart, Approach, FocusPrompt, Enter, Occupied,
    Lift, Hover, Land, Exit, SettleExit, WalkClear, Circuit
};

FString HashFile(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? FMD5::HashBytes(Bytes.GetData(), Bytes.Num()) : FString();
}

/** One disclosed supported on-foot start. The canonical vehicle remains at its
 * actual runtime spawn and keeps normal physics, collision, condition and input.
 * All pier/ramp travel thereafter uses CharacterMovement; F boarding/alighting
 * uses mapped production input. The helicopter follows a bounded sample-city
 * circuit through its existing driver port, with no pose, velocity, force or
 * landing-state writes. This is not a full island flight.
 */
class FAccess : public IAutomationLatentCommand
{
public:
    FAccess(FAutomationTestBase* InTest, EMode InMode) : Test(InTest), Mode(InMode) {}

    bool Update() override
    {
        if (!Started)
        {
            Started = FPlatformTime::Seconds();
            ConfigureOutput();
        }
        W = NeonCityValidationWorld();
        PC = W ? W->GetFirstPlayerController() : nullptr;
        const double Limit = Mode == EMode::Helicopter ? 360. : 180.;
        if (FPlatformTime::Seconds() - Started > Limit)
            return End(FString::Printf(TEXT("Finite %.0f-second access test limit exceeded"), Limit));
        if (!PC || W->GetTimeSeconds() < 8.f) return false;
        const float DT = FMath::Clamp(W->GetTimeSeconds() - PreviousGameTime, 0.f, .1f);
        PreviousGameTime = W->GetTimeSeconds();
        if (Stage == EStage::Initialize)
        {
            P = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
            if (!Initialize()) return End(Failure);
            return Next(EStage::SettleStart);
        }
        if (!IsValid(P) || !IsValid(Target())) return End(TEXT("Original player or canonical vehicle disappeared"));
        if (Mode == EMode::Helicopter) Metrics.Sample(W, FlightMetricStage());
        RecordTrajectory();
        const float Age = W->GetTimeSeconds() - At;

        if (Stage == EStage::SettleStart)
        {
            if (Age < .75f) return false;
            if (!GroundedPlayer()) return Age > 3.f ? End(TEXT("Disclosed start did not settle on native static ground")) : false;
            StartHealth = Health();
            const auto* VehicleHealth = Mode == EMode::Speedboat ? Boat->GetHealthComponent() : Helicopter->GetHealthComponent();
            if (StartHealth < VehicleHealth->GetMaxHealth() - .5f)
                return End(TEXT("Canonical transport was already damaged while settling at its new authored berth/stand"));
            if (Mode == EMode::Helicopter && !Helicopter->GetAircraftMovement()->IsGrounded())
                return Age > 4.f ? End(TEXT("Canonical helicopter did not naturally settle on its new landing surface")) : false;
            return Next(EStage::Approach);
        }
        if (Stage == EStage::Approach || Stage == EStage::WalkClear)
        {
            const int32 Result = WalkStep();
            if (Result < 0) return End(Failure);
            if (!Result) return false;
            if (Stage == EStage::WalkClear)
            {
                if (Mode == EMode::Speedboat && !ExitedOnPier)
                    return End(TEXT("Speedboat exit was not on the actual connected boarding pier"));
                if (Health() < StartHealth - .5f)
                    return End(TEXT("Normal berth/landing use damaged the canonical vehicle"));
                Capture(TEXT("walk-return"));
                return End(TEXT(""));
            }
            Capture(TEXT("reached-boarding"));
            return Next(EStage::FocusPrompt);
        }
        if (Stage == EStage::FocusPrompt)
        {
            if (!GroundedPlayer()) return Age > 3.f ? End(TEXT("Reached boarding point is not supported walking")) : false;
            // Native CanEnter rejects total hull speed >=150cm/s. Arrival can
            // coincide with vertical wave motion, despite a stationary berth.
            // Allow that exact native gate to settle; do not raise its limit,
            // freeze the boat or turn a rejected/occluded trace into success.
            if (Mode == EMode::Speedboat && Boat->GetVelocity().Size() >= 150.f && Age < 6.f) return false;
            if (!Focus()) return End(TEXT("Normal eye trace cannot focus the actual canonical vehicle from the reached approach"));
            if (!FocusedAt) FocusedAt = W->GetTimeSeconds();
            const float FocusAge = W->GetTimeSeconds() - FocusedAt;
            if (FocusAge < .4f) return false;
            const bool Prompt = Mode == EMode::Speedboat ? Boat->IsEntryPromptVisible() : Helicopter->IsEntryPromptVisible();
            if (!Prompt) return FocusAge > 2.f ? End(TEXT("Actual reached vehicle interaction prompt remained hidden")) : false;
            PromptSeen = true;
            Key(EKeys::F, true);
            return Next(EStage::Enter);
        }
        if (Stage == EStage::Enter)
        {
            if (Age < .3f) return false;
            Key(EKeys::F, false);
            const bool DriverMatches = Mode == EMode::Speedboat ? Boat->GetMarineInteraction()->GetDriver() == P
                : Helicopter->GetAircraftInteraction()->GetDriver() == P;
            const bool Engine = Mode == EMode::Speedboat ? Boat->GetMarineMovement()->IsEngineRunning()
                : Helicopter->GetAircraftMovement()->IsEngineRunning();
            if (PC->GetPawn() != Target() || !DriverMatches || !Engine)
                return End(TEXT("Mapped F failed canonical vehicle possession, driver attachment or normal engine start"));
            Entered = true;
            Capture(TEXT("normal-F-entered"));
            FlightOrigin = Target()->GetActorLocation();
            return Next(EStage::Occupied);
        }
        if ((Stage >= EStage::Occupied && Stage <= EStage::Land) || Stage == EStage::Circuit)
        {
            if (PC->GetPawn() != Target() || !Target()->GetActorEnableCollision()
                || Target()->GetActorLocation().ContainsNaN() || Health() < StartHealth - .5f)
                return End(TEXT("Native occupied vehicle lost possession, finite pose, collision or preserved condition"));
            if (Mode == EMode::Helicopter)
            {
                const FVector Here = Helicopter->GetActorLocation(), Velocity = Helicopter->GetVelocity();
                if (!Helicopter->GetHull()->IsSimulatingPhysics() || !Helicopter->GetAircraftMovement()->IsEngineRunning()
                    || Helicopter->GetHull()->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
                    || Velocity.ContainsNaN() || Here.X < -15500 || Here.X > 13500 || Here.Y < -6500 || Here.Y > 9000 || Here.Z > 4800
                    || ((Stage == EStage::Lift || Stage == EStage::Hover) && FVector::Dist2D(Here, FlightOrigin) > 150))
                    return End(TEXT("Sample flight lost native physics/engine/collision or left its finite flight envelope"));
                PeakRise = FMath::Max(PeakRise, float(Helicopter->GetActorLocation().Z - FlightOrigin.Z));
            }
        }
        if (Stage == EStage::Occupied)
        {
            if (Age < .5f) return false;
            if (Mode == EMode::Helicopter) return Next(EStage::Lift);
            FTransform Exit;
            if (!Boat->GetMarineInteraction()->FindExitTransform(Exit, true))
                return End(TEXT("Actual new speedboat berth has no collision-clear native shore exit"));
            PlannedExit = Exit.GetLocation();
            Key(EKeys::F, true);
            return Next(EStage::Exit);
        }
        if (Stage == EStage::Lift)
        {
            if (!FlyTo(FVector(FlightOrigin.X, FlightOrigin.Y, 4000), DT, 250, false))
                return End(TEXT("Production helicopter driver port rejected the possessed player's climb input"));
            if (FMath::Abs(Helicopter->GetActorLocation().Z - 4000) < 110 && FMath::Abs(Helicopter->GetVelocity().Z) < 150)
            {
                if (Helicopter->GetAircraftMovement()->IsGrounded()) return End(TEXT("Aircraft reached cruise height while incorrectly grounded"));
                Lifted = true;
                Capture(TEXT("airborne-warmup-transient-begin"));
                return Next(EStage::Hover);
            }
            return Age > 30.f ? End(TEXT("Actual helicopter did not physically climb to40m above the sample datum")) : false;
        }
        if (Stage == EStage::Hover)
        {
            if (!FlyTo(FVector(FlightOrigin.X, FlightOrigin.Y, 4000), DT, 250, false)) return End(TEXT("Native airborne warmup input rejected"));
            if (Helicopter->GetAircraftMovement()->IsGrounded()) return End(TEXT("Helicopter unexpectedly contacted ground during airborne warmup"));
            if (FMath::Abs(Helicopter->GetActorLocation().Z - 4000) < 110) AirborneWarmupSeconds += DT;
            if (AirborneWarmupSeconds < 15) return Age > 30 ? End(TEXT("Actual airborne warmup did not accumulate15 stable seconds")) : false;
            LastFlightPoint = Helicopter->GetActorLocation();
            Test->AddInfo(TEXT("Native 40m airborne warmup complete; start one approximately683m sample-city circuit. No image readback until after return and physical landing."));
            return Next(EStage::Circuit);
        }
        if (Stage == EStage::Circuit)
        {
            const FVector Here = Helicopter->GetActorLocation(), Velocity = Helicopter->GetVelocity();
            if (Helicopter->GetAircraftMovement()->IsGrounded() || FMath::Abs(Here.Z - 4000) > 180)
                return End(TEXT("Sample circuit unexpectedly landed or lost its native40m flight level"));
            const double StepDistance = FVector::Dist2D(Here, LastFlightPoint); CircuitTravelCm += StepDistance; LastFlightPoint = Here;
            const bool Translating = Velocity.Size2D() >= 200 && StepDistance > .1;
            if (Translating) TranslatingSampleSeconds += DT;
            if (!FlightGoals.IsValidIndex(FlightPoint)) return End(TEXT("Sample circuit waypoint index is invalid"));
            if (!FlyTo(FlightGoals[FlightPoint], DT, 1200, false)) return End(TEXT("Native sample flight input rejected"));
            PC->SetControlRotation((FVector(0, -800, 650) - Here).Rotation());
            const bool Arrived = FVector::Dist2D(Here, FlightGoals[FlightPoint]) < 220
                && FMath::Abs(Here.Z - 4000) < 110 && Velocity.Size2D() < 200;
            if (!Arrived) StableSince = 0;
            else if (!StableSince) StableSince = W->GetTimeSeconds();
            if (Arrived && W->GetTimeSeconds() - StableSince > .25f)
            {
                Test->AddInfo(FString::Printf(TEXT("Native sample flight reached ordered point%d/%d at%s; translating sample%.2fs travel%.1fm"),
                    FlightPoint + 1, FlightGoals.Num(), *Here.ToString(), TranslatingSampleSeconds, CircuitTravelCm / 100));
                if (++FlightPoint == FlightGoals.Num())
                {
                    if (TranslatingSampleSeconds < 30 || CircuitTravelCm < PlannedCircuitCm * .95)
                        return End(TEXT("Actual single circuit did not retain30 translating seconds and95 percent of its planned perimeter"));
                    return Next(EStage::Land);
                }
                At = W->GetTimeSeconds(); StableSince = 0;
            }
            return Age > 65 ? End(FString::Printf(TEXT("Native sample circuit stalled point%d at%s toward%s"), FlightPoint, *Here.ToString(), *FlightGoals[FlightPoint].ToString())) : false;
        }
        if (Stage == EStage::Land)
        {
            if (!FlyTo(FlightOrigin, DT, 250, true)) return End(TEXT("Native return-to-stand descent input rejected"));
            const bool Supported = Helicopter->GetAircraftMovement()->IsGrounded() && Helicopter->GetVelocity().Size() < 25;
            if (!Supported) StableSince = 0;
            else if (!StableSince) StableSince = W->GetTimeSeconds();
            if (Supported && W->GetTimeSeconds() - StableSince > .75f)
            {
                Helicopter->SubmitFlightInput_Implementation(PC, FVector::ZeroVector, 0);
                FHitResult Floor;
                if (!StaticFloor(Helicopter->GetActorLocation(), Floor)
                    || !IsSampleGround(Floor.GetActor())
                    || FVector::Dist2D(Floor.ImpactPoint, FlightOrigin) > 150)
                    return End(TEXT("Physical helicopter landing did not use the actual new sample ground at its authored stand"));
                Landed = true;
                FTransform Exit;
                if (!Helicopter->GetAircraftInteraction()->FindExitTransform(Exit))
                    return End(TEXT("Physically landed canonical helicopter has no collision-clear native walking exit"));
                PlannedExit = Exit.GetLocation();
                Capture(TEXT("native-landed-transient-end"));
                Key(EKeys::F, true);
                return Next(EStage::Exit);
            }
            return Age > 35.f ? End(TEXT("Actual helicopter did not physically settle after bounded descent")) : false;
        }
        if (Stage == EStage::Exit)
        {
            if (Age < .3f) return false;
            Key(EKeys::F, false);
            if (PC->GetPawn() != P) return Age > 3.f ? End(TEXT("Normal F did not exit the actual stationary canonical vehicle")) : false;
            const bool DriverClear = Mode == EMode::Speedboat ? !Boat->GetMarineInteraction()->GetDriver()
                : !Helicopter->GetAircraftInteraction()->GetDriver();
            if (!DriverClear) return End(TEXT("Normal F returned player possession without releasing its vehicle driver"));
            Exited = true;
            ActualExit = P->GetActorLocation();
            return Next(EStage::SettleExit);
        }
        if (Stage == EStage::SettleExit)
        {
            if (!GroundedPlayer()) return Age > 4.f ? End(TEXT("Normal vehicle exit did not settle into visible, collidable static-ground walking")) : false;
            if (Age < .75f) return false;
            FHitResult Floor;
            if (!StaticFloor(P->GetActorLocation(), Floor)) return End(TEXT("Normal vehicle exit has no static floor"));
            if (Mode == EMode::Speedboat)
            {
                ExitedOnPier = HasMesh(Floor.GetActor(), TEXT("SM_C920_BoardingPier"));
                if (!ExitedOnPier) return End(TEXT("Normal speedboat F exit selected a disconnected surface instead of the new boarding pier"));
                WalkGoals = {FVector(5000, -4400, 0), FVector(5000, -3850, 0), FVector(5000, -3150, 0), FVector(5000, -2800, 0)};
            }
            else
            {
                if (!IsSampleGround(Floor.GetActor())) return End(TEXT("Normal helicopter exit did not reach the actual authored ground"));
                const FVector Side = (P->GetActorLocation() - Helicopter->GetActorLocation()).GetSafeNormal2D();
                const FVector Clear = Helicopter->GetActorLocation() + Side * 750;
                WalkGoals = {FVector(Clear.X, Clear.Y, 0)};
            }
            Capture(TEXT("normal-F-ground-exit"));
            WalkPoint = 0;
            return Next(EStage::WalkClear);
        }
        return false;
    }

private:
    APawn* Target() const { return Mode == EMode::Speedboat ? static_cast<APawn*>(Boat) : static_cast<APawn*>(Helicopter); }
    float Health() const { return Mode == EMode::Speedboat ? Boat->GetHealthComponent()->GetCurrentHealth() : Helicopter->GetHealthComponent()->GetCurrentHealth(); }
    FString Name() const { return Mode == EMode::Speedboat ? TEXT("MarinaBerthAccess") : TEXT("HelicopterLandingAccess"); }
    bool Next(EStage Value) { Stage = Value; At = W->GetTimeSeconds(); StableSince = 0; return false; }

    FString FlightMetricStage() const
    {
        if (Stage == EStage::Lift) return TEXT("climb-warmup-transient");
        if (Stage == EStage::Hover) return TEXT("airborne-warmup-transient");
        if (Stage == EStage::Circuit)
        {
            const bool Translating = Helicopter->GetVelocity().Size2D() >= 200
                && FVector::Dist2D(Helicopter->GetActorLocation(), LastFlightPoint) > .1 && AirborneWarmupSeconds >= 15;
            return FString::Printf(TEXT("%s-leg%02d"), Translating ? TEXT("clean-translating-sample") : TEXT("flight-turn-transient"), FlightPoint);
        }
        if (Stage == EStage::Land) return TEXT("return-landing-transient");
        return FString::Printf(TEXT("access-transient-stage%02d"), int32(Stage));
    }

    // Same native velocity/attitude driver used by CurrentHelicopter246::FlyTo.
    // Requests only driver translation/yaw/power; all forces remain production.
    bool FlyTo(const FVector& Destination, float DT, float Limit, bool Landing)
    {
        const FVector Position = Helicopter->GetActorLocation(), Velocity = Helicopter->GetVelocity();
        const FVector Error = Destination - Position, Horizontal(Velocity.X, Velocity.Y, 0);
        const FVector Desired = (FVector(Error.X, Error.Y, 0) * .55f).GetClampedToMaxSize(Limit);
        const auto* Profile = Helicopter->GetProfile();
        const float Current = Helicopter->GetAircraftMovement()->GetCurrentPower();
        const float Boost = FMath::Clamp((Current - Profile->HoverPower) / (1 - Profile->HoverPower), 0.f, 1.f);
        const float Acceleration = Profile->Acceleration * 100 * FMath::Lerp(1.f, Profile->FastFlightAccelerationScale, Boost);
        FVector Local = FRotator(0, Helicopter->GetActorRotation().Yaw, 0).UnrotateVector(
            (((Desired - Horizontal) * .9f + Horizontal * (Profile->AirResistance + .05f)) / Acceleration).GetClampedToMaxSize(1));
        Local.Z = FMath::Clamp(Error.Z / (Profile->VerticalSpeed * 100 * 1.5f), -1., 1.);
        if (Landing) Local.Z = Helicopter->GetAircraftMovement()->IsGrounded() ? 0 : FMath::Min(Local.Z, -.12);
        const float Heading = Landing ? AuthoredPose.Rotator().Yaw : (FVector(0, -800, 650) - Position).Rotation().Yaw;
        FlightYaw = FMath::FInterpConstantTo(FlightYaw,
            FMath::Clamp(FMath::FindDeltaAngleDegrees(Helicopter->GetActorRotation().Yaw, Heading) / 50.f, -1.f, 1.f), DT, 1.5f);
        return Helicopter->SubmitFlightInput_Implementation(PC, Local, FlightYaw) && Helicopter->SubmitPowerInput(PC, 0);
    }

    bool Initialize()
    {
        const auto* Site = ANeonCityMainlandSite::Find(W);
        if (!P || !Site) { Failure = TEXT("Normal player and adopted mainland site are required"); return false; }
        int32 Matches = 0;
        if (Mode == EMode::Speedboat)
        {
            for (TActorIterator<ANeonCityMarineVehicleBase> I(W); I; ++I)
                if (I->VehicleID == UMarineWorldSubsystem::SpeedboatID()) { Boat = *I; ++Matches; }
            if (Matches != 1) { Failure = TEXT("Exactly one actual canonical speedboat is required; no substitute is spawned"); return false; }
            AuthoredPose = Site->GetSpeedboatBerth();
            const auto Defaults = W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot(true);
            const auto* Default = Defaults.FindByPredicate([](const auto& V) { return V.VehicleID == UMarineWorldSubsystem::SpeedboatID(); });
            if (!Default || !Default->Transform.Equals(AuthoredPose, .1f) || !Boat->GetHull()->IsSimulatingPhysics())
            { Failure = TEXT("Actual canonical speedboat did not bootstrap at the authored berth with native physics"); return false; }
            WalkGoals = {FVector(5000, -3150, 0), FVector(5000, -3850, 0), FVector(5000, -4400, 0), FVector(4900, -4700, 0)};
        }
        else
        {
            for (TActorIterator<ANeonCityAircraftVehicleBase> I(W); I; ++I)
                if (I->AircraftID == UAircraftWorldSubsystem::HelicopterID()) { Helicopter = *I; ++Matches; }
            if (Matches != 1) { Failure = TEXT("Exactly one actual canonical helicopter is required; no substitute is spawned"); return false; }
            AuthoredPose = Site->GetHelicopterLanding();
            const auto Defaults = W->GetSubsystem<UAircraftWorldSubsystem>()->BuildPersistenceSnapshot(true);
            const auto* Default = Defaults.FindByPredicate([](const auto& V) { return V.AircraftID == UAircraftWorldSubsystem::HelicopterID(); });
            if (!Default || !Default->Transform.Equals(AuthoredPose, .1f) || !Helicopter->GetHull()->IsSimulatingPhysics())
            { Failure = TEXT("Actual canonical helicopter did not bootstrap at the authored landing with native physics"); return false; }
            if (!Helicopter->GetProfile() || !Helicopter->GetProfile()->IsValidProfile())
            { Failure = TEXT("Actual native helicopter flight profile is invalid; no controller constants are substituted"); return false; }
            if (FVector::Dist2D(AuthoredPose.GetLocation(), FVector(-13500, 7000, 137)) > 1)
            { Failure = TEXT("The bounded sample circuit requires the actual authored northwest landing stand"); return false; }
            FlightGoals = {FVector(-11000, -4500, 4000), FVector(11000, -4500, 4000),
                FVector(11000, 5500, 4000), FVector(-13500, 7000, 4000)};
            FVector Previous(-13500, 7000, 4000);
            for (const FVector& Goal : FlightGoals) { PlannedCircuitCm += FVector::Dist2D(Previous, Goal); Previous = Goal; }
            WalkGoals = {FVector(-13000, 7000, 0), FVector(-13180, 7000, 0)};
        }
        const FVector Start = Mode == EMode::Speedboat ? FVector(5000, -2800, 500) : FVector(-13000, 6100, 500);
        FHitResult Floor;
        if (!StaticFloor(Start, Floor)) { Failure = TEXT("Declared initial fixture has no real native static floor"); return false; }
        InitialStand = Floor.ImpactPoint + FVector(0, 0, P->GetSimpleCollisionHalfHeight() + 3);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C920TransportInitialCapsule), false, P);
        if (W->OverlapBlockingTestByChannel(InitialStand, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(), P->GetSimpleCollisionHalfHeight()), Query))
        { Failure = TEXT("Declared initial fixture capsule is blocked; no obstruction is ignored"); return false; }
        // The only test-authored pose write. It starts on the promenade or at
        // the landing approach, never on/through the pier, hull or vehicle seat.
        P->SetActorLocation(InitialStand, false, nullptr, ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->StopMovementImmediately();
        Test->AddInfo(FString::Printf(TEXT("%s: one initial on-foot fixture%s; actual canonical spawn%s. No later pose, collision or velocity fixtures."),
            *Name(), *InitialStand.ToString(), *AuthoredPose.ToString()));
        return true;
    }

    bool StaticFloor(const FVector& Point, FHitResult& Hit) const
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C920TransportActualFloor), false, P);
        if (Target()) Query.AddIgnoredActor(Target());
        return W->LineTraceSingleByObjectType(Hit, Point + FVector(0, 0, 100), Point - FVector(0, 0, 700),
            FCollisionObjectQueryParams(ECC_WorldStatic), Query) && Hit.IsValidBlockingHit()
            && Hit.GetComponent() && !Hit.GetComponent()->IsSimulatingPhysics()
            && Hit.ImpactNormal.Z >= P->GetCharacterMovement()->GetWalkableFloorZ();
    }
    static bool HasMesh(AActor* Actor, const TCHAR* MeshName)
    {
        const auto* Static = Cast<AStaticMeshActor>(Actor);
        return Static && Static->GetStaticMeshComponent()->GetStaticMesh()
            && Static->GetStaticMeshComponent()->GetStaticMesh()->GetName() == MeshName;
    }
    static bool IsSampleGround(AActor* Actor)
    {
        const auto* Static = Cast<AStaticMeshActor>(Actor);
        const auto* World = Static ? Static->GetWorld() : nullptr;
        if (!World || Static->GetLevel() != World->PersistentLevel
            || Static->GetFName() != TEXT("StaticMeshActor_1661")
            || !Static->ActorHasTag(TEXT("NeonCity.MainlandC920"))
            || !Static->GetActorTransform().Equals(FTransform::Identity, .01)) return false;
        const auto* Mesh = Static->GetStaticMeshComponent()->GetStaticMesh().Get();
        if (!Mesh) return false;
        const FString Path = Mesh->GetPathName();
        // Author4/before.json identifies this exact retained C920 ground owner.
        // ApplyCurvedCoast921 replaces only its mesh with the audited derivative;
        // the inland landing remains on this same native collider at its old pose.
        // Reject other actors, packages and island levels rather than accepting a prefix.
        return Path == TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_SampleGround.SM_C920_SampleGround")
            || (Static->ActorHasTag(TEXT("NeonCity.MainlandC921.CurvedCoast.Revision1"))
                && Path == TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/SM_C921_SampleGround.SM_C921_SampleGround"));
    }
    bool GroundedPlayer() const
    {
        FHitResult Floor;
        return PC->GetPawn() == P && !P->IsHidden() && P->GetActorEnableCollision()
            && P->GetCharacterMovement()->IsMovingOnGround() && StaticFloor(P->GetActorLocation(), Floor)
            && FMath::Abs(P->GetActorLocation().Z - Floor.ImpactPoint.Z - P->GetSimpleCollisionHalfHeight()) < 12;
    }
    int32 WalkStep()
    {
        if (PC->GetPawn() != P || P->IsHidden() || !P->GetActorEnableCollision()
            || P->GetCharacterMovement()->IsSwimming())
        { Failure = TEXT("Physical access route lost normal visible, collidable on-foot movement"); return -1; }
        if (!WalkGoals.IsValidIndex(WalkPoint)) return 1;
        const FVector Delta = WalkGoals[WalkPoint] - P->GetActorLocation();
        if (Delta.Size2D() < 25)
        {
            P->GetCharacterMovement()->StopMovementImmediately();
            ++WalkPoint; ++CompletedWalkPoints; At = W->GetTimeSeconds();
            return WalkPoint == WalkGoals.Num() ? 1 : 0;
        }
        if (W->GetTimeSeconds() - At > 20.f)
        { Failure = FString::Printf(TEXT("Native walking stalled at point%d/%d pose%s goal%s"), WalkPoint, WalkGoals.Num(), *P->GetActorLocation().ToString(), *WalkGoals[WalkPoint].ToString()); return -1; }
        P->AddMovementInput(Delta.GetSafeNormal2D(), FMath::Clamp(float(Delta.Size2D() / 120.), .15f, 1.f));
        PC->SetControlRotation(FRotator(0, Delta.Rotation().Yaw, 0));
        return 0;
    }
    bool Focus()
    {
        FVector Eye; FRotator View;
        P->GetActorEyesViewPoint(Eye, View);
        TArray<FVector> AimPoints;
        if (Mode == EMode::Speedboat)
        {
            // The settled hull datum is below the connected finger pier.
            // Use the native boarding receiver above the deck as the target
            // instead of assuming a fixed height above the hull datum;
            // retain the normal player's trace distance and all occluders.
            TInlineComponentArray<UBoxComponent*> Boxes(Boat);
            for (const auto* Box : Boxes)
                if (Box->GetFName() == TEXT("InteractionBounds"))
                    for (const float Height : {.8f, .4f, 0.f})
                        AimPoints.Add(Box->GetComponentLocation() + Box->GetUpVector() * Box->GetScaledBoxExtent().Z * Height);
        }
        else for (const float Z : {0.f, 25.f, 45.f}) AimPoints.Add(Target()->GetActorLocation() + FVector(0, 0, Z));
        FString Diagnostics;
        for (const FVector& Aim : AimPoints)
        {
            PC->SetControlRotation((Aim - Eye).Rotation());
            if (P->GetInteractionComponent()->RefreshFocusedInteractable() == Target()) return true;
            FHitResult Hit;
            const auto* Interaction = P->GetInteractionComponent();
            Interaction->DetectInteractable(Eye, Eye + (Aim - Eye).GetSafeNormal() * Interaction->InteractionDistance, Hit);
            Diagnostics += FString::Printf(TEXT(" aim%s distance%.1f hit%s/%s at%s canInteract%d;"),
                *Aim.ToString(), FVector::Dist(Eye, Aim), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
                *Hit.ImpactPoint.ToString(), Interaction->CanInteractWith(Target()) ? 1 : 0);
        }
        Test->AddInfo(FString::Printf(TEXT("Native focus eye%s traceReach%.1f:%s"),
            *Eye.ToString(), P->GetInteractionComponent()->InteractionDistance, *Diagnostics));
        return false;
    }
    void Key(FKey K, bool Down)
    {
        if (PC) PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), K,
            Down ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    }
    void ConfigureOutput()
    {
        FString Run; FParse::Value(FCommandLine::Get(), TEXT("NeonCityArtRun="), Run);
        MetricsFile = FPaths::ProjectSavedDir() / TEXT("IslandLandPass1") / FPaths::MakeValidFileName(Run)
            / TEXT("Metrics/MainlandC920-HelicopterCircuit-allframes.csv");
        Run = FPaths::MakeValidFileName(Run.IsEmpty() ? TEXT("unlabelled") : Run);
        Directory = FPaths::ProjectSavedDir() / TEXT("Phase205cUpgrade/MainlandC920/GameplayAccess") / Run
            / (Name() + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*Directory, true);
        FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,walk_point,player_x,player_y,player_z,vehicle_x,vehicle_y,vehicle_z,vehicle_speed_cm_s,health\n"),
            *(Directory / TEXT("trajectory.csv")));
        MapHash = HashFile(FPaths::ProjectContentDir() / TEXT("NeonCity/Maps/NeonCity_SouthBeach_Test.umap"));
        SourceHash = HashFile(FPaths::ProjectDir() / TEXT("Source/NeonCity/Tests/NeonCityMainlandBerth920Tests.cpp"));
    }
    void RecordTrajectory()
    {
        if (W->GetTimeSeconds() - LastRecord < .25f) return;
        LastRecord = W->GetTimeSeconds();
        const FVector Player = P->GetActorLocation(), Vehicle = Target()->GetActorLocation();
        const FString Row = FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),
            LastRecord, int32(Stage), WalkPoint, Player.X, Player.Y, Player.Z, Vehicle.X, Vehicle.Y, Vehicle.Z,
            Target()->GetVelocity().Size(), Health());
        if (Mode == EMode::Helicopter)
        {
            TrajectoryRows += Row;
            if (TrajectoryRows.Len() > 32000) FlushTrajectory();
        }
        else FFileHelper::SaveStringToFile(Row, *(Directory / TEXT("trajectory.csv")),
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }
    bool FlushTrajectory()
    {
        if (TrajectoryRows.IsEmpty()) return true;
        if (!FFileHelper::SaveStringToFile(TrajectoryRows, *(Directory / TEXT("trajectory.csv")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead)) return false;
        TrajectoryRows.Reset(); return true;
    }
    void Capture(const FString& StageName)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtStills"))) return;
        if (Mode == EMode::Helicopter && Stage == EStage::Circuit)
        { ++FailedImages; Test->AddError(TEXT("Image readback is prohibited during the translating flight sample")); return; }
#if WITH_EDITOR
        const FString ImageName = TEXT("C920-") + Name() + TEXT("-") + StageName + TEXT(".jpg");
        if (FNeonCityVisualEvidence::CaptureRenderedJPEG(W, ImageName))
            Images.Add(MakeShared<FJsonValueString>(ImageName));
        else { ++FailedImages; Test->AddError(TEXT("Requested actual gameplay screenshot failed: ") + ImageName); }
#else
        ++FailedImages; Test->AddError(TEXT("Actual screenshot capture requires the editor evidence client"));
#endif
    }
    bool End(const FString& Error)
    {
        if (!Error.IsEmpty())
        {
            Failure = FString::Printf(TEXT("%s stage%d point%d: %s; player%s vehicle%s"), *Name(), int32(Stage), WalkPoint,
                *Error, *GetNameSafe(P), Target() ? *Target()->GetActorTransform().ToString() : TEXT("absent"));
            if (P) Failure += TEXT(" player_pose") + P->GetActorLocation().ToString();
            Test->AddError(Failure);
        }
        Key(EKeys::F, false);
        if (P && PC && PC->GetPawn() == P) P->GetCharacterMovement()->StopMovementImmediately();
        if (Boat) Boat->GetMarineMovement()->StopEngine();
        if (Helicopter)
        {
            if (PC && PC->GetPawn() == Helicopter) Helicopter->SubmitFlightInput_Implementation(PC, FVector::ZeroVector, 0);
            Helicopter->GetAircraftMovement()->StopEngine();
        }
        if (Mode == EMode::Helicopter)
        {
            const bool TrajectoryWritten = FlushTrajectory(), MetricsWritten = Metrics.Flush();
            if (!TrajectoryWritten || !MetricsWritten || IFileManager::Get().FileSize(*MetricsFile) <= 200)
            { Failure = TEXT("Actual flight trajectory/frame telemetry could not be retained"); Test->AddError(Failure); }
        }
        const bool Passed = Failure.IsEmpty() && PromptSeen && Entered && Exited && !FailedImages
            && (Mode == EMode::Speedboat ? ExitedOnPier && CompletedWalkPoints == 8 : Lifted && Landed && CompletedWalkPoints == 3
                && FlightPoint == FlightGoals.Num() && AirborneWarmupSeconds >= 15 && TranslatingSampleSeconds >= 30 && CircuitTravelCm >= PlannedCircuitCm * .95);
        if (Failure.IsEmpty() && !Passed) { Failure = TEXT("Sequence ended without every declared access gate"); Test->AddError(Failure); }
        auto Receipt = MakeShared<FJsonObject>();
        Receipt->SetStringField(TEXT("test"), Name()); Receipt->SetBoolField(TEXT("passed"), Passed);
        Receipt->SetStringField(TEXT("failure"), Failure); Receipt->SetNumberField(TEXT("final_stage"), int32(Stage));
        Receipt->SetStringField(TEXT("initial_disclosed_on_foot_fixture_cm"), InitialStand.ToString());
        Receipt->SetStringField(TEXT("authored_vehicle_spawn"), AuthoredPose.ToString());
        Receipt->SetStringField(TEXT("planned_native_exit_cm"), PlannedExit.ToString());
        Receipt->SetStringField(TEXT("actual_normal_F_exit_cm"), ActualExit.ToString());
        Receipt->SetStringField(TEXT("final_player_cm"), P ? P->GetActorLocation().ToString() : TEXT("absent"));
        Receipt->SetStringField(TEXT("final_vehicle_pose"), Target() ? Target()->GetActorTransform().ToString() : TEXT("absent"));
        Receipt->SetBoolField(TEXT("normal_prompt_seen"), PromptSeen); Receipt->SetBoolField(TEXT("normal_F_entered"), Entered);
        Receipt->SetBoolField(TEXT("normal_F_exited"), Exited); Receipt->SetBoolField(TEXT("exit_onto_actual_boarding_pier"), ExitedOnPier);
        Receipt->SetBoolField(TEXT("physical_local_lift"), Lifted); Receipt->SetBoolField(TEXT("physical_same_stand_landing"), Landed);
        Receipt->SetNumberField(TEXT("peak_physical_rise_cm"), PeakRise); Receipt->SetNumberField(TEXT("completed_walk_waypoints"), CompletedWalkPoints);
        Receipt->SetNumberField(TEXT("planned_sample_circuit_cm"), PlannedCircuitCm); Receipt->SetNumberField(TEXT("actual_sample_circuit_cm"), CircuitTravelCm);
        Receipt->SetNumberField(TEXT("ordered_flight_waypoints"), FlightPoint); Receipt->SetNumberField(TEXT("airborne_warmup_seconds"), AirborneWarmupSeconds);
        Receipt->SetNumberField(TEXT("genuinely_translating_sample_seconds"), TranslatingSampleSeconds);
        Receipt->SetStringField(TEXT("map_md5"), MapHash); Receipt->SetStringField(TEXT("test_source_md5"), SourceHash);
        Receipt->SetArrayField(TEXT("actual_player_images"), Images); Receipt->SetBoolField(TEXT("clean_performance_sample"), Mode == EMode::Helicopter && Passed);
        Receipt->SetStringField(TEXT("performance_policy"), Mode == EMode::Helicopter
            ? TEXT("Always-on raw frame/GPU/VRAM telemetry: MainlandC920-HelicopterCircuit-allframes.csv. Only clean-translating-sample-legNN rows after15s airborne warmup contribute moving-flight timing. At least30s actual motion >=200cm/s; turn/access/climb/landing and six optional transient stills remain separately labelled. No readback during Circuit.")
            : TEXT("No boat performance claim; unchanged native berth access only."));
        if (Mode == EMode::Helicopter) Receipt->SetStringField(TEXT("raw_flight_metrics_csv"), MetricsFile);
        Receipt->SetBoolField(TEXT("cacheable_success"), Passed);
        Receipt->SetStringField(TEXT("scope"), Mode == EMode::Speedboat
            ? TEXT("One disclosed initial promenade fixture; real character walks connected ramp/finger, normal prompt/F boards untouched canonical speedboat, normal F exits onto actual native pier, character walks entire ramp back. No vessel motion/pose/collision fixtures, no full island voyage, no save writes.")
            : TEXT("One disclosed initial ground approach; real character walks to untouched canonical helicopter, normal prompt/F enters, production driver input climbs to40m, completes one approximately683m sample-city perimeter (-135,70)->(-110,-45)->(110,-45)->(110,55)->(-135,70) metres, physically returns/descends onto the same stand, normal F exits and character walks clear. At least15s airborne warmup and30s translating frame/GPU/VRAM evidence. No actor pose/velocity/force/collision/grounded writes after initial player start; this is not a full island flight and performs no save writes."));
        Receipt->SetStringField(TEXT("cache_policy"), TEXT("Reuse successful results only while relevant native geometry/material/collision, authored transport poses and interaction/player/vehicle code remain unchanged. Failed attempts never substitute for success. Subjective handling/visual judgement is separate."));
        FString Json; FJsonSerializer::Serialize(Receipt, TJsonWriterFactory<>::Create(&Json));
        if (!FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("receipt.json")))) Test->AddError(TEXT("Could not write transport access receipt"));
        Test->AddInfo(TEXT("Unique actual gameplay access receipt: ") + Directory / TEXT("receipt.json"));
        return true;
    }

    FAutomationTestBase* Test;
    EMode Mode;
    EStage Stage = EStage::Initialize;
    UWorld* W = nullptr;
    APlayerController* PC = nullptr;
    ANeonCityPlayerCharacter* P = nullptr;
    ANeonCityMarineVehicleBase* Boat = nullptr;
    ANeonCityAircraftVehicleBase* Helicopter = nullptr;
    TArray<FVector> WalkGoals, FlightGoals;
    FNeonCityLandFrameMetrics Metrics{TEXT("MainlandC920-HelicopterCircuit"), true};
    TArray<TSharedPtr<FJsonValue>> Images;
    FVector InitialStand = FVector::ZeroVector, FlightOrigin = FVector::ZeroVector;
    FVector PlannedExit = FVector::ZeroVector, ActualExit = FVector::ZeroVector;
    FVector LastFlightPoint = FVector::ZeroVector;
    FTransform AuthoredPose;
    FString Failure, Directory, MapHash, SourceHash, TrajectoryRows, MetricsFile;
    double Started = 0;
    float At = 0, StableSince = 0, FocusedAt = 0, StartHealth = 0, PeakRise = 0, LastRecord = 0;
    float PreviousGameTime = 0, FlightYaw = 0, AirborneWarmupSeconds = 0, TranslatingSampleSeconds = 0;
    double PlannedCircuitCm = 0, CircuitTravelCm = 0;
    int32 WalkPoint = 0, CompletedWalkPoints = 0, FailedImages = 0, FlightPoint = 0;
    bool PromptSeen = false, Entered = false, Exited = false, ExitedOnPier = false, Lifted = false, Landed = false;
};

bool ValidateMode(FAutomationTestBase& Test)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityDisableAutomaticPersistence")))
    { Test.AddError(TEXT("Use an isolated automatic-persistence-disabled validation process; no live save writes")); return false; }
    if (FParse::Param(FCommandLine::Get(), TEXT("NeonCityArtMetrics")))
    { Test.AddError(TEXT("Gameplay access and optional still readback run separately from clean timing")); return false; }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandMarinaBerthAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.MarinaBerthAccess",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FMainlandMarinaBerthAccess920::RunTest(const FString&)
{
    if (!MainlandBerth920::ValidateMode(*this)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(MainlandBerth920::FAccess(this, MainlandBerth920::EMode::Speedboat));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandHelicopterLandingAccess920,
    "NeonCity.Phase205c.MainlandC920.Standalone.HelicopterLandingAccess",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FMainlandHelicopterLandingAccess920::RunTest(const FString&)
{
    if (!MainlandBerth920::ValidateMode(*this)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(MainlandBerth920::FAccess(this, MainlandBerth920::EMode::Helicopter));
    return true;
}
#endif
