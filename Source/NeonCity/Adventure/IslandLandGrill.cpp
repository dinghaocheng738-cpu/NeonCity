#include "Adventure/IslandLandGrill.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"

AIslandLandGrill::AIslandLandGrill()
{
 Kind=ENeonCityFishingStation::Grill;
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0;
 Coals=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrillCoals"));Coals->SetupAttachment(RootComponent);
 Coals->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 const auto Part=[&](const TCHAR* Name)
 {auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);return C;};
 Lid=Part(TEXT("HingedGrillLid"));Lid->SetRelativeLocation(FVector(0,30,90));
 Tongs=Part(TEXT("CookingTongs"));TongJaw=Part(TEXT("CookingTongJaw"));TongJaw->SetupAttachment(Tongs);
 Food=Part(TEXT("ActualReservedFish"));Plate=Part(TEXT("ServingPlate"));Plate->SetRelativeLocation(FVector(68,0,91));
 Steam=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CookingSteam"));Steam->SetupAttachment(RootComponent);
 Steam->SetMobility(EComponentMobility::Movable);Steam->SetCollisionEnabled(ECollisionEnabled::NoCollision);Steam->SetCanEverAffectNavigation(false);
 Steam->NumCustomDataFloats=1;Steam->SetCastShadow(false);
 Heat=CreateDefaultSubobject<UPointLightComponent>(TEXT("EmberGlow"));Heat->SetupAttachment(RootComponent);
 Heat->SetRelativeLocation(FVector(0,0,92));Heat->SetIntensityUnits(ELightUnits::Lumens);
 Heat->SetLightColor(FLinearColor(1,.15f,.015f));Heat->SetIntensity(0);Heat->SetAttenuationRadius(160);Heat->SetCastShadows(false);
 InteractionBounds->SetRelativeLocation(FVector(0,-18,95));
 Sizzle=CreateDefaultSubobject<UAudioComponent>(TEXT("CookingSizzle"));Sizzle->SetupAttachment(RootComponent);
 Sizzle->SetRelativeLocation(FVector(0,0,89));Sizzle->bAutoActivate=false;
 Sizzle->bOverrideAttenuation=true;Sizzle->AttenuationOverrides.bAttenuate=true;Sizzle->AttenuationOverrides.bSpatialize=true;
 Sizzle->AttenuationOverrides.AttenuationShapeExtents=FVector(80);Sizzle->AttenuationOverrides.FalloffDistance=650;
}
void AIslandLandGrill::BeginPlay()
{
 Super::BeginPlay();
 Lid->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillHingedLid.SM_GrillHingedLid")));
 // FBX handedness places the authored front controls at native +Y. The
 // station's established player approach is -Y; orient its fitted body once.
 if(!bIndoorCooktop && Visual->GetStaticMesh() && Visual->GetStaticMesh()->GetFName()==TEXT("SM_LandGrillBody"))
  Visual->SetRelativeRotation(FRotator(0,180,0));
 Tongs->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillTongs.SM_GrillTongs")));
 TongJaw->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillTongJaw.SM_GrillTongJaw")));
 Plate->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_GrillPlate.SM_GrillPlate")));
 Steam->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
 Steam->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/M_GrillSteam.M_GrillSteam")));
 for(int32 I=0;I<12;++I)Steam->AddInstance(FTransform(FVector(0,0,100)));
 Tongs->SetVisibility(false,true);Food->SetVisibility(false);
 if(Coals->GetMaterial(0))CoalMaterial=Coals->CreateDynamicMaterialInstance(0);
 // Original quiet frying noise, with short wet crackles. PCM is prepared once;
 // playback follows the existing fish transaction rather than an interaction press.
 constexpr int32 SampleRate=24000;
 SizzleWave=NewObject<USoundWaveProcedural>(this);SizzleWave->SetSampleRate(SampleRate);
 SizzleWave->NumChannels=1;SizzleWave->Duration=INDEFINITELY_LOOPING_DURATION;
 SizzlePCM.SetNumUninitialized(SampleRate*2);
 FRandomStream Noise(20503);float LowPass=0,Crackle=0;
 for(int32 Sample=0;Sample<SizzlePCM.Num();++Sample)
 {
  const float White=Noise.FRandRange(-1,1);LowPass+=.18f*(White-LowPass);
  if(Noise.FRand()<.0018f)Crackle=Noise.FRandRange(.18f,.65f);
  Crackle*=.991f;
  const float Envelope=.72f+.14f*FMath::Sin(Sample*(2*PI/(SampleRate*.57f)));
  const float Signal=(White-LowPass)*(.07f*Envelope+Crackle*.19f);
  SizzlePCM[Sample]=int16(FMath::Clamp(Signal,-1.f,1.f)*32767);
 }
 Sizzle->SetSound(SizzleWave);
 if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  FNeonCityLocalLightPolicy Policy;Policy.Intensity=110;Policy.DayMultiplier=1;Policy.bCastShadows=false;
  Policy.CullDistance=1800;Policy.bEnabled=false;S->RegisterLight(Heat,Policy);
 }
}
FText AIslandLandGrill::GetInteractionName_Implementation() const
{
 if(bIndoorCooktop)return FText::FromString(TEXT("Prepare your fresh catch on the kitchen hob"));
 return NSLOCTEXT("NeonCity","LandGrillSequence","Prepare a catch on the terrace grill");
}
void AIslandLandGrill::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 Super::Interact_Implementation(Visitor);
 // A saved reservation resumes on the existing grill. Rejoin its native
 // presentation only after the normal station interaction actually succeeds.
 // The inventory and cooking fraction remain authoritative.
 if(auto* Pawn=Cast<APawn>(Visitor))if(auto* PC=Cast<APlayerController>(Pawn->GetController()))
  if(auto* Fishing=PC->FindComponentByClass<UFishingComponent>();Fishing && Fishing->GetActiveStation()==this
   && Stage!=ENeonCityGrillStage::Idle && Stage!=ENeonCityGrillStage::Closing)SetCookVisitor(Visitor);
}
void AIslandLandGrill::Tick(float Delta)
{
 Super::Tick(Delta);
 const auto* PC=GetWorld()->GetFirstPlayerController();
 const auto* Cooking=PC?PC->FindComponentByClass<UFishingComponent>():nullptr;
 RestoreReservation();
 bool LidPoseReady=true;
 // A restored cook may retain the old walking heading. Camera focus does
 // not turn an idle orient-to-movement character. Turn naturally toward the
 // actual firebox before evaluating the finite-length arm IK; retain the
 // ordinary capsule, input ownership and exact standing position.
 if(auto* Person=Cast<ANeonCityPlayerCharacter>(CookVisitor.Get()))
 {
  // Walk into the work position and later step toward the serving shelf.
  // CharacterMovement sweeps the original capsule against the firebox/floor.
  // This input belongs only to the active station, while the fishing context
  // reserves player movement. No player pose or velocity is written.
  if(Stage!=ENeonCityGrillStage::Ready)
  {
   const float Side=Stage==ENeonCityGrillStage::Plating?60.f:
    Stage==ENeonCityGrillStage::CookingSecond?60.f*FMath::SmoothStep(.65f,.95f,GetStageFraction()):0.f;
   const FVector Work=GetActorTransform().TransformPosition(GetWorkPositionLocal(Side));
   const FVector Offset=Work-Person->GetActorLocation();const float Distance=Offset.Size2D();
   if(Distance>2)Person->AddMovementInput(Offset.GetSafeNormal2D(),FMath::Min(80.f,Distance*4.f)/FMath::Max(1.f,Person->GetCharacterMovement()->GetMaxSpeed()),true);
  }
  const FRotator Facing(0,(GetActorLocation()-Person->GetActorLocation()).Rotation().Yaw,0);
  Person->SetActorRotation(FMath::RInterpConstantTo(Person->GetActorRotation(),Facing,Delta,180));
  if(Stage==ENeonCityGrillStage::Opening || Stage==ENeonCityGrillStage::Closing)
   LidPoseReady=FVector::Dist2D(Person->GetActorLocation(),GetActorTransform().TransformPosition(GetWorkPositionLocal()))<8 && FMath::Abs(FMath::FindDeltaAngleDegrees(Person->GetActorRotation().Yaw,Facing.Yaw))<12;
 }
 if(Stage!=ENeonCityGrillStage::Idle && Stage!=ENeonCityGrillStage::Ready)
 {
  // Finish the swept approach and turn before moving the lid away from its
  // handle. A player arriving from behind retains the full opening action.
  if(LidPoseReady)StageSeconds+=FMath::Clamp(Delta,0.f,.1f);
  static constexpr float Durations[]={0,1.15f,.75f,1.65f,5.f,1.6f,5.f,1.65f,0,1.4f};
  const float Fraction=GetStageFraction();
  if(Stage==ENeonCityGrillStage::CookingFirst)CookFraction=FMath::Max(CookFraction,Fraction*.5f);
  if(Stage==ENeonCityGrillStage::CookingSecond)CookFraction=FMath::Max(CookFraction,.5f+Fraction*.5f);
  auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  if(ReservedFishID.IsValid())Inventory->UpdateGrillReservation(ReservedFishID,StationID,Stage,StageSeconds,CookFraction);
  if(StageSeconds>=Durations[uint8(Stage)])
  {
   if(Stage==ENeonCityGrillStage::Plating)
   {
    if(Inventory->CompleteGrillReservation(ReservedFishID,StationID,PlatedMealID)){ReservedFishID.Invalidate();ChangeStage(ENeonCityGrillStage::Ready);}
   }
   else if(Stage==ENeonCityGrillStage::Closing){bLit=false;SetCookVisitor(nullptr);ChangeStage(ENeonCityGrillStage::Idle);}
   else ChangeStage(ENeonCityGrillStage(uint8(Stage)+1));
  }
 }
 PresentSequence(Delta);
 const bool WasCooking=bCooking;
 bCooking=bLit && (Stage==ENeonCityGrillStage::CookingFirst || Stage==ENeonCityGrillStage::Turning || Stage==ENeonCityGrillStage::CookingSecond);
 if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())S->SetLightEnabled(Heat,bLit);
 if(bCooking && !WasCooking)
 {
  Sizzle->Stop();SizzleWave->ResetAudio();
  SizzleWave->QueueAudio(reinterpret_cast<const uint8*>(SizzlePCM.GetData()),SizzlePCM.Num()*sizeof(int16));
  Sizzle->FadeIn(.08f,.55f);
 }
 else if(!bCooking && WasCooking)Sizzle->FadeOut(.18f,0);
 if(bCooking && SizzleWave->GetAvailableAudioByteCount()<12000)
  SizzleWave->QueueAudio(reinterpret_cast<const uint8*>(SizzlePCM.GetData()),SizzlePCM.Num()*sizeof(int16));
 if(CoalMaterial)CoalMaterial->SetScalarParameterValue(TEXT("Heat"),bLit?(bCooking?1.f+.06f*FMath::Sin(GetWorld()->GetTimeSeconds()*6.f):1.f):0.f);
 if(bLit && Stage==ENeonCityGrillStage::Idle)
 {
  IdleTime=(Cooking && Cooking->GetActiveStation()==this)?0:IdleTime+Delta;
  if(IdleTime>90){bLit=false;if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())S->SetLightEnabled(Heat,false);}
 }
}
void AIslandLandGrill::EndPlay(EEndPlayReason::Type Reason)
{
 SetCookVisitor(nullptr);
 if(Sizzle)Sizzle->Stop();
 if(auto* S=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())S->UnregisterLight(Heat);
 Super::EndPlay(Reason);
}
bool AIslandLandGrill::IsCookingAudioPlaying() const { return Sizzle && Sizzle->IsPlaying(); }

