#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ANeonCityMotorYacht::ANeonCityMotorYacht()
{
 AutoPossessAI=EAutoPossessAI::Disabled;
 VehicleData=CreateDefaultSubobject<UMarineVehicleData>(TEXT("MotorYachtProfile"));
 VehicleData->VehicleType=TEXT("NeonCity.MotorYacht35");VehicleData->DisplayName=NSLOCTEXT("NeonCity","MotorYacht35","Riviera 35");
 VehicleData->MaxSpeed=55;VehicleData->Acceleration=2;VehicleData->TurningRate=18;
 // The inherited .22/s drag balanced the unchanged 2m/s² launch force at
 // only23km/h. This planing-yacht coefficient permits about49km/h with the
 // existing55km/h governor, without changing mass, power or buoyancy forces.
 VehicleData->WaterDrag=.07f;
 VehicleData->Mass=6000;VehicleData->EnginePower=600;VehicleData->Durability=1800;
 VehicleData->HullExtent=FVector(460,160,60);VehicleData->CenterOfMass=FVector(0,0,-25);
 VehicleData->InteractionReachCm=600;
 VehicleData->bPreferDeckExit=true;VehicleData->DeckExit=VehicleData->BoardingPoint=FVector(-480,0,49.9);
 VehicleData->CameraRig.HelmEye=FVector(43,58,192);VehicleData->CameraRig.ChaseAnchor=FVector(-40,0,200);VehicleData->CameraRig.ChaseDistance=1450;
 GetHull()->SetBoxExtent(VehicleData->HullExtent);GetHull()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
 TArray<UStaticMeshComponent*> Old;GetComponents(Old);
 for(auto* Part:Old){Part->SetVisibility(false);Part->SetHiddenInGame(true);}
 YachtBody=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YachtBody"));YachtBody->SetupAttachment(GetHull());
 YachtBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);YachtBody->SetCollisionResponseToAllChannels(ECR_Ignore);
 YachtBody->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);YachtBody->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 YachtBody->CanCharacterStepUpOn=ECB_Yes;YachtBody->SetCanEverAffectNavigation(false);
 YachtGlass=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YachtGlass"));YachtGlass->SetupAttachment(GetHull());
 YachtGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);YachtGlass->SetCanEverAffectNavigation(false);
 if(auto* Occupant=FindComponentByClass<UTransportOccupantComponent>())
 {
  Occupant->SeatHipLocation=FVector(10,58,119);Occupant->LeftHandTarget=FVector(39,44,148);Occupant->RightHandTarget=FVector(39,72,148);
  Occupant->LeftFootTarget=FVector(45,44,56);Occupant->RightFootTarget=FVector(45,72,56);
 }
}
void ANeonCityMotorYacht::BeginPlay()
{
 Super::BeginPlay();GetMarineMovement()->SetMooringBerth(GetActorTransform());
 if(auto* Effects=FindComponentByClass<UMarineWaterEffectsComponent>();Effects && YachtBody->GetStaticMesh())
 {
  FVector Low,High;YachtBody->GetLocalBounds(Low,High);
  const auto Relative=YachtBody->GetRelativeTransform();
  // The visible swim platform extends beyond the collision hull. Start the
  // surface churn just aft of that real mesh, rather than inside its cockpit.
  Effects->ConfigureTransomWash(Relative.TransformPosition(FVector(Low.X-15,0,0)),
   (High.Y-Low.Y)*.5f*FMath::Abs(Relative.GetScale3D().Y));
 }
}
FText ANeonCityMotorYacht::GetInteractionName_Implementation() const
{
 return NSLOCTEXT("NeonCity","EnterMotorYacht","Board Riviera 35");
}
bool ANeonCityMotorYacht::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(!Character || Hit.GetComponent()!=YachtBody || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())return false;
 const FVector Local=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
 // Exact source mesh support, restricted to usable deck/cockpit levels.
 // Roofs, high gunwales and mast details must not qualify for saved standing.
 if(Local.Z>130 || Local.Z<25 || FMath::Abs(Local.Y)>150 || FMath::Abs(Local.X)>500)return false;
 Surface=FDynamicStandingSurface();Surface.PointVelocity=GetHull()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);return true;
}
