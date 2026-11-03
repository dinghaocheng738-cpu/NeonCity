#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Sound/SoundWave.h"
#include "Misc/PackageName.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool AIslandSupercar::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(!Character || !PresentationProfile || Hit.GetComponent()!=RoofMechanism->GetFrontPanel())
  return Super::QueryStandingSurface_Implementation(Hit,Character,Surface);
 if(RoofMechanism->IsCycling() || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())return false;
 const FVector Normal=RoofMechanism->GetFrontPanel()->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal);
 if(Normal.Z<.8f)return false;
 Surface=FDynamicStandingSurface();
 Surface.PointVelocity=GetMesh()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint,TEXT("Root"));
 return true;
}

namespace HeroCar
{
 const FString Base=TEXT("/Game/NeonCity/Art/Polish/Adventure/Supercars/");
 const TCHAR* WheelNames[]={TEXT("wheel_fl"),TEXT("wheel_fr"),TEXT("wheel_rl"),TEXT("wheel_rr")};
 const FVector AsterCentres[]={FVector(157.30,-88.88,12.13),FVector(157.48,88.77,12.13),FVector(-139.29,-89.34,12.13),FVector(-139.29,89.41,12.13)};
 const FVector RoadsterCentres[]={FVector(119.49,-83.60,11.27),FVector(119.58,85.01,11.00),FVector(-154.87,-83.13,11.00),FVector(-154.73,82.80,11.00)};
 const FVector NacreCentres[]={FVector(142,-88,12),FVector(142,88,12),FVector(-142,-91,12),FVector(-142,91,12)};
 const FString NacreBase=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Nacre/");
 template<class T> T* Asset(const FString& Path){return FPackageName::DoesPackageExist(Path)?LoadObject<T>(nullptr,*Path):nullptr;}
}
UIslandSupercarWheel::UIslandSupercarWheel()
{
 WheelRadius=39.48f;WheelWidth=27;WheelMass=22;SuspensionMaxRaise=8;SuspensionMaxDrop=10;
 // Chaos integrates displacement in cm and converts this coefficient to
 // force units internally: 620 here is a 62 kN/m road spring, not 62000.
 SpringRate=620;SpringPreload=750;SuspensionDampingRatio=.82f;RollbarScaling=.58f;
 WheelLoadRatio=.85f;FrictionForceMultiplier=3.1f;
 // Rear-biased mass needs greater rear axle cornering stiffness. Equal axle
 // stiffness produced a speed-dependent oversteer instability above 230 km/h.
 CorneringStiffness=1800;
}
UIslandSupercarFrontWheel::UIslandSupercarFrontWheel(){AxleType=EAxleType::Front;bAffectedBySteering=true;bAffectedByHandbrake=false;MaxSteerAngle=38;CorneringStiffness=1050;}
UIslandRoadsterWheel::UIslandRoadsterWheel(){WheelRadius=35.80f;SpringRate=550;SuspensionDampingRatio=.80f;RollbarScaling=.48f;}
UIslandRoadsterFrontWheel::UIslandRoadsterFrontWheel(){AxleType=EAxleType::Front;bAffectedBySteering=true;bAffectedByHandbrake=false;MaxSteerAngle=38;CorneringStiffness=1050;}
UIslandReplicaFrontWheel::UIslandReplicaFrontWheel(){WheelRadius=32.715f;WheelWidth=24.5f;AxleType=EAxleType::Front;bAffectedByEngine=false;}
UIslandReplicaRearWheel::UIslandReplicaRearWheel(){WheelRadius=34.55f;WheelWidth=30.5f;AxleType=EAxleType::Rear;bAffectedByEngine=true;}

