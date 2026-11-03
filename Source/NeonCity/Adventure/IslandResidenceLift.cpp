#include "Adventure/IslandResidenceLift.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace {const TCHAR* Levels[]={TEXT("Garage"),TEXT("Living"),TEXT("Leisure"),TEXT("Suites"),TEXT("Roof")};}
AIslandResidenceLift::AIslandResidenceLift()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ShaftOrigin")));
 Cabin=CreateDefaultSubobject<USceneComponent>(TEXT("MovingCabin"));Cabin->SetupAttachment(RootComponent);
 auto Part=[&](const TCHAR* Name,USceneComponent* Parent)
 {
  auto* P=CreateDefaultSubobject<UStaticMeshComponent>(Name);P->SetupAttachment(Parent);
  P->SetMobility(EComponentMobility::Movable);P->SetCollisionProfileName(TEXT("BlockAll"));
  P->SetCollisionObjectType(ECC_WorldDynamic);P->SetGenerateOverlapEvents(false);P->SetCanEverAffectNavigation(false);
  P->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);return P;
 };
 Floor=Part(TEXT("CabinFloor"),Cabin);Interior=Part(TEXT("CabinInterior"),Cabin);Glazing=Part(TEXT("CabinGlazing"),Cabin);
 ControlPanelFace=Part(TEXT("MovingCabinPanelFace"),Cabin);
 Interior->CanCharacterStepUpOn=ECB_No;Glazing->CanCharacterStepUpOn=ECB_No;
 for(int32 I=0;I<2;++I)CabinDoors.Add(Part(*FString::Printf(TEXT("CabinDoor%d"),I),Cabin));
 CabinPanel=CreateDefaultSubobject<UBoxComponent>(TEXT("InsideCallPanel"));CabinPanel->SetupAttachment(Cabin);
 CabinPanel->SetBoxExtent(FVector(10,18,24));CabinPanel->SetRelativeLocation(FVector(117,103,140));
 CabinPanel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);CabinPanel->SetCollisionResponseToAllChannels(ECR_Ignore);CabinPanel->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 for(int32 Level=0;Level<5;++Level)
 {
  for(int32 I=0;I<2;++I)LandingDoors.Add(Part(*FString::Printf(TEXT("Landing%dDoor%d"),Level,I),RootComponent));
  auto* B=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Floor%dCallPanel"),Level));B->SetupAttachment(RootComponent);
  B->SetBoxExtent(FVector(12,18,25));B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  B->SetCollisionResponseToAllChannels(ECR_Ignore);B->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);CallPanels.Add(B);
 }
 Indicator=CreateDefaultSubobject<UTextRenderComponent>(TEXT("CabinFloorIndicator"));Indicator->SetupAttachment(Cabin);
 Indicator->SetRelativeLocation(FVector(-113,0,175));Indicator->SetWorldSize(11);Indicator->SetTextRenderColor(FColor(225,204,158));Indicator->SetHorizontalAlignment(EHTA_Center);
 CabinLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("CabinCeilingLight"));CabinLight->SetupAttachment(Cabin);
 CabinLight->SetRelativeLocation(FVector(0,0,242));CabinLight->SetMobility(EComponentMobility::Movable);
 CabinLight->SetIntensityUnits(ELightUnits::Lumens);CabinLight->SetIntensity(650);CabinLight->SetAttenuationRadius(520);
 CabinLight->SetLightColor(FLinearColor(1,.82f,.66f));CabinLight->SetCastShadows(false);
}
void AIslandResidenceLift::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 for(int32 I=0;I<CallPanels.Num();++I){
  if(GetLandingCount()>0)CallPanels[I]->SetRelativeLocation(FVector(145,103,GetLandingHeightCm(I)+140));
  CallPanels[I]->SetCollisionEnabled(I<GetLandingCount()?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 }
 PresentDoors();
}
void AIslandResidenceLift::BeginPlay()
{
 Super::BeginPlay();
 if(!LandingHeightsCm.IsEmpty() && !HasValidLandingProfile())
  UE_LOG(LogTemp,Warning,TEXT("Residence lift %s has invalid landing data; operation disabled"),*GetName());
 CurrentFloor=TargetFloor=0;DoorFraction=1;VelocityZ=0;bMoving=bQueued=false;Cabin->SetRelativeLocation(FVector::ZeroVector);PresentDoors();
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {FNeonCityLocalLightPolicy P;P.Intensity=650;P.DayMultiplier=.35f;P.CullDistance=2200;P.ShadowDistance=1000;P.bCastShadows=false;Lighting->RegisterLight(CabinLight,P);}
}
void AIslandResidenceLift::EndPlay(EEndPlayReason::Type Reason)
{if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->UnregisterLight(CabinLight);Super::EndPlay(Reason);}
bool AIslandResidenceLift::HasValidLandingProfile() const
{
 if(LandingHeightsCm.Num()<2 || LandingHeightsCm.Num()>CallPanels.Num() ||
    !FMath::IsFinite(LandingHeightsCm[0]) || FMath::Abs(LandingHeightsCm[0])>.01f)return false;
 for(int32 I=1;I<LandingHeightsCm.Num();++I)
  if(!FMath::IsFinite(LandingHeightsCm[I]) || LandingHeightsCm[I]-LandingHeightsCm[I-1]<260.f)return false;
 return true;
}
int32 AIslandResidenceLift::GetLandingCount() const
{return HasValidLandingProfile()?LandingHeightsCm.Num():LandingHeightsCm.IsEmpty()?CallPanels.Num():0;}
float AIslandResidenceLift::GetLandingHeightCm(int32 Level) const
{
 if(HasValidLandingProfile())return LandingHeightsCm[FMath::Clamp(Level,0,LandingHeightsCm.Num()-1)];
 if(!LandingHeightsCm.IsEmpty())return 0;
 return FMath::Clamp(Level,0,CallPanels.Num()-1)*(FMath::IsFinite(FloorSpacing) && FloorSpacing>=260.f?FloorSpacing:420.f);
}
FText AIslandResidenceLift::LandingName(int32 Level) const
{
 if(HasValidLandingProfile())
 {
  if(LandingNames.IsValidIndex(Level) && !LandingNames[Level].IsEmpty())return LandingNames[Level];
  return FText::Format(NSLOCTEXT("ResidenceLift","ConfiguredFloor","Floor {0}"),FText::AsNumber(Level+1));
 }
 return FText::FromString(Levels[FMath::Clamp(Level,0,UE_ARRAY_COUNT(Levels)-1)]);
}
int32 AIslandResidenceLift::ClosestFloor(const AActor* Visitor) const
{
 if(!Visitor)return CurrentFloor;
 const float Z=GetActorTransform().InverseTransformPosition(Visitor->GetActorLocation()).Z;
 int32 Closest=0;
 for(int32 I=1;I<GetLandingCount();++I)
  if(FMath::Abs(Z-GetLandingHeightCm(I))<FMath::Abs(Z-GetLandingHeightCm(Closest)))Closest=I;
 return Closest;
}
bool AIslandResidenceLift::InCabin(const AActor* Visitor) const
{
 if(!Visitor)return false;const FVector P=Cabin->GetComponentTransform().InverseTransformPosition(Visitor->GetActorLocation());
 return FMath::Abs(P.X)<110 && FMath::Abs(P.Y)<126 && P.Z>35 && P.Z<235;
}
bool AIslandResidenceLift::CanInteract_Implementation(AActor* Visitor) const
{
 const auto* C=Cast<ACharacter>(Visitor);const auto* PC=C?Cast<APlayerController>(C->GetController()):nullptr;
 if(GetLandingCount()<2 || !HasAuthority() || !PC || !PC->IsLocalController() || !C->GetCharacterMovement()->IsMovingOnGround() || bQueued || bMoving || !Floor->GetStaticMesh())return false;
 if(InCabin(Visitor))return true;
 return FVector::DistSquared(Visitor->GetActorLocation(),CallPanels[ClosestFloor(Visitor)]->GetComponentLocation())<FMath::Square(260.f);
}
FText AIslandResidenceLift::GetInteractionName_Implementation() const
{
 const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;const APawn* Visitor=PC?PC->GetPawn():nullptr;
 if(GetLandingCount()<2)return NSLOCTEXT("ResidenceLift","Unavailable","Lift unavailable");
 if(InCabin(Visitor))return FText::FromString(FString::Printf(TEXT("Ride lift to %s"),*LandingName((CurrentFloor+1)%GetLandingCount()).ToString()));
 return FText::FromString(CurrentFloor==ClosestFloor(Visitor)?TEXT("Lift ready — enter cabin, then use its panel"):TEXT("Call lift to this floor"));
}
void AIslandResidenceLift::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 const int32 Requested=InCabin(Visitor)?(CurrentFloor+1)%GetLandingCount():ClosestFloor(Visitor);
 if(Requested==CurrentFloor)return;TargetFloor=Requested;bQueued=true;
}
bool AIslandResidenceLift::DoorOccupied(int32 Level) const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ResidenceLiftDoor),false,this);
 const FVector Centre=GetActorTransform().TransformPosition(FVector(126,0,GetLandingHeightCm(Level)+115));
 return GetWorld()->OverlapAnyTestByObjectType(Centre,GetActorQuat(),FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeBox(FVector(16,78,114)),Q);
}
void AIslandResidenceLift::PresentDoors()
{
 if(GetLandingCount()<2)
 {
  // Keep the last physical landing barriers at their actual positions. A bad
  // new profile must not move them or reactivate a hidden retired fifth stop.
  for(const auto& Panel:CallPanels)Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Indicator->SetText(NSLOCTEXT("ResidenceLift","Unavailable","Lift unavailable"));
  return;
 }
 for(int32 I=0;I<2;++I)
 {
  const float Sign=I?1.f:-1.f;
  CabinDoors[I]->SetRelativeLocation(FVector(120,Sign*(32+65*DoorFraction),0));
  for(int32 L=0;L<CallPanels.Num();++L)
  {
   auto* Door=LandingDoors[L*2+I].Get();const bool Active=L<GetLandingCount();
   Door->SetRelativeLocation(FVector(129,Sign*(32+65*(L==CurrentFloor && !bMoving?DoorFraction:0)),GetLandingHeightCm(L)));
   Door->SetVisibility(Active);Door->SetHiddenInGame(!Active);
   Door->SetCollisionEnabled(Active?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  }
 }
 Indicator->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"),*LandingName(CurrentFloor).ToString(),bMoving?TEXT("In transit"):TEXT("F · Next floor"))));
}
bool AIslandResidenceLift::TryMoveCabin(float DeltaZ)
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ResidenceLiftClearance),false,this);
 for(TActorIterator<ACharacter> It(GetWorld());It;++It)
  if(It->GetMovementBaseObject()==Floor || InCabin(*It))Q.AddIgnoredActor(*It);
 FHitResult Hit;const FVector Centre=Cabin->GetComponentLocation()+GetActorQuat().RotateVector(FVector(0,0,127));
 if(GetWorld()->SweepSingleByChannel(Hit,Centre,Centre+FVector(0,0,DeltaZ),GetActorQuat(),ECC_WorldStatic,
    FCollisionShape::MakeBox(FVector(117,133,124)),Q))
 {
  if(!bReportedObstruction)
  {
   UE_LOG(LogTemp,Warning,TEXT("Residence lift clearance blocked: actor=%s component=%s mesh=%s start=%s impact=%s normal=%s penetration=%.2f"),
    *GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),
    *GetNameSafe(Cast<UStaticMeshComponent>(Hit.GetComponent())?Cast<UStaticMeshComponent>(Hit.GetComponent())->GetStaticMesh():nullptr),
    *Centre.ToString(),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString(),Hit.PenetrationDepth);
   bReportedObstruction=true;
  }
  return false;
 }
 bReportedObstruction=false;
 Cabin->AddRelativeLocation(FVector(0,0,DeltaZ));return true;
}
void AIslandResidenceLift::Tick(float Delta)
{
 Super::Tick(Delta);if(!HasAuthority() || Delta<=0)return;
 if(GetLandingCount()<2){VelocityZ=0;bMoving=bQueued=false;PresentDoors();return;}
 const float DT=FMath::Min(Delta,.05f);
 if(bQueued)
 {
  if(DoorOccupied(CurrentFloor)){DoorFraction=FMath::FInterpConstantTo(DoorFraction,1,DT,1.2f);}
  else DoorFraction=FMath::FInterpConstantTo(DoorFraction,0,DT,1.2f);
  if(DoorFraction<.001f){bQueued=false;bMoving=true;}
 }
 if(bMoving)
 {
  const float Gap=GetLandingHeightCm(TargetFloor)-Cabin->GetRelativeLocation().Z;
  const float Desired=FMath::Sign(Gap)*FMath::Min(TravelSpeed,FMath::Sqrt(2*160.f*FMath::Abs(Gap)));
  VelocityZ=FMath::FInterpConstantTo(VelocityZ,Desired,DT,160);
  const float Step=FMath::Clamp(VelocityZ*DT,-FMath::Abs(Gap),FMath::Abs(Gap));
  if(!TryMoveCabin(Step)){VelocityZ=0;return;}
  if(FMath::Abs(Gap)<.05f)
  {CurrentFloor=TargetFloor;bMoving=false;VelocityZ=0;}
 }
 else if(!bQueued)DoorFraction=FMath::FInterpConstantTo(DoorFraction,1,DT,1.2f);
 PresentDoors();
}
bool AIslandResidenceLift::QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const
{
 if(!Character || Hit.GetComponent()!=Floor || Hit.ImpactNormal.Z<.9f)return false;
 Surface.PointVelocity=GetActorQuat().RotateVector(FVector(0,0,VelocityZ));
 Surface.MaxLandingHorizontalSpeed=650;Surface.MaxLandingVerticalSpeed=650;Surface.ReleaseAcceleration=2200;Surface.MaxDepartureSpeed=600;return true;
}
bool AIslandResidenceLift::FindPersistenceExit(const ACharacter* Visitor,FTransform& Destination) const
{
 if(!Visitor || GetLandingCount()<2 || !AreDoorsOpen())return false;
 const auto* Capsule=Visitor->GetCapsuleComponent();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ResidenceLiftSaveLanding),false,this);Q.AddIgnoredActor(Visitor);
 for(float X:{210.f,245.f})
 {
  const FVector Centre=GetActorTransform().TransformPosition(FVector(X,0,GetLandingHeightCm(CurrentFloor)+Capsule->GetScaledCapsuleHalfHeight()+3));
  const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
  FHitResult Ground;
  if(GetWorld()->OverlapBlockingTestByChannel(Centre,FQuat::Identity,ECC_Pawn,Shape,Q)
   || !GetWorld()->LineTraceSingleByChannel(Ground,Centre,Centre-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+10),ECC_Pawn,Q)
   || Ground.ImpactNormal.Z<Visitor->GetCharacterMovement()->GetWalkableFloorZ())continue;
  Destination=Visitor->GetActorTransform();Destination.SetLocation(Centre);return true;
 }
 return false;
}
