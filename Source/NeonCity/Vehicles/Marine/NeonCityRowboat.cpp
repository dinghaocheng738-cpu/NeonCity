#include "Vehicles/Marine/NeonCityRowboat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace NeonCityRowboat
{
// Boat-local centimetres, x = bow, z = 0 at the waterline (see LakeConcept/wl_props.py, UE x = -Blender x).
// The hull draws 26 cm; its watertight cabin sole lies at SoleZ, just above the water, and everyone's feet are on it.
const float SoleZ=5;
const FVector RowerHip(-272,0,65);
const FVector PassengerHip(-60,-28,49);
const float OarlockX=-220,OarlockY=77.5f,OarlockZ=66;
const float Inboard=72,Outboard=260,OarMeshPivot=62;
// Sweep: 0 = oar athwartships, positive = blade toward the stern. The rower pushes: the drive
// carries the handles forward and the blades aft from the catch (-35) to the finish (+15).
const float MidSweep=-10,HalfSweep=25,DriveShare=.4f;
const float BladeIn=-12,BladeOut=14,BladeRest=6,RestSweep=8;
const float BladeK=70;                       // 1/2 rho Cd A of an immersed blade, N per (m/s)^2
const FVector RowerFeet[2]={FVector(-207,-15,SoleZ+4),FVector(-207,15,SoleZ+4)};
const FVector PassengerFeet[2]={FVector(-14,-40,SoleZ),FVector(-14,-16,SoleZ)};
const FVector RowerStand(-245,0,SoleZ),PassengerStand(-30,-28,SoleZ);
// Stepping ashore: anywhere along either side or over the bow, up to a long step onto a bank or pier.
const float LandingMaxRise=150,LandingWalkSpeed=260;
// Shipped: drawn in, loom on the oarlock, blade forward along the rail.
const float ShippedSweep=-84,ShippedHeight=50;
const float BoardTime=.65f,SitTime=.9f,StandTime=.8f,StepTime=.6f;
float Smooth(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);}
float Sign(int32 Side){return Side==0?-1.f:1.f;}   // 0 port / left hand, 1 starboard / right hand
}
using namespace NeonCityRowboat;

URowboatData::URowboatData()
{
 VehicleType=TEXT("NeonCity.WestLakeRowboat");DisplayName=NSLOCTEXT("NeonCity","WestLakeRowboat","West Lake rowboat");
 MaxSpeed=8;Acceleration=.5f;TurningRate=5;Mass=520;Buoyancy=2;WaterDrag=.07f;LateralDrag=2.5f;TurningResistance=1.2f;
 VerticalDamping=3;EnginePower=1;BrakeDrag=.2f;ThrottleRiseRate=1;ThrottleFallRate=1;Durability=600;
 HullExtent=FVector(330,85,30);CenterOfMass=FVector(-30,0,-15);bInlandWater=true;bFoamWake=false;
 BoardingPoint=RowerStand;InteractionReachCm=460;
 CameraRig.HelmEye=FVector(-262,0,140);CameraRig.ChaseAnchor=FVector(-140,0,172);CameraRig.ChaseDistance=640;
 CameraRig.HelmPitch=-6;CameraRig.HelmFieldOfView=80;CameraRig.HorizonStabilization=.8f;
}

