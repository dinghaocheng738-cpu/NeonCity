#include "Adventure/IslandHomePet.h"
#include "Adventure/IslandLandResidence.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
AIslandHomePet::AIslandHomePet()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;PrimaryActorTick.TickInterval=0;
 AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;AIControllerClass=AAIController::StaticClass();bUseControllerRotationYaw=false;
 GetCapsuleComponent()->InitCapsuleSize(26,50);GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 GetMesh()->SetRelativeLocation(FVector(0,0,-50));GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 auto* M=GetCharacterMovement();M->bTickBeforeOwner=false;M->MaxWalkSpeed=140;M->MaxAcceleration=450;M->BrakingDecelerationWalking=600;M->bOrientRotationToMovement=true;M->RotationRate=FRotator(0,170,0);M->MaxStepHeight=12;M->bEnablePhysicsInteraction=false;
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("CompanionPrompt"));Prompt->SetupAttachment(RootComponent);Prompt->SetRelativeLocation(FVector(0,0,65));Prompt->SetWorldSize(11);Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetTextRenderColor(FColor(241,227,196));Prompt->SetVisibility(false);
 HomePointsLocal={FVector(400,-900,470),FVector(650,-700,470),FVector(600,-250,470),FVector(300,-50,470)};
}
void AIslandHomePet::BeginPlay()
{
 Super::BeginPlay();HomeTransform=GetActorTransform();bOwned=bStartsOwned;Mode=0;
 for(TActorIterator<AIslandLandResidence> I(GetWorld());I;++I)if(I->Property->GetPropertyID()==PropertyID){ResidenceOrigin=I->GetActorTransform();break;}
 // AddMovementInput is produced every frame before CMC consumes it. A .1s
 // intermittent driver would let ordinary braking cancel the follow input.
 GetCharacterMovement()->AddTickPrerequisiteActor(this);if(GetController())GetController()->SetActorTickEnabled(false);
 GetMesh()->PlayAnimation(IdleAnimation,true);
}
bool AIslandHomePet::InSafeRoom(const FVector& Position)const
{
 const FVector Local=ResidenceOrigin.InverseTransformPosition(Position-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
 return Local.X>=SafeLocalMin.X && Local.X<=SafeLocalMax.X && Local.Y>=SafeLocalMin.Y && Local.Y<=SafeLocalMax.Y && Local.Z>=SafeLocalMin.Z && Local.Z<=SafeLocalMax.Z;
}
bool AIslandHomePet::HasSafeSupport(const FVector& Position,FVector& Supported)const
{
 if(!InSafeRoom(Position))return false;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(HomePetFloor),false,this);
 if(!GetWorld()->LineTraceSingleByChannel(H,Position+FVector(0,0,25),Position-FVector(0,0,110),ECC_Pawn,Q) || H.ImpactNormal.Z<.75)return false;
 Supported=H.ImpactPoint+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
 return InSafeRoom(Supported) && FMath::Abs(Supported.Z-Position.Z)<20;
}
bool AIslandHomePet::ClearBodyStep(const FVector& Direction,float Distance)const
{
 FVector Supported;const FVector At=GetActorLocation()+Direction*Distance;if(!HasSafeSupport(At,Supported))return false;
 const FQuat Yaw=Direction.Rotation().Quaternion();FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(HomePetWholeBody),false,this);
 // Full muzzle-to-tail body guard supplements the narrow CMC capsule. This
 // prevents the visible dog's head passing into a wall before its capsule hits.
 return !GetWorld()->SweepSingleByChannel(H,GetActorLocation(),Supported,Yaw,ECC_Pawn,FCollisionShape::MakeBox(FVector(58,28,39)),Q);
}
bool AIslandHomePet::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* P=Cast<ACharacter>(Visitor);const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 return HasAuthority() && bOwned && PC && PC->IsLocalController() && PC->GetPawn()==P && P->GetWorld()==GetWorld() && P->GetCharacterMovement()->IsMovingOnGround() && !P->GetAttachParentActor() && FVector::DistSquared(P->GetActorLocation(),GetActorLocation())<FMath::Square(230.f) && GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID);
}
FText AIslandHomePet::GetInteractionName_Implementation()const
{return FText::FromString(Mode==0?TEXT("Benny, follow me around the living floor"):Mode==1?TEXT("Benny, stay here"):TEXT("Benny, return to home activity"));}
void AIslandHomePet::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;Mode=(Mode+1)%3;HomeWait=2;GetCharacterMovement()->StopMovementImmediately();
 if(auto* N=Visitor->GetInstigatorController()?Visitor->GetInstigatorController()->FindComponentByClass<UNeonCityNavigationComponent>():nullptr)N->ShowNotice(GetInteractionName_Implementation(),4);
}
void AIslandHomePet::PresentAnimation()
{
 const bool Moving=GetVelocity().Size2D()>8;
 if(Moving!=bPlayingTrot){bPlayingTrot=Moving;GetMesh()->PlayAnimation(Moving?TrotAnimation:IdleAnimation,true);}
 if(auto* A=GetMesh()->GetSingleNodeInstance())A->SetPlayRate(Moving?FMath::Clamp(GetVelocity().Size2D()/180,.35f,1.1f):1);
}
void AIslandHomePet::Tick(float Delta)
{
 Super::Tick(Delta);auto* PC=GetWorld()->GetFirstPlayerController();auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;auto* Focus=Player?Player->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Focused=Focus && Focus->GetFocusedInteractable()==this;Prompt->SetVisibility(Focused);if(Focused){Prompt->SetText(FText::FromString(TEXT("F  ")+GetInteractionName_Implementation().ToString()));FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());}
 if(!bOwned || !GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID) || !GetMesh()->GetSkeletalMeshAsset() || !IdleAnimation || !TrotAnimation){GetCharacterMovement()->StopMovementImmediately();return;}
 PresentAnimation();if(Mode==2)return;
 FVector Target=HomeTransform.GetLocation();
 if(Mode==1 && Player && InSafeRoom(Player->GetActorLocation()) && Player->GetCharacterMovement()->IsMovingOnGround() && !Player->IsHidden())
 {Target=Player->GetActorLocation();if(FVector::Dist2D(Target,GetActorLocation())<185)return;}
 else
 {
  HomeWait=FMath::Max(0.f,HomeWait-Delta);if(HomeWait>0)return;
  if(HomePointsLocal.Num())Target=ResidenceOrigin.TransformPosition(HomePointsLocal[HomePoint%HomePointsLocal.Num()]);
  if(FVector::Dist2D(Target,GetActorLocation())<60){HomePoint=(HomePoint+1)%FMath::Max(1,HomePointsLocal.Num());HomeWait=4+HomePoint*1.3f;return;}
 }
 const FVector Desired=(Target-GetActorLocation()).GetSafeNormal2D();
 for(const float Angle:{0.f,35.f,-35.f,70.f,-70.f})
 {const FVector Direction=Desired.RotateAngleAxis(Angle,FVector::UpVector);if(ClearBodyStep(Direction,55)){AddMovementInput(Direction,1);return;}}
 // Furniture/glass/edge blocking causes an ordinary stop and wait, never a
 // transform write, collision disable or teleport to the requested target.
}
FNeonCityResidencePetSaveData AIslandHomePet::BuildPersistenceSnapshot(bool Defaults)const
{
 FNeonCityResidencePetSaveData D;D.PropertyID=PropertyID;D.PetID=PetID;D.Owned=Defaults?bStartsOwned:bOwned;D.Mode=Defaults?0:Mode;D.Transform=Defaults?HomeTransform:GetActorTransform();return D;
}
void AIslandHomePet::ApplyPersistenceSnapshot(const FNeonCityResidencePetSaveData& D)
{
 if(D.PropertyID!=PropertyID || D.PetID!=PetID)return;bOwned=D.Owned;Mode=FMath::Min<uint8>(D.Mode,2);HomeWait=2;
 FVector Supported;const bool Finite=!D.Transform.GetLocation().ContainsNaN() && !D.Transform.GetRotation().ContainsNaN();
 if(Finite && HasSafeSupport(D.Transform.GetLocation(),Supported))
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(HomePetRestore),false,this);
  if(!GetWorld()->OverlapBlockingTestByChannel(Supported,D.Transform.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(FVector(58,28,39)),Q))
  {SetActorLocationAndRotation(Supported,D.Transform.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);GetCharacterMovement()->StopMovementImmediately();return;}
 }
 // Keep the actual authored safe home when optional/corrupt/obsolete pet pose
 // fails preflight. Nothing here alters the house, money or other save domains.
 GetCharacterMovement()->StopMovementImmediately();Mode=0;
}
