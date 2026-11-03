#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Adventure/IslandSupercar.h"
#include "Combat/HealthComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/MiscTrace.h"

namespace NativeParking921
{
static const FFloatProperty* AutoBrakeProperty()
{ return FindFProperty<FFloatProperty>(UChaosWheeledVehicleMovementComponent::StaticClass(),TEXT("StopThreshold")); }
static float AutoBrake(UChaosWheeledVehicleMovementComponent* M)
{ const auto* P=AutoBrakeProperty();return P?P->GetPropertyValue_InContainer(M):-1.f; }
static void Tick(UVehicleComponent* C)
{ static_cast<UActorComponent*>(C)->TickComponent(1.f/60,LEVELTICK_All,&C->PrimaryComponentTick); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeParkingPolicy921,"NeonCity.Phase205c.MainlandC921.NativeParkingPolicy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNativeParkingPolicy921::RunTest(const FString&)
{
 using namespace NativeParking921;
 if(!TestNotNull(TEXT("Engine native auto-service-brake property"),AutoBrakeProperty()))return false;
 const auto Initialization=UWorld::InitializationValues().CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("C921NativeParkingPolicyUnit")),nullptr,true,ERHIFeatureLevel::Num,&Initialization);
 if(!TestNotNull(TEXT("Isolated native policy fixture world"),W))return false;
 // Native interface events use AActor::ProcessEvent, which intentionally
 // rejects an uninitialized world. Initialize once without starting gameplay.
 W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);A->SetRootComponent(Root);Root->RegisterComponent();
 auto* H=NewObject<UHealthComponent>(A);A->AddInstanceComponent(H);H->RegisterComponent();H->SetMaxHealth(100);H->SetCurrentHealth(100);
 auto* M=NewObject<UChaosWheeledVehicleMovementComponent>(A);A->AddInstanceComponent(M);
 auto* D=NewObject<UVehicleComponent>(A);A->AddInstanceComponent(D);D->RegisterComponent();D->BeginPlay();
 constexpr float Original=17.25f;AutoBrakeProperty()->SetPropertyValue_InContainer(M,Original);
 const float Sleep=M->SleepThreshold,Slope=M->SleepSlopeLimit;
 D->Configure(M,GetDefault<UVehicleData>());
 D->SetDrivingEnabled(false);Tick(D);
 TestEqual(TEXT("Non-opt-in original threshold is untouched"),AutoBrake(M),Original);
 TestEqual(TEXT("Non-opt-in original parked service brake stays held"),M->GetBrakeInput(),1.f);
 D->SetNativeParkingBrake(true);D->SetDrivingEnabled(false);Tick(D);
 TestEqual(TEXT("Healthy opted-in parking makes only automatic service brake condition unreachable"),AutoBrake(M),0.f);
 TestTrue(TEXT("Uses the native parking flag"),M->IsParked());TestEqual(TEXT("No raw service brake wake input"),M->GetBrakeInput(),0.f);
 TestEqual(TEXT("Native sleep speed threshold unchanged"),M->SleepThreshold,Sleep);TestEqual(TEXT("Native sleep slope threshold unchanged"),M->SleepSlopeLimit,Slope);
 D->SetDrivingEnabled(true);
 TestEqual(TEXT("Driver entry immediately restores exact authored automatic braking"),AutoBrake(M),Original);
 TestFalse(TEXT("Driver entry releases native parking"),M->IsParked());
 D->SetDrivingEnabled(false);D->SetMechanicalMotionInterlock(true);
 TestEqual(TEXT("Roof motion interlock immediately restores automatic braking"),AutoBrake(M),Original);
 TestEqual(TEXT("Roof motion retains full raw service brake"),M->GetBrakeInput(),1.f);TestFalse(TEXT("Interlocked roof cannot use ordinary parked policy"),M->IsParked());
 D->SetMechanicalMotionInterlock(false);Tick(D);TestEqual(TEXT("Completed roof can return to healthy parked policy"),AutoBrake(M),0.f);
 auto* Water=W->SpawnActor<ANeonCityWaterVolume>();W->GetSubsystem<UMarineWorldSubsystem>()->RegisterWater(Water);Tick(D);
 TestEqual(TEXT("Actual native deep-water query restores original automatic braking"),AutoBrake(M),Original);
 TestEqual(TEXT("Deep water keeps full service brake"),M->GetBrakeInput(),1.f);TestFalse(TEXT("Deep water is not classified as healthy parked"),M->IsParked());
 W->GetSubsystem<UMarineWorldSubsystem>()->UnregisterWater(Water);Tick(D);TestEqual(TEXT("Dry healthy state can park again"),AutoBrake(M),0.f);
 H->SetCurrentHealth(0);Tick(D);TestEqual(TEXT("Death restores exact original automatic braking"),AutoBrake(M),Original);
 TestEqual(TEXT("Death keeps full service brake"),M->GetBrakeInput(),1.f);TestFalse(TEXT("Dead vehicle does not use ordinary parking"),M->IsParked());
 H->ApplyPersistenceHealth(100,100);Tick(D);TestEqual(TEXT("Persistence-restored healthy car can return to parking"),AutoBrake(M),0.f);
 D->SetNativeParkingBrake(false);TestEqual(TEXT("Disabling opt-in immediately restores exact prior threshold"),AutoBrake(M),Original);TestFalse(TEXT("Disabling opt-in clears native parked flag"),M->IsParked());
 Tick(D);TestEqual(TEXT("Restored non-opt-in policy retains original service braking"),M->GetBrakeInput(),1.f);
 D->SetNativeParkingBrake(true);TestEqual(TEXT("Re-enabled healthy parking suppresses only automatic service brake"),AutoBrake(M),0.f);
 static_cast<UActorComponent*>(D)->EndPlay(EEndPlayReason::Destroyed);TestEqual(TEXT("Component shutdown restores exact borrowed property value"),AutoBrake(M),Original);
 W->DestroyWorld(false);
 AddInfo(TEXT("Native policy fixture checks opt-in boundaries, exact threshold restoration, native deep-water query, roof interlock and death. It has no simulated chassis, so natural sleep and player wake require the separate current-map diagnostic."));
 return true;
}

