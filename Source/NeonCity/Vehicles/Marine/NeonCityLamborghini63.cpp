#include "Vehicles/Marine/NeonCityLamborghini63.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineDriveTelemetry.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "TimerManager.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

namespace NeonCityLamborghini63
{
// hull-local cm, x forward, waterline z = 0 (lmb63.py, UE = Blender * 100 with y mirrored: starboard is +y)
const FVector HelmSeat(-125,85,148);
// where people are put aboard: the cockpit sole aft of the dinette table, and the swim platform
const FVector CockpitSole(-515,0,102),SwimPlatform(-985,0,47);
const float PlatformReach=330.f,AshoreFromX=-560.f,AlongsideReach=320.f;   // cm: in the water this far off her side, F climbs aboard                 // cm: platform climb radius; aft of this is 'the stern'
const float DockingKmh=11.f,CruisingKmh=74.f;                     // 6 kn, 40 kn; Corsa is the profile's 63 kn
// ambient lights (lmb63.py positions): name, hull-local cm, lumens, radius, colour, spot direction (zero: point), exterior
struct FAmbientLight{const TCHAR* Name;FVector At;float Lumens,Radius;FLinearColor Color;FVector Aim;bool bExterior;};
const FLinearColor Warm(1.f,.68f,.42f),Cabin(1.f,.86f,.72f),Teal(.15f,.9f,.85f);
const FAmbientLight Ambient[]=
{
 {TEXT("OwnerCove"),FVector(520,0,140),300,480,Cabin,FVector::ZeroVector,false},   // owner's suite: ceiling tray
 {TEXT("OwnerTV"),FVector(380,0,130),120,380,Cabin,FVector::ZeroVector,false},
 {TEXT("Lobby"),FVector(200,-60,125),220,380,Cabin,FVector::ZeroVector,false},     // galley lobby
 {TEXT("Head"),FVector(280,60,125),120,240,Cabin,FVector::ZeroVector,false},
 {TEXT("Guest"),FVector(-140,60,60),160,320,Cabin,FVector::ZeroVector,false},      // twin guest cabin
 {TEXT("Dinette"),FVector(-390,0,140),700,500,Warm,FVector::ZeroVector,true},
 {TEXT("Helm"),FVector(-40,0,135),400,380,Warm,FVector::ZeroVector,true},
 {TEXT("CanopyFwdP"),FVector(-180,-160,290),450,520,Warm,FVector(0,0,-1),true},   // canopy downlight bars
 {TEXT("CanopyFwdS"),FVector(-180,160,290),450,520,Warm,FVector(0,0,-1),true},
 {TEXT("CanopyAftP"),FVector(-400,-160,288),450,520,Warm,FVector(0,0,-1),true},
 {TEXT("CanopyAftS"),FVector(-400,160,288),450,520,Warm,FVector(0,0,-1),true},
 {TEXT("Tier1"),FVector(-860,0,115),350,300,Warm,FVector::ZeroVector,true},       // LED under the stern tiers
 {TEXT("Tier2"),FVector(-905,0,88),350,300,Warm,FVector::ZeroVector,true},
 {TEXT("SunPad"),FVector(-700,0,200),500,420,Warm,FVector::ZeroVector,true},
 {TEXT("UnderwaterP"),FVector(-965,-115,-30),3000,1300,Teal,FVector(-1,0,-.6),true},
 {TEXT("UnderwaterS"),FVector(-965,115,-30),3000,1300,Teal,FVector(-1,0,-.6),true},
};
}
using namespace NeonCityLamborghini63;

