#include "AI/Police/PoliceVehicle.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Police/PoliceSearchCoordinator.h"
#include "Physics/PhysicalImpactComponent.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Vehicles/NeonCitySedanVisuals.h"
#include "Wanted/SearchSystem.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Adventure/IslandSupercarPresentationData.h"
APoliceVehicle::APoliceVehicle()
{
 CreateDefaultSubobject<UCombatTargetComponent>(TEXT("CombatIdentity"))->Faction=ENeonCityFaction::Police;
 CreateDefaultSubobject<UVehicleDamageComponent>(TEXT("VehicleDamageState"));
 CreateDefaultSubobject<UPhysicalImpactComponent>(TEXT("PhysicalImpact"));
 PrimaryActorTick.bCanEverTick=true;
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 Sensor=CreateDefaultSubobject<UPoliceSensorComponent>(TEXT("PoliceVision")); Sensor->EyeOffset=FVector(0,0,110);
 Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("PoliceBody")); SetRootComponent(Collision);
 Collision->SetBoxExtent(FVector(235,96,63)); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic")); Collision->SetCollisionObjectType(ECC_Vehicle);
 USceneComponent* Visual=CreateDefaultSubobject<USceneComponent>(TEXT("Visual")); Visual->SetupAttachment(Collision); Visual->SetRelativeLocation(FVector(0,0,-60));
 Wheels=FNeonCitySedanVisuals::Create(*this,Visual,TEXT("Police"));
 const FVector Positions[]={FVector(135,-90,11),FVector(135,90,11),FVector(-129.2,-90,11),FVector(-129.2,90,11)};
 auto* Fitted=FNeonCitySedanVisuals::GetFleetPresentation();
 if(Fitted && Fitted->IsComplete()) {Visual->SetRelativeLocation(FVector(0,0,Fitted->Wheels[0].RadiusCm-81.f));PresentationWheelRadiusCm=Fitted->Wheels[0].RadiusCm;Collision->SetBoxExtent(FVector(250,105,63));}
 for (int32 I=0;I<Wheels.Num();++I) Wheels[I]->SetRelativeLocationAndRotation(Fitted && Fitted->IsComplete()?Fitted->Wheels[I].CentreCm:Positions[I],FRotator(0,0,90));
 Beacon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon")); Beacon->SetupAttachment(Collision);
 Beacon->SetRelativeLocation(FVector(-15,0,85)); Beacon->SetRelativeScale3D(FVector(.3f,1.35f,.15f)); Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blue(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Blue.M_SB_Blue"));
 if(Cube.Succeeded()) Beacon->SetStaticMesh(Cube.Object);
 if(Blue.Succeeded()) Beacon->SetMaterial(0,Blue.Object);
 if(NeonCityVisuals::Apply(Beacon,TEXT("Vehicles"),TEXT("PoliceBeacon"))) Beacon->SetRelativeLocation(FVector(-15,0,69));
 UTextRenderComponent* Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PoliceLabel")); Label->SetupAttachment(Collision);
 Label->SetRelativeLocationAndRotation(FVector(0,-99,40),FRotator(0,-90,0)); Label->SetText(FText::FromString(TEXT("POLICE")));
 Label->SetWorldSize(30); Label->SetHorizontalAlignment(EHTA_Center); Label->SetTextRenderColor(FColor::White);
 if(Beacon->ComponentHasTag(TEXT("NeonCity.Visual205B"))) Label->SetVisibility(false);
}
UVehicleDamageComponent* APoliceVehicle::GetDamageState() const { return FindComponentByClass<UVehicleDamageComponent>(); }
bool APoliceVehicle::ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event)
{ auto* Wreck=GetDamageState(); return Wreck && Wreck->OfferChainExplosion(Event); }
void APoliceVehicle::InitializeRoute(ANeonCityTrafficRoute* InRoute,float InDistance) { Route=InRoute; Distance=InDistance; }
void APoliceVehicle::SetSiren(bool Value) { if(bSiren==Value) return; bSiren=Value; OnSirenChanged.Broadcast(Value); }
void APoliceVehicle::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds); Velocity=FVector::ZeroVector;
 if(bRoadblock) { State=ENeonCityPoliceState::Search; Speed=0; if(FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()<=0) SetSiren(false); return; }
 if(!HasAuthority() || Health->IsDead() || FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()<=0 || !Route.IsValid() || DeltaSeconds<=0) { Speed=0; SetSiren(false); return; }
 USplineComponent* Spline=Route->GetRoadSpline(); const float Length=Spline->GetSplineLength(); if(Length<100) return;
 USearchSystem* Search=GetWorld()->GetSubsystem<USearchSystem>();
 float DesiredSpeed=220;
 State=ENeonCityPoliceState::Patrol;
 if(bResponding && Search->HasLocation())
 {
  const FVector LastKnown=Search->HasVisualContact()?Search->GetLastKnownLocation():GetWorld()->GetSubsystem<UPoliceSearchCoordinator>()->GetAssignment(DispatchSlot,650,EPoliceSearchPattern::SweepRoute);
  const float TargetDistance=Spline->GetDistanceAlongSplineAtLocation(LastKnown,ESplineCoordinateSpace::World);
  float ForwardDistance=FMath::Fmod(TargetDistance-Distance+Length,Length);
  // Nearby roadside evidence can project onto the opposite lane. Approach it on our current lane,
  // rather than needlessly circling the whole block. All inputs remain evidence snapshots.
  const FVector Delta=LastKnown-GetActorLocation();
  const float Along=FVector::DotProduct(Delta,GetActorForwardVector());
  const float Across=FMath::Abs(FVector::DotProduct(Delta,GetActorRightVector()));
  const bool bLocalCorridor=Across<FMath::Clamp(Sensor->SightRange,100.f,5000.f) && FMath::Abs(Along)<3000;
  if(bLocalCorridor && Along>=0) ForwardDistance=Along;
  const float Separation=FMath::Min(ForwardDistance,Length-ForwardDistance);
  const bool bClose=Separation<500 || (bLocalCorridor && FMath::Abs(Along)<500);
  State=Search->HasVisualContact()?ENeonCityPoliceState::Chase:bClose?ENeonCityPoliceState::Search:ENeonCityPoliceState::Investigate;
  DesiredSpeed=bClose?0:State==ENeonCityPoliceState::Chase?ResponseSpeed:650;
  // Brake before the recorded road position; do not circle through a stopped suspect.
  if(ForwardDistance<1500) DesiredSpeed=FMath::Min(DesiredSpeed,FMath::Max(0.f,(ForwardDistance-450.f)*1.2f));
 }
 // While approaching an old report, watch the road ahead. A moving car must not stare
 // backward at the report point; stop/search and confirmed pursuit can focus that evidence.
 Sensor->FocusEvidence(Search->GetLastKnownLocation(),bResponding && Search->HasLocation() && State!=ENeonCityPoliceState::Investigate);
 Sensor->SetScanning(State==ENeonCityPoliceState::Search && GetWorld()->GetTimeSeconds()-Search->GetEvidenceTime()>1.f);
 SetSiren(bResponding && State!=ENeonCityPoliceState::Patrol);
 Beacon->SetVisibility(!bSiren || FMath::Fmod(GetWorld()->GetTimeSeconds(),.6f)<.3f);
 const bool Recovering=RecoveryRemaining>0;
 const float TravelSign=Recovering?-1.f:1.f;
 Speed=FMath::FInterpConstantTo(Speed,Recovering?180.f:DesiredSpeed,DeltaSeconds,700);
 if(Speed<1) return;
 const FVector Start=GetActorLocation();
 const FVector Ahead=Spline->GetLocationAtDistanceAlongSpline(FMath::Fmod(Distance+TravelSign*FMath::Max(170.f,Speed*.45f)+Length,Length),ESplineCoordinateSpace::World);
 FHitResult Obstacle; FCollisionQueryParams Query(SCENE_QUERY_STAT(PoliceYield),false,this);
 if(GetWorld()->SweepSingleByChannel(Obstacle,Start,Ahead,GetActorQuat(),ECC_Vehicle,FCollisionShape::MakeBox(FVector(Collision->GetUnscaledBoxExtent().X,Collision->GetUnscaledBoxExtent().Y,60)),Query))
 {
  const float Now=GetWorld()->GetTimeSeconds();
  if(Recovering) { RecoveryRemaining=0; NextRecovery=Now+8; }
  else
  {
   if(BlockedSince<0) BlockedSince=Now;
   // A bounded, swept reverse creates space to retry the road approach. Never snap back to the spline.
   if(Now-BlockedSince>2.5f && Now>=NextRecovery) { RecoveryRemaining=250; NextRecovery=Now+8; BlockedSince=-1; }
  }
  Speed=0; return;
 }
 BlockedSince=-1;
 const float Step=TravelSign*FMath::Min(Speed*FMath::Min(DeltaSeconds,.1f),Recovering?RecoveryRemaining:1000.f), NextDistance=FMath::Fmod(Distance+Step+Length,Length);
 const FVector Next=Spline->GetLocationAtDistanceAlongSpline(NextDistance,ESplineCoordinateSpace::World);
 const FRotator Rotation=Spline->GetRotationAtDistanceAlongSpline(NextDistance,ESplineCoordinateSpace::World);
 FindComponentByClass<UPhysicalImpactComponent>()->SetKinematicVelocity((Next-Start)/DeltaSeconds);
 FHitResult Hit; SetActorLocationAndRotation(Next,FRotator(0,Rotation.Yaw,0),true,&Hit);
 if(Hit.bBlockingHit) FindComponentByClass<UPhysicalImpactComponent>()->SubmitSweep(Hit.GetActor(),Hit);
 const float ActualStep=Step*(Hit.bBlockingHit?Hit.Time:1.f);
 Distance=FMath::Fmod(Distance+ActualStep+Length,Length);
 if(Recovering) RecoveryRemaining=FMath::Max(0.f,RecoveryRemaining-FMath::Abs(ActualStep));
 Velocity=(GetActorLocation()-Start)/DeltaSeconds;
 WheelAngle=FMath::Fmod(WheelAngle+FMath::RadiansToDegrees(Velocity.Size()*DeltaSeconds/PresentationWheelRadiusCm),360.f);
 for(UStaticMeshComponent* Wheel:Wheels) Wheel->SetRelativeRotation(FRotator(0,0,90).Quaternion()*FQuat(FVector::UpVector,FMath::DegreesToRadians(WheelAngle)));
}
bool APoliceVehicle::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float APoliceVehicle::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState APoliceVehicle::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
