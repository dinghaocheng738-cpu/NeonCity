#include "Adventure/WingsuitComponent.h"
#include "Adventure/DivingComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

UWingsuitComponent::UWingsuitComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UWingsuitComponent::BeginPlay(){Super::BeginPlay();GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->OnReset.AddDynamic(this,&UWingsuitComponent::Unequip);}
bool UWingsuitComponent::Equip()
{
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(bEquipped)return true;
 if(!PC || !P->GetCharacterMovement()->IsMovingOnGround() || !P->FindComponentByClass<UDamageableCharacterComponent>()->CanAct())return false;
 auto* Suit=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Wingsuit/SK_AdventureWingsuit"));
 auto* Chute=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Wingsuit/SM_AdventureCanopy"));
 if(!Suit || !Chute)return false;
 P->FindComponentByClass<UDivingComponent>()->Unequip();P->GetCombatAdapter()->StowForUtility();
 OriginalSkin=P->GetMesh()->GetSkeletalMeshAsset();OriginalMaterials=P->GetMesh()->OverrideMaterials;OriginalAnimation=P->GetMesh()->GetAnimClass();OriginalTransform=P->GetMesh()->GetRelativeTransform();
 OriginalArmLength=P->GetCameraBoom()->TargetArmLength;OriginalSocketOffset=P->GetCameraBoom()->SocketOffset;OriginalFOV=P->GetFollowCamera()->FieldOfView;
 P->GetMesh()->EmptyOverrideMaterials();P->GetMesh()->SetSkeletalMesh(Suit,false);P->GetMesh()->SetMorphTarget(TEXT("Deploy"),0);
 bOldOrient=P->GetCharacterMovement()->bOrientRotationToMovement;
 Canopy=NewObject<UStaticMeshComponent>(P);P->AddInstanceComponent(Canopy);Canopy->SetStaticMesh(Chute);Canopy->SetupAttachment(P->GetRootComponent());Canopy->SetRelativeLocation(FVector(0,0,-90));Canopy->SetCollisionEnabled(ECollisionEnabled::NoCollision);Canopy->RegisterComponent();Canopy->SetVisibility(false);
 HUD=CreateWidget<UPolishDivingWidget>(PC);HUD->WidgetTree=NewObject<UWidgetTree>(HUD);auto* Canvas=HUD->WidgetTree->ConstructWidget<UCanvasPanel>();HUD->WidgetTree->RootWidget=Canvas;Status=HUD->WidgetTree->ConstructWidget<UTextBlock>();auto Font=Status->GetFont();Font.Size=18;Status->SetFont(Font);auto* Slot=Canvas->AddChildToCanvas(Status);Slot->SetPosition(FVector2D(35,150));Slot->SetAutoSize(true);HUD->AddToViewport(30);HUD->SetVisibility(ESlateVisibility::HitTestInvisible);
 bRestorePending=false;RestoreWait=0;bEquipped=true;bFlying=bCanopy=false;AirTime=JumpLaunchWindow=0;SetComponentTickEnabled(true);return true;
}
void UWingsuitComponent::RestoreEquipped(bool SavedEquipped)
{
 bRestorePending=false;Unequip();RestoreWait=0;
 if(SavedEquipped && !Equip()){bRestorePending=true;SetComponentTickEnabled(true);}
}
void UWingsuitComponent::Unequip()
{
 bRestorePending=false;
 if(!bEquipped)return;bEquipped=bFlying=bCanopy=false;SetComponentTickEnabled(false);
 if(auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner()))
 {
  auto* M=Cast<UNeonCitySurfaceMovementComponent>(P->GetCharacterMovement());if(M->IsIslandGliding())M->SetMovementMode(MOVE_Falling);
  M->bOrientRotationToMovement=bOldOrient;P->GetMesh()->SetRelativeTransform(OriginalTransform);
  P->GetCameraBoom()->TargetArmLength=OriginalArmLength;P->GetCameraBoom()->SocketOffset=OriginalSocketOffset;P->GetFollowCamera()->SetFieldOfView(OriginalFOV);
  P->GetMesh()->EmptyOverrideMaterials();if(OriginalSkin)P->GetMesh()->SetSkeletalMesh(OriginalSkin);P->GetMesh()->SetAnimInstanceClass(OriginalAnimation);
  for(int32 I=0;I<OriginalMaterials.Num();++I)P->GetMesh()->SetMaterial(I,OriginalMaterials[I]);OriginalMaterials.Reset();
 }
 if(Canopy)Canopy->DestroyComponent();Canopy=nullptr;OriginalSkin=nullptr;OriginalAnimation=nullptr;
 if(HUD)HUD->RemoveFromParent();HUD=nullptr;Status=nullptr;
}
void UWingsuitComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(bRestorePending)
 {
  RestoreWait+=Delta;
  if(PC && P->GetCharacterMovement()->IsMovingOnGround() && Equip())return;
  if(RestoreWait>8){bRestorePending=false;SetComponentTickEnabled(false);UE_LOG(LogTemp,Warning,TEXT("Saved wingsuit could not be equipped on safe restored ground; flight was not fabricated."));}
  return;
 }
 if(!PC || !P->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()){Unequip();return;}
 auto* M=Cast<UNeonCitySurfaceMovementComponent>(P->GetCharacterMovement());P->GetCombatAdapter()->StowForUtility();
 if(PC->WasInputKeyJustPressed(EKeys::X) && M->IsMovingOnGround()){Unequip();return;}
 // A walking step or small fall must not unexpectedly deploy flight controls.
 FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(WingsuitGround),false,P);
 const bool HasGround=GetWorld()->LineTraceSingleByChannel(Ground,P->GetActorLocation(),P->GetActorLocation()-FVector(0,0,100000),ECC_WorldStatic,Q);
 const float Height=HasGround?FVector::Dist(P->GetActorLocation(),Ground.ImpactPoint):100000;
 JumpLaunchWindow=FMath::Max(0.f,JumpLaunchWindow-Delta);
 if(!bFlying && PC->WasInputKeyJustPressed(EKeys::SpaceBar))JumpLaunchWindow=1.5f;
 bool ClearDownhillLaunch=false;
 if(!bFlying && JumpLaunchWindow>0 && M->IsFalling() && Height>180 && M->Velocity.Size2D()>150)
 {
  // A deliberate jump over a descending launch edge may inflate before a
  // nine-metre fall. Ordinary jumps over level ground remain ordinary jumps.
  const FVector Ahead=P->GetActorLocation()+PC->GetControlRotation().Vector().GetSafeNormal2D()*1500;
  FHitResult Landing;const bool AheadGround=GetWorld()->LineTraceSingleByChannel(Landing,Ahead+FVector(0,0,300),Ahead-FVector(0,0,60000),ECC_WorldStatic,Q);
  ClearDownhillLaunch=AheadGround && P->GetActorLocation().Z-Landing.ImpactPoint.Z>650;
 }
 if(!bFlying && M->IsFalling() && (Height>900 || ClearDownhillLaunch) && M->Velocity.Z<120)
 {
  bCanopy=false;Canopy->SetVisibility(false);bFlying=M->BeginIslandGlide(PC->GetControlRotation().Vector());M->bOrientRotationToMovement=false;AirTime=0;
  auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Wingsuit/A_WingsuitGlide"));
  if(Clip)P->GetMesh()->PlayAnimation(Clip,true);P->GetMesh()->SetMorphTarget(TEXT("Deploy"),0);
  FRotator View=PC->GetControlRotation();View.Pitch=-18;PC->SetControlRotation(View);
 }
 if(bFlying && M->IsIslandGliding())
 {
  AirTime+=Delta;
  // Pressurized cells fill during the launch transition; the live banked
  // character remains the source of the silhouette throughout the glide.
  if(!bCanopy)P->GetMesh()->SetMorphTarget(TEXT("Deploy"),FMath::SmoothStep(0.f,.48f,AirTime));
  // The reserve canopy is controllable too. It protects an approach, never moves the actor along a route.
  if(!bCanopy && AirTime>1 && ((Height<1600 && M->Velocity.Z<-150) || PC->WasInputKeyJustPressed(EKeys::SpaceBar)))
  {
   bCanopy=true;Canopy->SetVisibility(true);P->GetMesh()->SetMorphTarget(TEXT("Deploy"),0);
   if(auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Wingsuit/A_WingsuitCanopy")))P->GetMesh()->PlayAnimation(Clip,true);
  }
  const float Turn=(PC->IsInputKeyDown(EKeys::D)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::A)?1.f:0.f);
  const float Pitch=(PC->IsInputKeyDown(EKeys::W)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::S)?1.f:0.f);
  M->SetIslandFlightControls(Turn,Pitch,bCanopy);
  FRotator View=PC->GetControlRotation();View.Yaw+=Turn*(bCanopy?45.f:58.f)*Delta;PC->SetControlRotation(View);
  P->GetCameraBoom()->TargetArmLength=FMath::FInterpTo(P->GetCameraBoom()->TargetArmLength,bCanopy?1000.f:550.f,Delta,2.5f);
  P->GetCameraBoom()->SocketOffset=FMath::VInterpTo(P->GetCameraBoom()->SocketOffset,OriginalSocketOffset+FVector(0,0,bCanopy?220:100),Delta,3);
  P->GetFollowCamera()->SetFieldOfView(FMath::FInterpTo(P->GetFollowCamera()->FieldOfView,bCanopy?OriginalFOV:OriginalFOV+8,Delta,2));
  const FRotator Pose=bCanopy?OriginalTransform.Rotator():(FRotator(-75-25*Pitch,0,Turn*18).Quaternion()*OriginalTransform.GetRotation()).Rotator();
  const FQuat Rotation=FMath::RInterpTo(P->GetMesh()->GetRelativeRotation(),Pose,Delta,4).Quaternion();
  // Rotate around the torso, not the skeletal asset's origin at the soles.
  // The capsule and gameplay camera then continue to follow the flyer.
  const FVector TorsoLocal(0,0,95);
  const FVector Pivot=OriginalTransform.TransformPosition(TorsoLocal);
  P->GetMesh()->SetRelativeLocationAndRotation(Pivot-Rotation.RotateVector(OriginalTransform.GetScale3D()*TorsoLocal),Rotation);
 }
 else if(bFlying)
 {
  bFlying=false;Canopy->SetVisibility(false);P->GetMesh()->SetRelativeTransform(OriginalTransform);P->GetMesh()->SetAnimInstanceClass(OriginalAnimation);P->GetMesh()->SetMorphTarget(TEXT("Deploy"),0);M->bOrientRotationToMovement=bOldOrient;
  P->GetCameraBoom()->TargetArmLength=OriginalArmLength;P->GetCameraBoom()->SocketOffset=OriginalSocketOffset;P->GetFollowCamera()->SetFieldOfView(OriginalFOV);
 }
 if(Status)Status->SetText(FText::FromString(FString::Printf(TEXT("%s\nA / D Turn | W Dive | S Flare | Space Reserve canopy\nJump from the launch deck | X Remove on land\nSpeed %.0f km/h | Ground %.0f m"),bFlying?(bCanopy?TEXT("CANOPY APPROACH"):TEXT("WINGSUIT GLIDE")):TEXT("WINGSUIT EQUIPPED"),M->Velocity.Size()*.036f,Height*.01f)));
}
void UWingsuitComponent::EndPlay(EEndPlayReason::Type Reason){if(auto* S=GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>())S->OnReset.RemoveAll(this);Unequip();Super::EndPlay(Reason);}
