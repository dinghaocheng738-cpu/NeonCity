#include "Adventure/IslandEstateSport.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Property/PropertySubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Fishing/FishingComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UIslandTennisParticipant::UIslandTennisParticipant()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UIslandTennisParticipant::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{Super::TickComponent(Delta,Type,Function);UpdateRacket(Delta);}
bool UIslandTennisParticipant::GetHandTargets(FTransform& Right,FTransform& Left)const
{
 const auto* Body=Cast<ACharacter>(GetOwner());if(!Court || !Court->IsActive() || !Body || !Racket)return false;
 const FVector Ready(30,36,35),Contact=ContactLocal-FVector(36.3,0,21.1);
 const float Weight=SwingAge<.16f?FMath::SmoothStep(0.f,1.f,SwingAge/.16f):1-FMath::SmoothStep(.16f,.58f,SwingAge);
 FVector Grip=FMath::Lerp(Ready,Contact,Weight);
 // The lateral backswing/follow-through must pass through the requested
 // contact at .16s. The old full-cycle sine displaced the string bed12cm
 // sideways at impact, even when the player timed a reachable ball correctly.
 Grip.Y+=SwingAge<.16f?FMath::Sin(SwingAge/.16f*PI)*12:SwingAge<.58f?-FMath::Sin((SwingAge-.16f)/.42f*PI)*12:0;
 const FVector Shoulder(0,22,50);Grip=Shoulder+(Grip-Shoulder).GetClampedToMaxSize(59);
 const auto Frame=Body->GetActorTransform();
 // Wrist orientation follows the same finite grip; fingers retain the existing
 // licensed pose. Limb IK never changes bone length or moves the player's feet.
 const FQuat Wrist=Frame.GetRotation()*Body->GetMesh()->GetRelativeRotation().Quaternion();
 Right=FTransform(Wrist,Frame.TransformPosition(Grip));
 Left=FTransform(Wrist,Frame.TransformPosition(SwingAge<.58f?FVector(10,-35,35):FVector(30,-18,24)));
 return true;
}
void UIslandTennisParticipant::UpdateRacket(float Delta)
{
 SwingAge+=Delta;
}
AIslandEstateSport::AIslandEstateSport()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RegulationCourtFrame")));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("CourtConsolePrompt"));Interaction->SetupAttachment(RootComponent);
 Interaction->SetBoxExtent(FVector(35,35,65));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Console=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CourtConsole"));Console->SetupAttachment(RootComponent);Console->SetCollisionProfileName(TEXT("BlockAll"));
 Ball=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TennisBall"));Ball->SetupAttachment(RootComponent);Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CourtCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("ActiveCourtCamera"));CourtCamera->SetupAttachment(RootComponent);CourtCamera->FieldOfView=72;
 ScoreDisplay=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PhysicalScoreboard"));ScoreDisplay->SetupAttachment(RootComponent);ScoreDisplay->SetWorldSize(5);ScoreDisplay->SetHorizontalAlignment(EHTA_Center);
}
void AIslandEstateSport::BeginPlay()
{
 Super::BeginPlay();Interaction->SetRelativeLocation(ConsoleLocal);Ball->SetStaticMesh(BallAsset);Ball->SetVisibility(false);Present();SetActorTickEnabled(false);
}
bool AIslandEstateSport::CanInteract_Implementation(AActor* Visitor)const
{
 auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);
 return P && P->IsLocallyControlled() && P->HasAuthority() && !IsActive() && BallAsset && RacketAsset && OpponentAsset && OpponentAnimation
  && !P->GetAttachParentActor() && !P->FindComponentByClass<UIslandTennisParticipant>() && P->GetCharacterMovement()->IsMovingOnGround()
  && GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID)
  && FVector::Dist(P->GetActorLocation(),Interaction->GetComponentLocation())<250;
}
FText AIslandEstateSport::GetInteractionName_Implementation()const
{return FText::FromString(TEXT("F Tennis — play singles with the estate practice robot"));}
void AIslandEstateSport::Interact_Implementation(AActor* Visitor)
{if(CanInteract_Implementation(Visitor))StartSession(CastChecked<ANeonCityPlayerCharacter>(Visitor));}
bool AIslandEstateSport::BindInput(APlayerController* PC)
{
 auto* Input=Cast<UEnhancedInputComponent>(PC->InputComponent);
 auto* Sub=PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;if(!Input || !Sub)return false;
 BoundInput=Input;InputSubsystem=Sub;Mapping=NewObject<UInputMappingContext>(this);
 const auto Bind=[&](FKey Key,void(AIslandEstateSport::*Fn)())
 {auto* Action=NewObject<UInputAction>(this);Action->ValueType=EInputActionValueType::Boolean;Action->bConsumeInput=true;Actions.Add(Action);Mapping->MapKey(Action,Key);Bindings.Add(Input->BindAction(Action,ETriggerEvent::Started,this,Fn).GetHandle());return Action;};
 auto* Shot=Bind(EKeys::LeftMouseButton,&AIslandEstateSport::BeginShot);
 Bindings.Add(Input->BindAction(Shot,ETriggerEvent::Completed,this,&AIslandEstateSport::ReleaseShot).GetHandle());
 Bindings.Add(Input->BindAction(Shot,ETriggerEvent::Canceled,this,&AIslandEstateSport::ReleaseShot).GetHandle());
 Bind(EKeys::R,&AIslandEstateSport::Serve);Bind(EKeys::F,&AIslandEstateSport::EndSession);Bind(EKeys::Escape,&AIslandEstateSport::EndSession);
 for(FKey Key:{EKeys::RightMouseButton,EKeys::Tab,EKeys::P,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five})Bind(Key,&AIslandEstateSport::Consume);
 FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;Sub->AddMappingContext(Mapping,250,Options);return true;
}
void AIslandEstateSport::RemoveInput()
{
 if(BoundInput.IsValid())for(uint32 Binding:Bindings)BoundInput->RemoveBindingByHandle(Binding);
 if(InputSubsystem.IsValid() && Mapping){FModifyContextOptions O;O.bIgnoreAllPressedKeysUntilRelease=true;InputSubsystem->RemoveMappingContext(Mapping,O);}
 Bindings.Reset();Actions.Reset();Mapping=nullptr;BoundInput.Reset();InputSubsystem.Reset();
}
UIslandTennisParticipant* AIslandEstateSport::AddParticipant(ACharacter* P)
{
 auto* Pose=NewObject<UIslandTennisParticipant>(P);P->AddInstanceComponent(Pose);Pose->RegisterComponent();Pose->Court=this;P->GetMesh()->AddTickPrerequisiteComponent(Pose);
 Pose->PreviousVisibilityTick=P->GetMesh()->VisibilityBasedAnimTickOption;Pose->PreviousUpdateRateOptimization=P->GetMesh()->bEnableUpdateRateOptimizations;
 P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;P->GetMesh()->bEnableUpdateRateOptimizations=false;
 auto* R=NewObject<UStaticMeshComponent>(P);P->AddInstanceComponent(R);R->SetStaticMesh(RacketAsset);R->SetCollisionEnabled(ECollisionEnabled::NoCollision);R->SetupAttachment(P->GetMesh(),TEXT("hand_r"));R->RegisterComponent();
 const FQuat Wrist=P->GetActorQuat()*P->GetMesh()->GetRelativeRotation().Quaternion();
 const FVector Direction=P->GetActorTransform().TransformVectorNoScale(FVector(43,0,25)).GetSafeNormal();
 const FQuat Relative=Wrist.Inverse()*FRotationMatrix::MakeFromXZ(Direction,P->GetActorForwardVector()).ToQuat();
 R->SetRelativeTransform(FTransform(Relative,Relative.RotateVector(FVector(-8,0,0))));Pose->Racket=R;Pose->PreviousRacket=R->GetComponentTransform();return Pose;
}
bool AIslandEstateSport::StartSession(ANeonCityPlayerCharacter* P)
{
 if(!P || IsActive())return false;auto* PC=Cast<APlayerController>(P->GetController());if(!PC)return false;
 const auto* Dive=P->FindComponentByClass<UDivingComponent>();const auto* Wing=P->FindComponentByClass<UWingsuitComponent>();
 const auto* Fish=PC->FindComponentByClass<UFishingComponent>();const auto* Weapon=P->GetCombatComponent()->GetWeapon();
 const auto* Light=P->FindComponentByClass<UNeonCityFlashlightComponent>();
 if((Weapon && Weapon->IsEquipped()) || (Dive && (Dive->IsEquipped() || Dive->IsChangingGear())) || (Wing && Wing->IsEquipped()) || (Fish && Fish->IsActive()) || (Light && Light->IsEquipped()))
 {if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(TEXT("Stow equipment before playing tennis.")),5);return false;}
 const FVector AIStart=ToWorld(FVector(1000,0,3+96));FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(TennisRobotStart),false,this);Q.AddIgnoredActor(P);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,AIStart,AIStart-FVector(0,0,180),ECC_Pawn,Q) || Hit.ImpactNormal.Z<.7)return false;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
 Opponent=GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(),Hit.ImpactPoint+FVector(0,0,99),GetActorRotation()+FRotator(0,180,0),Params);if(!Opponent)return false;
 Opponent->GetCapsuleComponent()->SetCapsuleSize(34,96);Opponent->GetMesh()->SetSkeletalMeshAsset(OpponentAsset);
 Opponent->GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-96),FRotator(0,-90,0));Opponent->GetMesh()->SetAnimInstanceClass(OpponentAnimation);
 Opponent->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Opponent->GetCharacterMovement()->bRunPhysicsWithNoController=true;Opponent->GetCharacterMovement()->bOrientRotationToMovement=false;Opponent->GetCharacterMovement()->MaxWalkSpeed=OpponentSpeed;Opponent->GetCharacterMovement()->MaxAcceleration=1300;Opponent->GetCharacterMovement()->BrakingDecelerationWalking=1800;
 // A controllerless character was already initialized with MOVE_None by the
 // spawn. Enable its normal supported movement after configuring that flag.
 Opponent->GetCharacterMovement()->SetDefaultMovementMode();
 if(!BindInput(PC)){Opponent->Destroy();Opponent=nullptr;return false;}
 Player=P;Viewer=PC;PreviousView=PC->GetViewTarget();PlayerPose=AddParticipant(P);AIPose=AddParticipant(Opponent);
 OrientBefore=P->GetCharacterMovement()->bOrientRotationToMovement;ControllerFacingBefore=P->GetCharacterMovement()->bUseControllerDesiredRotation;
 P->GetCharacterMovement()->bOrientRotationToMovement=false;P->GetCharacterMovement()->bUseControllerDesiredRotation=true;
 Rules.Reset();PlayerContacts=AIContacts=0;OpponentDistance=0;MaxContactPlaneDistance=MaxContactEllipse=0;LastAIPosition=Opponent->GetActorLocation();PointAge=0;AIAge=0;
 PC->SetControlRotation(GetActorRotation());PC->SetViewTarget(this);SetActorTickEnabled(true);
 Feedback=TEXT("WASD move | Mouse aim | hold/release LMB swing | R serve behind baseline | F leave | F6/F7 safe");Present();return true;
}
void AIslandEstateSport::EndSession()
{
 const auto PreviousViewer=Viewer;
 RemoveInput();if(Viewer.IsValid() && Viewer->GetViewTarget()==this)Viewer->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():Player.Get());
 if(Player.IsValid()){Player->GetCharacterMovement()->bOrientRotationToMovement=OrientBefore;Player->GetCharacterMovement()->bUseControllerDesiredRotation=ControllerFacingBefore;}
 for(auto* Pose:{PlayerPose.Get(),AIPose.Get()})if(Pose){if(auto* Body=Cast<ACharacter>(Pose->GetOwner());Body && Body->GetMesh()){Body->GetMesh()->VisibilityBasedAnimTickOption=Pose->PreviousVisibilityTick;Body->GetMesh()->bEnableUpdateRateOptimizations=Pose->PreviousUpdateRateOptimization;Body->GetMesh()->RemoveTickPrerequisiteComponent(Pose);}if(Pose->Racket)Pose->Racket->DestroyComponent();Pose->Court=nullptr;Pose->DestroyComponent();}
 PlayerPose=nullptr;AIPose=nullptr;if(Opponent)Opponent->Destroy();Opponent=nullptr;Player.Reset();Viewer.Reset();PreviousView.Reset();
 Charging=false;PlayerShotPending=AIShotPending=false;Rules.Rally=false;Ball->SetVisibility(false);SetActorTickEnabled(false);Feedback=TEXT("F Tennis | estate practice robot | regulation singles");Present();
 if(PreviousViewer.IsValid())if(auto* N=PreviousViewer->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(TEXT("Practice finished | approach the courtside console to play again")),3);
}
void AIslandEstateSport::BeginShot(){if(IsActive() && Rules.Rally){Charging=true;ChargeAge=0;}}
void AIslandEstateSport::ReleaseShot()
{
 if(!Charging || !IsActive())return;Charging=false;Power=FMath::Clamp(ChargeAge/.7f,.15f,1.f);
 if(Rules.Rally && Rules.LastHitter==1 && !PlayerShotPending)
 {QueueSwing(Player.Get(),ToWorld(BallLocal+BallVelocity*.16f+FVector(0,0,-.5f*980.f*.16f*.16f)));PlayerShotPending=true;}
}
void AIslandEstateSport::Serve()
{
 if(!IsActive() || Rules.Rally || PlayerShotPending)return;const FVector Loc=ToLocal(Player->GetActorLocation());
 if(!Rules.CanServeFrom(Loc))
 {Feedback=(Rules.Points[0]+Rules.Points[1])%2?TEXT("Serve behind the baseline from the right/ad half of the court."):TEXT("Serve behind the baseline from the left/deuce half of the court.");Present();return;}
 BallLocal=FVector(Loc.X+35,Loc.Y,175);BallVelocity=FVector::ZeroVector;Ball->SetRelativeLocation(BallLocal);Ball->SetVisibility(true);
 QueueSwing(Player.Get(),ToWorld(BallLocal));PlayerShotPending=true;Power=.6f;
}
void AIslandEstateSport::QueueSwing(ACharacter* Body,const FVector& Contact)
{
 auto* Pose=Body?Body->FindComponentByClass<UIslandTennisParticipant>():nullptr;if(!Pose)return;
 Pose->ContactLocal=Body->GetActorTransform().InverseTransformPosition(Contact);Pose->ContactLocal.X=FMath::Clamp(Pose->ContactLocal.X,35.f,135.f);Pose->ContactLocal.Y=FMath::Clamp(Pose->ContactLocal.Y,-80.f,100.f);Pose->ContactLocal.Z=FMath::Clamp(Pose->ContactLocal.Z,-30.f,95.f);Pose->SwingAge=Pose->PreviousSwingAge=0;Pose->PreviousRacket=Pose->Racket->GetComponentTransform();
}
void AIslandEstateSport::Launch(int32 Side,const FVector& Target,float Apex)
{
 constexpr float Gravity=980;const float Height=FMath::Max(1.f,BallLocal.Z-6.5f);
 const float Up=FMath::Sqrt(2*Gravity*FMath::Max(40.f,Apex-BallLocal.Z));
 const float Flight=(Up+FMath::Sqrt(Up*Up+2*Gravity*Height))/Gravity;
 BallVelocity=(Target-BallLocal)/Flight;BallVelocity.Z=Up;AIAge=0;
}
void AIslandEstateSport::ResolveShot(ACharacter* Body,int32 Side)
{
 if(!Body)return;auto* Pose=Body->FindComponentByClass<UIslandTennisParticipant>();if(!Pose || !Pose->Racket)return;
 const bool Service=!Rules.Rally;const FVector Head=Pose->Racket->GetComponentTransform().TransformPosition(FVector(50,0,0));
 if(!Rules.Strike(Side,Service))return;
 const FVector Aim=GetActorTransform().InverseTransformVectorNoScale(Viewer.IsValid()?Viewer->GetControlRotation().Vector():GetActorForwardVector());
 const float AimY=FMath::Clamp(Aim.Y*650.f,-350.f,350.f);FVector Target;
 if(Service){const bool Ad=(Rules.Points[0]+Rules.Points[1])%2!=0;Target=FVector(440,Ad?-190:190,6.5f);}
 else if(Side==0)Target=FVector(600+Power*450,AimY,6.5f);
 else{const float PlayerY=ToLocal(Player->GetActorLocation()).Y;Target=FVector(-680-FMath::FRandRange(0.f,260.f),FMath::Clamp(-PlayerY*.45f+FMath::FRandRange(-150.f,150.f),-320.f,320.f),6.5f);}
 Launch(Side,Target,Service?330:310+Power*70);if(Side==0)++PlayerContacts;else ++AIContacts;
 Feedback=FString::Printf(TEXT("%s | rally %d | hold/release LMB • R next serve • F exit"),*Rules.Score(),PlayerContacts+AIContacts);Sound(HitSound,Head,1+Power*.12f);Present();
}
bool AIslandEstateSport::RacketContact(ACharacter* Body,int32 Side,const FVector& Before,const FVector& After,float Alpha0,float Alpha1)
{
 auto* Pose=Body?Body->FindComponentByClass<UIslandTennisParticipant>():nullptr;
 if(!Pose || !Pose->Racket || Pose->SwingAge<.06f || Pose->PreviousSwingAge>.38f || (Rules.Rally && (Rules.LastHitter==Side || (Rules.Service && Rules.FirstLanding))))return false;
 FTransform A,B;A.Blend(Pose->PreviousRacket,Pose->Racket->GetComponentTransform(),Alpha0);B.Blend(Pose->PreviousRacket,Pose->Racket->GetComponentTransform(),Alpha1);
 const FVector P0=A.InverseTransformPosition(ToWorld(Before)),P1=B.InverseTransformPosition(ToWorld(After));
 constexpr float Radius=3.35f,Tolerance=.5f;
 if(P0.Z*P1.Z>0 && FMath::Min(FMath::Abs(P0.Z),FMath::Abs(P1.Z))>Radius+Tolerance)return false;
 const float T=FMath::Abs(P0.Z-P1.Z)>SMALL_NUMBER?FMath::Clamp(P0.Z/(P0.Z-P1.Z),0.f,1.f):0;
 const FVector Contact=FMath::Lerp(P0,P1,T);
 const float ContactAge=FMath::Lerp(Pose->PreviousSwingAge,Pose->SwingAge,FMath::Lerp(Alpha0,Alpha1,T));
 if(ContactAge<.06f || ContactAge>.38f)return false;
 const float Ellipse=FMath::Square((Contact.X-50)/(17.f+Radius))+FMath::Square(Contact.Y/(11.6f+Radius));
 if(Ellipse>1)return false;
 MaxContactPlaneDistance=FMath::Max(MaxContactPlaneDistance,FMath::Abs(Contact.Z));MaxContactEllipse=FMath::Max(MaxContactEllipse,Ellipse);
 BallLocal=FMath::Lerp(Before,After,T);ResolveShot(Body,Side);
 if(Side==0)PlayerShotPending=false;else AIShotPending=false;return true;
}
void AIslandEstateSport::StepBall(float Delta)
{
 if(!Rules.Rally && !PlayerShotPending)return;const float Total=FMath::Min(Delta,.25f);float Remaining=Total;
 while(Remaining>0 && (Rules.Rally || PlayerShotPending))
 {
  const float Alpha0=1-Remaining/Total;const float Step=FMath::Min(Remaining,1.f/240);Remaining-=Step;const float Alpha1=1-Remaining/Total;const FVector Before=BallLocal;
  if(Rules.Rally)BallVelocity.Z-=980*Step;FVector Next=Before+BallVelocity*Step;
  if(PlayerShotPending && RacketContact(Player.Get(),0,Before,Next,Alpha0,Alpha1))continue;
  if(AIShotPending && RacketContact(Opponent,1,Before,Next,Alpha0,Alpha1))continue;
  if(!Rules.Rally)continue;
  // The net remains the actual authored mesh. This analytical continuous plane
  // prevents its millimetre strings from tunnelling at normal shot speed.
  if((Before.X<0)!=(Next.X<0))
  {
   const float T=FMath::Abs(Before.X)/(FMath::Abs(Before.X)+FMath::Abs(Next.X));const FVector Cross=FMath::Lerp(Before,Next,T);
   const float NetHeight=91.4f+16.f*FMath::Clamp(FMath::Abs(Cross.Y)/610.f,0.f,1.f);
   if(FMath::Abs(Cross.Y)<613.35f && Cross.Z-3.35f<NetHeight+3)
   {BallLocal=Cross;Rules.Net();Feedback=Rules.Winner<0?TEXT("Service fault — R second serve"):TEXT("Net — point to opponent");Sound(BounceSound,ToWorld(Cross),.65f);FinishPoint();break;}
  }
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TennisBallWorldSweep),false,this);Q.AddIgnoredActor(Player.Get());Q.AddIgnoredActor(Opponent.Get());
  const bool Hit=GetWorld()->SweepSingleByChannel(H,ToWorld(Before),ToWorld(Next),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(3.35f),Q);
  if(Hit)
  {
   const FVector N=GetActorTransform().InverseTransformVectorNoScale(H.ImpactNormal);
   BallLocal=ToLocal(H.Location)+N*.2f;
   if(N.Z>.7 && BallVelocity.Z<0)
   {
    if(!Rules.Bounce(BallLocal)){Feedback=Rules.Winner<0?TEXT("Service fault — R second serve"):TEXT("Point complete — R serve");FinishPoint();break;}
    BallVelocity.Z=FMath::Abs(BallVelocity.Z)*.76f;BallVelocity.X*=.84f;BallVelocity.Y*=.84f;Sound(BounceSound,ToWorld(BallLocal),.9f);
   }
   else if(N.Z<=.7){Rules.Net();Feedback=TEXT("Ball hit the court boundary — next serve");FinishPoint();break;}
  }
  else BallLocal=Next;
  if(BallLocal.Z<-120 || FMath::Abs(BallLocal.X)>2400 || FMath::Abs(BallLocal.Y)>1300)
  {Rules.Bounce(FVector(BallLocal.X,BallLocal.Y,-120));if(Rules.Rally)Rules.Bounce(BallLocal);Feedback=TEXT("Out — R next serve");FinishPoint();}
 }
 Ball->SetRelativeLocation(BallLocal);Ball->AddLocalRotation(FRotator(Delta*420,Delta*240,0));
}
void AIslandEstateSport::StepOpponent(float Delta)
{
 if(!Opponent || !Player.IsValid())return;AIAge+=Delta;const FVector Loc=ToLocal(Opponent->GetActorLocation());FVector Target(950,0,Loc.Z);
 if(Rules.Rally && Rules.LastHitter==0 && AIAge>=OpponentReaction)
 {
  // Predict a reachable intercept; acceleration and court collision, rather
  // than relocation, determine whether the robot can return a wide shot.
  const float Time=BallVelocity.X>1?FMath::Clamp((900-BallLocal.X)/BallVelocity.X,0.f,1.5f):0;
  Target.X=950;Target.Y=FMath::Clamp(BallLocal.Y+BallVelocity.Y*Time,-510.f,510.f);
 }
 const FVector Offset=ToWorld(Target)-Opponent->GetActorLocation();
 if(Offset.Size2D()>32)Opponent->AddMovementInput(Offset.GetSafeNormal2D(),1);
 const FRotator Facing=(ToWorld(BallLocal)-Opponent->GetActorLocation()).Rotation();Opponent->SetActorRotation(FRotator(0,Facing.Yaw,0));
 OpponentDistance+=FVector::Dist2D(LastAIPosition,Opponent->GetActorLocation());LastAIPosition=Opponent->GetActorLocation();
 if(Rules.Rally && Rules.LastHitter==0 && !AIShotPending && AIAge>=OpponentReaction && (!Rules.Service || !Rules.FirstLanding))
 {
  const FVector Contact=ToWorld(BallLocal+BallVelocity*.16f);const FVector Relative=Opponent->GetActorTransform().InverseTransformPosition(Contact);
  if(Relative.X>20 && Relative.X<135 && FMath::Abs(Relative.Y)<85 && Relative.Z>-31 && Relative.Z<89)
  {QueueSwing(Opponent,Contact);AIShotPending=true;}
 }
}
void AIslandEstateSport::FinishPoint(){PointAge=0;Charging=false;PlayerShotPending=AIShotPending=false;Sound(PointSound,ToWorld(BallLocal));Present();}
void AIslandEstateSport::Tick(float Delta)
{
 Super::Tick(Delta);if(!IsActive()){if(Viewer.IsValid() || Mapping || Opponent || PlayerPose || AIPose)EndSession();return;}
 if(!IsValid(Opponent) || !IsValid(PlayerPose) || !IsValid(AIPose) || !IsValid(PlayerPose->Racket) || !IsValid(AIPose->Racket) || !Viewer.IsValid() || Viewer->GetPawn()!=Player.Get() || Player->GetHealthComponent()->IsDead() || Player->GetAttachParentActor()
  || FVector::Dist2D(Player->GetActorLocation(),GetActorLocation())>3400 || Viewer->GetViewTarget()!=this)
 {EndSession();return;}
 const auto* Dive=Player->FindComponentByClass<UDivingComponent>();const auto* Wing=Player->FindComponentByClass<UWingsuitComponent>();const auto* Light=Player->FindComponentByClass<UNeonCityFlashlightComponent>();
 const auto* Weapon=Player->GetCombatComponent()->GetWeapon();
 if((Weapon && Weapon->IsEquipped()) || (Dive && (Dive->IsEquipped() || Dive->IsChangingGear())) || (Wing && Wing->IsEquipped()) || (Light && Light->IsEquipped()) || Player->GetCharacterMovement()->IsSwimming())
 {EndSession();return;}
 if(Charging)ChargeAge+=Delta;PointAge+=Delta;
 // Both participant clocks advance before the mesh's finite IK evaluates.
 // Sweep the actual previous/current string planes before expiring a swing;
 // a delayed render frame must not discard a physical contact in that interval.
 StepBall(Delta);StepOpponent(Delta);
 if(PlayerShotPending && PlayerPose->SwingAge>.38f){PlayerShotPending=false;Feedback=TEXT("Miss — move into range and time the racket swing.");Present();}
 if(AIShotPending && AIPose->SwingAge>.38f)AIShotPending=false;
 PlayerPose->PreviousSwingAge=PlayerPose->SwingAge;AIPose->PreviousSwingAge=AIPose->SwingAge;
 PlayerPose->PreviousRacket=PlayerPose->Racket->GetComponentTransform();AIPose->PreviousRacket=AIPose->Racket->GetComponentTransform();
 const FVector Feet=Player->GetActorLocation()-FVector(0,0,Player->GetSimpleCollisionHalfHeight());
 const FVector ViewDirection=Viewer->GetControlRotation().Vector().GetSafeNormal2D();
 const FVector Focus=Feet+ViewDirection*260+FVector(0,0,120),Desired=Feet-ViewDirection*460+FVector(0,0,330);
 FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TennisSafeCamera),false,this);Q.AddIgnoredActor(Player.Get());Q.AddIgnoredActor(Opponent.Get());
 FVector Eye=Desired;
 if(GetWorld()->SweepSingleByChannel(H,Focus,Desired,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Q))Eye=H.Location+H.Normal*16;
 CourtCamera->SetWorldLocation(Eye);CourtCamera->SetWorldRotation((Focus-Eye).Rotation());
 if(!Rules.Rally && PointAge>2.5f && !PlayerShotPending)Ball->SetVisibility(false);
}
void AIslandEstateSport::Present()
{
 ScoreDisplay->SetText(FText::FromString(Rules.Score()+TEXT("\n")+(Rules.Rally?TEXT("RALLY"):Rules.Faults?TEXT("SECOND SERVE"):TEXT("R • SERVE"))));
 if(Viewer.IsValid())if(auto* N=Viewer->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(Feedback),8);
}
void AIslandEstateSport::Sound(USoundBase* Asset,const FVector& At,float Pitch)
{if(Asset){UGameplayStatics::PlaySoundAtLocation(this,Asset,At,.65f,Pitch,0,SoundAttenuation);ContactAudioAt=GetWorld()->GetTimeSeconds();}}
bool AIslandEstateSport::IsRacketFeedbackPlaying()const{return GetWorld() && GetWorld()->GetTimeSeconds()-ContactAudioAt<1;}
void AIslandEstateSport::EndPlay(EEndPlayReason::Type Reason){EndSession();Super::EndPlay(Reason);}