namespace NativeParking921
{
// UE 5.8 ChaosVehicleMovementComponent.cpp:1504-1511 returns precisely this
// body from its protected GetBodyInstance(). ProcessSleeping:1366-1425 uses
// that body's awake state, not SkeletalMeshComponent::IsAnyRigidBodyAwake()
// (which also visits kinematic bones: SkeletalMeshComponentPhysics.cpp:477).
static FBodyInstance* MainBody(UChaosWheeledVehicleMovementComponent* M)
{return M && M->UpdatedPrimitive?M->UpdatedPrimitive->GetBodyInstance():nullptr;}

static TSharedPtr<FJsonObject> BodyFacts(FBodyInstance* B)
{
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("exists"),B!=nullptr);
 if(!B)return J;
 J->SetBoolField(TEXT("valid"),B->IsValidBodyInstance());
 J->SetBoolField(TEXT("awake"),B->IsInstanceAwake());
 J->SetBoolField(TEXT("simulating"),B->IsInstanceSimulatingPhysics());
 J->SetBoolField(TEXT("non_kinematic"),B->IsNonKinematic());
 J->SetBoolField(TEXT("physics_disabled"),B->IsPhysicsDisabled());
 J->SetNumberField(TEXT("instance_bone_index"),B->InstanceBoneIndex);
 J->SetBoolField(TEXT("auto_weld"),B->bAutoWeld);J->SetBoolField(TEXT("welded"),B->WeldParent!=nullptr);
 if(B->IsValidBodyInstance())
 {
  J->SetStringField(TEXT("world_transform"),B->GetUnrealWorldTransform().ToString());
  J->SetStringField(TEXT("linear_velocity_cm_s"),B->GetUnrealWorldVelocity().ToString());
  J->SetNumberField(TEXT("speed_3d_cm_s"),B->GetUnrealWorldVelocity().Size());
  J->SetStringField(TEXT("angular_velocity_rad_s"),B->GetUnrealWorldAngularVelocityInRadians().ToString());
  J->SetNumberField(TEXT("up_z"),B->GetUnrealWorldTransform().GetUnitAxis(EAxis::Z).Z);
 }
 return J;
}

static bool AddControlFacts(UChaosWheeledVehicleMovementComponent* M,const TSharedPtr<FJsonObject>& J)
{
 const auto* Rep=FindFProperty<FStructProperty>(M->GetClass(),TEXT("ReplicatedState"));
 if(!Rep)return false;
 const void* RepData=Rep->ContainerPtrToValuePtr<void>(M);
 auto Raw=MakeShared<FJsonObject>(),Filtered=MakeShared<FJsonObject>(),Replicated=MakeShared<FJsonObject>();
 for(const TCHAR* Name:{TEXT("ThrottleInput"),TEXT("BrakeInput"),TEXT("SteeringInput"),TEXT("RollInput"),TEXT("PitchInput"),TEXT("YawInput"),TEXT("HandbrakeInput")})
 {
  const auto* F=FindFProperty<FFloatProperty>(M->GetClass(),Name);
  const auto* R=FindFProperty<FFloatProperty>(Rep->Struct,Name);
  if(!F || !R)return false;
  Filtered->SetNumberField(Name,F->GetPropertyValue_InContainer(M));
  Replicated->SetNumberField(Name,R->GetPropertyValue_InContainer(RepData));
  if(FCString::Strcmp(Name,TEXT("HandbrakeInput"))==0)
  {
   const auto* H=FindFProperty<FBoolProperty>(M->GetClass(),TEXT("bRawHandbrakeInput"));
   if(!H)return false;Raw->SetBoolField(Name,H->GetPropertyValue_InContainer(M));
  }
  else
  {
   const auto* V=FindFProperty<FFloatProperty>(M->GetClass(),FName(*(FString(TEXT("Raw"))+Name)));
   if(!V)return false;Raw->SetNumberField(Name,V->GetPropertyValue_InContainer(M));
  }
 }
 J->SetObjectField(TEXT("raw_inputs"),Raw);J->SetObjectField(TEXT("filtered_inputs"),Filtered);J->SetObjectField(TEXT("replicated_inputs"),Replicated);
 return true;
}

static TSharedPtr<FJsonObject> SleepCVars()
{
 auto J=MakeShared<FJsonObject>();
 for(const TCHAR* Name:{TEXT("p.Vehicle.DisableVehicleSleep"),TEXT("p.Vehicle.SleepCounterThreshold"),TEXT("p.Vehicle.ControlInputWakeTolerance")})
 {
  const auto* V=IConsoleManager::Get().FindConsoleVariable(Name);
  if(V)J->SetNumberField(Name,V->GetFloat());else J->SetStringField(Name,TEXT("UNAVAILABLE"));
 }
 return J;
}