ANeonCityRowboat::ANeonCityRowboat()
{
 // Placed in the level: only the player (or nobody) ever controls a rowboat.
 AutoPossessAI=EAutoPossessAI::Disabled;AIControllerClass=nullptr;
 VehicleData=CreateDefaultSubobject<URowboatData>(TEXT("RowboatData"));
 GetHull()->SetBoxExtent(VehicleData->HullExtent);
 TArray<UStaticMeshComponent*> Placeholder;GetComponents(Placeholder);
 for(auto* Part:Placeholder){Part->SetVisibility(false);Part->SetHiddenInGame(true);}
 TArray<UBoxComponent*> Boxes;GetComponents(Boxes);
 for(auto* Box:Boxes)if(Box->GetFName()==TEXT("InteractionBounds")){Box->SetBoxExtent(FVector(360,110,60));Box->SetRelativeLocation(FVector(0,0,45));}
 static ConstructorHelpers::FObjectFinder<UStaticMesh> HullAsset(TEXT("/Game/NeonCity/Art/WestLake/Meshes/SM_WL_Rowboat.SM_WL_Rowboat"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> OarAsset(TEXT("/Game/NeonCity/Art/WestLake/Meshes/SM_WL_RowboatOar.SM_WL_RowboatOar"));
 HullMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RowboatHull"));HullMesh->SetupAttachment(GetHull());
 HullMesh->SetStaticMesh(HullAsset.Object);HullMesh->SetRelativeRotation(FRotator(0,180,0));   // source bow is -x
 HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);HullMesh->SetCanEverAffectNavigation(false);
 OarMesh0=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortOar"));
 OarMesh1=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarboardOar"));
 for(auto* Mesh:{OarMesh0.Get(),OarMesh1.Get()})
 {Mesh->SetupAttachment(GetHull());Mesh->SetStaticMesh(OarAsset.Object);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCanEverAffectNavigation(false);}
 Rower=CreateDefaultSubobject<URowerPoseComponent>(TEXT("Rower"));Rower->SetupAttachment(GetHull());Rower->SetHiddenInGame(true);
 Passenger=CreateDefaultSubobject<URowerPoseComponent>(TEXT("Passenger"));Passenger->SetupAttachment(GetHull());Passenger->SetHiddenInGame(true);
 BoatmanMesh=TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/Tidewater/CharacterBindings/SK_NCTW_Wood_Male_01_Gameplay.SK_NCTW_Wood_Male_01_Gameplay")));
}

void ANeonCityRowboat::BeginPlay()
{
 if(auto* Data=Cast<URowboatData>(VehicleData);Data && Service==ERowboatService::Hired)
 {
  Data->DisplayName=NSLOCTEXT("NeonCity","WestLakeBoatRide","West Lake boat ride");
  Data->BoardingPoint=PassengerStand;
  Data->CameraRig.HelmEye=PassengerHip+FVector(10,0,73);Data->CameraRig.HelmPitch=-2;
  Data->CameraRig.ChaseAnchor=FVector(-60,0,170);Data->CameraRig.ChaseDistance=780;
 }
 Super::BeginPlay();
 if(auto* Occupant=GetOccupantPresentation())Occupant->bPresentDriver=false;
 Berth=GetActorTransform();GetMarineMovement()->SetMooringBerth(Berth);
 for(int32 S=0;S<2;++S){Oars[S]=FOar();}
 if(Service==ERowboatService::Hired)
  if(USkeletalMesh* Body=BoatmanMesh.LoadSynchronous())
  {
   TArray<UMaterialInterface*> Materials;for(const auto& Slot:Body->GetMaterials())Materials.Add(Slot.MaterialInterface);
   Rower->SetBody(Body,Materials);
  }
 UpdateOars(0);UpdatePeople();
}

bool ANeonCityRowboat::IsDocked() const { return GetMarineMovement()->IsAtMooringBerth(); }

FVector ANeonCityRowboat::GetOarlock(int32 Side) const { return FVector(OarlockX,Sign(Side)*OarlockY,OarlockZ); }

FVector ANeonCityRowboat::OarDirection(int32 Side) const
{
 const FOar& O=Oars[Side];const float Sweep=FMath::DegreesToRadians(O.Sweep);
 const FVector Flat(-FMath::Sin(Sweep),Sign(Side)*FMath::Cos(Sweep),0);
 const float Pitch=FMath::Asin(FMath::Clamp((OarlockZ-O.Height)/Outboard,-.9f,.9f));
 return FVector(Flat.X*FMath::Cos(Pitch),Flat.Y*FMath::Cos(Pitch),-FMath::Sin(Pitch));
}

FVector ANeonCityRowboat::GetGrip(int32 Side) const { return GetOarlock(Side)-OarDirection(Side)*Inboard; }

FVector ANeonCityRowboat::StandSpot(bool bRowerSeat) const
{ return GetActorTransform().TransformPosition(bRowerSeat?RowerStand:PassengerStand); }

void ANeonCityRowboat::Feedback(const FText& Text){Message=Text;MessageTime=3.5f;}

bool ANeonCityRowboat::FindShoreLanding(const ACharacter* Character,FTransform& Out) const
{
 // Step over the gunwale onto a landing beside the boat or ahead of the bow: a ledge, stair, bank or
 // pier no higher than a long step above the waterline, with room to stand. The person walks along the
 // floorboards to the nearest one, so the whole length of the boat counts, not just the seat.
 if(!Character || !GetWorld())return false;
 const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
 const float Radius=Capsule->GetScaledCapsuleRadius(),Half=Capsule->GetScaledCapsuleHalfHeight();
 const FCollisionShape Shape=FCollisionShape::MakeCapsule(Radius,Half);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(RowboatLanding),false,this);Query.AddIgnoredActor(Character);
 const bool bRower=Service==ERowboatService::SelfRow;
 const FVector Seat=bRower?RowerStand:PassengerStand;
 const FTransform Boat=GetActorTransform();
 const FVector Extent=GetProfile()->HullExtent;
 const float WaterZ=Boat.GetLocation().Z;
 float Best=MAX_flt;
 auto Try=[&](const FVector& Local,float Cost)
 {
  const FVector Point=Boat.TransformPosition(Local);
  FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,260),Point-FVector(0,0,120),ECC_Pawn,Query))return;
  if(Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())return;
  if(Floor.ImpactPoint.Z<WaterZ+6 || Floor.ImpactPoint.Z>WaterZ+LandingMaxRise)return;          // above the water, within a step
  const FVector Destination=Floor.ImpactPoint+FVector(0,0,Half+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,ECC_Pawn,Shape,Query))return;
  Cost+=FMath::Max(0.f,Floor.ImpactPoint.Z-WaterZ-40)*2;
  if(Cost<Best){Best=Cost;Out=FTransform(FRotator(0,(Destination-Boat.GetLocation()).Rotation().Yaw,0),Destination);}
 };
 // Beside the hull, from stern to bow.
 for(float Side:{1.f,-1.f})for(float X=-Extent.X+30;X<=Extent.X-20;X+=50)for(float Out_:{25.f,55.f,90.f,130.f,180.f})
  Try(FVector(X,Side*(Extent.Y+Radius+Out_),0),FMath::Abs(X-Seat.X)*.6f+Out_*1.5f);
 // Over the bow (nosed into a bank or the end of a pier).
 for(float Y:{0.f,-55.f,55.f})for(float Out_:{20.f,50.f,90.f,140.f})
  Try(FVector(Extent.X+Radius+Out_,Y,0),(Extent.X-Seat.X)*.6f+Out_*1.5f+FMath::Abs(Y));
 return Best<MAX_flt;
}