ANeonCityLamborghini63::ANeonCityLamborghini63()
{
 AutoPossessAI=EAutoPossessAI::Disabled;
 VehicleData=CreateDefaultSubobject<UMarineVehicleData>(TEXT("Lamborghini63Profile"));
 auto& D=*VehicleData;
 D.VehicleType=TEXT("NeonCity.Lamborghini63");D.DisplayName=NSLOCTEXT("NeonCity","Lamborghini63","Tecnomar for Lamborghini 63");
 // 24 t, 2 x 1470 kW: a carbon planing hull that leaps onto the plane and holds a hard turn
 D.MaxSpeed=116;D.Acceleration=4.2f;D.TurningRate=24;D.TurningResistance=2.2f;D.WaterDrag=.05f;D.LateralDrag=2.4f;
 D.Mass=24000;D.EnginePower=2940;D.Buoyancy=1.9f;D.VerticalDamping=3.5f;D.Durability=6000;D.BrakeDrag=1.2f;
 D.ThrottleRiseRate=.9f;D.ThrottleFallRate=1.6f;
 D.HullExtent=FVector(940,255,95);D.CenterOfMass=FVector(-160,0,-35);
 D.bInlandWater=false;D.bFoamWake=true;
 D.InteractionReachCm=850;D.BoardingPoint=SwimPlatform;D.DeckExit=FVector(-60,-10,102);D.bWalkableDeck=true;   // walk the decks
 D.HelmReachCm=620;      // F takes the helm from anywhere in the cockpit and the dinette (the helm itself is a closed driver's well)
 D.CameraRig.HelmEye=HelmSeat+FVector(-5,0,78);D.CameraRig.HelmPitch=-5;D.CameraRig.HelmFieldOfView=86;
 D.CameraRig.ChaseAnchor=FVector(-100,0,260);D.CameraRig.ChaseDistance=1900;D.CameraRig.HorizonStabilization=.7f;
 ensureMsgf(D.IsValidProfile(),TEXT("Lamborghini 63 profile out of the marine limits: the default speedboat would be used"));
 GetHull()->SetBoxExtent(D.HullExtent);
 // people walk on the modelled decks, platform and stern tiers (the body mesh); the physics box would otherwise put
 // an invisible ledge 95 cm above the waterline across the swim platform and the lower tiers
 GetHull()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
 TArray<UStaticMeshComponent*> Placeholder;GetComponents(Placeholder);
 for(auto* Part:Placeholder){Part->SetVisibility(false);Part->SetHiddenInGame(true);}
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LMB63Body"));Body->SetupAttachment(GetHull());Body->SetCanEverAffectNavigation(false);
 // the hull box carries the physics; the body only blocks people and traces (not welded into the rigid body)
 Body->BodyInstance.bAutoWeld=false;Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionResponseToAllChannels(ECR_Ignore);
 Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);Body->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Glass=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LMB63Glass"));Glass->SetupAttachment(GetHull());
 Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);Glass->SetCanEverAffectNavigation(false);Glass->SetCastShadow(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_LMB63.SM_WL_LMB63"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassMesh(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_LMB63Glass.SM_WL_LMB63Glass"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> DetailMesh(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_LMB63Detail.SM_WL_LMB63Detail"));
 Detail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LMB63Detail"));Detail->SetupAttachment(GetHull());
 Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);Detail->SetCanEverAffectNavigation(false);
 if(DetailMesh.Succeeded())Detail->SetStaticMesh(DetailMesh.Object);
 if(BodyMesh.Succeeded())Body->SetStaticMesh(BodyMesh.Object);
 for(const FAmbientLight& A:Ambient)
 {
  ULocalLightComponent* L=nullptr;
  if(A.Aim.IsZero())L=CreateDefaultSubobject<UPointLightComponent>(A.Name);
  else{auto* S=CreateDefaultSubobject<USpotLightComponent>(A.Name);S->SetOuterConeAngle(60);S->SetInnerConeAngle(25);S->SetRelativeRotation(A.Aim.Rotation());L=S;}
  L->SetupAttachment(Body);L->SetRelativeLocation(A.At);L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(A.bExterior?0.f:A.Lumens);
  L->SetAttenuationRadius(A.Radius);L->SetLightColor(A.Color);L->SetCastShadows(false);
  AmbientLights.Add(L);AmbientLumens.Add(A.Lumens);AmbientExterior.Add(A.bExterior);
 }
 if(GlassMesh.Succeeded())Glass->SetStaticMesh(GlassMesh.Object);
 // passerelle: drawn by the gangway mesh (1 m long along +X, scaled to the reach), walked on by the tread box
 static ConstructorHelpers::FObjectFinder<UStaticMesh> GangwayMesh(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_LMB63Gangway.SM_WL_LMB63Gangway"));
 Gangway=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LMB63Gangway"));Gangway->SetupAttachment(Body);
 Gangway->SetCollisionEnabled(ECollisionEnabled::NoCollision);Gangway->SetCanEverAffectNavigation(false);Gangway->SetVisibility(false);
 if(GangwayMesh.Succeeded())Gangway->SetStaticMesh(GangwayMesh.Object);
 GangwayTeak=CreateDefaultSubobject<UBoxComponent>(TEXT("GangwayTeak"));GangwayTeak->SetupAttachment(Body);
 GangwayTeak->BodyInstance.bAutoWeld=false;GangwayTeak->SetCollisionEnabled(ECollisionEnabled::NoCollision);GangwayTeak->SetCollisionResponseToAllChannels(ECR_Ignore);
 GangwayTeak->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);GangwayTeak->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 GangwayTeak->SetCanEverAffectNavigation(false);GangwayTeak->SetHiddenInGame(true);
 if(auto* Occupant=FindComponentByClass<UTransportOccupantComponent>())
 {
  // the driver in the starboard bucket seat, hands on the Huracan wheel, feet on the footrest
  Occupant->SeatHipLocation=HelmSeat;Occupant->SeatReclineDegrees=12;
  Occupant->LeftHandTarget=FVector(-55,66,164);Occupant->RightHandTarget=FVector(-55,104,164);      // the wheel, 9 and 3 o'clock
  Occupant->LeftFootTarget=FVector(-28,72,112);Occupant->RightFootTarget=FVector(-28,98,112);        // the sloped footrest
 }
}