static void AddChassisAndRoofFacts(AIslandSupercar* Car,const TSharedPtr<FJsonObject>& J)
{
 auto* M=Car->GetChaosMovement();auto* Root=MainBody(M);
 J->SetStringField(TEXT("movement_updated_primitive"),GetPathNameSafe(M->UpdatedPrimitive.Get()));
 J->SetObjectField(TEXT("main_body"),BodyFacts(Root));
 TArray<TSharedPtr<FJsonValue>> Bodies;
 for(int32 K=0;K<Car->GetMesh()->Bodies.Num();++K)
 {
  auto* B=Car->GetMesh()->Bodies[K];auto V=BodyFacts(B);V->SetNumberField(TEXT("body_index"),K);
  V->SetBoolField(TEXT("is_movement_main_body"),B==Root);
  V->SetStringField(TEXT("bone"),B?Car->GetMesh()->GetBoneName(B->InstanceBoneIndex).ToString():TEXT("None"));
  Bodies.Add(MakeShared<FJsonValueObject>(V));
 }
 J->SetArrayField(TEXT("skeletal_bodies"),Bodies);
 TInlineComponentArray<UStaticMeshComponent*> Parts(Car);
 for(auto* Part:Parts)if(Part->GetFName()==TEXT("NacreRoofFront"))
 {
  auto Roof=BodyFacts(Part->GetBodyInstance());Roof->SetStringField(TEXT("component"),Part->GetPathName());
  Roof->SetStringField(TEXT("world_transform"),Part->GetComponentTransform().ToString());
  Roof->SetStringField(TEXT("relative_transform"),Part->GetRelativeTransform().ToString());
  Roof->SetNumberField(TEXT("collision_enabled"),int32(Part->GetCollisionEnabled()));
  Roof->SetBoolField(TEXT("component_simulating"),Part->IsSimulatingPhysics());J->SetObjectField(TEXT("roof_front_panel"),Roof);
 }
 TArray<TSharedPtr<FJsonValue>> Constraints;
 TInlineComponentArray<UPhysicsConstraintComponent*> Links(Car);
 for(auto* Link:Links)
 {
  auto C=MakeShared<FJsonObject>();const auto& CI=Link->ConstraintInstance;
  C->SetStringField(TEXT("component"),Link->GetPathName());C->SetBoolField(TEXT("registered"),Link->IsRegistered());
  C->SetBoolField(TEXT("native_constraint_valid"),CI.IsValidConstraintInstance());C->SetBoolField(TEXT("terminated"),CI.IsTerminated());
  C->SetBoolField(TEXT("broken"),Link->IsBroken());C->SetBoolField(TEXT("disable_collision"),CI.ProfileInstance.bDisableCollision);
  UPrimitiveComponent *A=nullptr,*B=nullptr;FName ABone,BBone;Link->GetConstrainedComponents(A,ABone,B,BBone);
  C->SetStringField(TEXT("component_a"),GetPathNameSafe(A));C->SetStringField(TEXT("component_b"),GetPathNameSafe(B));
  C->SetStringField(TEXT("bone_a"),ABone.ToString());C->SetStringField(TEXT("bone_b"),BBone.ToString());
  C->SetNumberField(TEXT("linear_x_motion"),int32(CI.GetLinearXMotion()));C->SetNumberField(TEXT("linear_y_motion"),int32(CI.GetLinearYMotion()));C->SetNumberField(TEXT("linear_z_motion"),int32(CI.GetLinearZMotion()));
  C->SetNumberField(TEXT("angular_swing1_motion"),int32(CI.GetAngularSwing1Motion()));C->SetNumberField(TEXT("angular_swing2_motion"),int32(CI.GetAngularSwing2Motion()));C->SetNumberField(TEXT("angular_twist_motion"),int32(CI.GetAngularTwistMotion()));
  FVector Force=FVector::ZeroVector,Torque=FVector::ZeroVector;Link->GetConstraintForce(Force,Torque);C->SetStringField(TEXT("linear_force"),Force.ToString());C->SetStringField(TEXT("angular_force"),Torque.ToString());
  Constraints.Add(MakeShared<FJsonValueObject>(C));
 }
 J->SetArrayField(TEXT("physics_constraints"),Constraints);
}

struct FBodyObservation
{
 bool Initialized=false,PreviousAwake=false;int32 Frames=0,AsleepFrames=0,WakeTransitions=0,SleepTransitions=0;float MaxSpeed=0;
 void Observe(FBodyInstance* B)
 {
  if(!B || !B->IsValidBodyInstance())return;const bool Awake=B->IsInstanceAwake();++Frames;AsleepFrames+=!Awake;
  if(Initialized && Awake!=PreviousAwake){WakeTransitions+=Awake;SleepTransitions+=!Awake;}
  PreviousAwake=Awake;Initialized=true;MaxSpeed=FMath::Max(MaxSpeed,float(B->GetUnrealWorldVelocity().Size()));
 }
 TSharedPtr<FJsonObject> Facts()const
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("observed_frames"),Frames);J->SetNumberField(TEXT("asleep_frames"),AsleepFrames);
  J->SetNumberField(TEXT("wake_transitions"),WakeTransitions);J->SetNumberField(TEXT("sleep_transitions"),SleepTransitions);J->SetNumberField(TEXT("max_speed_cm_s"),MaxSpeed);return J;
 }
};

