#include "Adventure/IslandEstatePrecisionSport.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Property/PropertySubsystem.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UIslandPrecisionParticipant::UIslandPrecisionParticipant()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}
void UIslandPrecisionParticipant::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);if(!Sport.IsValid())return;Sport->FollowActualHand();
 // Release only after the same frame's player mesh has evaluated the finite
 // swing. A slow rendering frame cannot release from an old backswing pose.
 if(Sport->Stage==EIslandPrecisionStage::Swing && Sport->SwingSeconds>=.18f && !Sport->bShotLaunched)Sport->Launch();
}
bool UIslandPrecisionParticipant::GetHandTargets(FTransform& Right,FTransform& Left)const
{return Sport.IsValid() && Sport->HandTargets(Right,Left);}
float UIslandPrecisionParticipant::GetBodyLeanDegrees()const
{return Sport.IsValid() && Sport->HasHandPose()?Sport->Kind==EIslandPrecisionSport::Putting?18.f:12.f:0.f;}

TSharedRef<SWidget> UIslandPrecisionSportWidget::RebuildWidget()
{
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
 auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.018,.028,.026,.9));Panel->SetPadding(FMargin(18));
 auto* CanvasSlot=Canvas->AddChildToCanvas(Panel);CanvasSlot->SetAnchors(FAnchors(.5,1));CanvasSlot->SetAlignment(FVector2D(.5,1));CanvasSlot->SetPosition(FVector2D(0,-32));CanvasSlot->SetAutoSize(true);
 auto* Width=WidgetTree->ConstructWidget<USizeBox>();Width->SetWidthOverride(490);Panel->SetContent(Width);
 auto* Stack=WidgetTree->ConstructWidget<UVerticalBox>();Width->SetContent(Stack);
 Body=WidgetTree->ConstructWidget<UTextBlock>();Body->SetAutoWrapText(true);Body->SetColorAndOpacity(FSlateColor(FLinearColor(.93,.92,.85)));auto Font=Body->GetFont();Font.Size=16;Body->SetFont(Font);Stack->AddChildToVerticalBox(Body)->SetPadding(FMargin(0,0,0,9));
 Meter=WidgetTree->ConstructWidget<UProgressBar>();Meter->SetFillColorAndOpacity(FLinearColor(.45,.72,.57));Stack->AddChildToVerticalBox(Meter);
 return Super::RebuildWidget();
}
void UIslandPrecisionSportWidget::Present(const FText& Text,float Power,bool Charging)
{if(Body)Body->SetText(Text);if(Meter){Meter->SetPercent(Power);Meter->SetVisibility(Charging?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);}}