void ANeonCityLamborghini63::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 ApplyLivery();
}
void ANeonCityLamborghini63::ApplyLivery()
{
 for(UStaticMeshComponent* Part:{Body.Get(),Detail.Get()})
 {
  const int32 Slot=Part?Part->GetMaterialIndex(TEXT("Paint")):INDEX_NONE;if(Slot==INDEX_NONE)continue;
  if(!bCustomLivery){Part->SetMaterial(Slot,nullptr);continue;}      // back to the mesh's Arancio paint
  auto* Paint=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(Slot));
  if(!Paint)Paint=Part->CreateDynamicMaterialInstance(Slot);
  if(Paint)Paint->SetVectorParameterValue(TEXT("BaseColor"),LiveryColor);
 }
}
void ANeonCityLamborghini63::BeginPlay()
{
 Super::BeginPlay();
 ApplyLivery();
 // F works from the deck, the quay and the water, wherever one looks (added here: a placed instance keeps its saved tags)
 Tags.AddUnique(UInteractionComponent::ProximityTag);
 GetMarineMovement()->SetMooringBerth(GetActorTransform());
 DriveMode=ENeonCityDriveMode::Cruising;
 UpdateAmbientLights();
 GetWorldTimerManager().SetTimer(AmbientTimer,[this](){UpdateAmbientLights();UpdateGangway();},.5f,true);
 UpdateGangway();
}

bool ANeonCityLamborghini63::FindQuay(FVector& Hinge,FVector& Foot) const
{
 // from the platform's aft corner on either side, swing out (square to her side, then raked aft) and run out until
 // the far end rests on dry, walkable ground at a walkable rise: that is the quay alongside
 const FTransform X=GetActorTransform();auto* Marine=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(YachtQuay),false,this);
 for(float Side:{-1.f,1.f})
 {
  const FVector H=X.TransformPosition(FVector(-1045,Side*125,47));
  for(float Rake:{25.f,15.f,35.f,5.f})
  {
   const FVector Dir=X.TransformVectorNoScale(FRotator(0,Side*(90.f+Rake),0).Vector());
   for(float Run=200;Run<=520;Run+=20)
   {
    const FVector P=H+Dir*Run;FHitResult G;
    if(!GetWorld()->LineTraceSingleByChannel(G,P+FVector(0,0,250),P-FVector(0,0,250),ECC_Visibility,Q) || G.ImpactNormal.Z<.75f)continue;
    float WaterZ;FVector Current;
    if(Marine && Marine->SampleWater(G.ImpactPoint,WaterZ,Current) && WaterZ>G.ImpactPoint.Z-15)continue;   // a wet ledge, not the quay
    const float Rise=G.ImpactPoint.Z-H.Z;
    if(Rise<-20 || Rise>Run*.5f)break;                                   // steeper than ~27 deg: not this way
    // nothing in the way along the run (the wing tip, a bollard, a lamp post)
    FHitResult Block;const FVector Lift(0,0,60);
    if(GetWorld()->SweepSingleByChannel(Block,H+Lift,G.ImpactPoint+Dir*20+Lift,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(25),Q))break;
    Hinge=H;Foot=G.ImpactPoint+Dir*25;return true;
   }
  }
 }
 return false;
}