class FParkedDiagnostic : public IAutomationLatentCommand
{
public:
 explicit FParkedDiagnostic(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();auto* W=NeonCityValidationWorld();
  if(FPlatformTime::Seconds()-Started>40)return End(TEXT("Current-map native parked-state diagnostic timed out"));
  if(!W || W->GetTimeSeconds()<8)return false;
  if(!At)At=W->GetTimeSeconds();
  // Read every frame before the JSON sampling cadence, so a short native
  // sleep followed by external wake cannot disappear between 1s snapshots.
  for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC920")))
   Observations.FindOrAdd(I->VehicleID).Observe(MainBody(I->GetChaosMovement()));
  if(W->GetTimeSeconds()-LastSample<1)return false;LastSample=W->GetTimeSeconds();
  int32 Count=0,Asleep=0,Grounded=0,NativeParked=0;TArray<TSharedPtr<FJsonValue>> Cars;
  const auto* Filtered=FindFProperty<FFloatProperty>(UChaosWheeledVehicleMovementComponent::StaticClass(),TEXT("BrakeInput"));
  const auto* Rep=FindFProperty<FStructProperty>(UChaosWheeledVehicleMovementComponent::StaticClass(),TEXT("ReplicatedState"));
  const auto* RepBrake=Rep?FindFProperty<FFloatProperty>(Rep->Struct,TEXT("BrakeInput")):nullptr;
  if(!Filtered || !Rep || !RepBrake)return End(TEXT("Native filtered/replicated brake diagnostic fields are unavailable"));
  for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC920")))
  {
   auto* M=I->GetChaosMovement();auto* Root=MainBody(M);
   if(!Root || !Root->IsValidBodyInstance() || !Root->IsNonKinematic())return End(TEXT("Actual Chaos movement main body is missing/invalid/kinematic"));
   const bool AnyAwake=I->GetMesh()->IsAnyRigidBodyAwake(),Awake=Root->IsInstanceAwake();int32 Contacts=0,OutputContacts=0;
   TArray<TSharedPtr<FJsonValue>> Wheels;
   for(int32 K=0;K<M->Wheels.Num();++K)
   {
    const auto& S=M->GetWheelState(K);Contacts+=S.bInContact?1:0;auto V=MakeShared<FJsonObject>();
    V->SetNumberField(TEXT("index"),K);V->SetBoolField(TEXT("status_in_contact"),S.bInContact);
    V->SetStringField(TEXT("contact_point"),S.ContactPoint.ToString());V->SetNumberField(TEXT("spring_force"),S.SpringForce);
    V->SetNumberField(TEXT("drive_torque"),S.DriveTorque);V->SetNumberField(TEXT("brake_torque"),S.BrakeTorque);
    const auto& Output=M->PhysicsVehicleOutput();
    if(Output && Output->Wheels.IsValidIndex(K)){const bool Hit=Output->Wheels[K].InContact;OutputContacts+=Hit?1:0;V->SetBoolField(TEXT("native_output_in_contact"),Hit);}
    Wheels.Add(MakeShared<FJsonValueObject>(V));
   }
   ++Count;Asleep+=!Awake?1:0;Grounded+=Contacts==4 && OutputContacts==4?1:0;
   NativeParked+=M->IsParked() && AutoBrake(M)==0?1:0;
   const void* RepData=Rep->ContainerPtrToValuePtr<void>(M);
   const float ActualFiltered=Filtered->GetPropertyValue_InContainer(M),ActualRep=RepBrake->GetPropertyValue_InContainer(RepData);
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),I->GetPathName());J->SetStringField(TEXT("id"),I->VehicleID.ToString(EGuidFormats::Digits));
   // Keep the previous all-bodies field for comparison; only the actual Chaos
   // movement body determines natural chassis sleep and first-driver wake.
   J->SetBoolField(TEXT("awake"),AnyAwake);J->SetBoolField(TEXT("main_body_awake"),Awake);J->SetBoolField(TEXT("parked"),M->IsParked());J->SetNumberField(TEXT("contacts"),Contacts);
   J->SetNumberField(TEXT("native_output_contacts"),OutputContacts);J->SetArrayField(TEXT("wheel_states"),Wheels);
   J->SetNumberField(TEXT("raw_brake"),M->GetBrakeInput());J->SetNumberField(TEXT("raw_throttle"),M->GetThrottleInput());
   J->SetNumberField(TEXT("filtered_brake"),ActualFiltered);J->SetNumberField(TEXT("replicated_brake"),ActualRep);
   J->SetNumberField(TEXT("auto_brake_stop_threshold"),AutoBrake(M));J->SetNumberField(TEXT("sleep_threshold"),M->SleepThreshold);
   J->SetNumberField(TEXT("sleep_slope_limit"),M->SleepSlopeLimit);J->SetObjectField(TEXT("sleep_cvars"),SleepCVars());
   if(!AddControlFacts(M,J))return End(TEXT("Complete native raw/filtered/replicated control fields unavailable"));
   AddChassisAndRoofFacts(*I,J);J->SetObjectField(TEXT("main_body_frame_observations"),Observations.FindChecked(I->VehicleID).Facts());
   J->SetNumberField(TEXT("speed_3d_cm_s"),I->GetMesh()->GetPhysicsLinearVelocity().Size());J->SetNumberField(TEXT("up_z"),I->GetActorUpVector().Z);
   J->SetStringField(TEXT("controller"),GetPathNameSafe(I->GetController()));Cars.Add(MakeShared<FJsonValueObject>(J));
   TRACE_BOOKMARK(TEXT("C921ParkedVehicle id=%s awake=%d contacts=%d rawBrake=%.3f filteredBrake=%.3f repBrake=%.3f park=%d speed=%.3f"),
    *I->VehicleID.ToString(EGuidFormats::Digits),Awake,Contacts,M->GetBrakeInput(),ActualFiltered,ActualRep,M->IsParked(),I->GetMesh()->GetPhysicsLinearVelocity().Size());
  }
  auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("game_seconds"),W->GetTimeSeconds());Sample->SetArrayField(TEXT("cars"),Cars);Samples.Add(MakeShared<FJsonValueObject>(Sample));
  if(Count!=7)return End(FString::Printf(TEXT("Expected7 retained mainland cars, found%d"),Count));
  if(W->GetTimeSeconds()-At<12)return false;
  if(Asleep!=Count || Grounded!=Count || NativeParked!=Count)return End(FString::Printf(TEXT("Current authored cars did not naturally settle/sleep: mainBodyAsleep%d grounded%d nativeParked%d total%d"),Asleep,Grounded,NativeParked,Count));
  return End(TEXT(""));
 }
