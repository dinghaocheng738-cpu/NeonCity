#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace MainlandTown923
{
const FName TownTag(TEXT("NeonCity.MainlandC923.Town9"));
struct FGoal
{
    FVector Position;
    FString Name;
    bool bEntry=false;
};

/** Only the initial supported fixture and final cleanup restore a pose.
 * Every connector, lane, stair and doorway between them uses the normal
 * character's movement component, capsule, walking speed and collision. */
class FAccess : public IAutomationLatentCommand
{
public:
    FAccess(FAutomationTestBase* InTest,bool InRear,bool InRidge=false):Test(InTest),bRear(InRear),bRidge(InRidge) {}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
        W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
        if(Now-Started>(bRidge?240.:(bRear?300.:120.)))return Finish(TEXT("Town normal walking route exceeded its bounded time"));
        if(!W || !PC || W->GetTimeSeconds()<8)return false;
        if(!Player)
        {
            Player=Cast<ACharacter>(PC->GetPawn());
            if(!Player)return Finish(TEXT("Normal on-foot character is required"));
            if(!Initialize())return Finish(Failure);
            StageAt=W->GetTimeSeconds();return false;
        }
        auto* Movement=Player->GetCharacterMovement();
        if(PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision()
            || Movement->IsSwimming() || Movement->MovementMode==MOVE_Flying)
            return Finish(TEXT("Route lost normal visible collidable on-foot movement"));
        if(!FMath::IsNearlyEqual(Movement->MaxWalkSpeed,WalkSpeed)
            || !FMath::IsNearlyEqual(Movement->MaxStepHeight,StepHeight)
            || !FMath::IsNearlyEqual(Movement->GravityScale,Gravity)
            || !FMath::IsNearlyEqual(Player->GetSimpleCollisionRadius(),Radius)
            || !FMath::IsNearlyEqual(Player->GetSimpleCollisionHalfHeight(),HalfHeight))
            return Finish(TEXT("Normal movement or capsule configuration changed during route"));
        if(!bSettled)
        {
            if(W->GetTimeSeconds()-StageAt<.8f)return false;
            if(!Grounded())return Finish(TEXT("Initial normal capsule did not settle on the actual old promenade"));
            bSettled=true;LastProgress=Now;BestDistance=DBL_MAX;StageAt=W->GetTimeSeconds();
        }
        if(!Goals.IsValidIndex(Goal))return Finish(FString());
        const FVector Position=Player->GetActorLocation();const auto& Target=Goals[Goal];
        const FVector Delta=Target.Position-Position;const double Distance=Delta.Size2D();
        if(Now-LastSample>.08)
        {
            CSV+=FString::Printf(TEXT("%.6f,%.6f,%d,%.3f,%.3f,%.3f,%.3f,%d\n"),
                Now-Started,W->GetTimeSeconds(),Goal,Position.X,Position.Y,Position.Z,
                Movement->Velocity.Size2D(),Movement->IsMovingOnGround()?1:0);LastSample=Now;
        }
        if(Position.Z < -100)return Finish(TEXT("Normal player fell below the supported town"));
        if(Distance<12 && Grounded())
        {
            if(bRidge && FMath::Abs(Position.Z-HalfHeight-Target.Position.Z)>45.)
                return Finish(FString::Printf(TEXT("Public ridge goal reached on the wrong level: %s at %s expected floor %s"),*Target.Name,*Position.ToString(),*Target.Position.ToString()));
            Movement->StopMovementImmediately();
            if(Target.bEntry)
            {
                FHitResult Floor;
                if(!StaticFloor(Position,Floor) || !Floor.GetActor() || !Floor.GetActor()->ActorHasTag(TownTag)
                    || FMath::Abs(Floor.ImpactPoint.Z-Target.Position.Z)>3.)
                    return Finish(FString::Printf(TEXT("Finished first-floor entry lacks the expected actual town support: %s"),*Target.Name));
                ++Entries;
            }
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("goal"),Target.Name);
            Row->SetStringField(TEXT("actual_player_cm"),Position.ToString());Row->SetBoolField(TEXT("grounded"),true);
            Row->SetBoolField(TEXT("finished_entry"),Target.bEntry);Reached.Add(MakeShared<FJsonValueObject>(Row));
            ++Goal;StageAt=W->GetTimeSeconds();LastProgress=Now;BestDistance=DBL_MAX;return false;
        }
        if(Distance<BestDistance-5.){BestDistance=Distance;LastProgress=Now;}
        if(Now-LastProgress>8 || W->GetTimeSeconds()-StageAt>20)
            return Finish(FString::Printf(TEXT("Normal walking blocked at %s; player %s target %s grounded %d floor %s"),
                *Target.Name,*Position.ToString(),*Target.Position.ToString(),Movement->IsMovingOnGround(),
                *GetNameSafe(Movement->CurrentFloor.HitResult.GetActor())));
        PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));
        Player->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Distance/120.),.12f,1.f));
        return false;
    }