void ANeonCityLamborghini63::UpdateGangway()
{
 // out while she lies still with nobody at the helm and a quay in reach; in as soon as someone takes the helm
 const bool bStill=!GetController() && !GetMarineInteraction()->GetDriver() && GetVelocity().Size()<40.f;
 if(bGangwayOut && bStill && GetActorTransform().Equals(GangwayBerth,15.f))return;
 FVector Hinge,Foot;
 const bool bOut=bStill && FindQuay(Hinge,Foot);
 bGangwayOut=bOut;GangwayBerth=GetActorTransform();
 Gangway->SetVisibility(bOut);GangwayTeak->SetCollisionEnabled(bOut?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 if(!bOut)return;
 const FVector Run=Foot-Hinge;const float Length=Run.Size();const FRotator Along=Run.Rotation();
 Gangway->SetWorldLocationAndRotation(Hinge,Along);Gangway->SetWorldScale3D(FVector(Length/100.f,1,1));
 // the tread: 64 cm wide, 8 cm deep, its top on the plank's top face
 GangwayTeak->SetBoxExtent(FVector(Length/2,32,4),false);
 GangwayTeak->SetWorldLocationAndRotation(Hinge+Run/2+FRotationMatrix(Along).GetUnitAxis(EAxis::Z)*(-4.f),Along);
 GangwayTeak->SetWorldScale3D(FVector::OneVector);
}

void ANeonCityLamborghini63::UpdateAmbientLights()
{
 // exterior ambient follows the city's night (off in full daylight, full from dusk); the cabins stay lit
 const auto* Lighting=GetWorld()?GetWorld()->GetSubsystem<UWorldLightingSubsystem>():nullptr;
 const float Level=FMath::Clamp(((Lighting?Lighting->GetNightAmount():0.f)-.15f)/.5f,0.f,1.f);
 if(FMath::IsNearlyEqual(Level,ExteriorLevel,.01f))return;
 ExteriorLevel=Level;
 for(int32 I=0;I<AmbientLights.Num();++I)if(AmbientLights[I] && AmbientExterior[I])
 {
  AmbientLights[I]->SetIntensity(AmbientLumens[I]*Level);AmbientLights[I]->SetVisibility(Level>0);
 }
}

float ANeonCityLamborghini63::GetModeSpeedLimit() const
{
 return DriveMode==ENeonCityDriveMode::Docking?DockingKmh:DriveMode==ENeonCityDriveMode::Cruising?CruisingKmh:GetProfile()->MaxSpeed;
}

void ANeonCityLamborghini63::ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue)
{
 // the selected mode governs the throttle: Docking is gentle and capped at 6 kn, Cruising at 40 kn; ahead of the
 // cap the throttle backs off smoothly instead of hitting a wall
 const float Kmh=GetVelocity().Size2D()*.036f,Cap=GetModeSpeedLimit();
 float Throttle=ThrottleValue;
 if(DriveMode==ENeonCityDriveMode::Docking)Throttle*=.35f;
 if(Throttle>0)Throttle*=FMath::Clamp((Cap-Kmh)/FMath::Max(4.f,Cap*.08f),0.f,1.f);
 Super::ApplyDriverInput(Throttle,SteeringValue,BrakeValue);
}

void ANeonCityLamborghini63::CycleDriveMode()
{
 DriveMode=DriveMode==ENeonCityDriveMode::Docking?ENeonCityDriveMode::Cruising:DriveMode==ENeonCityDriveMode::Cruising?ENeonCityDriveMode::Corsa:ENeonCityDriveMode::Docking;
}

void ANeonCityLamborghini63::BindExtraHelmKeys(UEnhancedInputComponent* Input,UInputMappingContext* Context)
{
 if(!ModeAction){ModeAction=NewObject<UInputAction>(this);Context->MapKey(ModeAction,EKeys::M);}
 Input->BindAction(ModeAction,ETriggerEvent::Started,this,&ANeonCityLamborghini63::CycleDriveMode);
}