private:
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetArrayField(TEXT("samples"),Samples);
   J->SetStringField(TEXT("scope"),TEXT("Actual7 retained mainland Chaos cars, unchanged spawn/physics/collision/velocity.12s observation after world8s. Natural sleep uses the identical UpdatedPrimitive BodyInstance returned by UE5.8 movement.GetBodyInstance, matching native ProcessSleeping. Legacy IsAnyRigidBodyAwake remains diagnostic only; full per-bone/roof/constraint/controls/cvars and per-frame root sleep/wake counts are recorded. All4 actual output/status wheel contacts and healthy native parking are required. No forced sleep or input; driver wake is separate normal F flow."));
   J->SetStringField(TEXT("engine_source_basis"),TEXT("UE5.8 ChaosVehicleMovementComponent.cpp:1504-1511 GetBodyInstance=UpdatedPrimitive->GetBodyInstance;1366-1425 ProcessSleeping uses this body. SkeletalMeshComponentPhysics.cpp:477-494 IsAnyRigidBodyAwake visits every skeletal body including kinematic bodies."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("NativeParkedState.json"))))Test->AddError(TEXT("Native parked-state receipt could not be written"));return true;
 }
  FAutomationTestBase* Test;double Started=0;float At=0,LastSample=0;TArray<TSharedPtr<FJsonValue>> Samples;TMap<FGuid,FBodyObservation> Observations;
};

