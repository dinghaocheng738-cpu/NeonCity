#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/NeonCitySedanVisuals.h"
#include "Vehicles/VehicleData.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandOffroadVehicle.h"

ANeonCityStarterSedan::ANeonCityStarterSedan()
{
 bStartsOwned=true;
	// Epic's tiny rig/physics proxy only. No sports body, vehicle Blueprint, inputs or tuning are imported.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rig(TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar"));
	if (Rig.Succeeded()) { GetMesh()->SetSkeletalMesh(Rig.Object); }
	static ConstructorHelpers::FObjectFinder<UPhysicsAsset> Collision(TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan.PA_StarterSedan"));
	// Cooked exports can construct actors on the async loading thread. The
	// runtime setter refreshes bones immediately; registration initializes this
	// default override on the game thread before creating the physics state.
	if (Collision.Succeeded()) { SetChassisPhysicsAsset(Collision.Object); }
	static ConstructorHelpers::FObjectFinder<UVehicleData> Tuning(TEXT("/Game/NeonCity/Vehicles/DA_StarterSedan.DA_StarterSedan"));
	if (Tuning.Succeeded()) { VehicleData = Tuning.Object; }
	GetMesh()->SetVisibility(false, false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	WheelVisuals = FNeonCitySedanVisuals::Create(*this,GetMesh());
}

void ANeonCityStarterSedan::SetChassisPhysicsAsset(UPhysicsAsset* Asset)
{
	if (GetMesh()->IsRegistered()) { GetMesh()->SetPhysicsAsset(Asset, false); }
	else { GetMesh()->PhysicsAssetOverride = Asset; }
}

void ANeonCityStarterSedan::OnConstruction(const FTransform& Transform)
{
	ConfigureFleetFit();
	Super::OnConstruction(Transform);
	UpdateWheelVisuals(false);
}

void ANeonCityStarterSedan::ConfigureFleetFit()
{
 // Dedicated island/showroom supercars configure their own fitted mechanics.
 // The common sedan base must not overwrite those after their configuration.
 if(IsA<AIslandSupercar>() || IsA<AIslandOffroadVehicle>()) {FleetPresentation=nullptr;FleetCalipers.Reset();return;}
 FleetPresentation=FNeonCitySedanVisuals::GetFleetPresentation();
 if(!FleetPresentation || !FleetPresentation->ResolveRequiredAssets()){FleetPresentation=nullptr;return;}
 FleetCalipers.SetNum(4);
 for(int32 I=0;I<4;++I)FleetCalipers[I]=FindObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("SedanCaliper%d"),I));
 const auto& P=*FleetPresentation;auto* M=GetChaosMovement();
 // Map instances may contain serialized legacy wheel overrides. Re-apply the
 // same measured presentation during construction and before physics starts.
 for(int32 I=0;I<WheelVisuals.Num() && I<4;++I)
 {
  WheelVisuals[I]->SetStaticMesh(P.Wheels[I].Mesh.Get());WheelVisuals[I]->EmptyOverrideMaterials();
  WheelVisuals[I]->SetRelativeScale3D(FVector::OneVector);
  if(FleetCalipers[I])FleetCalipers[I]->SetStaticMesh(P.Wheels[I].Caliper.Get());
 }
 if(auto* Legacy=FindObject<UStaticMeshComponent>(this,TEXT("ArtVisual"))) {Legacy->SetVisibility(false);Legacy->SetHiddenInGame(true);Legacy->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
 SetChassisPhysicsAsset(P.Chassis.Get());
 M->ChassisWidth=P.ChassisDimensionsCm.Y;M->ChassisHeight=P.ChassisDimensionsCm.Z;
 M->CenterOfMassOverride=P.CentreOfMassCm;
 const auto* Rig=GetMesh()->GetSkeletalMeshAsset();
 if(Rig)for(int32 I=0;I<4 && I<M->WheelSetups.Num();++I)
 {
  const auto& S=Rig->GetRefSkeleton();int32 B=S.FindBoneIndex(M->WheelSetups[I].BoneName);
  if(B==INDEX_NONE)continue;FTransform T=S.GetRefBonePose()[B];
  while((B=S.GetParentIndex(B))!=INDEX_NONE)T*=S.GetRefBonePose()[B];
  M->WheelSetups[I].AdditionalOffset=P.Wheels[I].CentreCm-T.GetLocation();
 }
 if(auto* Seat=FindComponentByClass<UTransportOccupantComponent>())
 {
  Seat->SeatHipLocation=P.DriverHipCm;Seat->LeftHandTarget=P.LeftHandCm;Seat->RightHandTarget=P.RightHandCm;
  Seat->LeftFootTarget=P.LeftFootCm;Seat->RightFootTarget=P.RightFootCm;Seat->SeatReclineDegrees=P.DriverReclineDegrees;
 }
 if(auto* Box=FindComponentByClass<UBoxComponent>()) {Box->SetBoxExtent(P.InteractionHalfExtentsCm);Box->SetRelativeLocation(FVector(0,0,25));}
}

void ANeonCityStarterSedan::BeginPlay()
{
 ConfigureFleetFit();Super::BeginPlay();
}

TArray<float> ANeonCityStarterSedan::GetFittedWheelRadii() const
{
 TArray<float> Radii;
 if(FleetPresentation)for(int32 I=0;I<4;++I)Radii.Add(FleetPresentation->Wheels[I].RadiusCm);
 return Radii;
}

void ANeonCityStarterSedan::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWheelVisuals(true);
}

void ANeonCityStarterSedan::UpdateWheelVisuals(bool bSimulating)
{
	UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement();
	for (int32 Index = 0; Index < WheelVisuals.Num() && Index < Movement->WheelSetups.Num(); ++Index)
	{
		const USkeletalMesh* Rig = GetMesh()->GetSkeletalMeshAsset();
		if (!Rig) { continue; }
		const FReferenceSkeleton& Skeleton = Rig->GetRefSkeleton();
		int32 BoneIndex = Skeleton.FindBoneIndex(Movement->WheelSetups[Index].BoneName);
		if (BoneIndex == INDEX_NONE) { continue; }
		FTransform BoneTransform = Skeleton.GetRefBonePose()[BoneIndex];
		while ((BoneIndex = Skeleton.GetParentIndex(BoneIndex)) != INDEX_NONE)
		{ BoneTransform *= Skeleton.GetRefBonePose()[BoneIndex]; }
		FVector Position = BoneTransform.GetLocation() + Movement->WheelSetups[Index].AdditionalOffset;
		float Steer = 0.f;
		float Spin = 0.f;
		if (bSimulating && Movement->Wheels.IsValidIndex(Index))
		{
			UChaosVehicleWheel* Wheel = Movement->Wheels[Index];
			Position.Z += Wheel->GetSuspensionOffset();
			Steer = Wheel->GetSteerAngle();
			Spin = Wheel->GetRotationAngle();
		}
		const FQuat Orientation = FRotator(0.f,Steer,0.f).Quaternion()
			* FRotator(0.f,0.f,90.f).Quaternion() * FQuat(FVector::UpVector,FMath::DegreesToRadians(Spin));
		WheelVisuals[Index]->SetRelativeLocationAndRotation(Position, Orientation);
  if(FleetPresentation && FleetCalipers.IsValidIndex(Index))
   if(auto* Caliper=FleetCalipers[Index].Get())
    Caliper->SetRelativeLocationAndRotation(Position,FRotator(0,Steer,FleetPresentation->Wheels[Index].AxleRollDegrees));
	}
}