bool ANeonCityLamborghini63::IsAboard(const ACharacter* Character) const
{
 const auto* Base=Character?Cast<UPrimitiveComponent>(Character->GetMovementBaseObject()):nullptr;
 if(Base && Base->GetOwner()==this)return true;
 // also while airborne a moment over the deck (a step down a tier)
 const FVector L=GetActorTransform().InverseTransformPosition(Character?Character->GetActorLocation():FVector::ZeroVector);
 return Character && Character->GetCharacterMovement()->IsFalling() && FMath::Abs(L.X)<1000 && FMath::Abs(L.Y)<230 && L.Z>60;
}

ENeonCityYachtAction ANeonCityLamborghini63::GetActionFor(const ACharacter* Character) const
{
 if(!Character || GetController() || GetMarineInteraction()->GetDriver())return ENeonCityYachtAction::None;
 const FVector L=GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
 if(IsAboard(Character))
 {
  // on the aft deck and stern tiers alongside the quay F steps ashore; anywhere forward of that it takes the helm
  FTransform Unused;
  if(L.X<AshoreFromX && GetVelocity().Size2D()<60 && FindAshore(Character,Unused))return ENeonCityYachtAction::GoAshore;
  return GetMarineInteraction()->CanEnter(const_cast<ACharacter*>(Character))?ENeonCityYachtAction::TakeHelm:ENeonCityYachtAction::None;
 }
 const auto* Move=Character->GetCharacterMovement();
 const auto* Water=Character->FindComponentByClass<UWaterInteractionComponent>();
 const bool bInWater=Move->IsSwimming() || (Water && Water->GetWaterState()!=EWaterInteractionState::Dry);
 if(bInWater)
 {
  // at the platform, or anywhere along her side (the boarding ladder): F climbs out onto the swim platform
  const FVector Ext=GetProfile()->HullExtent;
  const bool bAtPlatform=FVector::Dist2D(GetActorTransform().TransformPosition(SwimPlatform),Character->GetActorLocation())<PlatformReach;
  const bool bAlongside=FMath::Abs(L.X)<Ext.X+150 && FMath::Abs(L.Y)<Ext.Y+AlongsideReach;
  return bAtPlatform || bAlongside?ENeonCityYachtAction::ClimbAboard:ENeonCityYachtAction::None;
 }
 return GetVelocity().Size2D()<60 && FVector::Dist(GetActorLocation(),Character->GetActorLocation())<GetProfile()->InteractionReachCm+300?ENeonCityYachtAction::Board:ENeonCityYachtAction::None;
}

bool ANeonCityLamborghini63::CanInteract_Implementation(AActor* Interactor) const
{
 const auto* Character=Cast<ACharacter>(Interactor);
 const auto* PC=Character?Cast<APlayerController>(Character->GetController()):nullptr;
 return PC && PC->IsLocalController() && GetActionFor(Character)!=ENeonCityYachtAction::None;
}

void ANeonCityLamborghini63::Interact_Implementation(AActor* Interactor)
{
 auto* Character=Cast<ACharacter>(Interactor);
 switch(GetActionFor(Character))
 {
  case ENeonCityYachtAction::TakeHelm: GetMarineInteraction()->Enter(Character); break;
  case ENeonCityYachtAction::Board: PlaceAboard(Character,CockpitSole); break;
  case ENeonCityYachtAction::ClimbAboard: PlaceAboard(Character,SwimPlatform); break;
  case ENeonCityYachtAction::GoAshore:
  {
   FTransform Out;
   if(FindAshore(Character,Out))
   {
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->SetActorLocationAndRotation(Out.GetLocation(),Out.GetRotation(),false,nullptr,ETeleportType::TeleportPhysics);
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   }
   break;
  }
  default: break;
 }
}

