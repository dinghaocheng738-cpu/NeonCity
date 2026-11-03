#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingFlightComponent.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
ANeonCityFixedWingAircraft::ANeonCityFixedWingAircraft(const FObjectInitializer& Initializer)
 : Super(Initializer.SetDefaultSubobjectClass<UFixedWingFlightComponent>(TEXT("AircraftMovement")))
{
 AutoPossessAI=EAutoPossessAI::Disabled; AIControllerClass=nullptr;
 auto* Occupant=FindComponentByClass<UTransportOccupantComponent>();
 Occupant->SeatHipLocation=FVector(35,0,8);
 Occupant->LeftHandTarget=FVector(55,-14,40);Occupant->RightHandTarget=FVector(55,14,40);
 Occupant->LeftFootTarget=FVector(95,-16,-18);Occupant->RightFootTarget=FVector(95,16,-18);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Visual=[&](const TCHAR* Name,FVector Center,FVector Size)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name); Mesh->SetupAttachment(Hull);
  Mesh->SetStaticMesh(Cube.Object); Mesh->SetRelativeLocation(Center); Mesh->SetRelativeScale3D(Size/100); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 };
 Visual(TEXT("Fuselage"),FVector(0,0,0),FVector(640,150,110));
 Visual(TEXT("Canopy"),FVector(70,0,75),FVector(180,130,55));
 Visual(TEXT("Tailplane"),FVector(-280,0,50),FVector(100,380,16));
 Visual(TEXT("Fin"),FVector(-275,0,100),FVector(100,18,130));
 Visual(TEXT("NoseGear"),FVector(230,0,-57),FVector(35,25,16));
 for(int Side:{-1,1})
 {
  const FString Name=Side<0?TEXT("LeftWing"):TEXT("RightWing");
  auto* Wing=CreateDefaultSubobject<UBoxComponent>(*Name); Wing->SetupAttachment(Hull);
  Wing->InitBoxExtent(FVector(100,170,12)); Wing->SetRelativeLocation(FVector(0,Side*250,0));
  Wing->SetCollisionProfileName(TEXT("PhysicsActor")); Wing->SetCollisionObjectType(ECC_Vehicle);
  Wing->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block); Wing->CanCharacterStepUpOn=ECB_Yes;
  Wing->BodyInstance.bAutoWeld=true; Wings.Add(Wing);
  Visual(*(Name+TEXT("Visual")),FVector(0,Side*250,0),FVector(200,340,24));
  Visual(*(Name+TEXT("Gear")),FVector(-30,Side*70,-57),FVector(40,25,16));
 }
 NeonCityVisuals::Create(*this,Hull,TEXT("Aircraft"),TEXT("FixedWing"),
  {TEXT("Fuselage"),TEXT("Canopy"),TEXT("Tailplane"),TEXT("Fin"),TEXT("NoseGear"),TEXT("LeftWingVisual"),TEXT("RightWingVisual"),TEXT("LeftWingGear"),TEXT("RightWingGear")});
 for(const FName Name:{FName(TEXT("Nose")),FName(TEXT("LeftWing")),FName(TEXT("RightWing"))})
 {
  auto* Point=CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Hardpoint_%s"),*Name.ToString())); Point->SetupAttachment(Hull);
  Point->SetRelativeLocation(Name==TEXT("Nose")?FVector(325,0,0):FVector(20,Name==TEXT("LeftWing")?-270:270,-18)); Hardpoints.Add(Name,Point);
 }
 Arm->bUsePawnControlRotation=false; Arm->bInheritRoll=false; Arm->SetRelativeRotation(FRotator(-10,0,0)); Arm->TargetArmLength=1500;
 Prompt->SetText(NSLOCTEXT("NeonCity","EnterFixedWing","F / Enter Light Aircraft"));
}
const UAircraftVehicleData* ANeonCityFixedWingAircraft::GetProfile() const
{ const auto* D=Cast<UFixedWingAircraftData>(VehicleData); return D && D->IsValidProfile()?D:GetDefault<UFixedWingAircraftData>(); }
UFixedWingFlightComponent* ANeonCityFixedWingAircraft::GetFlight() const { return CastChecked<UFixedWingFlightComponent>(Movement); }
FText ANeonCityFixedWingAircraft::GetInteractionName_Implementation() const { return NSLOCTEXT("NeonCity","FixedWingEntry","Enter Light Aircraft"); }
bool ANeonCityFixedWingAircraft::SubmitAircraftControls_Implementation(AController* Pilot,float T,float P,float R,float Y,bool Brake)
{ return HasAuthority() && Pilot && Pilot==GetController() && GetFlight()->SetControls(T,P,R,Y,Brake); }
TArray<FName> ANeonCityFixedWingAircraft::GetWeaponHardpoints_Implementation() const { TArray<FName> Names; Hardpoints.GetKeys(Names); return Names; }
bool ANeonCityFixedWingAircraft::GetHardpointTransform_Implementation(FName Name,FTransform& Transform) const
{ if(const auto* Point=Hardpoints.Find(Name);Point && *Point) { Transform=(*Point)->GetComponentTransform(); return true; } return false; }
bool ANeonCityFixedWingAircraft::IsSafeRestoreTransform(const FTransform& Transform) const
{
 if(FMath::Abs(Transform.Rotator().Pitch)>10 || FMath::Abs(Transform.Rotator().Roll)>10) return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(FixedWingRestore),false,this);
 // Validate support across both wings as well as center, excluding water/vehicles/temporary objects.
 for(const FVector Local:{FVector::ZeroVector,FVector(0,-400,0),FVector(0,400,0),FVector(280,0,0),FVector(-280,0,0)})
 {
  const FVector Point=Transform.TransformPosition(Local); FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByObjectType(Floor,Point,Point-FVector(0,0,GetProfile()->BodyExtent.Z+20),FCollisionObjectQueryParams(ECC_WorldStatic),Query)
   || Floor.ImpactNormal.Z<.97f || Floor.Distance<GetProfile()->BodyExtent.Z-8) return false;
 }
 const FVector Extent=GetProfile()->BodyExtent.ComponentMax(FVector(320,420,12))-FVector(3);
 return !GetWorld()->OverlapBlockingTestByChannel(Transform.GetLocation(),Transform.GetRotation(),ECC_Vehicle,FCollisionShape::MakeBox(Extent),Query);
}
bool ANeonCityFixedWingAircraft::ResolveRestoreTransform(FTransform& Transform,const FTransform& Stand) const
{
 if(IsSafeRestoreTransform(Transform)) return true;
 // Bounded stand search, only during load. Rechecked during apply after other fleets are restored.
 for(int Index=0;Index<21;++Index)
 {
  FTransform Candidate=Stand;
  const float Offset=Index==0?0.f:((Index+1)/2)*1200.f*(Index%2?1.f:-1.f);
  Candidate.AddToTranslation(Stand.TransformVectorNoScale(FVector(Offset,0,0)));
  if(IsSafeRestoreTransform(Candidate)) { Transform=Candidate; return true; }
 }
 return false;
}
bool ANeonCityFixedWingAircraft::CanSupportStandingCharacter() const
{ return Movement->IsGrounded() && GetVelocity().Size()<100; }
bool ANeonCityFixedWingAircraft::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(Super::QueryStandingSurface_Implementation(Hit,Character,Surface)) return true;
 if(!Character || !CanSupportStandingCharacter()) return false;
 for(const UBoxComponent* Wing:Wings) if(Hit.GetComponent()==Wing || Hit.GetComponent()==Hull)
 {
  const FVector P=Wing->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
  if(FMath::Abs(P.X)<=102 && FMath::Abs(P.Y)<=172 && P.Z>=7 && Hit.ImpactNormal.Z>=Character->GetCharacterMovement()->GetWalkableFloorZ())
  { Surface=FDynamicStandingSurface(); Surface.PointVelocity=Hull->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint); return true; }
 }
 return false;
}
void ANeonCityFixedWingAircraft::UpdateAircraftCamera(float Delta)
{ CameraExtension->ApplyFixedWingCamera(Camera,Arm,*CastChecked<UFixedWingAircraftData>(GetProfile()),GetVelocity().Size(),GetActorRotation().Roll,Delta); }
void ANeonCityFixedWingAircraft::Tick(float Delta)
{
 Super::Tick(Delta);
 if(IsLocallyControlled() && InputSubsystem.IsValid() && (bInputDirty || ThrottleDelta!=0))
 {
  const auto* D=CastChecked<UFixedWingAircraftData>(GetProfile());
  SubmitAircraftControls_Implementation(GetController(),GetFlight()->GetThrottle()+ThrottleDelta*D->ThrottleRate*Delta,PitchCommand,RollCommand,RudderCommand,bBrakeCommand); bInputDirty=false;
 }
}
void ANeonCityFixedWingAircraft::SetupPlayerInputComponent(UInputComponent* Input)
{
 // APawn has no required setup; do not install the helicopter context.
 RemoveMapping(); ThrottleDelta=PitchCommand=RollCommand=RudderCommand=0; bBrakeCommand=false;
 auto* Enhanced=Cast<UEnhancedInputComponent>(Input); auto* PC=Cast<APlayerController>(GetController());
 auto* Sub=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Enhanced || !Sub) return;
 if(!Mapping)
 {
  Mapping=NewObject<UInputMappingContext>(this,TEXT("FixedWingInputContext"));
  auto Axis=[&](FKey Positive,FKey Negative)
  { auto* A=NewObject<UInputAction>(this); A->ValueType=EInputActionValueType::Axis1D; Mapping->MapKey(A,Positive); Mapping->MapKey(A,Negative).Modifiers.Add(NewObject<UInputModifierNegate>(Mapping)); return A; };
  ForwardAction=Axis(EKeys::W,EKeys::S); StrafeAction=Axis(EKeys::D,EKeys::A); YawAction=Axis(EKeys::E,EKeys::Q);
  // Arrow keys provide the same pitch command for accessibility and deterministic input tests.
  VerticalAction=Axis(EKeys::Down,EKeys::Up); Mapping->MapKey(VerticalAction,EKeys::MouseY);
  BrakeAction=NewObject<UInputAction>(this); Mapping->MapKey(BrakeAction,EKeys::SpaceBar);
  ExitAction=NewObject<UInputAction>(this); Mapping->MapKey(ExitAction,EKeys::F);
 }
 for(auto Event:{ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
 {
  Enhanced->BindAction(ForwardAction,Event,this,&ANeonCityFixedWingAircraft::ThrottleAxis);
  Enhanced->BindAction(StrafeAction,Event,this,&ANeonCityFixedWingAircraft::RollAxis);
  Enhanced->BindAction(YawAction,Event,this,&ANeonCityFixedWingAircraft::RudderAxis);
  Enhanced->BindAction(VerticalAction,Event,this,&ANeonCityFixedWingAircraft::PitchAxis);
  Enhanced->BindAction(BrakeAction,Event,this,&ANeonCityFixedWingAircraft::BrakeAxis);
 }
 Enhanced->BindAction(ExitAction,ETriggerEvent::Started,this,&ANeonCityFixedWingAircraft::ExitAircraft);
 FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true; Sub->AddMappingContext(Mapping,100,Options); InputSubsystem=Sub;
}
void ANeonCityFixedWingAircraft::ThrottleAxis(const FInputActionValue& V) { ThrottleDelta=V.Get<float>(); bInputDirty=true; }
void ANeonCityFixedWingAircraft::PitchAxis(const FInputActionValue& V) { PitchCommand=V.Get<float>(); bInputDirty=true; }
void ANeonCityFixedWingAircraft::RollAxis(const FInputActionValue& V) { RollCommand=V.Get<float>(); bInputDirty=true; }
void ANeonCityFixedWingAircraft::RudderAxis(const FInputActionValue& V) { RudderCommand=V.Get<float>(); bInputDirty=true; }
void ANeonCityFixedWingAircraft::BrakeAxis(const FInputActionValue& V) { bBrakeCommand=V.Get<bool>(); bInputDirty=true; }
void ANeonCityFixedWingAircraft::ExitAircraft() { Interaction->Exit(); }