private:
    void Add(const FVector& Point,const FString& Name,bool bEntry=false)
    {
        if(!Goals.IsEmpty())
        {
            const FVector Previous=Goals.Last().Position;const double Length=FVector::Dist2D(Previous,Point);
            const int32 Parts=FMath::CeilToInt(Length/1400.);
            for(int32 I=1;I<Parts;++I)Goals.Add({RidgeFloorGoal(FMath::Lerp(Previous,Point,double(I)/Parts)),Name+TEXT(" approach"),false});
        }
        Goals.Add({RidgeFloorGoal(Point),Name,bEntry});
    }
    FVector RidgeFloorGoal(FVector Point)const
    {
        if(!bRidge)return Point;
        double Highest=-DBL_MAX;
        for(const auto& Value:RidgeSteps)
        {
            const auto Step=Value->AsObject();const auto& Polygon=Step->GetArrayField(TEXT("footprint"));
            bool Positive=true,Negative=true;
            for(int32 I=0;I<Polygon.Num();++I)
            {
                const auto& A=Polygon[I]->AsArray();const auto& B=Polygon[(I+1)%Polygon.Num()]->AsArray();
                const double AX=A[0]->AsNumber()*100.,AY=A[1]->AsNumber()*100.,BX=B[0]->AsNumber()*100.,BY=B[1]->AsNumber()*100.;
                const double Cross=(BX-AX)*(Point.Y-AY)-(BY-AY)*(Point.X-AX);
                Positive&=Cross>=-1.e-4;Negative&=Cross<=1.e-4;
            }
            if(Positive || Negative)Highest=FMath::Max(Highest,Step->GetNumberField(TEXT("top_m"))*100.);
        }
        if(Highest>-DBL_MAX)Point.Z=Highest;
        return Point;
    }
    bool Initialize()
    {
        if(bRidge)return InitializeRidge();
        FString Text;TSharedPtr<FJsonObject> Doc;
        if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/MainlandC923/Town/Revision2/town.json")))
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Doc) || !Doc.IsValid()
            || Doc->GetStringField(TEXT("status"))!=TEXT("EXPORTED_SOURCE_READBACK_COMPLETE_NATIVE_VISUAL_PENDING")
            || Doc->GetStringField(TEXT("layout_sha256"))!=TEXT("ad8cf1cd2c64ad53a716b57ca64ac387c94d8dcf689125e0aa3be47606119352"))
        {Failure=TEXT("Frozen Town9 Revision2 source manifest is required");return false;}
        int32 Actors=0;for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->ActorHasTag(TownTag))++Actors;
        if(Actors!=25){Failure=TEXT("Exactly the 25 imported Town9 static actors are required");return false;}
        const auto& Lots=Doc->GetArrayField(TEXT("lots"));const auto Checks=Doc->GetObjectField(TEXT("building_source_checks"));
        Add(bRear?FVector(-6500,8500,55):FVector(16000,1900,55),TEXT("old promenade fixture"));
        if(bRear){Add(FVector(-6500,9400,0),TEXT("west garden connection"));Add(FVector(-9800,9400,0),TEXT("rear lane west"));}
        else {Add(FVector(16700,1900,0),TEXT("east old promenade join"));Add(FVector(17400,1900,0),TEXT("east lane corner north"));Add(FVector(17400,300,0),TEXT("east lane corner south"));}
        bool bLowSecond=false;
        for(const auto& Value:Doc->GetArrayField(TEXT("stairs")))
        {
            const auto Stair=Value->AsObject();const FString ID=Stair->GetStringField(TEXT("lot_id"));
            const bool bIsRear=ID.StartsWith(TEXT("Rear")) || ID==TEXT("Landmark");if(bIsRear!=bRear)continue;
            TSharedPtr<FJsonObject> Lot;
            for(const auto& Candidate:Lots)if(Candidate->AsObject()->GetStringField(TEXT("id"))==ID){Lot=Candidate->AsObject();break;}
            if(!Lot.IsValid()){Failure=TEXT("Town route stair has no source lot");return false;}
            const auto& SourcePosition=Lot->GetArrayField(TEXT("position_m"));const double Floor=SourcePosition[2]->AsNumber()*100.;
            const FString Type=Lot->GetStringField(TEXT("type"));const double Front=Checks->GetObjectField(Type)->GetNumberField(TEXT("entry_front_y_m"));
            const double X=Stair->GetNumberField(TEXT("x_m"))*100.,LaneY=Stair->GetNumberField(TEXT("lane_y_m"))*100.;
            const double BottomY=Stair->GetNumberField(TEXT("bottom_y_m"))*100.,TopY=Stair->GetNumberField(TEXT("top_y_m"))*100.;
            const double DoorY=(SourcePosition[1]->AsNumber()+Front)*100.;
            if(!bRear && ID==TEXT("Low02") && !bLowSecond)
            {Add(FVector(19800,300,0),TEXT("east lane elbow"));Add(FVector(19800,-1300,0),TEXT("east lane lower corner"));bLowSecond=true;}
            const FString Name=FString::Printf(TEXT("%s door%d"),*ID,int32(Stair->GetNumberField(TEXT("door_index"))));
            Add(FVector(X,LaneY,Stair->GetNumberField(TEXT("lane_z_m"))*100.),Name+TEXT(" lane"));
            Add(FVector(X,BottomY-60,0),Name+TEXT(" stair approach"));
            Add(FVector(X,TopY+90,Floor),Name+TEXT(" landing"));
            Add(FVector(X,DoorY+90,Floor+.5),Name+TEXT(" finished room"),true);
            Add(FVector(X,TopY+90,Floor),Name+TEXT(" return landing"));
            Add(FVector(X,BottomY-60,0),Name+TEXT(" return apron"));
            Add(FVector(X,LaneY,Stair->GetNumberField(TEXT("lane_z_m"))*100.),Name+TEXT(" return lane"));
        }
        if(bRear){Add(FVector(7700,9400,0),TEXT("rear lane to east connection"));Add(FVector(7700,8500,55),TEXT("east garden old promenade"));ExpectedEntries=14;}
        else {Add(FVector(19800,-1300,0),TEXT("east return lower corner"));Add(FVector(19800,300,0),TEXT("east return elbow"));Add(FVector(17400,300,0),TEXT("east return south corner"));Add(FVector(17400,1900,0),TEXT("east return north corner"));Add(FVector(16000,1900,55),TEXT("east old promenade return"));ExpectedEntries=3;}
        return InitializePlayerFixture();
    }
    bool InitializeRidge()
    {
        FString Text;TSharedPtr<FJsonObject> Doc;
        if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/MainlandC923/VisibleRidgeTown33/Revision8/town33-plan.json")))
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Doc) || !Doc.IsValid()
            || !Doc->GetBoolField(TEXT("source_combined_site_pass")))
        {Failure=TEXT("Settled combined public exterior source routes are required");return false;}
        int32 Actors=0;for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.VisibleRidgeTown33")))++Actors;
        const auto& Routes=Doc->GetArrayField(TEXT("routes"));
        if(Actors!=16 || Routes.Num()!=3){Failure=TEXT("Exactly16 new hill-town actors and3 public exterior routes are required");return false;}
        for(const auto& Value:Routes)RidgeSteps.Append(Value->AsObject()->GetArrayField(TEXT("steps")));
        for(const auto& Value:Routes)
        {
            const auto Route=Value->AsObject();
            if(!Route->GetBoolField(TEXT("pass_")) || Route->GetStringField(TEXT("access_scope"))!=TEXT("PUBLIC_EXTERIOR_ONLY"))
            {Failure=TEXT("Unapproved public route source");return false;}
            TArray<FVector> Points;
            for(const auto& Point:Route->GetArrayField(TEXT("points_m")))
            {
                const auto& P=Point->AsArray();if(P.Num()!=3){Failure=TEXT("Malformed real route point");return false;}
                Points.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber())*100.);
            }
            if(Points.Num()<2){Failure=TEXT("Incomplete public exterior route");return false;}
            for(int32 I=0;I<Points.Num();++I)Add(Points[I],Route->GetStringField(TEXT("id"))+FString::Printf(TEXT(" ascent%d"),I));
            for(int32 I=Points.Num()-2;I>=0;--I)Add(Points[I],Route->GetStringField(TEXT("id"))+FString::Printf(TEXT(" return%d"),I));
        }
        ExpectedEntries=0;return InitializePlayerFixture();
    }
    bool InitializePlayerFixture()
    {
        Original=Player->GetActorTransform();OriginalControl=PC->GetControlRotation();
        auto* Movement=Player->GetCharacterMovement();WalkSpeed=Movement->MaxWalkSpeed;StepHeight=Movement->MaxStepHeight;Gravity=Movement->GravityScale;
        Radius=Player->GetSimpleCollisionRadius();HalfHeight=Player->GetSimpleCollisionHalfHeight();
        FHitResult Floor;if(!StaticFloor(Goals[0].Position+FVector(0,0,300),Floor)){Failure=TEXT("Initial old promenade fixture has no walkable floor");return false;}
        const FVector Start=Floor.ImpactPoint+FVector(0,0,HalfHeight+3.);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C923TownFixtureCapsule),false,Player);
        if(W->OverlapBlockingTestByChannel(Start,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Query))
        {Failure=TEXT("Initial old promenade fixture has a blocked normal capsule");return false;}
        Player->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);Movement->StopMovementImmediately();bOwnsFixture=true;
        ConfigureOutput();return true;
    }
    bool StaticFloor(const FVector& Position,FHitResult& Hit)const
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(C923TownActualFloor),false,Player);
        return W->LineTraceSingleByObjectType(Hit,Position+FVector(0,0,30),Position-FVector(0,0,700),
            FCollisionObjectQueryParams(ECC_WorldStatic),Query) && Hit.IsValidBlockingHit()
            && Hit.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ();
    }
    bool Grounded()const
    {
        if(bRidge)
        {
            const auto* Movement=Player->GetCharacterMovement();
            return Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
                && Movement->CurrentFloor.GetDistanceToFloor()>=-1. && Movement->CurrentFloor.GetDistanceToFloor()<12.;
        }
        FHitResult Floor;return Player->GetCharacterMovement()->IsMovingOnGround() && StaticFloor(Player->GetActorLocation(),Floor)
            && FMath::Abs(Player->GetActorLocation().Z-HalfHeight-Floor.ImpactPoint.Z)<12.;
    }
    void ConfigureOutput()
    {
        FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
        Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC923")/FPaths::MakeValidFileName(Label);
        IFileManager::Get().MakeDirectory(*Directory,true);
    }
    bool Finish(const FString& Error)
    {
        if(Directory.IsEmpty())ConfigureOutput();
        if(!Error.IsEmpty())Test->AddError(Error);
        if(Error.IsEmpty() && Entries!=ExpectedEntries)Test->AddError(TEXT("Normal route did not enter every frozen source doorway"));
        auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("passed"),Error.IsEmpty() && Entries==ExpectedEntries);
        Result->SetStringField(TEXT("failure"),Error);Result->SetNumberField(TEXT("entries"),Entries);Result->SetArrayField(TEXT("reached"),Reached);
        Result->SetNumberField(TEXT("normal_walk_speed_cm_s"),WalkSpeed);Result->SetNumberField(TEXT("normal_step_height_cm"),StepHeight);
        Result->SetNumberField(TEXT("normal_capsule_radius_cm"),Radius);Result->SetNumberField(TEXT("normal_capsule_half_height_cm"),HalfHeight);
        Result->SetStringField(TEXT("scope"),bRidge?TEXT("Three new public exterior ridge paths, ascent and return, through normal CharacterMovement. One supported initial fixture; no in-route teleport, speed, step, capsule, collision, flight, persistence or production-input changes. No new interior access claim."):TEXT("Town9 source Revision2; one initial supported old-promenade fixture then normal AddMovementInput through real connector/lane/stairs/finished first floors and back. No in-route teleport, speed, step, capsule, collision, flight, persistence or production-input changes. Cleanup restores initial player pose. Not a Dock drive or subjective review."));
        if(bOwnsFixture && IsValid(Player) && IsValid(PC) && PC->GetPawn()==Player)
        {Player->ConsumeMovementInputVector();Player->GetCharacterMovement()->StopMovementImmediately();Player->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(OriginalControl);}
        FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
        const FString Stem=bRidge?TEXT("RidgePublicAccess37"):(bRear?TEXT("TownRearAccess"):TEXT("TownEastAccess"));
        if(!FFileHelper::SaveStringToFile(Text,*(Directory/(Stem+TEXT(".json")))) || !FFileHelper::SaveStringToFile(CSV,*(Directory/(Stem+TEXT("-trajectory.csv")))))
            Test->AddError(TEXT("Town normal-route evidence could not be saved"));
        return true;
    }
    FAutomationTestBase* Test;bool bRear,bRidge=false,bSettled=false,bOwnsFixture=false;
    UWorld* W=nullptr;APlayerController* PC=nullptr;ACharacter* Player=nullptr;
    TArray<FGoal> Goals;TArray<TSharedPtr<FJsonValue>> Reached;int32 Goal=0,Entries=0,ExpectedEntries=0;
    TArray<TSharedPtr<FJsonValue>> RidgeSteps;
    FTransform Original;FRotator OriginalControl;double Started=0,LastProgress=0,BestDistance=DBL_MAX,LastSample=0;
    float StageAt=0,WalkSpeed=0,StepHeight=0,Gravity=0,Radius=0,HalfHeight=0;
    FString Directory,Failure,CSV=TEXT("wall_seconds,game_seconds,goal,x_cm,y_cm,z_cm,speed_cm_s,grounded\n");
};
bool Queue(FAutomationTestBase& Test,bool bRear)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
    {Test.AddError(TEXT("Town tests require isolated standalone validation without the user's save slot"));return false;}
    Test.AddCommand(new FAccess(&Test,bRear));return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandTownRearAccess923,"NeonCity.Phase205c.MainlandC923.Standalone.TownRearAccess",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandTownRearAccess923::RunTest(const FString&){return MainlandTown923::Queue(*this,true);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandTownEastAccess923,"NeonCity.Phase205c.MainlandC923.Standalone.TownEastAccess",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandTownEastAccess923::RunTest(const FString&){return MainlandTown923::Queue(*this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandRidgePublicAccess37,"NeonCity.Phase205c.MainlandC923.Standalone.RidgePublicAccess37",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandRidgePublicAccess37::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
    {AddError(TEXT("Ridge tests require isolated standalone validation without the user's save slot"));return false;}
    AddCommand(new MainlandTown923::FAccess(this,false,true));return true;
}
#endif
