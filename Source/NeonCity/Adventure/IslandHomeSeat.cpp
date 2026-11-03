#include "Adventure/IslandHomeSeat.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Property/PropertySubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
AIslandHomeSeat::AIslandHomeSeat()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.05f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SeatAnchor")));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("SeatInteraction"));Interaction->SetupAttachment(RootComponent);Interaction->SetRelativeLocation(FVector(0,0,85));Interaction->SetBoxExtent(FVector(45,45,85));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Occupant=CreateDefaultSubobject<UTransportOccupantComponent>(TEXT("VisibleSeatedPerson"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Sit(TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Sitting_Idle_Loop.AN_NC205C_Sitting_Idle_Loop"));
 Occupant->SeatedAnimation=Sit.Object;Occupant->SeatedAnimClass=nullptr;
}
bool AIslandHomeSeat::CanInteract_Implementation(AActor* Visitor)const
{
 auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);
 const FVector Local=P?GetActorTransform().InverseTransformPosition(P->GetActorLocation()):FVector::ZeroVector;
 // Bed entry starts standing on its clear left side. Do not start that
 // authored movement from the opposite side, headboard or across the room.
 const bool Bedside=!bSleepBed || (Local.Y>=115 && Local.Y<=145 && FMath::Abs(Local.X)<=45);
 return P && P->IsLocallyControlled() && P->HasAuthority() && (!User.IsValid() || User.Get()==P)
  && GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID)
  && (User.Get()==P || (Bedside && P->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist(P->GetActorLocation(),GetActorLocation()+FVector(0,0,85))<240));
}
FText AIslandHomeSeat::GetInteractionName_Implementation()const
{
 if(bSleepBed)return FText::FromString(IsOccupied()?FString::Printf(TEXT("Sleeping %.1f / 6 hours | F or movement to get up | F6 safe save"),SleptHours):TEXT("F Sleep for 6 hours — clear left bedside"));
 return FText::FromString(IsOccupied()?TEXT("F Stand up | W/A/S/D or Space also leave the seat"):FString::Printf(TEXT("F Sit and rest — %s"),*SeatName.ToString()));
}
void AIslandHomeSeat::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 if(IsOccupied()){RequestStand();return;}auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);auto* PC=Cast<APlayerController>(P->GetController());
 if(const auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
 {if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(TEXT("Stow your weapon before resting.")),5);return;}
 const auto* Dive=P->FindComponentByClass<UDivingComponent>();const auto* Wing=P->FindComponentByClass<UWingsuitComponent>();
 if((Dive && (Dive->IsEquipped() || Dive->IsChangingGear())) || (Wing && Wing->IsEquipped()))
 {if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(TEXT("Remove bulky dive or flight equipment before sitting. Your owned gear is retained.")),5);return;}
 if(bSleepBed)
 {
  for(auto* Clip:{BedEnter.Get(),BedSleep.Get(),BedExit.Get()})if(!Clip || !Clip->GetSkeleton() || !Clip->GetSkeleton()->IsCompatibleMesh(P->GetMesh()->GetSkeletalMeshAsset()))return;
  Occupant->SeatedAnimation=BedEnter;
 }
 Occupant->SeatHipLocation=SeatHipLocal;Occupant->SeatYaw=SeatBodyYaw;if(!Occupant->ShowDriver(P))return;
 if(bSleepBed)if(auto* Body=Occupant->GetDriverMesh()){BedBodyBaseLocation=Body->GetRelativeLocation();BedBodyBaseRotation=Body->GetRelativeRotation().Quaternion();}
 BedStage=0;BedStageSeconds=0;SleptHours=0;bReverseEntry=false;
 if(bSleepBed){BedStage=1;PlayBedMotion(BedEnter);}
 Entry=P->GetActorTransform();bHiddenBefore=P->IsHidden();bCollisionBefore=P->GetActorEnableCollision();User=P;
 auto* Capsule=P->GetCapsuleComponent();CapsuleRadiusBefore=Capsule->GetUnscaledCapsuleRadius();CapsuleHalfHeightBefore=Capsule->GetUnscaledCapsuleHalfHeight();CapsuleCollisionBefore=Capsule->GetCollisionEnabled();
 P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->DisableMovement();Capsule->SetCapsuleHalfHeight(60);Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);P->SetActorHiddenInGame(true);
 P->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepWorldTransform);P->SetActorRelativeTransform(FTransform(FRotator::ZeroRotator,SeatHipLocal));
 if((SeatedViewLift>0 || !FMath::IsNearlyZero(SeatedViewYaw) || !FMath::IsNearlyZero(SeatedViewPitch)) && P->GetCameraBoom())
 {SavedViewOffset=P->GetCameraBoom()->TargetOffset;SavedViewRotation=PC->GetControlRotation();bAdjustedSeatView=true;P->GetCameraBoom()->TargetOffset=SavedViewOffset+FVector(0,0,SeatedViewLift);}
 PC->SetControlRotation(GetActorRotation()+FRotator(SeatedViewPitch,SeatedViewYaw,0));
 if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(bSleepBed?TEXT("Lying down. Six hours of sleep recover energy; F or movement gets up. F6/F7 use the clear bedside standing point."):TEXT("Resting. F or movement to stand. F6 saves at the real clear standing position; F7 restores safely on foot.")),6);
}
bool AIslandHomeSeat::PlayBedMotion(UAnimSequence* Motion,bool Loop)
{
 auto* Body=Occupant->GetDriverMesh();if(!Body || !Motion)return false;Body->PlayAnimation(Motion,Loop);Body->SetPlayRate(1);return true;
}
void AIslandHomeSeat::FitRestPose(float Blend)
{
 if(BedRestOffset.IsNearlyZero() && FMath::IsNearlyZero(BedRestPitch))return;
 auto* Body=Occupant->GetDriverMesh();if(!Body)return;
 Blend=FMath::Clamp(Blend,0.f,1.f);
 const FVector Hip=Body->GetSocketTransform(TEXT("pelvis"),RTS_Component).GetLocation();
 const FQuat Rotation=FRotator(BedRestPitch*Blend,0,0).Quaternion()*BedBodyBaseRotation;
 // Rotate about the currently evaluated pelvis, so a fitted recline does not
 // swing the entry anchor around the source animation's distant root.
 Body->SetRelativeLocationAndRotation(BedBodyBaseLocation+BedBodyBaseRotation.RotateVector(Hip)-Rotation.RotateVector(Hip)+BedRestOffset*Blend,Rotation);
}
void AIslandHomeSeat::RequestStand()
{
 if(!bSleepBed){Leave();return;}if(!User.IsValid() || BedStage==3 || !FindPersistenceExit(PendingExit))return;
 if(BedStage==1)
 {
  // Reverse the actual entry position, avoiding a jump to the flat sleep pose.
  auto* Body=Occupant->GetDriverMesh();auto* Anim=Body?Body->GetSingleNodeInstance():nullptr;if(!Anim){Leave();return;}
  BedStageSeconds=Anim->GetCurrentTime();bReverseEntry=true;Anim->SetReverse(true);Anim->SetPlaying(true);
 }
 else{BedStageSeconds=0;bReverseEntry=false;PlayBedMotion(BedExit);}
 BedStage=3;
}
bool AIslandHomeSeat::FindPersistenceExit(FTransform& Transform)const
{
 const auto* P=User.Get();if(!P)return false;const auto* C=P->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(HomeSeatSafeExit),false,this);Q.AddIgnoredActor(P);
 const FVector Candidates[]={Entry.GetLocation(),GetActorTransform().TransformPosition(FVector(145,0,100)),GetActorTransform().TransformPosition(FVector(140,90,100)),GetActorTransform().TransformPosition(FVector(140,-90,100))};
 for(const auto& At:Candidates)
 {
  FHitResult Floor;if(!GetWorld()->LineTraceSingleByChannel(Floor,At+FVector(0,0,130),At-FVector(0,0,220),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.65)continue;
  const float Half=CapsuleHalfHeightBefore*C->GetShapeScale();const FVector Stand=Floor.ImpactPoint+FVector(0,0,Half+3);
  if(GetWorld()->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),Half),Q))continue;
  Transform=FTransform(Entry.Rotator(),Stand);return true;
 }
 return false;
}
void AIslandHomeSeat::Restore(const FTransform& Transform)
{
 if(auto* P=User.Get())
 {if(bAdjustedSeatView){if(P->GetCameraBoom())P->GetCameraBoom()->TargetOffset=SavedViewOffset;if(auto* PC=Cast<APlayerController>(P->GetController()))PC->SetControlRotation(SavedViewRotation);}
 P->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);P->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);P->GetCapsuleComponent()->SetCapsuleSize(CapsuleRadiusBefore,CapsuleHalfHeightBefore);P->GetCapsuleComponent()->SetCollisionEnabled(CapsuleCollisionBefore);P->SetActorHiddenInGame(bHiddenBefore);P->SetActorEnableCollision(bCollisionBefore);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);P->GetCharacterMovement()->StopMovementImmediately();}
 bAdjustedSeatView=false;
 Occupant->HideDriver();User.Reset();BedStage=0;BedStageSeconds=0;
}
bool AIslandHomeSeat::Leave(){FTransform Exit;if(!FindPersistenceExit(Exit))return false;Restore(Exit);return true;}
void AIslandHomeSeat::Tick(float Delta)
{
 Super::Tick(Delta);auto* P=User.Get();if(!P)return;auto* PC=Cast<APlayerController>(P->GetController());
 if(!PC || PC->GetPawn()!=P || P->GetAttachParentActor()!=this){Restore(Entry);return;}
 if(const auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped()){Leave();return;}
 if(P->GetHealthComponent()->IsDead()){Leave();return;}
 for(const auto K:{EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar})if(PC->IsInputKeyDown(K)){RequestStand();if(!User.IsValid())return;break;}
 if(bSleepBed)
 {
  if(BedStage==1)
  {BedStageSeconds+=Delta;FitRestPose(FMath::SmoothStep(.65f,1.f,BedStageSeconds/BedEnter->GetPlayLength()));if(BedStageSeconds>=BedEnter->GetPlayLength()){BedStage=2;BedStageSeconds=0;PlayBedMotion(BedSleep,true);}return;}
  if(BedStage==3)
  {
   BedStageSeconds+=bReverseEntry?-Delta:Delta;
   const float Fraction=bReverseEntry?BedStageSeconds/BedEnter->GetPlayLength():1.f-BedStageSeconds/BedExit->GetPlayLength();
   FitRestPose(FMath::SmoothStep(.65f,1.f,Fraction));
   if((bReverseEntry && BedStageSeconds<=0) || (!bReverseEntry && BedStageSeconds>=BedExit->GetPlayLength())){FTransform Safe;if(FindPersistenceExit(Safe))Restore(Safe);}
   return;
  }
  if(BedStage!=2)return;
  FitRestPose(1);
  const float Hours=FMath::Min(6.f-SleptHours,Delta*.5f);SleptHours+=Hours;
  for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It)It->SetTimeOfDay(It->GetTimeOfDay()+Hours);
  auto* Data=P->GetPlayerDataComponent();if(Data->GetStamina()<100)Data->SetStamina(FMath::Min(100.f,Data->GetStamina()+Hours*10));P->GetHealthComponent()->Heal(Hours*2);
  if(SleptHours>=6)RequestStand();return;
 }
 auto* Data=P->GetPlayerDataComponent();if(Data->GetStamina()<100)Data->SetStamina(FMath::Min(100.f,Data->GetStamina()+Delta*1.2f));P->GetHealthComponent()->Heal(Delta*.2f);
}
void AIslandHomeSeat::EndPlay(EEndPlayReason::Type Reason){if(User.IsValid())Restore(Entry);Super::EndPlay(Reason);}