bool ANeonCityRowboat::HasLandingInReach() const
{
 const auto* Character=GetMarineInteraction()->GetDriver();FTransform Unused;
 return Character && GetVelocity().Size2D()<=60 && FindShoreLanding(Character,Unused);
}

void ANeonCityRowboat::ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue)
{
 // Oars, not a motor: the marine movement keeps only buoyancy and hull drag.
 GetMarineMovement()->SetInput(0,0,0);
 if(Service!=ERowboatService::SelfRow || Phase!=EPhase::Seated)return;
 InForward=FMath::IsFinite(ThrottleValue)?FMath::Clamp(ThrottleValue,-1.f,1.f):0;
 InSteer=FMath::IsFinite(SteeringValue)?FMath::Clamp(SteeringValue,-1.f,1.f):0;
 InBrake=FMath::IsFinite(BrakeValue)?FMath::Clamp(BrakeValue,0.f,1.f):0;
}

void ANeonCityRowboat::PossessedBy(AController* NewController)
{
 Super::PossessedBy(NewController);
 InForward=InSteer=InBrake=0;
 if(Service==ERowboatService::Hired && Cast<APlayerController>(NewController))
 {
  bTourActive=true;bReturning=false;bDocking=TourRoute.IsEmpty();bUnberthing=!bDocking;Waypoint=0;WaypointTime=-3.f;BestDistance=MAX_flt;StuckTime=BackOffTime=0;
  UpdateClearance();PushSide=Oars[1].bBlocked?1.f:Oars[0].bBlocked?-1.f:0.f;PushTime=PushSide!=0?2.8f:0.f;
  Feedback(NSLOCTEXT("NeonCity","RowboatCastOff","The boatman casts off"));
 }
}

void ANeonCityRowboat::OnHelmCameraChanged(bool bHelm)
{
 // Seen from the seat: the rower's own arms and the oars stay, the head the eye sits in does not.
 for(URowerPoseComponent* Body:{Rower.Get(),Passenger.Get()})
  for(const TCHAR* Bone:{TEXT("head"),TEXT("neck_02")})
   if(bHelm)Body->HideBoneByName(Bone,EPhysBodyOp::PBO_None);else Body->UnHideBoneByName(Bone);
}

bool ANeonCityRowboat::CanInteract_Implementation(AActor* Interactor) const
{
 const auto* Character=Cast<ACharacter>(Interactor);
 if(Phase!=EPhase::Idle || !Character || !Character->GetCharacterMovement() || !Character->GetCharacterMovement()->IsMovingOnGround())return false;
 if(Service==ERowboatService::Hired && (bTourActive || !IsDocked()))return false;
 return GetVelocity().Size2D()<60 && Super::CanInteract_Implementation(Interactor);
}

FText ANeonCityRowboat::GetInteractionName_Implementation() const
{
 return Service==ERowboatService::Hired?NSLOCTEXT("NeonCity","RowboatHire","Take a boat ride"):NSLOCTEXT("NeonCity","RowboatRow","Row the boat");
}

void ANeonCityRowboat::Interact_Implementation(AActor* Interactor)
{
 auto* Character=Cast<ACharacter>(Interactor);
 if(!CanInteract_Implementation(Interactor))return;
 auto* PC=Cast<APlayerController>(Character->GetController());if(!PC)return;
 // Step down into the boat: one short arc onto the floorboards, then sit (UpdateBoarding).
 Person=Character;Phase=EPhase::Boarding;PhaseTime=0;StepFrom=Character->GetActorLocation();
 Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
 Character->SetActorEnableCollision(false);PC->SetIgnoreMoveInput(true);
}