AIslandSupercar::AIslandSupercar()
{
 bStartsOwned=true;
 RoofMechanism=CreateDefaultSubobject<UNacreRoofComponent>(TEXT("NacreRoofMechanism"));RoofMechanism->SetupAttachment(GetMesh());
 LeftLamp=CreateDefaultSubobject<USpotLightComponent>(TEXT("HeroHeadlightLeft"));RightLamp=CreateDefaultSubobject<USpotLightComponent>(TEXT("HeroHeadlightRight"));
 for(auto* Light:{LeftLamp.Get(),RightLamp.Get()})
 {
  Light->SetupAttachment(GetMesh());Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(900);
  Light->SetAttenuationRadius(6500);Light->SetInnerConeAngle(16);Light->SetOuterConeAngle(30);
  Light->SetLightColor(FLinearColor(.86f,.94f,1));Light->SetRelativeRotation(FRotator(-3,0,0));Light->SetVisibility(false);
 }
 for(int32 I=0;I<4;++I)
 {
  auto* Caliper=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HeroBrakeCaliper%d"),I));Caliper->SetupAttachment(GetMesh());Caliper->SetCollisionEnabled(ECollisionEnabled::NoCollision);BrakeCalipers.Add(Caliper);
  auto* Audio=CreateDefaultSubobject<UAudioComponent>(*FString::Printf(TEXT("HeroEngineLayer%d"),I));
  Audio->SetupAttachment(GetMesh());Audio->bAutoActivate=false;EngineLayers.Add(Audio);
 }
 Overrun=CreateDefaultSubobject<UAudioComponent>(TEXT("HeroOverrun"));Shift=CreateDefaultSubobject<UAudioComponent>(TEXT("HeroShift"));
 Overrun->SetupAttachment(GetMesh());Shift->SetupAttachment(GetMesh());Overrun->bAutoActivate=false;Shift->bAutoActivate=false;
 ConfigureChassis();
}
void AIslandSupercar::ConfigureChassis()
{
 const auto* VehicleProfile=GetVehicleProfile();
 PresentationProfile=VehicleProfile && !VehicleProfile->HeroPresentation.IsNull()
  ? VehicleProfile->HeroPresentation.LoadSynchronous()
  : bRetractable?HeroCar::Asset<UIslandSupercarPresentationData>(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/Assembly393/DA_720SSpiderPresentation")):nullptr;
 if(PresentationProfile && !PresentationProfile->ResolveRequiredAssets())PresentationProfile=nullptr;
 auto* M=GetChaosMovement();M->DifferentialSetup.DifferentialType=EVehicleDifferential::AllWheelDrive;
 M->DifferentialSetup.FrontRearSplit=bRoadster?.32f:.4f;M->TransmissionSetup.FinalRatio=bRoadster?3.7f:3.9f;
 M->TransmissionSetup.ForwardGearRatios={3.15f,2.24f,1.65f,1.28f,1.03f,.86f,.72f};M->TransmissionSetup.GearChangeTime=.15f;
 M->EngineSetup.MaxRPM=bRetractable?8400:(bRoadster?8200:9000);M->EngineSetup.EngineIdleRPM=950;M->EngineSetup.EngineRevDownRate=1400;
 M->TransmissionSetup.ChangeUpRPM=bRetractable?7700:(bRoadster?7300:8200);M->TransmissionSetup.ChangeDownRPM=bRetractable?3400:(bRoadster?3300:3700);
 M->EngineSetup.TorqueCurve.EditorCurveData.Reset();
 for(const auto& Key:TArray<FVector2D>{FVector2D(0,.45),FVector2D(.3,bRoadster?.98:.69),FVector2D(.72,1),FVector2D(1,.72)})M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(Key.X*M->EngineSetup.MaxRPM,Key.Y);
 M->CenterOfMassOverride=FVector(-12,0,3);M->DragCoefficient=bRoadster?.35f:.29f;M->ChassisWidth=195;M->ChassisHeight=112;
 M->DownforceCoefficient=bRoadster?.50f:.38f;
 if(PresentationProfile)
 {
  const auto& P=*PresentationProfile;
  M->DifferentialSetup.DifferentialType=EVehicleDifferential::RearWheelDrive;
  M->TransmissionSetup.FinalRatio=P.FinalDriveRatio;M->TransmissionSetup.ForwardGearRatios=P.ForwardGearRatios;
  M->EngineSetup.MaxRPM=P.MaximumEngineRPM;M->TransmissionSetup.ChangeUpRPM=P.MaximumEngineRPM-600;
  M->CenterOfMassOverride=P.CentreOfMassCm;M->ChassisWidth=P.ChassisDimensionsCm.Y;M->ChassisHeight=P.ChassisDimensionsCm.Z;
  M->EngineSetup.TorqueCurve.EditorCurveData.Reset();
  for(const auto& Key:TArray<FVector2D>{FVector2D(0,.42),FVector2D(2500,.82),FVector2D(5500,1),FVector2D(7200,.97),FVector2D(P.MaximumEngineRPM,.76)})M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(Key.X,Key.Y);
 }
 M->SteeringSetup.SteeringCurve.EditorCurveData.Reset();
 // Chaos evaluates this curve in mph. Preserve usable low-speed lock while
 // progressively reducing motorway sensitivity through native wheel steering.
 for(const auto& Key:TArray<FVector2D>{FVector2D(0,1),FVector2D(25,.85),FVector2D(45,.65),FVector2D(70,.45),FVector2D(100,.30),FVector2D(170,.20)})M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(Key.X,Key.Y);
 for(int32 I=0;I<M->WheelSetups.Num() && I<4;++I)
 {
  M->WheelSetups[I].WheelClass=PresentationProfile?(I<2?UIslandReplicaFrontWheel::StaticClass():UIslandReplicaRearWheel::StaticClass()):((bRoadster || bRetractable)?(I<2?UIslandRoadsterFrontWheel::StaticClass():UIslandRoadsterWheel::StaticClass()):(I<2?UIslandSupercarFrontWheel::StaticClass():UIslandSupercarWheel::StaticClass()));
  if(const auto* Rig=GetMesh()->GetSkeletalMeshAsset())
  {
   const auto& S=Rig->GetRefSkeleton();int32 B=S.FindBoneIndex(M->WheelSetups[I].BoneName);
   if(B!=INDEX_NONE){FTransform T=S.GetRefBonePose()[B];while((B=S.GetParentIndex(B))!=INDEX_NONE)T*=S.GetRefBonePose()[B];
    const auto* Centres=bRetractable?HeroCar::NacreCentres:(bRoadster?HeroCar::RoadsterCentres:HeroCar::AsterCentres);const int32 Axle=I<2?0:2;
    FVector Centre=(Centres[Axle]+Centres[Axle+1])*.5;Centre.Y=(FMath::Abs(Centres[Axle].Y)+FMath::Abs(Centres[Axle+1].Y))*.5*(I%2?1:-1);
    // Mesh bounds include asymmetric brake/tread detail; the mechanical axle
    // itself must stay square and symmetric at highway speeds.
    if(PresentationProfile)Centre=PresentationProfile->Wheels[I].CentreCm;
    M->WheelSetups[I].AdditionalOffset=Centre-T.GetLocation();}
  }
 }
 if(auto* Seat=FindComponentByClass<UTransportOccupantComponent>())
 {
  const float Y=(bRoadster || bRetractable)?-40:0;Seat->SeatHipLocation=FVector(-23,Y,(bRoadster || bRetractable)?21:5);
  Seat->LeftHandTarget=FVector(34,Y-14,48);Seat->RightHandTarget=FVector(34,Y+14,48);
  Seat->LeftFootTarget=FVector(60,Y-12,-9);Seat->RightFootTarget=FVector(60,Y+12,-9);
  Seat->SeatReclineDegrees=PresentationProfile?PresentationProfile->DriverReclineDegrees:0;
  if(PresentationProfile){const auto& P=*PresentationProfile;Seat->SeatHipLocation=P.DriverHipCm;Seat->LeftHandTarget=P.LeftHandCm;Seat->RightHandTarget=P.RightHandCm;Seat->LeftFootTarget=P.LeftFootCm;Seat->RightFootTarget=P.RightFootCm;}
 }
 if(auto* Box=FindComponentByClass<UBoxComponent>())Box->SetBoxExtent(FVector(245,110,75));
 if(auto* PA=HeroCar::Asset<UPhysicsAsset>(bRetractable?HeroCar::NacreBase+TEXT("PA_HeroNacre"):(HeroCar::Base+(bRoadster?TEXT("PA_HeroKestrel"):TEXT("PA_HeroAster")))))SetChassisPhysicsAsset(PA);
 // The optical origin must clear the opaque nose, which otherwise shadows the
 // entire beam despite the lamps reporting visible in normal night driving.
 if(LeftLamp){LeftLamp->SetRelativeLocation(FVector(bRetractable?241:(bRoadster?226:256),-68,29));RightLamp->SetRelativeLocation(FVector(bRetractable?241:(bRoadster?226:256),68,29));}
 if(PresentationProfile)
 {
  if(auto* PA=PresentationProfile->Chassis.LoadSynchronous())SetChassisPhysicsAsset(PA);
  if(auto* Box=FindComponentByClass<UBoxComponent>())Box->SetBoxExtent(PresentationProfile->InteractionHalfExtentsCm);
  LeftLamp->SetRelativeLocation(PresentationProfile->LeftHeadlightCm);RightLamp->SetRelativeLocation(PresentationProfile->RightHeadlightCm);
 }
}
void AIslandSupercar::ConfigureReplicaRuntimePhysics()
{
 GetDrivingComponent()->SetNativeParkingBrake(true);
 if(!PresentationProfile)return;
 auto* M=GetChaosMovement();M->SetMaxEngineTorque(PresentationProfile->PeakEngineTorqueNm);
 M->EngineSetup.MaxTorque=PresentationProfile->PeakEngineTorqueNm;
 for(int32 I=0;I<M->Wheels.Num() && I<4;++I)
 {
  if(M->Wheels[I])M->Wheels[I]->WheelRadius=PresentationProfile->Wheels[I].RadiusCm;
  M->SetWheelRadius(I,PresentationProfile->Wheels[I].RadiusCm);
 }
}
void AIslandSupercar::ApplyHeroBody()
{
 HeroWheels.SetNum(4);
 const FString Model=bRetractable?TEXT("Nacre"):(bRoadster?TEXT("Kestrel"):TEXT("Aster"));const FString Base=bRetractable?HeroCar::NacreBase:HeroCar::Base;TInlineComponentArray<UStaticMeshComponent*> Parts(this);
 for(int32 I=0;I<4;++I)
 {
  BrakeCalipers[I]->SetStaticMesh(HeroCar::Asset<UStaticMesh>(Base+TEXT("SM_")+Model+TEXT("_")+HeroCar::WheelNames[I]+TEXT("_caliper")));
  BrakeCalipers[I]->SetRelativeLocationAndRotation(bRetractable?HeroCar::NacreCentres[I]:(bRoadster?HeroCar::RoadsterCentres[I]:HeroCar::AsterCentres[I]),FRotator(0,0,-90));
  if(PresentationProfile){const auto& W=PresentationProfile->Wheels[I];BrakeCalipers[I]->SetStaticMesh(W.Caliper.LoadSynchronous());BrakeCalipers[I]->SetRelativeLocationAndRotation(W.CentreCm,FRotator(0,0,W.AxleRollDegrees));}
 }
 for(auto* Part:Parts)
 {
  if(Part->GetFName()==TEXT("ArtVisual"))
  {
   if(auto* Body=PresentationProfile?PresentationProfile->Body.LoadSynchronous():HeroCar::Asset<UStaticMesh>(Base+TEXT("SM_Hero")+Model))
   {Part->SetStaticMesh(Body);Part->EmptyOverrideMaterials();Part->SetRelativeTransform(FTransform::Identity);Part->bSortTriangles=true;Part->MarkRenderStateDirty();}
  }
  for(int32 I=0;I<4;++I)if(Part->GetFName()==FName(*FString::Printf(TEXT("SedanWheel%d"),I)))
  {
   if(auto* Wheel=PresentationProfile?PresentationProfile->Wheels[I].Mesh.LoadSynchronous():HeroCar::Asset<UStaticMesh>(Base+TEXT("SM_")+Model+TEXT("_")+HeroCar::WheelNames[I]))
   {Part->SetStaticMesh(Wheel);Part->EmptyOverrideMaterials();Part->SetRelativeScale3D(FVector::OneVector);HeroWheels[I]=Part;}
  }
 }
}
void AIslandSupercar::OnConstruction(const FTransform& T){ConfigureChassis();Super::OnConstruction(T);ApplyHeroBody();RoofMechanism->Configure(bRetractable,PresentationProfile);}
void AIslandSupercar::BeginPlay(){ConfigureChassis();Super::BeginPlay();ConfigureReplicaRuntimePhysics();ApplyHeroBody();ConfigureSound();RoofMechanism->Configure(bRetractable,PresentationProfile);}
void AIslandSupercar::ConfigureSound()
{
 const FString Path=(bRetractable?HeroCar::NacreBase:HeroCar::Base)+TEXT("Audio/");const FString Family=bRetractable?TEXT("Nacre"):(bRoadster?TEXT("Kestrel"):TEXT("Aster"));
 const TCHAR* Bands[]={TEXT("Idle"),TEXT("Low"),TEXT("Mid"),TEXT("High")};
 FSoundAttenuationSettings Att;Att.bAttenuate=true;Att.bSpatialize=true;Att.AttenuationShapeExtents=FVector(220);Att.FalloffDistance=13000;
 for(int32 I=0;I<4;++I){EngineLayers[I]->SetSound(HeroCar::Asset<USoundWave>(Path+TEXT("SW_")+Family+Bands[I]));EngineLayers[I]->AdjustAttenuation(Att);EngineLayers[I]->SetRelativeLocation(FVector(-105,0,15));}
 Overrun->SetSound(HeroCar::Asset<USoundWave>(Path+TEXT("SW_")+Family+TEXT("Overrun")));Shift->SetSound(HeroCar::Asset<USoundWave>(Path+TEXT("SW_")+Family+TEXT("Shift")));
 Overrun->AdjustAttenuation(Att);Shift->AdjustAttenuation(Att);
}
void AIslandSupercar::Tick(float Dt)
{
 Super::Tick(Dt);const bool Running=GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Running;
 for(int32 I=0;I<4 && I<GetChaosMovement()->Wheels.Num();++I)
 {
  auto* Wheel=GetChaosMovement()->Wheels[I].Get();if(!Wheel)continue;
  FVector P=bRetractable?HeroCar::NacreCentres[I]:(bRoadster?HeroCar::RoadsterCentres[I]:HeroCar::AsterCentres[I]);P.Z+=Wheel->GetSuspensionOffset();
  if(PresentationProfile)P=PresentationProfile->Wheels[I].CentreCm+FVector(0,0,Wheel->GetSuspensionOffset());
  // The FBX wheels have an outward +Z/-Z axle. Unreal's positive roll is
  // clockwise around X; the sedan's symmetric wheel convention reverses them.
  const FQuat Axle=FRotator(0,Wheel->GetSteerAngle(),0).Quaternion()*FRotator(0,0,PresentationProfile?PresentationProfile->Wheels[I].AxleRollDegrees:-90).Quaternion();
  BrakeCalipers[I]->SetRelativeLocationAndRotation(P,Axle);
  if(HeroWheels.IsValidIndex(I) && HeroWheels[I])HeroWheels[I]->SetRelativeLocationAndRotation(P,Axle*FQuat(FVector::UpVector,FMath::DegreesToRadians(-Wheel->GetRotationAngle())));
 }
 LeftLamp->SetVisibility(Running);RightLamp->SetVisibility(Running);
 const float Throttle=GetChaosMovement()->GetThrottleInput();const int32 Gear=GetChaosMovement()->GetCurrentGear();
 SmoothedRPM=FMath::FInterpTo(SmoothedRPM,FMath::Max(950.f,GetDrivingComponent()->GetEngineRPM()),Dt,12);
 if(Running && PreviousGear>0 && Gear>0 && PreviousGear!=Gear){Shift->SetVolumeMultiplier(.24f);Shift->Play();ShiftEnvelope=1;}
 ShiftEnvelope=FMath::Max(0.f,ShiftEnvelope-Dt*7);PopCooldown=FMath::Max(0.f,PopCooldown-Dt);
 if(Running && PreviousThrottle>.6f && Throttle<.15f && SmoothedRPM>3200 && PopCooldown<=0){Overrun->SetVolumeMultiplier(.32f);Overrun->Play();PopCooldown=.65f;}
 const float RpmNodes[]={950,2800,5100,bRetractable?7700.f:(bRoadster?7300.f:8200.f)};float Weights[4]={};
 for(int32 I=0;I<3;++I)if(SmoothedRPM>=RpmNodes[I] && (SmoothedRPM<RpmNodes[I+1] || I==2))
 {const float T=FMath::Clamp((SmoothedRPM-RpmNodes[I])/(RpmNodes[I+1]-RpmNodes[I]),0.f,1.f);Weights[I]=FMath::Sqrt(1-T);Weights[I+1]=FMath::Sqrt(T);break;}
 if(SmoothedRPM<=950)Weights[0]=1;
 for(int32 I=0;I<4;++I)
 {
  auto* A=EngineLayers[I].Get();if(Running && A->Sound && !A->IsPlaying())A->Play();else if(!Running && A->IsPlaying())A->Stop();
  A->SetPitchMultiplier(FMath::Clamp(SmoothedRPM/RpmNodes[I],.65f,1.65f));
  A->SetVolumeMultiplier(Weights[I]*(.27f+.20f*Throttle)*(1-.50f*ShiftEnvelope)*FMath::Lerp(1.f,.66f,InteriorMix));
  A->SetLowPassFilterEnabled(InteriorMix>.01f);A->SetLowPassFilterFrequency(FMath::Lerp(20000.f,3000.f,InteriorMix));
 }
 PreviousThrottle=Throttle;PreviousGear=Gear;
}
void AIslandSupercar::SetupPlayerInputComponent(UInputComponent* Component)
{
 Super::SetupPlayerInputComponent(Component);RemoveRoofMapping();if(!bRetractable)return;
 auto* Input=Cast<UEnhancedInputComponent>(Component);auto* PC=Cast<APlayerController>(GetController());
 auto* Subsystem=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Input || !Subsystem)return;
 if(!RoofMapping)
 {
  RoofMapping=NewObject<UInputMappingContext>(this,NAME_None,RF_Transient);RoofAction=NewObject<UInputAction>(this,NAME_None,RF_Transient);
  RoofAction->ValueType=EInputActionValueType::Boolean;RoofMapping->MapKey(RoofAction,EKeys::R);
 }
 Input->BindAction(RoofAction,ETriggerEvent::Started,this,&AIslandSupercar::ToggleRoof);
 FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;Subsystem->AddMappingContext(RoofMapping,101,Options);RoofInputSubsystem=Subsystem;
}
void AIslandSupercar::ToggleRoof(){RoofMechanism->RequestToggle();}
void AIslandSupercar::RemoveRoofMapping()
{if(RoofInputSubsystem.IsValid())RoofInputSubsystem->RemoveMappingContext(RoofMapping);RoofInputSubsystem.Reset();}
void AIslandSupercar::UnPossessed(){RemoveRoofMapping();Super::UnPossessed();}
void AIslandSupercar::EndPlay(EEndPlayReason::Type Reason){RemoveRoofMapping();Super::EndPlay(Reason);}
