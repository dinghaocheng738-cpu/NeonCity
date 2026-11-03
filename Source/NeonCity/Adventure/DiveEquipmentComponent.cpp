#include "Adventure/DiveEquipmentComponent.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/CoastalWildlife.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/SwimmingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UDiveEquipmentComponent::UDiveEquipmentComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UDiveEquipmentComponent::Equip()
{
 if(bEquipped)return true;
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P)return false;
 const FString Base=TEXT("/Game/NeonCity/Art/OceanExperience/Equipment/R2/");
 const TCHAR* Names[]={TEXT("SM_OceanSpeargun"),TEXT("SM_OceanSpearShaft"),TEXT("SM_OceanSpearBands"),TEXT("SM_OceanSpearReel"),TEXT("SM_OceanDPV"),TEXT("SM_OceanDPVRotor")};
 TArray<UStaticMesh*> Meshes;for(const auto* Name:Names){auto* M=LoadObject<UStaticMesh>(nullptr,*(Base+Name));if(!M)return false;Meshes.Add(M);}
 auto* Skin=LoadObject<USkeletalMesh>(nullptr,*(Base+TEXT("Hands/SK_ExpeditionHands")));
 auto* Graph=LoadClass<UAnimInstance>(nullptr,*(Base+TEXT("Hands/ABP_DiveHands.ABP_DiveHands_C")));
 auto* Thread=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Fishing/Art/SM_NC_FishingLine"));
 if(!Skin || !Graph || !Thread)return false;
 EquipmentRoot=NewObject<USceneComponent>(P,TEXT("DiveEquipmentMount"));P->AddInstanceComponent(EquipmentRoot);EquipmentRoot->SetupAttachment(P->GetFollowCamera());EquipmentRoot->RegisterComponent();
 const auto Part=[&](UStaticMesh* M)
 {
  auto* C=NewObject<UStaticMeshComponent>(P);P->AddInstanceComponent(C);C->SetStaticMesh(M);C->SetMobility(EComponentMobility::Movable);
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetupAttachment(EquipmentRoot);C->RegisterComponent();return C;
 };
 Speargun=Part(Meshes[0]);Shaft=Part(Meshes[1]);Bands=Part(Meshes[2]);Reel=Part(Meshes[3]);DPV=Part(Meshes[4]);Rotor=Part(Meshes[5]);
 Hands=NewObject<USkeletalMeshComponent>(P,TEXT("DrysuitFirstPersonHands"));P->AddInstanceComponent(Hands);Hands->SetSkeletalMesh(Skin);Hands->SetAnimInstanceClass(Graph);
 Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);Hands->SetOnlyOwnerSee(true);Hands->SetCastShadow(false);Hands->SetupAttachment(P->GetFollowCamera());
 Hands->SetRelativeLocationAndRotation(FVector(0,0,-160),FRotator(0,-90,0));Hands->RegisterComponent();
 for(int32 I=0;I<3;++I)
 {
  auto* C=NewObject<USplineMeshComponent>(P);P->AddInstanceComponent(C);C->SetStaticMesh(Thread);C->SetMobility(EComponentMobility::Movable);C->SetForwardAxis(ESplineMeshAxis::X,false);
  C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Fishing/Art/M_NC_FishingLine")));
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetupAttachment(P->GetRootComponent());C->SetAbsolute(true,true,true);C->RegisterComponent();C->SetWorldTransform(FTransform::Identity);C->SetVisibility(false);Line.Add(C);
 }
 MotorAudio=NewObject<UAudioComponent>(P,TEXT("DPVSealedMotor"));P->AddInstanceComponent(MotorAudio);MotorAudio->bAutoActivate=false;MotorAudio->SetupAttachment(DPV);MotorAudio->RegisterComponent();
 MotorAudio->SetSound(LoadObject<USoundBase>(nullptr,*(Base+TEXT("Audio/SW_DPV_Motor"))));MotorAudio->SetLowPassFilterEnabled(true);MotorAudio->SetLowPassFilterFrequency(2100);
 MotorAudio->bAllowSpatialization=false;
 WashSystem=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/FX/NS_DPVWash"));
 bEquipped=true;bDPV=bSpearDeployed=false;bAim=false;bTriggerHeld=false;MotorSpool=0;DeployBlend=0;OriginalFOV=P->GetFollowCamera()->FieldOfView;SpeedFOV=0;SetPhase(ESpearPhase::Ready);
 Notice=TEXT("Hands free. G: speargun | Q: DPV | V: view");UpdatePresentation(0);return true;
}
void UDiveEquipmentComponent::SetPhase(ESpearPhase Value){Phase=Value;PhaseAt=GetWorld()->GetTimeSeconds();}
float UDiveEquipmentComponent::GetActionAge()const{return GetWorld()?GetWorld()->GetTimeSeconds()-PhaseAt:0;}
int32 UDiveEquipmentComponent::GetHandPose() const
{
 if(bDPV)return 4;
 if(Phase==ESpearPhase::Reloading || Phase==ESpearPhase::Retrieving)return 3;
 if(Phase==ESpearPhase::Flying && GetActionAge()<.75f)return 2;
 return bAim?1:0;
}
bool UDiveEquipmentComponent::ToggleDPV()
{
 if(bDPV){StowDPV();return true;}
 if(!bEquipped || Phase!=ESpearPhase::Ready)return false;
 const auto* Kit=GetOwner()->FindComponentByClass<UDivingComponent>();
 if(!Kit || !Kit->IsEquipped() || !Kit->OwnsDPV() || Kit->IsChangingGear())return false;
 const auto* Water=GetOwner()->FindComponentByClass<UWaterInteractionComponent>();
 if(!Water || !Water->IsFullySubmerged()){Notice=TEXT("DPV stowed. Submerge, then Q to equip.");return false;}
 bDPV=true;bSpearDeployed=false;bAim=false;bMotorHeld=false;bTriggerHeld=false;DeployBlend=0;
 Notice=TEXT("DPV ready. Hold LMB on the thumb paddles; steer with your view.");return true;
}
bool UDiveEquipmentComponent::ToggleSpeargun()
{
 if(!bEquipped)return false;
 if(Phase!=ESpearPhase::Ready){Notice=TEXT("Recover the shaft with R before stowing or changing equipment.");return false;}
 if(bSpearDeployed){bSpearDeployed=bAim=bTriggerHeld=false;DeployBlend=0;Notice=TEXT("Speargun stowed. Hands free.");return true;}
 const auto* Kit=GetOwner()->FindComponentByClass<UDivingComponent>();
 const auto* Water=GetOwner()->FindComponentByClass<UWaterInteractionComponent>();
 if(!Kit || !Kit->IsEquipped() || Kit->IsChangingGear() || !Water || !Water->IsFullySubmerged())
 {Notice=TEXT("Submerge, then G to equip the speargun.");return false;}
 StowDPV();bSpearDeployed=true;bAim=bTriggerHeld=false;DeployBlend=0;
 Notice=TEXT("Speargun ready. V first person | RMB aim | LMB fire | R retrieve | G stow.");return true;
}
void UDiveEquipmentComponent::StowDPV()
{
 if(!bDPV)return;
 bDPV=false;bMotorHeld=false;bAim=false;bTriggerHeld=false;MotorSpool=0;DeployBlend=0;
 if(MotorAudio)MotorAudio->Stop();
 Notice=TEXT("DPV stowed. Hands free. G: speargun | Q: DPV.");
}
void UDiveEquipmentComponent::StartShot()
{
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P || !Shaft)return;
 const auto* Kit=P->FindComponentByClass<UDivingComponent>();const auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
 if(!Kit || !Kit->IsFirstPerson() || !Water || !Water->IsFullySubmerged() || !bSpearDeployed || Phase!=ESpearPhase::Ready)return;
 // The visible shaft's tip is the launch origin. The camera ray only guides the
 // pose; all hit decisions are finite sphere sweeps of the traveling spear.
 ShotDirection=Speargun->GetForwardVector();Tip=Speargun->GetComponentTransform().TransformPosition(FVector(94.4,0,3.4));
 FHitResult Block;FCollisionQueryParams Q(SCENE_QUERY_STAT(SpearMuzzleClearance),false,P);
 if(GetWorld()->LineTraceSingleByChannel(Block,Speargun->GetComponentLocation(),Tip,ECC_WorldStatic,Q))
 {Notice=TEXT("The muzzle is obstructed. Move clear before releasing.");return;}
 ShaftVelocity=ShotDirection*1800+P->GetVelocity()*.5f;Traveled=0;++Fired;SetPhase(ESpearPhase::Flying);Notice=TEXT("Spear released. Hold R to recover the line.");
 for(TActorIterator<AMarineSchool> It(GetWorld());It;++It)It->StartleAt(Tip,450);
}
bool UDiveEquipmentComponent::ReserveTarget(const FHitResult& Hit)
{
 School=Cast<AMarineSchool>(Hit.GetActor());Individual=Cast<ACoastalWildlife>(Hit.GetActor());Instance=Hit.Item;
 const bool Allowed=School.IsValid()?School->bCapturable && School->Behaviour!=EMarineBehaviour::Benthic:Individual.IsValid() && Individual->bCapturable && !Individual->bBird;
 if(!Allowed){School.Reset();Individual.Reset();return false;}
 Reservation=FGuid::NewGuid();
 const bool Reserved=School.IsValid()?School->ReserveSpear(Instance,Reservation,Hit.ImpactPoint,ShotDirection,PendingCatch):Individual->ReserveSpear(Reservation,Hit.ImpactPoint,ShotDirection,PendingCatch);
 if(!Reserved){Reservation.Invalidate();School.Reset();Individual.Reset();return false;}
 const FVector Position=School.IsValid()?School->CreaturePosition(Instance):Individual->GetActorLocation();TargetOffset=Position-Tip;
 ++Hits;PendingCatch.CaughtAt=FDateTime::UtcNow();bool Collectible=false;return ReadTargetAttachment(Collectible);
}
bool UDiveEquipmentComponent::ReadTargetAttachment(bool& Collectible)
{
 FTransform Pose;const bool Valid=School.IsValid()?School->GetSpearAttachment(Instance,Reservation,Pose,Collectible):Individual.IsValid() && Individual->GetSpearAttachment(Reservation,Pose,Collectible);
 if(Valid){Tip=Pose.GetLocation();ShotDirection=Pose.GetRotation().GetForwardVector();}return Valid;
}
bool UDiveEquipmentComponent::MoveTarget(FVector Position,bool Landed)
{
 return School.IsValid()?School->MoveHook(Instance,Reservation,Position,true,Landed):Individual.IsValid() && Individual->MoveHook(Reservation,Position,true,Landed);
}
void UDiveEquipmentComponent::ReleaseTarget()
{
 if(Reservation.IsValid())
 {
  if(School.IsValid())School->FinishHook(Instance,Reservation,nullptr,FNeonCityCaughtFish(),false);
  else if(Individual.IsValid())Individual->FinishHook(Reservation,nullptr,FNeonCityCaughtFish(),false);
 }
 School.Reset();Individual.Reset();Reservation.Invalidate();PendingCatch=FNeonCityCaughtFish();Instance=INDEX_NONE;
}
void UDiveEquipmentComponent::StepShaft(float Delta,bool Recover)
{
 if(Phase==ESpearPhase::Ready)return;
 if(Phase==ESpearPhase::Reloading)
 {
  if(GetActionAge()>=2.2f){SetPhase(ESpearPhase::Ready);Notice=TEXT("Shaft seated and bands reset. RMB aim | LMB fire.");}return;
 }
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P)return;
 const FVector RecoveryPoint=Speargun->GetComponentTransform().TransformPosition(FVector(35,0,3.4));
 bool Collectible=true;
 if(Reservation.IsValid() && !ReadTargetAttachment(Collectible)){ReleaseTarget();Notice=TEXT("The line is free. Hold R to recover the shaft.");}
 if(Reservation.IsValid() && !Collectible)Notice=TEXT("Contact. Wait for the fish to settle; keep the line clear.");
 if((Phase==ESpearPhase::Tethered && Recover && Collectible) || (Phase==ESpearPhase::Flying && Recover && GetActionAge()>.3f))
 {SetPhase(ESpearPhase::Retrieving);Notice=TEXT("Retrieving. Hold R to draw in the line; release to pause.");}
 FCollisionQueryParams Q(SCENE_QUERY_STAT(PhysicalSpear),false,P);
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(Delta*120),1,12);const float Step=FMath::Min(Delta,.1f)/Steps;
 for(int32 I=0;I<Steps;++I)
 {
  if(Phase==ESpearPhase::Flying)
  {
   ShaftVelocity+=(FVector(0,0,-38)-ShaftVelocity*(.18f+.00055f*ShaftVelocity.Size()))*Step;
   const FVector Next=Tip+ShaftVelocity*Step;FHitResult Hit;
   if(GetWorld()->SweepSingleByChannel(Hit,Tip,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(1.2f),Q))
   {
    Tip=Hit.Location;const bool Animal=ReserveTarget(Hit);ShaftVelocity*=.08f;SetPhase(ESpearPhase::Tethered);
    Notice=Animal?TEXT("Contact. The fish is losing control. Keep the line clear."):TEXT("Spear stopped. Hold R to recover it.");
    for(TActorIterator<AMarineSchool> It(GetWorld());It;++It)It->StartleAt(Tip,550);
    for(TActorIterator<ACoastalWildlife> It(GetWorld());It;++It)It->StartleAt(Tip,550);
   }
   else {Traveled+=FVector::Distance(Tip,Next);Tip=Next;ShotDirection=ShaftVelocity.GetSafeNormal();}
   if(Traveled>1000 || ShaftVelocity.Size()<95){SetPhase(ESpearPhase::Tethered);Notice=TEXT("Line extended. Hold R to recover the shaft.");}
  }
  else if(Phase==ESpearPhase::Retrieving && Recover)
  {
   const FVector To=RecoveryPoint-Tip;const FVector Next=Tip+To.GetClampedToMaxSize(155*Step);FHitResult Block;
   if(GetWorld()->LineTraceSingleByChannel(Block,Tip,Next,ECC_WorldStatic,Q)){Notice=TEXT("The line is caught on the seabed. Move around the obstruction.");break;}
   if(Reservation.IsValid())
   {
    const bool Pulled=School.IsValid()?School->PullSpear(Instance,Reservation,Next-Tip):Individual.IsValid() && Individual->PullSpear(Reservation,Next-Tip);
    if(!Pulled){ReleaseTarget();Notice=TEXT("The fish is no longer attached. Recover the shaft.");break;}
    ReadTargetAttachment(Collectible);
   }
   else {Tip=Next;ShotDirection=-To.GetSafeNormal();}
   ReelAngle=FMath::Fmod(ReelAngle+Step*420,360);
   if(To.Size()<50)
   {
    if(Reservation.IsValid())
    {
     auto* Inventory=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();MoveTarget(Tip+TargetOffset,true);
     const bool Kept=School.IsValid()?School->FinishHook(Instance,Reservation,Inventory,PendingCatch,true):Individual.IsValid() && Individual->FinishHook(Reservation,Inventory,PendingCatch,true);
     if(Kept)
     {
      ++Retrieved;const auto* Species=Inventory->FindSpecies(PendingCatch.SpeciesID);
      Notice=FString::Printf(TEXT("%s  |  %.0f cm  |  %.2f kg  |  %s  |  Collection updated"),Species?*Species->Name.ToString():*PendingCatch.SpeciesID.ToString(),PendingCatch.SizeCm,PendingCatch.WeightKg,Species?*UEnum::GetDisplayValueAsText(Species->Rarity).ToString():TEXT("Unknown"));
      School.Reset();Individual.Reset();Reservation.Invalidate();
     }
     else {ReleaseTarget();Notice=TEXT("No pouch space. The fish remains in the water.");}
    }
    SetPhase(ESpearPhase::Reloading);break;
   }
  }
 }
 if(Reservation.IsValid() && Phase!=ESpearPhase::Reloading)
 {
  // Shaft penetration is stored in the body's local frame and follows its
  // actual roll/drift, including while the player is not holding retrieve.
  if(!ReadTargetAttachment(Collectible)){ReleaseTarget();Notice=TEXT("The line is free. Recover the shaft.");}
  else if(Collectible && Phase==ESpearPhase::Tethered)Notice=TEXT("Fish settled. Hold R to retrieve and collect.");
 }
 if(FVector::Distance(Tip,RecoveryPoint)>1450 || GetActionAge()>45)
 {ReleaseTarget();SetPhase(ESpearPhase::Retrieving);Notice=TEXT("Line limit reached. Return toward the shaft and hold R.");}
}
void UDiveEquipmentComponent::Operate(bool Aim,bool Trigger,float Delta,bool ControlsAvailable)
{
 if(!bEquipped || !FMath::IsFinite(Delta) || Delta<=0)return;
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* Kit=P?P->FindComponentByClass<UDivingComponent>():nullptr;
 const auto* Water=P?P->FindComponentByClass<UWaterInteractionComponent>():nullptr;
 const bool Under=Water && Water->IsFullySubmerged();const bool FP=Kit && Kit->IsFirstPerson();
 if(!Kit || !Kit->IsEquipped()){Stow();return;}
 if(Water && Water->GetWaterState()==EWaterInteractionState::Dry)
 {
  StowDPV();
  if(bSpearDeployed){ReleaseTarget();SetPhase(ESpearPhase::Ready);bSpearDeployed=bAim=bTriggerHeld=false;Notice=TEXT("Equipment stowed on shore. Hands free.");}
 }
 Trigger=Trigger && ControlsAvailable;
 const bool Pressed=Trigger && !bTriggerHeld;bTriggerHeld=Trigger;bAim=Aim && ControlsAvailable && Under && FP && bSpearDeployed;
 bMotorHeld=bDPV && Trigger && Under;
 if(bDPV)Notice=FString::Printf(TEXT("DPV %s  |  %.1f m/s\nHold LMB: build thrust   Release: coast   Mouse: steer   Q: stow"),bMotorHeld?TEXT("THRUST"):TEXT("COAST"),P->GetVelocity().Size()*.01f);
 MotorSpool=FMath::FInterpConstantTo(MotorSpool,bMotorHeld?1.f:0.f,Delta,bMotorHeld?.72f:1.2f);
 if(Pressed && bAim && Phase==ESpearPhase::Ready && DeployBlend>.95f)StartShot();
 const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 StepShaft(Delta,Under && FP && ControlsAvailable && PC && PC->IsInputKeyDown(EKeys::R));UpdatePresentation(FMath::Min(Delta,.1f));
}
void UDiveEquipmentComponent::ApplyPropulsion(UCharacterMovementComponent* M,float Delta)
{
 if(!bEquipped || !M)return;
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());const auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
 const bool Running=bDPV && bMotorHeld && Water && Water->IsFullySubmerged();
 if(bDPV && Running)if(auto* Swim=P->FindComponentByClass<USwimmingComponent>();Swim && Swim->IsSeafloorStanding())Swim->PushOffSeafloor();
 if(!bDPV || !M->IsSwimming())return;
 M->MaxSwimSpeed=450;M->MaxAcceleration=110;M->BrakingDecelerationSwimming=0;
 // Motor thrust builds over 1.4 s. Quadratic drag limits speed and preserves
 // coasting when the dead-man paddles are released. CMC owns collision/motion.
 const FVector Direction=P->GetFollowCamera()->GetForwardVector();
 const FVector Resistance=M->Velocity*(.28f+.0013f*M->Velocity.Size());
 M->AddForce((Direction*(MotorSpool*550)-Resistance)*M->Mass);
 if(MotorSpool>.08f)
 {
  const FRotator Heading(0,Direction.Rotation().Yaw,0);
  P->SetActorRotation(FMath::RInterpTo(P->GetActorRotation(),Heading,Delta,2.f));
 }
}
void UDiveEquipmentComponent::UpdatePresentation(float Delta)
{
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());const auto* Kit=P?P->FindComponentByClass<UDivingComponent>():nullptr;if(!P || !Kit || !EquipmentRoot)return;
 const bool FP=Kit->IsFirstPerson();const auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();const bool Visible=Water && Water->IsFullySubmerged();
 SpeedFOV=FMath::FInterpTo(SpeedFOV,bDPV && Visible?FMath::Clamp(P->GetVelocity().Size()/450.f,0.f,1.f)*5.f:0.f,Delta,2.5f);
 P->GetFollowCamera()->SetFieldOfView(OriginalFOV+SpeedFOV);
 Hands->SetVisibility(FP && Visible && (bDPV || bSpearDeployed));Hands->SetComponentTickEnabled(FP && Visible && (bDPV || bSpearDeployed));
 AimBlend=FMath::FInterpTo(AimBlend,bAim?1.f:0.f,Delta,6.f);DeployBlend=FMath::FInterpConstantTo(DeployBlend,1.f,Delta,2.8f);
 if(FP)
 {
  EquipmentRoot->AttachToComponent(P->GetFollowCamera(),FAttachmentTransformRules::KeepRelativeTransform);
  EquipmentRoot->SetRelativeLocationAndRotation(FVector::ZeroVector,FRotator::ZeroRotator);
 }
 else
 {
  EquipmentRoot->AttachToComponent(P->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
  const FVector HandCentre=(P->GetMesh()->GetSocketLocation(TEXT("hand_l"))+P->GetMesh()->GetSocketLocation(TEXT("hand_r")))*.5;
  // Orbiting the third-person camera must not rotate a held device sideways
  // through the diver's wrists. Powered steering already turns the body.
  const FRotator Heading=P->GetActorRotation();
  // The DPV centre is twelve centimetres ahead of its wrist contacts. The old
  // generic tool pivot placed its propulsor inside the grounded diver's pelvis.
  const FVector WristMid=bDPV?FVector(48,0,-14.6):FVector(45.5,11.5,-24.75);
  EquipmentRoot->SetWorldLocationAndRotation(HandCentre-Heading.RotateVector(WristMid),Heading);
 }
 const float Recoil=Phase==ESpearPhase::Flying?FMath::Exp(-GetActionAge()*14)*4:0;
 const float Reload=Phase==ESpearPhase::Reloading?FMath::Sin(PI*FMath::Clamp(GetActionAge()/2.2f,0.f,1.f)):0;
 const FVector GunAt(72-(FP?AimBlend*3+Recoil:0),14,-18+(FP?AimBlend*7-Reload*6-(1-DeployBlend)*35:0));
 // Converge the visible shaft on the sight ray. This trace only poses the
 // equipment; the moving spear's finite sweeps remain the sole hit authority.
 // Rotate around the trigger grip so the right hand does not slide off it.
 const FVector GripLocal(-33.5,0,-8),GripAt=GunAt+GripLocal;
 FQuat SightRotation=FQuat::Identity;
 if(FP && !bDPV)
 {
  const FTransform Eye=P->GetFollowCamera()->GetComponentTransform();
  const FVector EyeAt=Eye.GetLocation(),SightEnd=EyeAt+Eye.GetRotation().GetForwardVector()*1000;
  FHitResult Sight;FCollisionQueryParams Query(SCENE_QUERY_STAT(SpearSightPose),false,P);
  const bool Blocked=GetWorld()->LineTraceSingleByChannel(Sight,EyeAt,SightEnd,ECC_Visibility,Query);
  const float Distance=Blocked?FMath::Clamp(Sight.Distance,220.f,1000.f):750.f;
  const FVector Target(Distance,0,0);
  for(int32 I=0;I<3;++I)
  {
   const FVector ShaftAt=GripAt+SightRotation.RotateVector(FVector(0,0,3.4)-GripLocal);
   SightRotation=(Target-ShaftAt).Rotation().Quaternion();
  }
 }
 const FVector AlignedGunAt=GripAt-SightRotation.RotateVector(GripLocal);
 Speargun->SetRelativeLocationAndRotation(AlignedGunAt,SightRotation);Bands->SetRelativeTransform(Speargun->GetRelativeTransform());
 Reel->SetRelativeLocation(AlignedGunAt+SightRotation.RotateVector(FVector(-12,0,-8.5)));
 Reel->SetRelativeRotation(SightRotation*FRotator(ReelAngle,0,0).Quaternion());
 // The arm clips and gun share this small aiming correction, keeping both
 // support and trigger contacts attached during camera-relative convergence.
 Hands->SetRelativeLocationAndRotation(GripAt+SightRotation.RotateVector(FVector(0,0,-160)-GripAt),
  SightRotation*FRotator(0,-90,0).Quaternion());
 const FVector DeviceAt(60,0,-27.5-(FP?(1-DeployBlend)*35:0));DPV->SetRelativeLocation(DeviceAt);Rotor->SetRelativeLocation(DeviceAt+FVector(-33.8,0,0));
 RotorAngle=FMath::Fmod(RotorAngle+MotorSpool*Delta*5400,360);Rotor->SetRelativeRotation(FRotator(0,0,RotorAngle));
 for(auto* C:{Speargun.Get(),Bands.Get(),Reel.Get()})C->SetVisibility(Visible && bSpearDeployed);
 DPV->SetVisibility(Visible && bDPV);Rotor->SetVisibility(Visible && bDPV);
 // After release the elastic assembly relaxes toward the muzzle, visibly
 // distinct from the loaded span. Reload gradually tensions the wishbone.
 const float Loaded=Phase==ESpearPhase::Ready?1.f:Phase==ESpearPhase::Reloading?FMath::SmoothStep(.7f,2.f,GetActionAge()):0.f;
 Bands->SetRelativeScale3D(FVector(FMath::Lerp(.32f,1.f,Loaded),1,1));Bands->AddLocalOffset(FVector((1-Loaded)*30,0,-(1-Loaded)*3));
 const bool Flying=Phase==ESpearPhase::Flying || Phase==ESpearPhase::Tethered || Phase==ESpearPhase::Retrieving;
 if(Flying)Shaft->SetWorldLocationAndRotation(Tip-ShotDirection.Rotation().RotateVector(FVector(94.4,0,3.4)),ShotDirection.Rotation());
 else Shaft->SetRelativeTransform(Speargun->GetRelativeTransform());
 Shaft->SetVisibility(Flying || (Visible && bSpearDeployed));
 const FVector Start=Reel->GetComponentLocation(),End=Tip-ShotDirection*95;
 for(int32 I=0;I<Line.Num();++I)
 {
  Line[I]->SetVisibility(Flying);if(!Flying)continue;
  const auto Point=[&](float T){return FMath::Lerp(Start,End,T)-FVector(0,0,FMath::Sin(T*PI)*(Phase==ESpearPhase::Retrieving?8:24));};
  const FVector A=Point(I/3.f),B=Point((I+1)/3.f),Tangent=(End-Start)/3.f;Line[I]->SetStartAndEnd(A,Tangent,B,Tangent,true);
 }
 if(MotorAudio)
 {
  MotorAudio->SetVolumeMultiplier(.5f*MotorSpool);MotorAudio->SetPitchMultiplier(.60f+MotorSpool*.7f);
  if(MotorSpool>.02f && !MotorAudio->IsPlaying())MotorAudio->Play();else if(MotorSpool<.005f)MotorAudio->Stop();
 }
 Wash.RemoveAllSwap([](const auto& N){return !IsValid(N) || !N->IsActive();});WashClock+=Delta;
 if(bDPV && Visible && MotorSpool>.12f && WashSystem && WashClock>.1f && Wash.Num()<12)
 {
  WashClock=0;const FVector At=DPV->GetComponentTransform().TransformPosition(FVector(-40,0,0));
  if(auto* N=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),WashSystem,At,FRotator::ZeroRotator,FVector::OneVector,true,false))
  {
   N->SetVariableVec3(TEXT("User.ReleaseDirection"),-DPV->GetForwardVector());N->SetVariableFloat(TEXT("User.ReleaseSpeed"),90+MotorSpool*180);
   N->SetVariableFloat(TEXT("User.SurfaceZ"),Water->GetSample().SurfacePoint.Z);N->Activate(true);Wash.Add(N);
  }
 }
}
void UDiveEquipmentComponent::Stow()
{
 if(bEquipped)if(auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner()))P->GetFollowCamera()->SetFieldOfView(OriginalFOV);
 ReleaseTarget();bEquipped=bDPV=bSpearDeployed=bAim=bMotorHeld=bTriggerHeld=false;MotorSpool=0;
 for(auto* C:{Speargun.Get(),Shaft.Get(),Bands.Get(),Reel.Get(),DPV.Get(),Rotor.Get()})if(C)C->DestroyComponent();
 for(const auto& C:Line)if(C)C->DestroyComponent();Line.Reset();if(Hands)Hands->DestroyComponent();if(MotorAudio)MotorAudio->DestroyComponent();if(EquipmentRoot)EquipmentRoot->DestroyComponent();
 for(const auto& N:Wash)if(IsValid(N))N->DestroyComponent();Wash.Reset();WashSystem=nullptr;
 Speargun=Shaft=Bands=Reel=DPV=Rotor=nullptr;Hands=nullptr;MotorAudio=nullptr;EquipmentRoot=nullptr;Phase=ESpearPhase::Ready;
}
void UDiveEquipmentComponent::EndPlay(EEndPlayReason::Type Reason){Stow();Super::EndPlay(Reason);}
void UDiveEquipmentAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);const auto* P=TryGetPawnOwner();const auto* Equipment=P?P->FindComponentByClass<UDiveEquipmentComponent>():nullptr;
 HandPose=Equipment?Equipment->GetHandPose():0;ActionTime=Equipment?Equipment->GetActionAge():0;
 if(Equipment && Equipment->GetSpearPhase()==ESpearPhase::Retrieving)ActionTime=FMath::Fmod(ActionTime,2.2f);
}