bool ANeonCityRowboat::RequestDriverExit()
{
 if(Phase!=EPhase::Seated)return false;
 if(Service==ERowboatService::Hired && bTourActive)
 {
  if(!bReturning){bReturning=true;bDocking=false;bUnberthing=false;Waypoint=FMath::Max(0,TourRoute.Num()-2);WaypointTime=0;BestDistance=MAX_flt;}
  Feedback(NSLOCTEXT("NeonCity","RowboatReturning","The boatman turns back to the pier"));return true;
 }
 if(GetVelocity().Size2D()>60){Feedback(NSLOCTEXT("NeonCity","RowboatSlow","Let the boat stop before standing up"));return false;}
 FTransform Ashore;
 if(!GetMarineInteraction()->FindExitTransform(Ashore,true)){Feedback(NSLOCTEXT("NeonCity","RowboatNoLanding","Bring the boat alongside a pier or bank to step ashore"));return false;}
 auto* Character=GetMarineInteraction()->GetDriver();
 URowerPoseComponent* Seat=Service==ERowboatService::SelfRow?Rower.Get():Passenger.Get();
 if(!Character || !Seat->HasStandingPose())return GetMarineInteraction()->Exit();
 InForward=InSteer=InBrake=0;
 const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const FTransform Standing(FRotator(0,GetActorRotation().Yaw,0),StandSpot(Service==ERowboatService::SelfRow)+FVector(0,0,Half));
 Seat->BeginStandTo(Character->GetMesh()->GetRelativeTransform()*Standing,StandTime);
 Phase=EPhase::StandingUp;PhaseTime=0;return true;
}

void ANeonCityRowboat::UpdateBoarding(float Delta)
{
 PhaseTime+=Delta;
 ACharacter* Character=Person.Get();
 const bool bRowerSeat=Service==ERowboatService::SelfRow;
 URowerPoseComponent* Seat=bRowerSeat?Rower.Get():Passenger.Get();
 auto* PC=Character?Cast<APlayerController>(Character->GetController()):nullptr;
 if(Phase==EPhase::Boarding)
 {
  if(!Character){Phase=EPhase::Idle;return;}
  const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const FVector To=StandSpot(bRowerSeat)+FVector(0,0,Half);
  const float A=FMath::Clamp(PhaseTime/BoardTime,0.f,1.f);
  Character->SetActorLocation(FMath::Lerp(StepFrom,To,Smooth(A))+FVector(0,0,FMath::Sin(PI*A)*22));
  Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),FRotator(0,GetActorRotation().Yaw,0),Delta,8));
  if(A<1)return;
  // Feet on the floorboards: the hull takes the step, then the person sits down.
  GetHull()->AddImpulseAtLocation(FVector(0,0,-75*110),To-FVector(0,0,Half));
  USkeletalMeshComponent* Body=Character->GetMesh();
  TArray<UMaterialInterface*> Materials;for(int32 I=0;I<Body->GetNumMaterials();++I)Materials.Add(Body->GetMaterial(I));
  const bool bShown=Seat->SetBody(Body->GetSkeletalMeshAsset(),Materials);
  if(bShown)Seat->BeginSitFrom(Body,SitTime);
  if(PC)PC->SetIgnoreMoveInput(false);
  if(!GetMarineInteraction()->Enter(Character))
  {
   if(bShown)Seat->ClearBody();
   Character->SetActorEnableCollision(true);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Phase=EPhase::Idle;Person.Reset();return;
  }
  Phase=EPhase::Seated;PhaseTime=0;return;
 }
 if(Phase==EPhase::StandingUp)
 {
  if(Seat->IsBlending())return;
  FTransform Ashore;
  if(!Character || !GetMarineInteraction()->FindExitTransform(Ashore,true) || !GetMarineInteraction()->Exit())
  {
   // Nowhere to step: sit back down.
   Seat->SnapSeated();
   Phase=EPhase::Seated;Feedback(NSLOCTEXT("NeonCity","RowboatNoLanding","Bring the boat alongside a pier or bank to step ashore"));return;
  }
  // The person is the character again; carry it from the floorboards up onto the landing.
  if(Service==ERowboatService::SelfRow)Rower->ClearBody();else Passenger->ClearBody();
  const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  StepFrom=StandSpot(bRowerSeat)+FVector(0,0,Half);StepTo=Ashore.GetLocation();
  Character->SetActorEnableCollision(false);Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
  Character->SetActorLocationAndRotation(StepFrom,FRotator(0,(StepTo-StepFrom).Rotation().Yaw,0));
  if(auto* NewPC=Cast<APlayerController>(Character->GetController()))NewPC->SetIgnoreMoveInput(true);
  GetHull()->AddImpulseAtLocation(FVector(0,0,-75*90),StepFrom-FVector(0,0,Half));
  Phase=EPhase::Stepping;PhaseTime=0;return;
 }
 if(Phase==EPhase::Stepping)
 {
  if(!Character){Phase=EPhase::Idle;return;}
  // A short walk along the floorboards, then the step up over the gunwale.
  const float Duration=FMath::Max(StepTime,FVector::Dist2D(StepFrom,StepTo)/LandingWalkSpeed);
  const float A=FMath::Clamp(PhaseTime/Duration,0.f,1.f);
  const float Lift=FMath::Max(28.f,StepTo.Z-StepFrom.Z+28.f);
  Character->SetActorLocation(FMath::Lerp(StepFrom,StepTo,Smooth(A))+FVector(0,0,FMath::Sin(PI*FMath::Pow(A,1.6f))*Lift*.5f));
  if(A<1)return;
  Character->SetActorEnableCollision(true);Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  if(PC)PC->SetIgnoreMoveInput(false);
  Phase=EPhase::Idle;Person.Reset();PhaseTime=0;
 }
}