float AIslandLandGrill::GetStageFraction() const
{
 static constexpr float Durations[]={1,1.15f,.75f,1.65f,5.f,1.6f,5.f,1.65f,1,1.4f};
 return FMath::Clamp(StageSeconds/Durations[uint8(Stage)],0.f,1.f);
}
void AIslandLandGrill::ChangeStage(ENeonCityGrillStage Next)
{ Stage=Next;StageSeconds=0;if(Stage==ENeonCityGrillStage::Igniting)bLit=true; }
bool AIslandLandGrill::BeginCook(FGuid ID,AActor* Visitor)
{
 if(!IsInReach(Visitor) || Stage!=ENeonCityGrillStage::Idle || (!bIndoorCooktop && !Lid->GetStaticMesh()) || !Tongs->GetStaticMesh()
  || !TongJaw->GetStaticMesh() || !Plate->GetStaticMesh())return false;
 const FVector Access=GetActorTransform().TransformPosition(FVector(0,bIndoorCooktop?95:-95,96));
 if(FVector::Dist2D(Visitor->GetActorLocation(),Access)>75)return false;
 auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 const auto* Fish=Inventory->FindFish(ID);const auto* Species=Fish?Inventory->FindSpecies(Fish->SpeciesID):nullptr;
 if(!Species || !Species->Mesh.LoadSynchronous() || !Inventory->ReserveForGrill(ID,StationID))return false;
 SetCookVisitor(Visitor);ReservedFishID=ID;PlatedMealID.Invalidate();PresentedSpecies=Fish->SpeciesID;FoodSizeCm=Fish->SizeCm;CookFraction=Fish->CookFraction;
 ChangeStage(ENeonCityGrillStage::Opening);IdleTime=0;return true;
}
void AIslandLandGrill::LeaveCooking(AActor* Visitor)
{
 if(CookVisitor.IsValid() && Visitor!=CookVisitor.Get())return;
 SetCookVisitor(nullptr);
 if(ReservedFishID.IsValid())
 {
  // A full pouch leaves the reservation safely on the grill, to be resumed on
  // the next interaction. Never discard an owned fish to free a visual slot.
  if(!GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->CancelGrillReservation(ReservedFishID,StationID))return;
  ReservedFishID.Invalidate();
 }
 if(Stage!=ENeonCityGrillStage::Idle)ChangeStage(ENeonCityGrillStage::Closing);
}
bool AIslandLandGrill::StorePlatedMeal()
{
 if(Stage!=ENeonCityGrillStage::Ready || !PlatedMealID.IsValid())return false;
 // The plate transaction already inserted exactly one meal in the shared food
 // inventory. Store acknowledges that receipt; it must never insert it again.
 PlatedMealID.Invalidate();ChangeStage(ENeonCityGrillStage::Closing);return true;
}
bool AIslandLandGrill::EatPlatedMeal(AActor* Visitor)
{
 if(Stage!=ENeonCityGrillStage::Ready || !PlatedMealID.IsValid() || !IsInReach(Visitor))return false;
 if(!GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->EatMeal(PlatedMealID,Visitor))return false;
 PlatedMealID.Invalidate();ChangeStage(ENeonCityGrillStage::Closing);return true;
}
void AIslandLandGrill::RestoreReservation()
{
 if(Stage!=ENeonCityGrillStage::Idle)return;
 const auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 for(const auto& Fish:Inventory->GetFish())if(Fish.Storage==ENeonCityFishStorage::Grill && Fish.ContainerID==StationID)
 {
  ReservedFishID=Fish.FishID;PresentedSpecies=Fish.SpeciesID;FoodSizeCm=Fish.SizeCm;
  Stage=Fish.GrillStage;StageSeconds=Fish.GrillStageSeconds;CookFraction=Fish.CookFraction;bLit=uint8(Stage)>=uint8(ENeonCityGrillStage::Igniting);
  return;
 }
}
float AIslandLandGrill::GetCookingBodyLeanDegrees() const
{
 // The return-tool shelf lies farther across the fitted indoor counter than
 // the front control. Bend at the torso before the finite arm solve.
 if(bIndoorCooktop && Stage==ENeonCityGrillStage::Closing)return 55.f;
 const bool FrontControl=Stage==ENeonCityGrillStage::Igniting ||
  (bIndoorCooktop && (Stage==ENeonCityGrillStage::Opening || Stage==ENeonCityGrillStage::Plating));
 return FrontControl?40.f:25.f;
}
bool AIslandLandGrill::GetCookingHandTargets(FTransform& Right,FTransform& Left) const
{
 if(!CookVisitor.IsValid() || Stage==ENeonCityGrillStage::Idle)return false;
 Right=RightHand;Left=LeftHand;return true;
}
void AIslandLandGrill::SetCookVisitor(AActor* Visitor)
{
 if(CookVisitor.Get()==Visitor && (Visitor || !CookingPoseMesh.IsValid()))return;
 if(CookingPoseMesh.IsValid()){CookingPoseMesh->VisibilityBasedAnimTickOption=CookingTickBefore;CookingPoseMesh->bEnableUpdateRateOptimizations=CookingUpdateRateBefore;CookingPoseMesh->RemoveTickPrerequisiteActor(this);}
 if(CookingMovement.IsValid())CookingMovement->bOrientRotationToMovement=CookingOrientToMovementBefore;
 CookingMovement.Reset();
 CookingPoseMesh.Reset();CookVisitor=Visitor;
 if(auto* Person=Cast<ANeonCityPlayerCharacter>(Visitor))
 {
  auto* Mesh=Person->GetMesh();CookingPoseMesh=Mesh;CookingTickBefore=Mesh->VisibilityBasedAnimTickOption;CookingUpdateRateBefore=Mesh->bEnableUpdateRateOptimizations;Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Mesh->bEnableUpdateRateOptimizations=false;Mesh->AddTickPrerequisiteActor(this);
  // The station faces the counter while taking a lateral serving step.
  // Walking's automatic velocity heading would otherwise overwrite that pose
  // by 90 degrees after this tick and put the physical shelf out of arm reach.
  CookingMovement=Person->GetCharacterMovement();CookingOrientToMovementBefore=CookingMovement->bOrientRotationToMovement;CookingMovement->bOrientRotationToMovement=false;
 }
}
FText AIslandLandGrill::GetCookingInstructions() const
{
 switch(Stage)
 {
 case ENeonCityGrillStage::Opening:return FText::FromString(bIndoorCooktop?TEXT("Preparing the pan and hob controls"):TEXT("Opening the lid"));
 case ENeonCityGrillStage::Igniting:return FText::FromString(bIndoorCooktop?TEXT("Heating the induction zone"):TEXT("Igniting the burners"));
 case ENeonCityGrillStage::Placing:return FText::FromString(bIndoorCooktop?TEXT("Placing your fish in the pan"):TEXT("Placing your fish on the grate"));
 case ENeonCityGrillStage::CookingFirst:return FText::FromString(TEXT("Cooking the first side"));
 case ENeonCityGrillStage::Turning:return FText::FromString(TEXT("Turning the fish with the tongs"));
 case ENeonCityGrillStage::CookingSecond:return FText::FromString(TEXT("Cooking the second side"));
 case ENeonCityGrillStage::Plating:return FText::FromString(TEXT("Lifting the cooked fish onto its plate"));
 case ENeonCityGrillStage::Ready:return FText::FromString(TEXT("Ready · 1 Store · 2 Eat\nIf you are full, Store keeps the meal."));
 case ENeonCityGrillStage::Closing:return FText::FromString(bIndoorCooktop?TEXT("Returning the tools and switching off the hob"):TEXT("Returning the tongs and closing the lid"));
 default:return FText::FromString(bIndoorCooktop?TEXT("ENTER Prepare selected fish\nStand close to the front of the kitchen hob."):TEXT("ENTER Prepare selected fish\nStand close to the front of the grill."));
 }
}
void AIslandLandGrill::PresentSequence(float Delta)
{
 const float T=GetStageFraction(),Smooth=T*T*(3-2*T);
 const bool Active=Stage!=ENeonCityGrillStage::Idle;
 const float Target=Stage==ENeonCityGrillStage::Opening?Smooth:Stage==ENeonCityGrillStage::Closing?1-Smooth:Active?1.f:0.f;
 LidOpen=FMath::FInterpConstantTo(LidOpen,Target,Delta,3.f);Lid->SetRelativeRotation(FRotator(0,180,-82*LidOpen));
 // Outdoors the chef faces +Y, so their right-hand tool rest is native -X.
 // The indoor chef faces -Y and keeps the original +X rest. Returning to
 // centre must not force the right arm across the opposite serving shelf.
 const FVector Rest(bIndoorCooktop?60:-60,-17,91),Pouch(0,bIndoorCooktop?75:-75,74),Grate(0,0,94),Serving(68,0,94);
 FVector Tip=Rest;float Jaw=0;
 if(Stage==ENeonCityGrillStage::Placing){Tip=FMath::Lerp(Pouch,Grate,Smooth)+FVector(0,0,18*FMath::Sin(T*PI));Jaw=2;}
 else if(Stage==ENeonCityGrillStage::Turning){Tip=Grate+FVector(0,0,12*FMath::Sin(T*PI));Jaw=1;}
 else if(Stage==ENeonCityGrillStage::Plating){Tip=FMath::Lerp(Grate,Serving,Smooth)+FVector(0,0,16*FMath::Sin(T*PI));Jaw=1;}
 else if(Stage==ENeonCityGrillStage::CookingFirst || Stage==ENeonCityGrillStage::CookingSecond)Tip=FVector(0,bIndoorCooktop?23:-23,97);
 else if(Stage==ENeonCityGrillStage::Closing)Tip=FMath::Lerp(Serving,Rest,Smooth);
 // The authored tool is 34 cm long along X. Its clamp follows the ingredient;
 // palm and wrist targets remain at the spring handle, outside the hot grate.
 // Aim the clamp from the actual shoulder toward the ingredient. The authored
 // 31cm handle-to-tip distance stays exact, including the serving shelf.
 FVector GripDirection(0,1,0);
 if(const auto* Person=Cast<ANeonCityPlayerCharacter>(CookVisitor.Get()))
 {
  const FVector Shoulder=GetActorTransform().InverseTransformPosition(Person->GetMesh()->GetBoneLocation(TEXT("upperarm_r")));
  GripDirection=(Tip-Shoulder).GetSafeNormal();
 }
 FRotator ToolRotation=GripDirection.Rotation();ToolRotation.Roll=Stage==ENeonCityGrillStage::Turning?180*Smooth:0;
 const FVector Handle=Tip-ToolRotation.RotateVector(FVector(31,0,0));
 Tongs->SetRelativeLocationAndRotation(Handle,ToolRotation);TongJaw->SetRelativeRotation(FRotator(Jaw,0,0));
 Tongs->SetVisibility(Active && Stage!=ENeonCityGrillStage::Ready && (!bIndoorCooktop || (Stage!=ENeonCityGrillStage::Opening && Stage!=ENeonCityGrillStage::Igniting)),true);
 FVector Right=Handle,Left(0,bIndoorCooktop?37:-37,100);
 // Native measured grip centre: +63.5Y / 12Z cm relative to its hinge.
 // Use the same component transform as the visible lid, including its facing.
 const FVector LidGrip=GetActorTransform().InverseTransformPosition(Lid->GetComponentTransform().TransformPosition(FVector(0,63.5f,12)));
 if(Stage==ENeonCityGrillStage::Opening)Right=bIndoorCooktop?FVector(0,38,90):LidGrip;
 if(Stage==ENeonCityGrillStage::Igniting)Right=bIndoorCooktop?FVector(0,38,90):FVector(0,-38,85);
 if(Stage==ENeonCityGrillStage::Closing)Left=bIndoorCooktop?FVector(0,38,90):LidGrip;
 if(const auto* Person=Cast<ANeonCityPlayerCharacter>(CookVisitor.Get()))
  Left.X=FMath::Clamp(GetActorTransform().InverseTransformPosition(Person->GetActorLocation()).X,-50.,70.);
 RightHand=FTransform(GetActorQuat()*ToolRotation.Quaternion()*FRotator(0,-90,90).Quaternion(),GetActorTransform().TransformPosition(Right));
 LeftHand=FTransform(GetActorQuat()*FRotator(-20,0,90).Quaternion(),GetActorTransform().TransformPosition(Left));
 const auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 const auto* Species=Inventory->FindSpecies(PresentedSpecies);auto* Mesh=Species?Species->Mesh.LoadSynchronous():nullptr;
 if(Mesh && Food->GetStaticMesh()!=Mesh)
 {
  Food->SetStaticMesh(Mesh);FoodMaterials.Reset();
  for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I)
  {
   const FString Key=PresentedSpecies.ToString().Replace(TEXT("."),TEXT("_"));
   const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/M_GrillFish_%s_%d.M_GrillFish_%s_%d"),*Key,I,*Key,I);
   if(auto* CookingMaterial=LoadObject<UMaterialInterface>(nullptr,*Path))Food->SetMaterial(I,CookingMaterial);
   FoodMaterials.Add(Food->CreateDynamicMaterialInstance(I));
  }
  const auto* Data=Mesh->GetRenderData();const float Length=FMath::Max(1.f,float((Data?Data->Bounds:Mesh->GetBounds()).BoxExtent.X*2));
  Food->SetRelativeScale3D(FVector(FoodSizeCm/Length));
 }
 const bool Show=Mesh && (uint8(Stage)>=uint8(ENeonCityGrillStage::Placing) && uint8(Stage)<=uint8(ENeonCityGrillStage::Ready));
 Food->SetVisibility(Show);
 const FVector FoodPosition=Stage==ENeonCityGrillStage::Placing || Stage==ENeonCityGrillStage::Turning || Stage==ENeonCityGrillStage::Plating?Tip:
  Stage==ENeonCityGrillStage::Ready?Serving:Grate;
 Food->SetRelativeLocationAndRotation(FoodPosition,FRotator(0,0,90+(Stage==ENeonCityGrillStage::Turning?180*Smooth:
  uint8(Stage)>uint8(ENeonCityGrillStage::Turning)?180:0)));
 for(const auto& Material:FoodMaterials)if(Material)
 {Material->SetScalarParameterValue(TEXT("CookFraction"),CookFraction);Material->SetVectorParameterValue(TEXT("CookTint"),FMath::Lerp(FLinearColor::White,FLinearColor(.34f,.19f,.08f),CookFraction));}
 const bool Smoking=Stage==ENeonCityGrillStage::CookingFirst || Stage==ENeonCityGrillStage::Turning || Stage==ENeonCityGrillStage::CookingSecond;
 Steam->SetVisibility(Smoking);
 if(Smoking)
 {
  const auto* Camera=GetWorld()->GetFirstPlayerController()?GetWorld()->GetFirstPlayerController()->PlayerCameraManager.Get():nullptr;
  const float Clock=GetWorld()->GetTimeSeconds();
  for(int32 I=0;I<12;++I)
  {
   const float Age=FMath::Fmod(Clock*.32f+I/12.f,1.f);
   const FVector Pos(FMath::Sin(I*2.4f)*14+Age*8,FMath::Cos(I*1.8f)*10+Age*12,97+Age*64);
   const FVector WorldPos=GetActorTransform().TransformPosition(Pos);
   const FVector Facing=Camera?(Camera->GetCameraLocation()-WorldPos).GetSafeNormal():FVector::UpVector;
   const FQuat Rotation=GetActorQuat().Inverse()*FQuat::FindBetweenNormals(FVector::UpVector,Facing);
   const float Scale=.11f+Age*.32f;Steam->UpdateInstanceTransform(I,FTransform(Rotation,Pos,FVector(Scale)),false,I==11);
   Steam->SetCustomDataValue(I,0,.12f*FMath::Sin(Age*PI),I==11);
  }
 }
}