bool ANeonCityLamborghini63::PlaceAboard(ACharacter* Character,const FVector& Local) const
{
 // step aboard onto a deck spot (facing forward), out of the water if swimming
 if(!Character)return false;
 const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const FVector At=GetActorTransform().TransformPosition(Local+FVector(0,0,Half+4));
 auto* Move=Character->GetCharacterMovement();Move->StopMovementImmediately();
 if(!Character->SetActorLocationAndRotation(At,FRotator(0,GetActorRotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics))return false;
 if(auto* PC=Cast<APlayerController>(Character->GetController()))PC->SetControlRotation(FRotator(-8,GetActorRotation().Yaw,0));
 Move->SetMovementMode(MOVE_Falling);
 return true;
}

bool ANeonCityLamborghini63::FindAshore(const ACharacter* Character,FTransform& Out) const
{
 // alongside the quay: the nearest free, walkable spot on the shore beside the hull, aft half first
 if(!Character)return false;
 const auto* Capsule=Character->GetCapsuleComponent();const float Radius=Capsule->GetScaledCapsuleRadius(),Half=Capsule->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(YachtAshore),false,Character);Q.AddIgnoredActor(this);
 const FVector Extent=GetProfile()->HullExtent;
 for(float Along:{-500.f,-250.f,0.f,-750.f,250.f})for(float Side:{1.f,-1.f})for(float Clearance:{70.f,110.f,160.f})
 {
  const FVector P=GetActorTransform().TransformPosition(FVector(Along,Side*(Extent.Y+Radius+Clearance),0));FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,300),P-FVector(0,0,150),ECC_Pawn,Q)
   || Floor.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())continue;
  const FVector Dest=Floor.ImpactPoint+FVector(0,0,Half+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Dest,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))continue;
  Out=FTransform(FRotator(0,(Dest-GetActorLocation()).GetSafeNormal2D().Rotation().Yaw,0),Dest);return true;
 }
 return false;
}

bool ANeonCityLamborghini63::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 // The modelled decks are the walking surfaces: platform, stern tiers, aft deck, cockpit, side decks, foredeck and
 // the cabins below. The canopy roof and mast are not.
 if(!Character || Hit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ())return false;
 if(Hit.GetComponent()==GangwayTeak)
 {
  Surface=FDynamicStandingSurface();Surface.PointVelocity=GetHull()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);return true;
 }
 if(Hit.GetComponent()!=Body)return false;
 const FVector L=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
 if(L.Z>260 || L.Z<0 || FMath::Abs(L.Y)>280 || L.X<-1050 || L.X>980)return false;
 Surface=FDynamicStandingSurface();Surface.PointVelocity=GetHull()->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);return true;
}

FText ANeonCityLamborghini63::GetInteractionName_Implementation() const
{
 const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
 switch(GetActionFor(PC?Cast<ACharacter>(PC->GetPawn()):nullptr))
 {
  case ENeonCityYachtAction::TakeHelm: return NSLOCTEXT("NeonCity","LMBTakeHelm","Take the helm");
  case ENeonCityYachtAction::ClimbAboard: return NSLOCTEXT("NeonCity","LMBClimbAboard","Climb aboard");
  case ENeonCityYachtAction::GoAshore: return NSLOCTEXT("NeonCity","LMBGoAshore","Step ashore");
  default: return NSLOCTEXT("NeonCity","BoardLamborghini63","Board the Lamborghini 63");
 }
}

FText ANeonCityLamborghini63::BuildStatusText(const FMarineDriveTelemetry& T,const FText& State) const
{
 const FText Mode=DriveMode==ENeonCityDriveMode::Docking?NSLOCTEXT("NeonCity","LMBDocking","DOCKING"):DriveMode==ENeonCityDriveMode::Cruising?NSLOCTEXT("NeonCity","LMBCruising","CRUISING")
  :NSLOCTEXT("NeonCity","LMBCorsa","CORSA");
 return FText::Format(NSLOCTEXT("NeonCity","LMBStatus","{0}  |  {1} kn  ({2} km/h)  |  Throttle {3}%  |  {4}\n{5}\nW / S Thrust   A / D Steer   Space Slow   M Drive mode   F Leave   V Helm view   Mouse Look"),
  GetProfile()->DisplayName,FText::AsNumber(FMath::RoundToInt(T.SpeedKmh/1.852f)),FText::AsNumber(FMath::RoundToInt(T.SpeedKmh)),
  FText::AsNumber(FMath::RoundToInt(T.Throttle*100)),Mode,State);
}