void ANeonCityRowboat::UpdateAutopilot(float Delta)
{
 if(Service!=ERowboatService::Hired)return;
 if(!bTourActive){InForward=InSteer=InBrake=0;return;}
 WaypointTime+=Delta;
 if(WaypointTime<0){InForward=InSteer=InBrake=0;return;}       // casting off
 const FVector Here=GetActorLocation();
 const FVector Forward=GetActorForwardVector().GetSafeNormal2D();
 const float Speed=FVector::DotProduct(GetVelocity(),Forward);
 FVector Target=bDocking || TourRoute.IsEmpty()?Berth.GetLocation():TourRoute[Waypoint];
 if(bDocking)
 {
  // Come in along the berth line: aim at a point on that line a few metres ahead of the boat
  // (pure pursuit), so she lines up with the landing well before she reaches it.
  const FVector Along=Berth.GetRotation().GetForwardVector().GetSafeNormal2D();
  const float S=FVector::DotProduct(Here-Berth.GetLocation(),Along);      // < 0 while still out on the lake
  Target=Berth.GetLocation()+Along*FMath::Min(S+900.f,0.f);
 }
 const float Distance=FVector::Dist2D(Here,Target);
 const float Error=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,(Target-Here).Rotation().Yaw);
 InBrake=0;
 // Something under the bow (pier end, bank): back off before trying again.
 if(BackOffTime>0){BackOffTime-=Delta;InForward=-1;InSteer=0;return;}
 // Leaving the landing: back her out stern-first, straight, until she is clear of the pier.
 if(bUnberthing)
 {
  if(FVector::Dist2D(Here,Berth.GetLocation())<1600 && WaypointTime<60){InForward=-1;InSteer=0;return;}
  bUnberthing=false;
 }
 if(InForward>.5f && FMath::Abs(Speed)<20 && Rate>15)StuckTime+=Delta;else StuckTime=0;
 // No headway towards the mark for a while (fouled on a pier end, bank or weed): back off and retry.
 const float Goal=bDocking?FVector::Dist2D(Here,Berth.GetLocation()):Distance;
 if(Goal<BestDistance-100){BestDistance=Goal;ProgressTime=0;}else ProgressTime+=Delta;
 if(StuckTime>4 || ProgressTime>14){StuckTime=0;ProgressTime=0;BestDistance=MAX_flt;BackOffTime=3.5f;}
 if(!bDocking)
 {
  InSteer=FMath::Clamp(Error/30.f,-1.f,1.f);InForward=FMath::Abs(Error)>70?0.f:1.f;
  // Reached, or already abeam of it (the leg's direction says we are past): next leg.
  bool bPast=false;
  if(Waypoint>0)
  {
   const FVector Leg=(TourRoute[Waypoint]-TourRoute[Waypoint-1]).GetSafeNormal2D();
   bPast=FVector::DotProduct(Here-Target,Leg)>0 && Distance<2500;
  }
  if(Distance<700 || bPast || WaypointTime>120)
  {
   WaypointTime=0;BestDistance=MAX_flt;ProgressTime=0;
   if(++Waypoint>=TourRoute.Num()){Waypoint=TourRoute.Num()-1;bDocking=true;}
  }
  return;
 }
 // Coming alongside: row gently, let her glide, hold water to take the way off.
 const float Remaining=FVector::Dist2D(Here,Berth.GetLocation());
 InSteer=FMath::Clamp(Error/28.f,-1.f,1.f);
 if(Remaining>1300)InForward=FMath::Abs(Error)>70?0.f:1.f;
 else if(Remaining>480)InForward=FMath::Abs(Error)>50?0.f:(Speed<75?.6f:0.f);
 else {InForward=0;InBrake=Speed>40?1.f:0.f;}
 if(GetMarineMovement()->IsMoored() || (Remaining<650 && FMath::Abs(Speed)<70))
 {
  // Close and slow: the boatman makes fast and hauls her alongside on the line.
  if(!GetMarineMovement()->IsMoored()){GetMarineMovement()->StopEngine();GetMarineMovement()->SetMooringBerth(Berth);}
  InForward=InSteer=InBrake=0;
  if(IsDocked())
  {bTourActive=bDocking=bReturning=false;Feedback(NSLOCTEXT("NeonCity","RowboatDocked","Alongside the pier - press F to step ashore"));}
 }
}