class FFirstWake : public IAutomationLatentCommand
{
public:
 explicit FFirstWake(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
   if(bEnding)return Cleanup();
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>45)return End(TEXT("Native first-driver-wake microflow timed out"));
  if(!PC || W->GetTimeSeconds()<8)return false;const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal on-foot player is missing"));
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC920")) && !I->GetController()){Car=*I;break;}
   if(!Car)return End(TEXT("No actual retained mainland car available"));M=Car->GetChaosMovement();Health=Car->GetHealthComponent()->GetCurrentHealth();return Next(1);
  }
  if(Stage==1)
  {
   const auto* Root=MainBody(M);if(!Root || !Root->IsValidBodyInstance() || !Root->IsNonKinematic())return End(TEXT("First-wake actual Chaos movement main body is missing/invalid/kinematic"));
   if(Root->IsInstanceAwake()){if(Age<12)return false;return End(TEXT("Actual movement main body did not naturally sleep before its first driver entry"));}
   InitialBodyFacts=BodyFacts(MainBody(M));InitialAnyAwake=Car->GetMesh()->IsAnyRigidBodyAwake();
   if(!M->IsParked() || AutoBrake(M)!=0)return End(TEXT("Initial car is not using healthy native parking"));bInitiallyAsleep=true;
   const FVector Point=Car->GetActorTransform().TransformPosition(FVector(0,180,0));FHitResult Floor;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(C921DriverWakeFloor),false,P);
   if(!W->LineTraceSingleByObjectType(Floor,Point+FVector(0,0,300),Point-FVector(0,0,600),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
    || Floor.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ())return End(TEXT("Single initial driver-door fixture lacks native static floor"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))
    return End(TEXT("Single initial driver-door fixture is obstructed"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return Next(2);
  }
  if(Stage==2 && Age>.7f)
  {
   if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Initial on-foot driver fixture did not settle normally"));
   FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);bool Focus=false;
   for(float X:{0.f,-70.f,70.f})
   {
    for(float Z:{80.f,110.f,140.f})
    {
     PC->SetControlRotation((Car->GetActorTransform().TransformPosition(FVector(X,0,Z))-Eye).Rotation());
     if(P->GetInteractionComponent()->RefreshFocusedInteractable()==Car){Focus=true;break;}
    }
    if(Focus)break;
   }
   if(!Focus)return End(TEXT("Actual sleeping car cannot be focused by normal interaction"));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3 && Age>.25f)
  {
   Key(EKeys::F,false);const auto* Root=MainBody(M);
   // Possession explicitly wakes in BaseVehicle::PossessedBy, but a neutral
   // driver may naturally sleep again before this sample. Native UE sleeping
   // is input-driven (ProcessSleeping:1387-1410), so F validates ownership and
   // policy restoration; the subsequent real S input must wake and drive.
   if(PC->GetPawn()!=Car || !Root || !Root->IsValidBodyInstance() || M->IsParked() || AutoBrake(M)<=0)
    return End(TEXT("Normal F driver entry did not possess/release parking/restore original automatic braking"));
   EntryBodyFacts=BodyFacts(MainBody(M));EntryAnyAwake=Car->GetMesh()->IsAnyRigidBodyAwake();bNativeEntryPolicy=true;Start=Car->GetActorLocation();Key(EKeys::S,true);return Next(4);
  }
  if(Stage==4 && Age>.45f)
  {
   Key(EKeys::S,false);Key(EKeys::SpaceBar,true);Travel=FVector::Dist2D(Start,Car->GetActorLocation());
   const auto* Root=MainBody(M);DriveBodyFacts=BodyFacts(MainBody(M));
   if(!Root || !Root->IsInstanceAwake() || Travel<15 || M->GetForwardSpeed()>-20 || Car->GetActorLocation().Y-Start.Y<15)
    return End(TEXT("Ordinary S did not wake and reverse the real south-facing car north toward the clear aisle"));
   bNativeInputWake=true;bNativeDrive=true;return Next(5);
  }
  if(Stage==5)
  {
   if(Car->GetHealthComponent()->GetCurrentHealth()<Health-.1f)return End(TEXT("First-wake native movement caused collision damage"));
   if(FMath::Abs(M->GetForwardSpeed())>30){if(Age<5)return false;return End(TEXT("Normal service brake did not slow first-wake movement"));}
   Key(EKeys::SpaceBar,false);Key(EKeys::F,true);return Next(6);
  }
  if(Stage==6 && Age>.25f)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=P)return End(TEXT("Normal F could not exit after first-wake native drive"));
   if(Age<1)return false;if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal first-wake F exit did not settle on actual ground"));
   bNormalExit=true;return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Error)
 {
   for(FKey K:{EKeys::F,EKeys::S,EKeys::SpaceBar})Key(K,false);
   if(PC && IsValid(Car) && PC->GetPawn()==Car)
   {
    PendingError=Error;bEnding=true;bCleanupAttempted=true;CleanupStarted=FPlatformTime::Seconds();Key(EKeys::SpaceBar,true);return false;
   }
   return Finish(Error);
  }
  bool Cleanup()
  {
   const double Elapsed=FPlatformTime::Seconds()-CleanupStarted;
   if(!PC || !IsValid(Car))return Finish(PendingError+TEXT("; normal-F cleanup lost its actual player/car"));
   if(PC->GetPawn()!=Car)
   {
    for(FKey K:{EKeys::F,EKeys::S,EKeys::SpaceBar})Key(K,false);
    if(PC->GetPawn()==P && IsValid(P) && P->GetCharacterMovement()->IsMovingOnGround())
    {bCleanupNormalExit=true;return Finish(PendingError);}
    if(Elapsed<9)return false;
    return Finish(PendingError+TEXT("; normal-F cleanup did not restore the grounded on-foot player"));
   }
   if(Elapsed>10){for(FKey K:{EKeys::F,EKeys::S,EKeys::SpaceBar})Key(K,false);return Finish(PendingError+TEXT("; normal-F cleanup could not exit the car within10s"));}
   if(CleanupStage==0)
   {
    if(FMath::Abs(M->GetForwardSpeed())>30)return false;
    Key(EKeys::SpaceBar,false);Key(EKeys::F,true);CleanupPressedAt=FPlatformTime::Seconds();CleanupStage=1;return false;
   }
   if(CleanupStage==1 && FPlatformTime::Seconds()-CleanupPressedAt>.25)
   {Key(EKeys::F,false);CleanupStage=2;return false;}
   if(CleanupStage==2 && FPlatformTime::Seconds()-CleanupPressedAt>1.)
   {Key(EKeys::SpaceBar,true);CleanupStage=0;}
   return false;
  }
  bool Finish(const FString& Error)
  {
   if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
   J->SetBoolField(TEXT("initially_naturally_asleep"),bInitiallyAsleep);J->SetBoolField(TEXT("normal_F_possessed_released_parking_restored_threshold"),bNativeEntryPolicy);
   J->SetBoolField(TEXT("normal_S_woke_main_body"),bNativeInputWake);
  J->SetBoolField(TEXT("normal_S_actual_reverse_toward_aisle"),bNativeDrive);J->SetBoolField(TEXT("normal_F_grounded_exit"),bNormalExit);J->SetNumberField(TEXT("actual_drive_cm"),Travel);
  J->SetStringField(TEXT("vehicle_id"),Car?Car->VehicleID.ToString(EGuidFormats::Digits):TEXT("absent"));
   if(InitialBodyFacts)J->SetObjectField(TEXT("initial_main_body"),InitialBodyFacts);
   if(EntryBodyFacts)J->SetObjectField(TEXT("driver_entry_main_body"),EntryBodyFacts);
    if(DriveBodyFacts)J->SetObjectField(TEXT("drive_input_main_body"),DriveBodyFacts);
   J->SetBoolField(TEXT("initial_legacy_any_body_awake"),InitialAnyAwake);J->SetBoolField(TEXT("entry_legacy_any_body_awake"),EntryAnyAwake);
   J->SetStringField(TEXT("natural_sleep_and_wake_body"),TEXT("Actual movement.UpdatedPrimitive->GetBodyInstance, identical to protected native GetBodyInstance implementation; IsAny retained only as diagnostic."));
   J->SetBoolField(TEXT("failure_cleanup_attempted"),bCleanupAttempted);J->SetBoolField(TEXT("failure_cleanup_normal_F_grounded_exit"),bCleanupNormalExit);
   J->SetBoolField(TEXT("on_foot_player_restored_at_end"),PC && IsValid(P) && PC->GetPawn()==P);
   J->SetStringField(TEXT("scope"),TEXT("Current retained mainland car must first naturally sleep. One supported on-foot fixture; normal F possession releases native parking and restores automatic braking without requiring a neutral driver to stay awake. Mapped S0.45s must wake the actual main body and reverse north toward the clear aisle; Space stop/F grounded exit. Failure cleanup also uses ordinary Space/F to restore on-foot test isolation. No vehicle pose/velocity, forced sleep, collision change, route or save fixture."));
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("NativeParkingFirstWake.json"))))Test->AddError(TEXT("First driver wake receipt could not be saved"));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandSupercar* Car=nullptr;UChaosWheeledVehicleMovementComponent* M=nullptr;
   double Started=0;float At=0,Health=0;int32 Stage=0;FVector Start;double Travel=0;bool bInitiallyAsleep=false,bNativeEntryPolicy=false,bNativeInputWake=false,bNativeDrive=false,bNormalExit=false;
   TSharedPtr<FJsonObject> InitialBodyFacts,EntryBodyFacts,DriveBodyFacts;bool InitialAnyAwake=false,EntryAnyAwake=false;
   bool bEnding=false,bCleanupAttempted=false,bCleanupNormalExit=false;FString PendingError;double CleanupStarted=0,CleanupPressedAt=0;int32 CleanupStage=0;
};

