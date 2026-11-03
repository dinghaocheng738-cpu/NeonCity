#include "Adventure/IslandOffroadVehicle.h"
#include "Vehicles/VehicleData.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Misc/PackageName.h"

UIslandTrailWheel::UIslandTrailWheel()
{
 WheelRadius=39;WheelWidth=28;WheelMass=29;SuspensionMaxRaise=18;SuspensionMaxDrop=18;
 SpringRate=43000;SpringPreload=1000;SuspensionDampingRatio=.67f;RollbarScaling=.38f;
 WheelLoadRatio=.78f;FrictionForceMultiplier=2.7f;
}
UIslandTrailFrontWheel::UIslandTrailFrontWheel(){AxleType=EAxleType::Front;bAffectedBySteering=true;bAffectedByHandbrake=false;MaxSteerAngle=38;}
UIslandPickupWheel::UIslandPickupWheel(){SpringRate=51000;SpringPreload=1500;SuspensionDampingRatio=.52f;SuspensionMaxDrop=20;RollbarScaling=.24f;}
UIslandPickupFrontWheel::UIslandPickupFrontWheel(){AxleType=EAxleType::Front;bAffectedBySteering=true;bAffectedByHandbrake=false;MaxSteerAngle=36;SpringRate=42000;}
AIslandOffroadVehicle::AIslandOffroadVehicle()
{
 bStartsOwned=true;
 LeftHeadlight=CreateDefaultSubobject<USpotLightComponent>(TEXT("ExpeditionHeadlightLeft"));
 RightHeadlight=CreateDefaultSubobject<USpotLightComponent>(TEXT("ExpeditionHeadlightRight"));
 for(auto* Light:{LeftHeadlight.Get(),RightHeadlight.Get()})
 {
  Light->SetupAttachment(GetMesh());Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(4200);
  Light->SetAttenuationRadius(4500);Light->SetInnerConeAngle(17);Light->SetOuterConeAngle(32);
  Light->SetLightColor(FLinearColor(1,.94f,.84f));Light->SetRelativeRotation(FRotator(-4,0,0));Light->SetVisibility(false);
 }
 ConfigureTrailChassis();
}
void AIslandOffroadVehicle::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const bool bDriverPresent=IsPlayerControlled();
 if(LeftHeadlight->IsVisible()!=bDriverPresent){LeftHeadlight->SetVisibility(bDriverPresent);RightHeadlight->SetVisibility(bDriverPresent);}
}
void AIslandOffroadVehicle::ConfigureTrailChassis()
{
 auto* M=GetChaosMovement();M->DifferentialSetup.DifferentialType=EVehicleDifferential::AllWheelDrive;
 if(LeftHeadlight){LeftHeadlight->SetRelativeLocation(FVector(bPickup?246:195,-62,bPickup?70:80));RightHeadlight->SetRelativeLocation(FVector(bPickup?246:195,62,bPickup?70:80));}
 M->DifferentialSetup.FrontRearSplit=bPickup?.4f:.5f;
 M->TransmissionSetup.FinalRatio=4.2f;M->TransmissionSetup.ForwardGearRatios={4.1f,2.6f,1.8f,1.25f,.9f};
 M->CenterOfMassOverride=FVector(bPickup?-12:0,0,30);M->DragCoefficient=bPickup?.42f:.45f;
 M->ChassisWidth=195;M->ChassisHeight=bPickup?175:195;
 M->SteeringSetup.SteeringCurve.EditorCurveData.Reset();
 M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(0,1);
 M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(35,.55);
 M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(70,.24);
 M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(100,.16);
 for(int32 I=0;I<M->WheelSetups.Num();++I)
 {
  M->WheelSetups[I].WheelClass=bPickup?(I<2?UIslandPickupFrontWheel::StaticClass():UIslandPickupWheel::StaticClass()):(I<2?UIslandTrailFrontWheel::StaticClass():UIslandTrailWheel::StaticClass());
  if(const auto* Rig=GetMesh()->GetSkeletalMeshAsset())
  {
   const auto& Skeleton=Rig->GetRefSkeleton();int32 Bone=Skeleton.FindBoneIndex(M->WheelSetups[I].BoneName);
   if(Bone!=INDEX_NONE)
   {
    FTransform T=Skeleton.GetRefBonePose()[Bone];while((Bone=Skeleton.GetParentIndex(Bone))!=INDEX_NONE)T*=Skeleton.GetRefBonePose()[Bone];
    const FVector Centre((I<2?1:-1)*(bPickup?153.658f:121.466f),(I%2==0?-1:1)*(bPickup?89.f:86.f),11);
    M->WheelSetups[I].AdditionalOffset=Centre-T.GetLocation();
   }
  }
 }
 if(auto* Occupant=FindComponentByClass<UTransportOccupantComponent>())
 {
  Occupant->SeatHipLocation=FVector(bPickup?13.f:-12.f,-40,59);
  Occupant->LeftHandTarget=FVector(bPickup?67.f:29.f,-53,bPickup?94.f:98.f);
  Occupant->RightHandTarget=FVector(bPickup?67.f:29.f,-27,bPickup?94.f:98.f);
  Occupant->LeftFootTarget=FVector(bPickup?86.f:48.f,-53,25);
  Occupant->RightFootTarget=FVector(bPickup?86.f:48.f,-27,25);
 }
 if(auto* Bounds=FindComponentByClass<UBoxComponent>())Bounds->SetBoxExtent(FVector(bPickup?285:215,112,105));
 const FString Physics=TEXT("/Game/NeonCity/Art/Polish/Adventure/Offroad/")+(bPickup?FString(TEXT("PA_ExpeditionPickup")):FString(TEXT("PA_ExpeditionSUV")));
 if(FPackageName::DoesPackageExist(Physics))if(auto* Asset=LoadObject<UPhysicsAsset>(nullptr,*Physics))SetChassisPhysicsAsset(Asset);
}
void AIslandOffroadVehicle::ApplyExpeditionBody()
{
 const FString Base=TEXT("/Game/NeonCity/Art/Polish/Adventure/Offroad/");
 const FString BodyPath=Base+(bPickup?TEXT("SM_ExpeditionPickup"):TEXT("SM_ExpeditionSUV"));
 auto* Body=FPackageName::DoesPackageExist(BodyPath)?LoadObject<UStaticMesh>(nullptr,*BodyPath):nullptr;
 auto* Wheel=FPackageName::DoesPackageExist(Base+TEXT("SM_ExpeditionWheel"))?LoadObject<UStaticMesh>(nullptr,*(Base+TEXT("SM_ExpeditionWheel"))):nullptr;
 TInlineComponentArray<UStaticMeshComponent*> Parts(this);
 for(auto* Part:Parts)
 {
  if(Body && Part->GetFName()==TEXT("ArtVisual"))
  {
   Part->SetStaticMesh(Body);Part->EmptyOverrideMaterials();
   // Exterior and cabin panes share the authored mesh. Dynamic triangle order
   // prevents alternating dark faces as the gameplay camera moves past glass.
   Part->bSortTriangles=true;Part->MarkRenderStateDirty();
  }
  if(Wheel && Part->GetName().StartsWith(TEXT("SedanWheel"))){Part->SetStaticMesh(Wheel);Part->EmptyOverrideMaterials();Part->SetRelativeScale3D(FVector::OneVector);}
 }
}
void AIslandOffroadVehicle::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);ConfigureTrailChassis();ApplyExpeditionBody();}
void AIslandOffroadVehicle::BeginPlay(){ConfigureTrailChassis();Super::BeginPlay();ApplyExpeditionBody();}