void ANeonCityRowboat::UpdateOars(float Delta)
{
 // Commands -> what each oar does. Turning: the outside oar pulls harder; in place, one rows
 // ahead while the other backs water.
 EOarMode Want[2]={EOarMode::Rest,EOarMode::Rest};float Amp[2]={1,1};float TargetRate=0;
 const bool Seated=Phase==EPhase::Seated || Service==ERowboatService::Hired;
 if(Seated && InBrake>.5f){Want[0]=Want[1]=EOarMode::Hold;}
 else if(Seated && InForward>.15f)
 {
  Want[0]=Want[1]=EOarMode::Forward;TargetRate=19+4*InForward;
  if(InSteer>.1f)Amp[1]=1-.7f*InSteer;else if(InSteer<-.1f)Amp[0]=1+.7f*InSteer;
 }
 else if(Seated && InForward<-.15f)
 {
  Want[0]=Want[1]=EOarMode::Back;TargetRate=18;
  if(InSteer>.1f)Amp[0]=1-.7f*InSteer;else if(InSteer<-.1f)Amp[1]=1+.7f*InSteer;
 }
 else if(Seated && FMath::Abs(InSteer)>.15f)
 {
  Want[0]=InSteer>0?EOarMode::Forward:EOarMode::Back;Want[1]=InSteer>0?EOarMode::Back:EOarMode::Forward;TargetRate=20;
 }
 // Fending off from the landing (self-row): no strokes until the pier-side oar is clear.
 if(Service==ERowboatService::SelfRow && PushTime>0){Want[0]=Want[1]=EOarMode::Rest;TargetRate=0;}
 // At the berth the oars lie shipped; an oar whose sweep is blocked by the pier or a bank stays shipped.
 const bool Moored=GetMarineMovement()->IsMoored();
 for(int32 S=0;S<2;++S)
  if(Oars[S].bBlocked || (Moored && Want[S]==EOarMode::Rest))Want[S]=EOarMode::Shipped;
 const bool Stroking=Want[0]==EOarMode::Forward || Want[0]==EOarMode::Back || Want[1]==EOarMode::Forward || Want[1]==EOarMode::Back;
 // One oar shipped (pier alongside): work the free oar with a short sweep so she does not slew round.
 for(int32 S=0;S<2;++S) if(Want[1-S]==EOarMode::Shipped && (Want[S]==EOarMode::Forward || Want[S]==EOarMode::Back))Amp[S]=FMath::Min(Amp[S],.55f);
 if(Stroking && Rate<1)Stroke=.88f;                      // begin from the end of a recovery: square, drop, catch
 Rate=FMath::FInterpConstantTo(Rate,Stroking?TargetRate:0.f,Delta,10.f);
 if(Stroking)Stroke=FMath::Fmod(Stroke+FMath::Max(Rate,12.f)/60*Delta,1.f);
 for(int32 S=0;S<2;++S)
 {
  FOar& O=Oars[S];O.Mode=Want[S];O.Amplitude=FMath::FInterpTo(O.Amplitude,Amp[S],Delta,3);
  float Sweep=RestSweep,Height=BladeRest,Feather=1;
  if(O.Mode==EOarMode::Hold){Sweep=0;Height=BladeIn;Feather=0;}
  else if(O.Mode==EOarMode::Shipped){Sweep=ShippedSweep;Height=ShippedHeight;Feather=1;}
  else if(O.Mode==EOarMode::Forward || O.Mode==EOarMode::Back)
  {
   const bool Ahead=O.Mode==EOarMode::Forward;
   // Handle progress 0 (catch) -> 1 (finish) on the drive, back on the recovery.
   const float Progress=Stroke<DriveShare?.5f-.5f*FMath::Cos(PI*Stroke/DriveShare):.5f+.5f*FMath::Cos(PI*(Stroke-DriveShare)/(1-DriveShare));
   Sweep=MidSweep+O.Amplitude*HalfSweep*(2*Progress-1);
   const bool Pulling=Ahead?(Stroke>.02f && Stroke<DriveShare-.03f):(Stroke>DriveShare+.03f && Stroke<.97f);
   const bool Feathered=Ahead?(Stroke>DriveShare+.07f && Stroke<.9f):(Stroke>.06f && Stroke<DriveShare-.05f);
   Height=Pulling?BladeIn:BladeOut;Feather=Pulling?0.f:Feathered?1.f:0.f;
  }
  O.Sweep=Delta>0?FMath::FInterpConstantTo(O.Sweep,Sweep,Delta,170.f):Sweep;
  O.Height=Delta>0?FMath::FInterpConstantTo(O.Height,Height,Delta,170.f):Height;
  O.Feather=Delta>0?FMath::FInterpConstantTo(O.Feather,Feather,Delta,6.f):Feather;
  // Oar mesh: loom along the oar, blade square (vertical) or feathered (rolled flat).
  const FVector Along=OarDirection(S);
  const FQuat Square=FRotationMatrix::MakeFromXZ(Along,FVector::UpVector).ToQuat();
  const FQuat Pose=FQuat(Along,Sign(S)*O.Feather*FMath::DegreesToRadians(80))*Square;
  UStaticMeshComponent* Mesh=S==0?OarMesh0.Get():OarMesh1.Get();
  Mesh->SetRelativeLocationAndRotation(GetOarlock(S)-Pose.RotateVector(FVector(OarMeshPivot,0,0)),Pose);
  O.Blade=GetOarlock(S)+Along*Outboard;
 }
}