/** Focused, reversible diagnostic selector. It does not change production roof
 * policy. A removed joint also removes that joint's collision-pair suppression;
 * an unchanged awake result is therefore inconclusive rather than a refutation. */
class FNacreConstraintAB : public IAutomationLatentCommand
{
public:
 explicit FNacreConstraintAB(FAutomationTestBase* T):Test(T){}
 ~FNacreConstraintAB()override{RestorePair();}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();auto* W=NeonCityValidationWorld();
  if(FPlatformTime::Seconds()-Started>35)return End(TEXT("Focused Nacre joint A/B diagnostic timed out"));
  if(!W || W->GetTimeSeconds()<8)return false;
  if(!Car.IsValid())
  {
   AIslandSupercar* Selected=nullptr;UPhysicsConstraintComponent* Pair=nullptr;
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.MainlandC920")) && !I->GetController())
   {
    TInlineComponentArray<UPhysicsConstraintComponent*> Links(*I);
    for(auto* L:Links)if(L->GetFName()==TEXT("RoofChassisContactFilter") && L->ConstraintInstance.IsValidConstraintInstance())
     if(!Selected || I->GetPathName()<Selected->GetPathName()){Selected=*I;Pair=L;}
   }
   if(!Selected || !Pair)return End(TEXT("No actual unoccupied mainland Nacre with its valid roof-chassis pair"));
   UPrimitiveComponent *A=nullptr,*B=nullptr;Pair->GetConstrainedComponents(A,BoneA,B,BoneB);
   if(A!=Selected->GetMesh() || BoneA!=TEXT("Root") || !B || B->GetFName()!=TEXT("NacreRoofFront"))
    return End(TEXT("Focused pair is not exact Root chassis to NacreRoofFront"));
   const auto& CI=Pair->ConstraintInstance;
   if(CI.GetLinearXMotion()!=LCM_Free || CI.GetLinearYMotion()!=LCM_Free || CI.GetLinearZMotion()!=LCM_Free
    || CI.GetAngularSwing1Motion()!=ACM_Free || CI.GetAngularSwing2Motion()!=ACM_Free || CI.GetAngularTwistMotion()!=ACM_Free
    || !CI.ProfileInstance.bDisableCollision || !B->GetBodyInstance() || B->GetBodyInstance()->IsNonKinematic())
    return End(TEXT("Focused pair does not match the diagnosed six-free-axis kinematic roof filter"));
   Car=Selected;Link=Pair;ComponentA=A;ComponentB=B;OriginalFrameA=CI.GetRefFrame(EConstraintFrame::Frame1);OriginalFrameB=CI.GetRefFrame(EConstraintFrame::Frame2);
   RoofCollision=B->GetCollisionEnabled();ChassisCollision=A->GetCollisionEnabled();OriginalSleep=Selected->GetChaosMovement()->SleepThreshold;
   OriginalSlope=Selected->GetChaosMovement()->SleepSlopeLimit;OriginalAutoBrake=AutoBrake(Selected->GetChaosMovement());
   InitialHealth=Selected->GetHealthComponent()->GetCurrentHealth();At=W->GetTimeSeconds();Capture(TEXT("A_original_start"));
  }
  auto* M=Car->GetChaosMovement();auto* Root=MainBody(M);
  if(!Root || !Root->IsValidBodyInstance() || Car->GetController())return End(TEXT("Focused joint A/B lost actual unoccupied main body"));
  if(M->SleepThreshold!=OriginalSleep || M->SleepSlopeLimit!=OriginalSlope || AutoBrake(M)!=OriginalAutoBrake
   || ComponentA->GetCollisionEnabled()!=ChassisCollision || ComponentB->GetCollisionEnabled()!=RoofCollision)
   return End(TEXT("A/B diagnostic detected an unrelated sleep/brake/collision-setting mutation"));
  if(Car->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.1f)return End(TEXT("A/B diagnostic caused collision damage; pair restored"));
  Observations[Stage].Observe(Root);
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0 && Age>=3)
  {
   Capture(TEXT("A_original_end"));Link->TermComponentConstraint();bPairRemoved=true;
   if(Link->ConstraintInstance.IsValidConstraintInstance())return End(TEXT("Native pair did not terminate for focused B"));
   Stage=1;At=W->GetTimeSeconds();Capture(TEXT("B_only_pair_removed_start"));
  }
  else if(Stage==1 && Age>=3)
  {
   Capture(TEXT("B_only_pair_removed_end"));if(!RestorePair())return End(TEXT("Exact native pair restoration failed"));
   Stage=2;At=W->GetTimeSeconds();Capture(TEXT("A_restored_start"));
  }
  else if(Stage==2 && Age>=3){Capture(TEXT("A_restored_end"));return End(TEXT(""));}
  return false;
 }