AIslandEstatePrecisionSport::AIslandEstatePrecisionSport()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.TickGroup=TG_PrePhysics;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("StationFrame"));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("NormalFPrompt"));Interaction->SetupAttachment(RootComponent);Interaction->SetBoxExtent(FVector(20,35,28));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Interaction->SetCanEverAffectNavigation(false);
 Ball=CreateDefaultSubobject<USphereComponent>(TEXT("ActualRigidBall"));Ball->SetupAttachment(RootComponent);Ball->InitSphereRadius(10.8f);Ball->SetCollisionProfileName(TEXT("PhysicsActor"));Ball->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Ball->SetNotifyRigidBodyCollision(true);Ball->SetUseCCD(true);Ball->SetCanEverAffectNavigation(false);
 BallVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetailedBall"));BallVisual->SetupAttachment(Ball);BallVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);BallVisual->SetCanEverAffectNavigation(false);
 Putter=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnimatedMilledPutter"));Putter->SetupAttachment(RootComponent);Putter->SetCollisionEnabled(ECollisionEnabled::NoCollision);Putter->SetCanEverAffectNavigation(false);
 for(int32 I=0;I<10;++I){auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RetainedRegulationPin%d"),I));C->SetupAttachment(RootComponent);C->SetCollisionProfileName(TEXT("PhysicsActor"));C->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);C->SetNotifyRigidBodyCollision(true);C->SetUseCCD(true);C->SetCanEverAffectNavigation(false);Pins.Add(C);}
 for(int32 I=0;I<3;++I){auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RemovableCupFlag%d"),I));C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);Flags.Add(C);}
 ActivityCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("PracticeCamera"));ActivityCamera->SetupAttachment(RootComponent);ActivityCamera->FieldOfView=60;ActivityCamera->bUsePawnControlRotation=false;
 ImpactAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("PhysicalImpactAudio"));ImpactAudio->SetupAttachment(RootComponent);ImpactAudio->bAutoActivate=false;
 RollAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("MeasuredBallRollAudio"));RollAudio->SetupAttachment(Ball);RollAudio->bAutoActivate=false;
 FSoundAttenuationSettings Att;Att.bAttenuate=true;Att.bSpatialize=true;Att.AttenuationShapeExtents=FVector(130);Att.FalloffDistance=2000;ImpactAudio->AdjustAttenuation(Att);RollAudio->AdjustAttenuation(Att);
}
bool AIslandEstatePrecisionSport::HasPresentation()const
{return !StationID.IsNone() && BallMesh && (Kind==EIslandPrecisionSport::Bowling?PinMesh && PinPositions.Num()==10:PutterMesh && FlagMesh && HolePositions.Num()==3 && TeePositions.Num()==3);}
void AIslandEstatePrecisionSport::BeginPlay()
{
 Super::BeginPlay();Interaction->SetRelativeLocation(PromptLocal);BallVisual->SetStaticMesh(BallMesh);Putter->SetStaticMesh(PutterMesh);Putter->SetVisibility(false);ActivityCamera->FieldOfView=70;
 Surface=NewObject<UPhysicalMaterial>(this);Surface->Friction=Kind==EIslandPrecisionSport::Bowling?.12f:.24f;Surface->StaticFriction=Surface->Friction;Surface->Restitution=Kind==EIslandPrecisionSport::Bowling?.22f:.08f;
 Ball->SetSphereRadius(Kind==EIslandPrecisionSport::Bowling?10.8f:2.135f);Ball->SetPhysMaterialOverride(Surface);Ball->SetMassOverrideInKg(NAME_None,Kind==EIslandPrecisionSport::Bowling?7.2f:.04593f,true);Ball->SetLinearDamping(RollingDamping);Ball->SetAngularDamping(RollingDamping);Ball->OnComponentHit.AddDynamic(this,&AIslandEstatePrecisionSport::PhysicsHit);
 // Small golf balls need more angular speed than the project default allows.
 // Preserve v = omega*r at the strongest supported stroke, without changing
 // friction, damping, cup capture or the player's chosen launch speed.
 if(Kind==EIslandPrecisionSport::Putting)Ball->SetPhysicsMaxAngularVelocityInRadians(MaximumSpeed/Ball->GetUnscaledSphereRadius()*1.1f,false,NAME_None);
 for(int32 I=0;I<Pins.Num();++I){UStaticMeshComponent* C=Pins[I].Get();C->SetStaticMesh(PinMesh);C->SetVisibility(Kind==EIslandPrecisionSport::Bowling);C->SetCollisionEnabled(Kind==EIslandPrecisionSport::Bowling?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Kind==EIslandPrecisionSport::Bowling && PinMesh){C->SetPhysMaterialOverride(Surface);C->SetMassOverrideInKg(NAME_None,1.53f,true);C->SetLinearDamping(.08f);C->SetAngularDamping(.15f);C->OnComponentHit.AddDynamic(this,&AIslandEstatePrecisionSport::PhysicsHit);}}
 for(int32 I=0;I<Flags.Num();++I){Flags[I]->SetStaticMesh(FlagMesh);Flags[I]->SetVisibility(Kind==EIslandPrecisionSport::Putting);if(HolePositions.IsValidIndex(I))Flags[I]->SetRelativeLocation(HolePositions[I]);}
 ResetPins();ResetBall(Kind==EIslandPrecisionSport::Putting && !TeePositions.IsEmpty()?TeePositions[0]:FVector(30,0,0));PrepareAudio();
}
bool AIslandEstatePrecisionSport::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!HasPresentation() || !P || !PC || !PC->IsLocalController() || !HasAuthority() || P->GetHealthComponent()->IsDead())return false;
 if(IsPlaying())return ActivePlayer.Get()==P;
 if(!GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID) || P->GetAttachParentActor() || !P->GetCharacterMovement()->IsMovingOnGround())return false;
 if(const auto* D=P->FindComponentByClass<UDamageableCharacterComponent>();D && !D->CanAct())return false;
 if(const auto* D=P->FindComponentByClass<UDivingComponent>();D && (D->IsEquipped() || D->IsChangingGear()))return false;
 if(const auto* W=P->FindComponentByClass<UWingsuitComponent>();W && W->IsEquipped())return false;
 if(const auto* W=P->GetCombatComponent()->GetWeapon();W && W->IsEquipped())return false;
 for(TActorIterator<AIslandEstatePrecisionSport> I(GetWorld());I;++I)if(*I!=this && I->IsPlaying())return false;
 return FVector::DistSquared(P->GetActorLocation(),Interaction->GetComponentLocation())<FMath::Square(235.f);
}
FText AIslandEstatePrecisionSport::GetInteractionName_Implementation()const
{return IsPlaying()?GetSessionText():FText::FromString(TEXT("F ")+DisplayName.ToString()+TEXT(" | Hold/release LMB, arrows aim"));}
void AIslandEstatePrecisionSport::Interact_Implementation(AActor* Visitor)
{if(!CanInteract_Implementation(Visitor))return;if(IsPlaying())FinishSession();else BeginSession(CastChecked<ANeonCityPlayerCharacter>(Visitor));}
bool AIslandEstatePrecisionSport::InstallInput(APlayerController* PC)
{
 auto* Enhanced=Cast<UEnhancedInputComponent>(PC->InputComponent);auto* Sub=PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;if(!Enhanced || !Sub)return false;
 BoundInput=Enhanced;InputSubsystem=Sub;Mapping=NewObject<UInputMappingContext>(this);Actions.Reset();
 const auto Bind=[&](FKey Key,void(AIslandEstatePrecisionSport::*Method)())
 {auto* A=NewObject<UInputAction>(this);A->ValueType=EInputActionValueType::Boolean;A->bConsumeInput=true;Actions.Add(A);Mapping->MapKey(A,Key);Bindings.Add(Enhanced->BindAction(A,ETriggerEvent::Started,this,Method).GetHandle());return A;};
 auto* Trigger=Bind(EKeys::LeftMouseButton,&AIslandEstatePrecisionSport::Press);Bindings.Add(Enhanced->BindAction(Trigger,ETriggerEvent::Completed,this,&AIslandEstatePrecisionSport::Release).GetHandle());Bindings.Add(Enhanced->BindAction(Trigger,ETriggerEvent::Canceled,this,&AIslandEstatePrecisionSport::Release).GetHandle());
 Bind(EKeys::F,&AIslandEstatePrecisionSport::FinishSession);Bind(EKeys::Escape,&AIslandEstatePrecisionSport::FinishSession);Bind(EKeys::R,&AIslandEstatePrecisionSport::Restart);Bind(EKeys::Enter,&AIslandEstatePrecisionSport::Next);
 Bind(EKeys::Left,&AIslandEstatePrecisionSport::LeftAim);Bind(EKeys::Right,&AIslandEstatePrecisionSport::RightAim);
 auto* Mouse=NewObject<UInputAction>(this);Mouse->ValueType=EInputActionValueType::Axis1D;Mouse->bConsumeInput=true;Actions.Add(Mouse);Mapping->MapKey(Mouse,EKeys::MouseX);Bindings.Add(Enhanced->BindAction(Mouse,ETriggerEvent::Triggered,this,&AIslandEstatePrecisionSport::Aim).GetHandle());
 FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;Sub->AddMappingContext(Mapping,250,Options);return true;
}
void AIslandEstatePrecisionSport::RemoveInput()
{
 if(BoundInput.IsValid())for(uint32 H:Bindings)BoundInput->RemoveBindingByHandle(H);
 if(InputSubsystem.IsValid() && Mapping){FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;InputSubsystem->RemoveMappingContext(Mapping,Options);}
 Bindings.Reset();BoundInput.Reset();InputSubsystem.Reset();Mapping=nullptr;Actions.Reset();
}
bool AIslandEstatePrecisionSport::BeginSession(ANeonCityPlayerCharacter* P)
{
 auto* PC=CastChecked<APlayerController>(P->GetController());if(!InstallInput(PC))return false;
 ActivePlayer=P;ActiveController=PC;PreviousViewTarget=PC->GetViewTarget();StartPlayerPosition=P->GetActorLocation();bOwnerNoSeeBefore=P->GetMesh()->bOwnerNoSee;
 bOrientBefore=P->GetCharacterMovement()->bOrientRotationToMovement;bDesiredRotationBefore=P->GetCharacterMovement()->bUseControllerDesiredRotation;bControllerYawBefore=P->bUseControllerRotationYaw;
 // Gameplay grip/contact requires evaluated bones even when offscreen. The
 // Blueprint's normal visibility/URO policy is restored on every session exit.
 AnimTickBefore=P->GetMesh()->VisibilityBasedAnimTickOption;bUpdateRateBefore=P->GetMesh()->bEnableUpdateRateOptimizations;
 P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;P->GetMesh()->bEnableUpdateRateOptimizations=false;
 Participant=NewObject<UIslandPrecisionParticipant>(P);P->AddInstanceComponent(Participant);Participant->Sport=this;Participant->RegisterComponent();
 P->GetMesh()->AddTickPrerequisiteActor(this);Participant->AddTickPrerequisiteComponent(P->GetMesh());
 Frame=ThrowInFrame=FrameFallen=Hole=Score=Shots=CompletedHoles=PhysicsHits=0;SessionSeconds=0;Stage=EIslandPrecisionStage::Aiming;ResetPins();ResetBall(Kind==EIslandPrecisionSport::Bowling?FVector(30,0,0):TeePositions[0]);
 SetAddressed(Kind==EIslandPrecisionSport::Bowling);Widget=CreateWidget<UIslandPrecisionSportWidget>(PC);if(Widget){Widget->SetVisibility(ESlateVisibility::HitTestInvisible);Widget->AddToViewport(40);}SetActorTickEnabled(true);UpdatePresentation(0);Notice(GetSessionText());return true;
}
void AIslandEstatePrecisionSport::FinishSession()
{
 if(!IsPlaying() && !ActiveController.IsValid()){RemoveInput();return;}
 RemoveInput();SetAddressed(false);auto* PC=ActiveController.Get();if(PC && bIgnoredMove){PC->SetIgnoreMoveInput(false);bIgnoredMove=false;}if(PC && PC->GetViewTarget()==this)PC->SetViewTarget(PC->GetPawn()!=ActivePlayer.Get()?PC->GetPawn():PreviousViewTarget.IsValid()?PreviousViewTarget.Get():PC->GetPawn());
 if(Participant){Participant->Sport.Reset();Participant->DestroyComponent();Participant=nullptr;}
 if(ActivePlayer.IsValid()){ActivePlayer->GetMesh()->RemoveTickPrerequisiteActor(this);ActivePlayer->GetMesh()->SetOwnerNoSee(bOwnerNoSeeBefore);ActivePlayer->GetMesh()->VisibilityBasedAnimTickOption=AnimTickBefore;ActivePlayer->GetMesh()->bEnableUpdateRateOptimizations=bUpdateRateBefore;}
 bIgnoredMove=false;ActivePlayer.Reset();ActiveController.Reset();PreviousViewTarget.Reset();if(Widget)Widget->RemoveFromParent();Widget=nullptr;
 ImpactAudio->Stop();RollAudio->Stop();if(ImpactSound)ImpactSound->ResetAudio();if(RollingSound)RollingSound->ResetAudio();Putter->SetVisibility(false);Stage=EIslandPrecisionStage::Idle;
 ResetPins();ResetBall(Kind==EIslandPrecisionSport::Putting && !TeePositions.IsEmpty()?TeePositions[0]:FVector(30,0,0));for(UStaticMeshComponent* F:Flags)F->SetVisibility(Kind==EIslandPrecisionSport::Putting);SetActorTickEnabled(false);
}
void AIslandEstatePrecisionSport::EndPlay(EEndPlayReason::Type Reason){FinishSession();Super::EndPlay(Reason);}
void AIslandEstatePrecisionSport::ResetPins()
{
 RemovedPins.Reset();for(int32 I=0;I<Pins.Num();++I){UStaticMeshComponent* C=Pins[I].Get();C->SetSimulatePhysics(false);C->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepWorldTransform);if(PinPositions.IsValidIndex(I))C->SetRelativeLocationAndRotation(PinPositions[I],FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);C->SetVisibility(Kind==EIslandPrecisionSport::Bowling);C->SetCollisionEnabled(Kind==EIslandPrecisionSport::Bowling?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);}
}
void AIslandEstatePrecisionSport::ResetBall(const FVector& Local)
{
 Ball->SetSimulatePhysics(false);Ball->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepWorldTransform);Ball->SetRelativeLocationAndRotation(Local+FVector(0,0,Ball->GetUnscaledSphereRadius()+.3f),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Ball->SetVisibility(true,true);
 LastShotLocal=Local;AimDegrees=Kind==EIslandPrecisionSport::Putting && HolePositions.IsValidIndex(Hole)?(HolePositions[Hole]-Local).Rotation().Yaw:0;Charge=SwingSeconds=RollingSeconds=RestSeconds=0;bShotLaunched=bHoleCaptured=bOutOfBounds=false;
 for(int32 I=0;I<Flags.Num();++I)Flags[I]->SetVisibility(Kind==EIslandPrecisionSport::Putting && (!IsPlaying() || I!=Hole));
}
void AIslandEstatePrecisionSport::Press()
{
 if(Stage!=EIslandPrecisionStage::Aiming)return;
 if(Kind==EIslandPrecisionSport::Putting)
 {
  if(!ActivePlayer.IsValid() || FVector::Dist2D(ActivePlayer->GetActorLocation(),GetShotStandLocation())>18){Notice(FText::FromString(TEXT("Walk beside the ball before addressing it | WASD move | LMB hold/release")));return;}
  SetAddressed(true);
 }
 Stage=EIslandPrecisionStage::Charging;Charge=0;
}
void AIslandEstatePrecisionSport::Release(){if(Stage==EIslandPrecisionStage::Charging){LaunchSpeed=FMath::Lerp(MinimumSpeed,MaximumSpeed,Charge);Stage=EIslandPrecisionStage::Swing;SwingSeconds=0;bShotLaunched=false;}}
void AIslandEstatePrecisionSport::Aim(const FInputActionValue& V){if(Kind==EIslandPrecisionSport::Putting && !bIgnoredMove){if(ActiveController.IsValid())ActiveController->AddYawInput(V.Get<float>());return;}if(Stage==EIslandPrecisionStage::Aiming || Stage==EIslandPrecisionStage::Charging)AimDegrees=Kind==EIslandPrecisionSport::Bowling?FMath::Clamp(AimDegrees+V.Get<float>()*.22f,-8.f,8.f):FMath::UnwindDegrees(AimDegrees+V.Get<float>()*.3f);}
void AIslandEstatePrecisionSport::LeftAim(){if(Stage==EIslandPrecisionStage::Aiming || Stage==EIslandPrecisionStage::Charging)AimDegrees=Kind==EIslandPrecisionSport::Bowling?FMath::Clamp(AimDegrees-.2f,-8.f,8.f):FMath::UnwindDegrees(AimDegrees-1.f);}
void AIslandEstatePrecisionSport::RightAim(){if(Stage==EIslandPrecisionStage::Aiming || Stage==EIslandPrecisionStage::Charging)AimDegrees=Kind==EIslandPrecisionSport::Bowling?FMath::Clamp(AimDegrees+.2f,-8.f,8.f):FMath::UnwindDegrees(AimDegrees+1.f);}
void AIslandEstatePrecisionSport::Launch()
{
 if(Kind==EIslandPrecisionSport::Bowling)FollowActualHand();
 const FVector Direction=GetActorTransform().TransformVectorNoScale(FRotator(0,AimDegrees,0).Vector()).GetSafeNormal();Ball->SetSimulatePhysics(true);Ball->WakeAllRigidBodies();Ball->AddImpulse(Direction*LaunchSpeed*Ball->GetMass());Ball->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector,Direction)*LaunchSpeed/Ball->GetUnscaledSphereRadius());
 if(Kind==EIslandPrecisionSport::Bowling)for(int32 I=0;I<Pins.Num();++I)if(!RemovedPins.Contains(I)){Pins[I]->SetSimulatePhysics(true);Pins[I]->WakeAllRigidBodies();}
 ++Shots;bShotLaunched=true;RollingSeconds=RestSeconds=0;
}
bool AIslandEstatePrecisionSport::IsPinFallen(int32 I)const
{return PinPositions.IsValidIndex(I) && (RemovedPins.Contains(I) || FVector::DotProduct(Pins[I]->GetUpVector(),GetActorUpVector())<.65f || FVector::DistSquared(Pins[I]->GetComponentLocation(),GetActorTransform().TransformPosition(PinPositions[I]))>FMath::Square(25.f));}
int32 AIslandEstatePrecisionSport::GetFallenPins()const{int32 N=0;for(int32 I=0;I<Pins.Num();++I)if(IsPinFallen(I))++N;return N;}
FVector AIslandEstatePrecisionSport::GetBallPosition()const{return Ball->GetComponentLocation();}
void AIslandEstatePrecisionSport::ResolveRoll()
{
 RollAudio->Stop();if(Kind==EIslandPrecisionSport::Putting)SetAddressed(false);if(Kind==EIslandPrecisionSport::Bowling){const int32 Count=GetFallenPins();Score+=FMath::Max(0,Count-FrameFallen);FrameFallen=Count;++ThrowInFrame;Stage=EIslandPrecisionStage::Result;}
 else if(bHoleCaptured){++CompletedHoles;Stage=EIslandPrecisionStage::Result;Notice(FText::FromString(FString::Printf(TEXT("Hole %d complete | %d strokes"),Hole+1,Shots)));}
 else if(bOutOfBounds){Stage=EIslandPrecisionStage::Result;Notice(FText::FromString(TEXT("Ball outside green | Enter returns to last supported lie")));}
 else{const FVector Local=GetActorTransform().InverseTransformPosition(Ball->GetComponentLocation());ResetBall(FVector(Local.X,Local.Y,0));Stage=EIslandPrecisionStage::Aiming;}
}
void AIslandEstatePrecisionSport::Next()
{
 if(Stage!=EIslandPrecisionStage::Result)return;
 if(Kind==EIslandPrecisionSport::Bowling)
 {
  if(ThrowInFrame>=2 || FrameFallen==10){++Frame;if(Frame>=3){Stage=EIslandPrecisionStage::Finished;Notice(GetSessionText());return;}ThrowInFrame=FrameFallen=0;ResetPins();}
  else for(int32 I=0;I<Pins.Num();++I)if(IsPinFallen(I)){RemovedPins.Add(I);Pins[I]->SetSimulatePhysics(false);Pins[I]->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pins[I]->SetVisibility(false);}else Pins[I]->SetSimulatePhysics(false);
  ResetBall(FVector(30,0,0));Stage=EIslandPrecisionStage::Aiming;
 }
 else if(bHoleCaptured){++Hole;if(Hole>=3){Stage=EIslandPrecisionStage::Finished;Notice(GetSessionText());return;}ResetBall(TeePositions[Hole]);SetAddressed(false);Stage=EIslandPrecisionStage::Aiming;}
 else{ResetBall(LastShotLocal);SetAddressed(false);Stage=EIslandPrecisionStage::Aiming;}
}
void AIslandEstatePrecisionSport::Restart()
{if(!IsPlaying())return;Frame=ThrowInFrame=FrameFallen=Hole=Score=Shots=CompletedHoles=0;ResetPins();ResetBall(Kind==EIslandPrecisionSport::Bowling?FVector(30,0,0):TeePositions[0]);Stage=EIslandPrecisionStage::Aiming;if(Kind==EIslandPrecisionSport::Putting)SetAddressed(false);RollAudio->Stop();}
void AIslandEstatePrecisionSport::Tick(float Delta)
{
 Super::Tick(Delta);auto* P=ActivePlayer.Get();auto* PC=ActiveController.Get();if(!IsPlaying())return;
 if(!P || !PC || PC->GetPawn()!=P || P->GetHealthComponent()->IsDead() || (Kind==EIslandPrecisionSport::Bowling?FVector::DistSquared(P->GetActorLocation(),StartPlayerPosition)>FMath::Square(65.f):GetActorTransform().InverseTransformPosition(P->GetActorLocation()).SizeSquared2D()>FMath::Square(850.f)) || P->GetCharacterMovement()->IsFalling() || P->GetCharacterMovement()->IsSwimming())
 {
  UE_LOG(LogTemp,Display,TEXT("Precision707 automatic exit: station=%s stage=%d shots=%d cups=%d player=%s controller=%s possession=%d dead=%d mode=%d position=%s start=%s ignored=%d"),*StationID.ToString(),int(Stage),Shots,CompletedHoles,*GetNameSafe(P),*GetNameSafe(PC),P && PC && PC->GetPawn()==P,P && P->GetHealthComponent()->IsDead(),P?int(P->GetCharacterMovement()->MovementMode):-1,P?*P->GetActorLocation().ToString():TEXT("none"),*StartPlayerPosition.ToString(),PC && PC->IsMoveInputIgnored());
  FinishSession();return;
 }
 SessionSeconds+=Delta;if(SessionSeconds>240){FinishSession();return;}
 if(Stage==EIslandPrecisionStage::Charging)Charge=FMath::Clamp(Charge+Delta/ChargeSeconds,0.f,1.f);
 if(Stage==EIslandPrecisionStage::Swing){SwingSeconds+=Delta;if(SwingSeconds>=.38f && bShotLaunched)Stage=EIslandPrecisionStage::Rolling;}
 if(Stage==EIslandPrecisionStage::Rolling)
 {
  RollingSeconds+=Delta;const FVector Local=GetActorTransform().InverseTransformPosition(Ball->GetComponentLocation());const float Speed=Ball->GetPhysicsLinearVelocity().Size();
  if(Kind==EIslandPrecisionSport::Putting)
  {
   const FVector Cup=HolePositions[Hole];bHoleCaptured=FVector::DistSquared2D(Local,Cup)<FMath::Square(5.4f-Ball->GetUnscaledSphereRadius()*.4f) && Local.Z<-.75f;
   bOutOfBounds=Local.SizeSquared2D()>FMath::Square(755.f) || Local.Z<-75;
   if(bHoleCaptured){ResolveRoll();UpdatePresentation(Delta);return;}
  }
  else bOutOfBounds=Local.X>2200 || FMath::Abs(Local.Y)>110 || Local.Z<-100;
  bool Still=Speed<5 && Ball->GetPhysicsAngularVelocityInRadians().Size()<2;
  if(Kind==EIslandPrecisionSport::Bowling)for(int32 I=0;I<Pins.Num();++I)if(!RemovedPins.Contains(I) && (Pins[I]->GetPhysicsLinearVelocity().Size()>7 || Pins[I]->GetPhysicsAngularVelocityInRadians().Size()>.5f))Still=false;
  RestSeconds=Still?RestSeconds+Delta:0;
  if(RestSeconds>.8f || bOutOfBounds || RollingSeconds>18)ResolveRoll();
  else if(Speed>10 && RollingSound){if(RollingSound->GetAvailableAudioByteCount()<2400)RollingSound->QueueAudio(reinterpret_cast<const uint8*>(RollingPCM.GetData()),RollingPCM.Num()*sizeof(int16));RollAudio->SetVolumeMultiplier(FMath::Clamp(Speed/1200,0.f,.23f));RollAudio->SetPitchMultiplier(.75f+FMath::Clamp(Speed/1000,0.f,1.f));if(!RollAudio->IsPlaying())RollAudio->Play();}
 }
 UpdatePresentation(Delta);
}
FVector AIslandEstatePrecisionSport::GetShotStandLocation()const
{
 const FVector Direction=GetActorTransform().TransformVectorNoScale(FRotator(0,AimDegrees,0).Vector());
 const FVector Beside=FVector::CrossProduct(FVector::UpVector,Direction);
 const float Height=ActivePlayer.IsValid()?ActivePlayer->GetSimpleCollisionHalfHeight():92.f;
 const FVector Ground=GetActorTransform().TransformPosition(FVector(LastShotLocal.X,LastShotLocal.Y,0));
 return Ground-Beside*35-Direction*18+FVector(0,0,Height);
}
void AIslandEstatePrecisionSport::SetAddressed(bool Addressed)
{
 auto* P=ActivePlayer.Get();auto* PC=ActiveController.Get();if(!P || !PC)return;
 if(Addressed)
 {
  if(!bIgnoredMove){PC->SetIgnoreMoveInput(true);bIgnoredMove=true;}
  P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->bOrientRotationToMovement=false;P->GetCharacterMovement()->bUseControllerDesiredRotation=false;P->bUseControllerRotationYaw=false;
  P->SetActorRotation(FRotator(0,GetActorRotation().Yaw+(Kind==EIslandPrecisionSport::Putting?AimDegrees+90:0),0));
  P->GetMesh()->SetOwnerNoSee(false);PC->SetViewTarget(this);
 }
 else
 {
  if(bIgnoredMove){PC->SetIgnoreMoveInput(false);bIgnoredMove=false;}
  P->GetCharacterMovement()->bOrientRotationToMovement=bOrientBefore;P->GetCharacterMovement()->bUseControllerDesiredRotation=bDesiredRotationBefore;P->bUseControllerRotationYaw=bControllerYawBefore;
  P->GetMesh()->SetOwnerNoSee(bOwnerNoSeeBefore);
  if(PC->GetViewTarget()==this)PC->SetViewTarget(PC->GetPawn()!=P?PC->GetPawn():PreviousViewTarget.IsValid()?PreviousViewTarget.Get():P);
 }
}
bool AIslandEstatePrecisionSport::HasHandPose()const
{
 return ActivePlayer.IsValid() && bIgnoredMove && (Stage==EIslandPrecisionStage::Aiming || Stage==EIslandPrecisionStage::Charging || Stage==EIslandPrecisionStage::Swing);
}
FTransform AIslandEstatePrecisionSport::DesiredPropTransform()const
{
 auto* P=ActivePlayer.Get();if(!P)return FTransform::Identity;
 if(Kind==EIslandPrecisionSport::Bowling)
 {
  const FVector Ready(32,22,10),Back(-22,25,0),Released(50,22,-5);
  FVector Centre=Stage==EIslandPrecisionStage::Charging?FMath::Lerp(Ready,Back,Charge):Stage==EIslandPrecisionStage::Swing?FMath::Lerp(FMath::Lerp(Ready,Back,Charge),Released,FMath::SmoothStep(0.f,1.f,FMath::Clamp(SwingSeconds/.18f,0.f,1.f))):Ready;
  return FTransform(GetActorQuat(),P->GetActorTransform().TransformPosition(Centre));
 }
 const float ImpactBack=5.25f+Ball->GetUnscaledSphereRadius();
 const float Back=Stage==EIslandPrecisionStage::Charging?8+20*Charge:Stage==EIslandPrecisionStage::Swing?(SwingSeconds<=.18f?FMath::Lerp(8+20*Charge,ImpactBack,FMath::Clamp(SwingSeconds/.18f,0.f,1.f)):FMath::Lerp(ImpactBack,-12.f,FMath::Clamp((SwingSeconds-.18f)/.2f,0.f,1.f))):8.f;
 const FVector Direction=GetActorTransform().TransformVectorNoScale(FRotator(0,AimDegrees,0).Vector());
 const FVector Head=GetActorTransform().TransformPosition(FVector(LastShotLocal.X,LastShotLocal.Y,0))-Direction*Back;
 return FTransform(FRotator(0,GetActorRotation().Yaw+AimDegrees,0).Quaternion(),Head);
}
bool AIslandEstatePrecisionSport::HandTargets(FTransform& Right,FTransform& Left)const
{
 if(!HasHandPose())return false;const auto* P=ActivePlayer.Get();const FTransform Prop=DesiredPropTransform();
 const FQuat Wrist=P->GetActorQuat()*P->GetMesh()->GetRelativeRotation().Quaternion();
 FVector R=Prop.TransformPosition(Kind==EIslandPrecisionSport::Bowling?FVector(2.6,-1.5,10.1):FVector(-17.2,0,82));
 FVector L=Prop.TransformPosition(Kind==EIslandPrecisionSport::Bowling?FVector(0,-9.5,0):FVector(-18,0,87));
 // Targets are finite and within the existing rig's reach. Feet are never
 // translated; the presentation graph supplies a small bounded torso lean.
 const auto BodyFrame=P->GetActorTransform();const FVector ShoulderR(12,22,46),ShoulderL(12,-22,46);
 FVector Local=BodyFrame.InverseTransformPosition(R);R=BodyFrame.TransformPosition(ShoulderR+(Local-ShoulderR).GetClampedToMaxSize(63));
 Local=BodyFrame.InverseTransformPosition(L);L=BodyFrame.TransformPosition(ShoulderL+(Local-ShoulderL).GetClampedToMaxSize(63));
 if(Kind==EIslandPrecisionSport::Bowling && Stage==EIslandPrecisionStage::Swing)L=BodyFrame.TransformPosition(FMath::Lerp(BodyFrame.InverseTransformPosition(L),FVector(5,-40,20),FMath::Clamp(SwingSeconds/.18f,0.f,1.f)));
 Right=FTransform(Wrist,R);Left=FTransform(Wrist,L);return true;
}
void AIslandEstatePrecisionSport::FollowActualHand()
{
 FTransform Right,Left;if(!HandTargets(Right,Left) || !ActivePlayer.IsValid())return;
 const auto* Mesh=ActivePlayer->GetMesh();if(!Mesh->DoesSocketExist(TEXT("hand_r")))return;
 const FTransform Desired=DesiredPropTransform(),Hand=Mesh->GetSocketTransform(TEXT("hand_r"));
 // Marker coordinates belong to the authored source mesh, in centimetres.
 // Fit the real grip to the evaluated hand, rather than pulling a hand toward
 // a floating prop or updating any released rigid body's trajectory.
 const FVector Grip=Kind==EIslandPrecisionSport::Bowling?FVector(2.6,-1.5,10.1):FVector(-17.2,0,82);
 const FQuat Rotation=Hand.GetRotation()*Right.GetRotation().Inverse()*Desired.GetRotation();
 const FTransform Actual(Rotation,Hand.GetLocation()-Rotation.RotateVector(Grip));
 if(Kind==EIslandPrecisionSport::Bowling){if(!bShotLaunched)Ball->SetWorldTransform(Actual,false,nullptr,ETeleportType::TeleportPhysics);}
 else if(HasHandPose())Putter->SetWorldTransform(Actual);
}
void AIslandEstatePrecisionSport::UpdatePresentation(float Delta)
{
 const FVector Local=GetActorTransform().InverseTransformPosition(Ball->GetComponentLocation());const FVector Direction=FRotator(0,AimDegrees,0).Vector();
 if(Kind==EIslandPrecisionSport::Putting)
 {
  Putter->SetVisibility(HasHandPose());
  if(bIgnoredMove && ActivePlayer.IsValid())
  {
   const FVector PlayerLocal=GetActorTransform().InverseTransformPosition(ActivePlayer->GetActorLocation());
   const FVector Eye=PlayerLocal-Direction*140+FVector(-Direction.Y,Direction.X,0)*-130+FVector(0,0,65);
   ActivityCamera->SetRelativeLocation(Eye);ActivityCamera->SetRelativeRotation((FVector(LastShotLocal.X,LastShotLocal.Y,55)-Eye).Rotation());
  }
 }
 else
 {
  const float Follow=Stage==EIslandPrecisionStage::Rolling?FMath::Clamp(Local.X*.35f,0.f,500.f):0;ActivityCamera->SetRelativeLocation(FVector(-240+Follow,-160,170));ActivityCamera->SetRelativeRotation((FVector(100+Follow,0,75)-ActivityCamera->GetRelativeLocation()).Rotation());
 }
 if(Widget)Widget->Present(GetSessionText(),Charge,Stage==EIslandPrecisionStage::Charging);
}
FText AIslandEstatePrecisionSport::GetSessionText()const
{
 FString S=Kind==EIslandPrecisionSport::Bowling?FString::Printf(TEXT("PRIVATE BOWLING | 3-frame practice (pins only, no bonus)\nFrame %d/3 | Ball %d/2 | Pins %d | Total %d"),FMath::Min(Frame+1,3),FMath::Min(ThrowInFrame+1,2),GetFallenPins(),Score):FString::Printf(TEXT("PRIVATE PUTTING | 3 real cups\nHole %d/3 | Complete %d | Strokes %d"),FMath::Min(Hole+1,3),CompletedHoles,Shots);
 if(Stage==EIslandPrecisionStage::Finished)S+=TEXT("\nPractice complete | R restart | F leave");
 else if(Stage==EIslandPrecisionStage::Result)S+=TEXT("\nEnter continue | R restart | F leave");
 else if(Stage==EIslandPrecisionStage::Rolling || Stage==EIslandPrecisionStage::Swing)S+=TEXT("\nBall in play | F leave");
 else if(Kind==EIslandPrecisionSport::Putting && !bIgnoredMove)S+=FString::Printf(TEXT("\nWASD approach the ball (stance %.2fm) | LMB hold/release\nR restart | F leave"),ActivePlayer.IsValid()?FVector::Dist2D(ActivePlayer->GetActorLocation(),GetShotStandLocation())/100.f:0.f);
 else S+=FString::Printf(TEXT("\nMouse / arrows aim (%+.1f deg) | Hold/release LMB\nR restart | F leave"),AimDegrees);
 return FText::FromString(S);
}
void AIslandEstatePrecisionSport::PrepareAudio()
{
 const int32 Rate=24000;FRandomStream Random(707);
 for(int32 I=0;I<Rate*.36f;++I){const float T=float(I)/Rate;const float Noise=Random.FRandRange(-1.f,1.f);const float Wood=FMath::Sin(T*2*PI*185)*FMath::Exp(-T*19)+.55f*FMath::Sin(T*2*PI*410)*FMath::Exp(-T*28);ImpactPCM.Add(int16(FMath::Clamp((Wood*.19f+Noise*.14f*FMath::Exp(-T*46))*28000.f,-32700.f,32700.f)));}
 for(int32 I=0;I<Rate/2;++I){const float T=float(I)/Rate;RollingPCM.Add(int16((Random.FRandRange(-1.f,1.f)*.045f+FMath::Sin(T*2*PI*72)*.025f)*20000));}
 auto Make=[&](){auto* S=NewObject<USoundWaveProcedural>(this);S->SetSampleRate(Rate);S->NumChannels=1;S->Duration=INDEFINITELY_LOOPING_DURATION;S->bLooping=false;return S;};
 ImpactSound=Make();RollingSound=Make();ImpactAudio->SetSound(ImpactSound);ImpactAudio->SetVolumeMultiplier(.32f);RollAudio->SetSound(RollingSound);
}
void AIslandEstatePrecisionSport::PhysicsHit(UPrimitiveComponent* C,AActor* Other,UPrimitiveComponent* OtherC,FVector Impulse,const FHitResult& Hit)
{
 if(!IsPlaying() || !bShotLaunched || Impulse.Size()<2)return;++PhysicsHits;
 if(GetWorld()->GetTimeSeconds()-LastHitAt<.045f || !ImpactSound)return;LastHitAt=GetWorld()->GetTimeSeconds();ImpactAudio->SetWorldLocation(Hit.ImpactPoint);ImpactSound->ResetAudio();ImpactSound->QueueAudio(reinterpret_cast<const uint8*>(ImpactPCM.GetData()),ImpactPCM.Num()*sizeof(int16));ImpactAudio->SetPitchMultiplier(Kind==EIslandPrecisionSport::Putting?1.9f:FMath::Clamp(.8f+Impulse.Size()/12000,.8f,1.25f));ImpactAudio->Play();
}
bool AIslandEstatePrecisionSport::IsFeedbackPlaying()const{return ImpactAudio->IsPlaying() || RollAudio->IsPlaying();}
void AIslandEstatePrecisionSport::Notice(const FText& Text)const
{if(auto* PC=ActiveController.Get())if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(Text,5);}