void ANeonCityRowboat::ApplyBladeForces(float Delta)
{
 auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();
 UPrimitiveComponent* Body=GetHull();
 if(!Water || !Body || !Body->IsSimulatingPhysics() || Delta<=0)return;
 const FTransform Boat=Body->GetComponentTransform();
 const float ComZ=Body->GetCenterOfMass().Z;
 for(int32 S=0;S<2;++S)
 {
  FOar& O=Oars[S];
  if(!O.bPrev){O.PrevBlade=O.Blade;O.bPrev=true;}
  const FVector LocalVelocity=(O.Blade-O.PrevBlade)/Delta;O.PrevBlade=O.Blade;
  const FVector World=Boat.TransformPosition(O.Blade);
  float Surface=0;FVector Flow=FVector::ZeroVector;
  O.Immersion=0;O.LastForce=FVector::ZeroVector;
  if(!Water->SampleWater(World,Surface,Flow))continue;
  // Immersed fraction of a 20 cm blade (centre depth), nearly nothing once feathered.
  O.Immersion=FMath::Clamp((Surface-World.Z+6)/18,0.f,1.f)*(1-.9f*O.Feather);
  if(O.Immersion<=0)continue;
  const FVector Along=OarDirection(S);
  const FVector Normal=Boat.TransformVectorNoScale(FVector(-Along.Y,Along.X,0).GetSafeNormal());
  const FVector Velocity=Body->GetPhysicsLinearVelocityAtPoint(World)+Boat.TransformVectorNoScale(LocalVelocity)-Flow;
  const float Normal_ms=FVector::DotProduct(Velocity,Normal)/100;
  // Pressure drag on the blade face: opposes the blade's motion through the water.
  const float Newtons=FMath::Clamp(BladeK*Normal_ms*FMath::Abs(Normal_ms)*O.Immersion,-900.f,900.f);
  O.LastForce=-Normal*Newtons;
  Body->AddForceAtLocation(O.LastForce*100,FVector(World.X,World.Y,ComZ));
 }
}

void ANeonCityRowboat::UpdateClearance()
{
 // Would this oar's stroke reach into the pier or a bank? Three horizontal probes across its sweep,
 // just above the water, out to the blade tip.
 if(!GetWorld())return;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(RowboatOarClearance),false,this);
 const FTransform Boat=GetActorTransform();
 for(int32 S=0;S<2;++S)
 {
  bool Blocked=false;
  for(float Sweep:{-35.f,-10.f,15.f})
  {
   const float R=FMath::DegreesToRadians(Sweep);
   const FVector From=GetOarlock(S)+FVector(0,0,-OarlockZ+12);
   const FVector To=From+FVector(-FMath::Sin(R),Sign(S)*FMath::Cos(R),0)*(Outboard+45);
   if(GetWorld()->LineTraceTestByChannel(Boat.TransformPosition(From),Boat.TransformPosition(To),ECC_WorldStatic,Query)){Blocked=true;break;}
  }
  Oars[S].bBlocked=Blocked;
 }
}

void ANeonCityRowboat::UpdatePushOff(float Delta)
{
 // Starting from alongside: shove her off the landing with a hand / the loom, and keep fending off
 // until the pier-side oar has room to swing. A rower does not pull one oar against the pier (that only
 // swings the bow back into it); both oars go to work once she lies clear.
 const bool bSelf=Service==ERowboatService::SelfRow;
 const bool Wants=bSelf?(FMath::Abs(InForward)>.15f || FMath::Abs(InSteer)>.15f):(bUnberthing || WaypointTime<0);
 const bool Blocked=Oars[0].bBlocked || Oars[1].bBlocked;
 if(Wants && Phase==EPhase::Seated && Blocked && !(Oars[0].bBlocked && Oars[1].bBlocked))
 {
  if(PushTime<=0)PushSide=Oars[1].bBlocked?1.f:-1.f;
  PushTime=FMath::Max(PushTime,bSelf?.7f:2.f);
 }
 if(PushTime<=0)return;
 PushTime-=Delta;
 const FVector Away=-GetActorRightVector().GetSafeNormal2D()*PushSide;
 if(FVector::DotProduct(GetVelocity(),Away)<70)GetHull()->AddForce(Away*GetHull()->GetMass()*55);
}

FRowerTargets ANeonCityRowboat::RowingTargets() const
{
 FRowerTargets T;T.Hip=RowerHip;T.Yaw=0;T.Lean=Lean;T.HeadPitch=-Lean*.8f;
 for(int32 S=0;S<2;++S){T.Hand[S]=GetGrip(S);T.HandAxis[S]=OarDirection(S);T.Foot[S]=RowerFeet[S];T.HandOn[S]=Oars[S].Mode!=EOarMode::Shipped;}
 return T;
}

FRowerTargets ANeonCityRowboat::PassengerTargets() const
{
 FRowerTargets T;T.Hip=PassengerHip;T.Yaw=0;T.Lean=3;T.HeadPitch=-2;T.bHands=false;
 for(int32 S=0;S<2;++S)T.Foot[S]=PassengerFeet[S];
 return T;
}