private:
 bool RestorePair()
 {
  if(!bPairRemoved)return true;
  if(!Link.IsValid() || !ComponentA.IsValid() || !ComponentB.IsValid())return false;
  Link->InitComponentConstraint();
  auto& CI=Link->ConstraintInstance;CI.SetRefFrame(EConstraintFrame::Frame1,OriginalFrameA);CI.SetRefFrame(EConstraintFrame::Frame2,OriginalFrameB);
  UPrimitiveComponent *A=nullptr,*B=nullptr;FName BA,BB;Link->GetConstrainedComponents(A,BA,B,BB);
  bRestored=CI.IsValidConstraintInstance() && !CI.IsTerminated() && !Link->IsBroken() && CI.ProfileInstance.bDisableCollision
   && A==ComponentA.Get() && B==ComponentB.Get() && BA==BoneA && BB==BoneB
   && CI.GetRefFrame(EConstraintFrame::Frame1).Equals(OriginalFrameA,1.e-5f) && CI.GetRefFrame(EConstraintFrame::Frame2).Equals(OriginalFrameB,1.e-5f)
   && CI.GetLinearXMotion()==LCM_Free && CI.GetLinearYMotion()==LCM_Free && CI.GetLinearZMotion()==LCM_Free
   && CI.GetAngularSwing1Motion()==ACM_Free && CI.GetAngularSwing2Motion()==ACM_Free && CI.GetAngularTwistMotion()==ACM_Free;
  if(bRestored)bPairRemoved=false;return bRestored;
 }
 void Capture(const TCHAR* Phase)
 {
  if(!Car.IsValid())return;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("phase"),Phase);J->SetNumberField(TEXT("game_seconds"),Car->GetWorld()->GetTimeSeconds());
  J->SetStringField(TEXT("actor"),Car->GetPathName());AddChassisAndRoofFacts(Car.Get(),J);AddControlFacts(Car->GetChaosMovement(),J);
  J->SetObjectField(TEXT("stage_frame_observations"),Observations[Stage].Facts());J->SetObjectField(TEXT("sleep_cvars"),SleepCVars());
  Samples.Add(MakeShared<FJsonValueObject>(J));
 }
 bool End(const FString& Error)
 {
  const bool Restored=RestorePair();const FString FinalError=Restored?Error:Error+TEXT("; native pair restoration could not be verified");
  if(!FinalError.IsEmpty())Test->AddError(FinalError);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("diagnostic_completed"),FinalError.IsEmpty());
  J->SetStringField(TEXT("failure"),FinalError);J->SetArrayField(TEXT("samples"),Samples);J->SetBoolField(TEXT("native_pair_restored_exactly"),bRestored);
  J->SetBoolField(TEXT("joint_wake_hypothesis_supported"),Observations[0].Frames>0 && Observations[0].AsleepFrames==0 && Observations[1].AsleepFrames>15
   && Observations[2].Frames>0 && Observations[2].AsleepFrames==0 && bRestored);
  J->SetStringField(TEXT("scope"),TEXT("One exact actual mainland Nacre Root-to-kinematic-FrontPanel six-free-axis collision-filter joint:3s original A,3s native TermComponentConstraint B,exact same pair/ref-frames reinitialized and verified,3s restored A. No vehicle pose/velocity/input, forced sleep, threshold, roof/chassis collision property, or production policy change. Other vehicles untouched."));
  J->SetStringField(TEXT("interpretation_limit"),TEXT("Terminating a joint also removes its pair collision suppression. If B remains awake, a new roof/chassis self-contact can confound this negative result. Only B natural sleep plus restored A sustained wake supports the joint-wake hypothesis; this receipt alone is not a production fix/PASS."));
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Dir,true);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("NacreParkingConstraintAB.json"))))Test->AddError(TEXT("Focused Nacre pair A/B receipt could not be saved"));return true;
 }
 FAutomationTestBase* Test;TWeakObjectPtr<AIslandSupercar> Car;TWeakObjectPtr<UPhysicsConstraintComponent> Link;
 TWeakObjectPtr<UPrimitiveComponent> ComponentA,ComponentB;FName BoneA,BoneB;FTransform OriginalFrameA,OriginalFrameB;
 ECollisionEnabled::Type RoofCollision=ECollisionEnabled::NoCollision,ChassisCollision=ECollisionEnabled::NoCollision;
 float OriginalSleep=0,OriginalSlope=0,OriginalAutoBrake=0,InitialHealth=0,At=0;double Started=0;int32 Stage=0;
 bool bPairRemoved=false,bRestored=false;FBodyObservation Observations[3];TArray<TSharedPtr<FJsonValue>> Samples;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeParkedState921,"NeonCity.Phase205c.MainlandC921.Standalone.NativeParkedState",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FNativeParkedState921::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Native parked diagnostic preserves human saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(NativeParking921::FParkedDiagnostic(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeParkingFirstWake921,"NeonCity.Phase205c.MainlandC921.Standalone.NativeParkingFirstWake",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FNativeParkingFirstWake921::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Native first-wake test preserves human saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(NativeParking921::FFirstWake(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNacreParkingConstraintAB921,"NeonCity.Phase205c.MainlandC921.Standalone.NacreParkingConstraintAB",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FNacreParkingConstraintAB921::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Focused Nacre joint A/B preserves human saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(NativeParking921::FNacreConstraintAB(this));return true;}
#endif