void ANeonCityRowboat::UpdatePeople()
{
 // Torso follows the hands: back at the catch, forward through the push to the finish.
 float Progress=0;int32 Count=0;
 for(const FOar& O:Oars) if(O.Mode==EOarMode::Forward || O.Mode==EOarMode::Back)
 {Progress+=FMath::Clamp((O.Sweep-MidSweep)/(2*HalfSweep*FMath::Max(.2f,O.Amplitude))+.5f,0.f,1.f);++Count;}
 const float TargetLean=Count?-6+30*Progress/Count:(Oars[0].Mode==EOarMode::Hold?6.f:3.f);
 Lean=FMath::FInterpTo(Lean,TargetLean,GetWorld()->GetDeltaSeconds(),9);
 if(Rower->HasBody())Rower->ApplyTargets(RowingTargets());
 if(Passenger->HasBody())Passenger->ApplyTargets(PassengerTargets());
}

void ANeonCityRowboat::Tick(float Delta)
{
 Super::Tick(Delta);
 MessageTime=FMath::Max(0.f,MessageTime-Delta);
 if(Phase!=EPhase::Idle && Phase!=EPhase::Seated)UpdateBoarding(Delta);
 if(Phase==EPhase::Seated && !GetController())
 {
  // Left by other means (save restore, destruction of the driver): free the seat.
  Phase=EPhase::Idle;Person.Reset();
  if(Service==ERowboatService::SelfRow)Rower->ClearBody();else Passenger->ClearBody();
 }
 GetMarineMovement()->SetInput(0,0,0);
 UpdateAutopilot(Delta);
 if((ClearanceTime-=Delta)<=0){ClearanceTime=.2f;UpdateClearance();}
 if(Phase==EPhase::Seated && Service==ERowboatService::SelfRow && (LandingProbeTime-=Delta)<=0){LandingProbeTime=.4f;bLandingInReach=HasLandingInReach();}
 UpdatePushOff(Delta);
 UpdateOars(Delta);
 ApplyBladeForces(Delta);
 UpdatePeople();
}

FText ANeonCityRowboat::BuildStatusText(const FMarineDriveTelemetry& T,const FText& State) const
{
 const FText Speed=FText::AsNumber(FMath::RoundToInt(T.SpeedKmh*10)/10.f);
 if(Service==ERowboatService::Hired)
 {
  FText Leg=MessageTime>0?Message:bTourActive?(bReturning || bDocking?NSLOCTEXT("NeonCity","RowboatLegHome","The boatman is rowing back to the pier")
   :FText::Format(NSLOCTEXT("NeonCity","RowboatLeg","The boatman is rowing - stretch {0} of {1}"),FText::AsNumber(Waypoint+1),FText::AsNumber(FMath::Max(1,TourRoute.Num())))):NSLOCTEXT("NeonCity","RowboatAlongside","Alongside the pier");
  return FText::Format(NSLOCTEXT("NeonCity","RowboatRideStatus","{0}  |  {1} km/h\n{2}\n{3}   V View   Mouse Look"),GetProfile()->DisplayName,Speed,Leg,
   bTourActive?NSLOCTEXT("NeonCity","RowboatAskReturn","F Ask to return"):NSLOCTEXT("NeonCity","RowboatStepAshore","F Step ashore"));
 }
 FText Doing=NSLOCTEXT("NeonCity","RowboatGliding","Gliding");
 if(PushTime>0)Doing=NSLOCTEXT("NeonCity","RowboatPushing","Pushing off from the landing");
 else if(Oars[0].Mode==EOarMode::Shipped && Oars[1].Mode==EOarMode::Shipped)Doing=NSLOCTEXT("NeonCity","RowboatShipped","Oars shipped");
 else
 if(Oars[0].Mode==EOarMode::Hold)Doing=NSLOCTEXT("NeonCity","RowboatHolding","Holding water");
 else if(Oars[0].Mode==EOarMode::Forward && Oars[1].Mode==EOarMode::Forward)Doing=NSLOCTEXT("NeonCity","RowboatRowing","Rowing");
 else if(Oars[0].Mode==EOarMode::Back && Oars[1].Mode==EOarMode::Back)Doing=NSLOCTEXT("NeonCity","RowboatBacking","Backing water");
 else if(Oars[0].Mode!=Oars[1].Mode)Doing=NSLOCTEXT("NeonCity","RowboatTurning","Turning");
 if(MessageTime>0)Doing=Message;
 const FText Ashore=bLandingInReach?NSLOCTEXT("NeonCity","RowboatLandingReady","F Step ashore (landing in reach)")
  :NSLOCTEXT("NeonCity","RowboatLandingNone","F Leave: come alongside a bank or pier, or nose into it");
 return FText::Format(NSLOCTEXT("NeonCity","RowboatStatus","{0}  |  {1} km/h  |  {2} strokes/min\n{3}\n{4}\nW Row   S Back water   A / D Turn   Space Hold water   V View   Mouse Look"),
  GetProfile()->DisplayName,Speed,FText::AsNumber(FMath::RoundToInt(Rate)),Doing,Ashore);
}
