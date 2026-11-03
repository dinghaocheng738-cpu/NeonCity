#include "AI/NeonCityNPCCharacter.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "Physics/CharacterImpactComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Combat/CombatCollision.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "Combat/DamageableCharacterComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "AI/NeonCityNPCController.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NPCData.h"
#include "AI/NPCThreatComponent.h"
#include "AI/NPCStateComponent.h"
#include "Animation/AnimSequence.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

ANeonCityNPCCharacter::ANeonCityNPCCharacter()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
 CreateDefaultSubobject<UCharacterWeaponPresentationComponent>(TEXT("WeaponPresentation"));
	DamageHealth=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 CombatAI=CreateDefaultSubobject<UNeonCityCombatAIComponent>(TEXT("CombatAI"));
 CreateDefaultSubobject<UCombatTargetComponent>(TEXT("CombatIdentity"));
 CreateDefaultSubobject<UCharacterAppearanceComponent>(TEXT("Appearance"));
	BodyState=CreateDefaultSubobject<UDamageableCharacterComponent>(TEXT("BodyState"));
	DamageFeedback=CreateDefaultSubobject<UCharacterDamageFeedbackComponent>(TEXT("DamageFeedback"));
 CreateDefaultSubobject<UCharacterImpactComponent>(TEXT("PhysicalReaction"));
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ANeonCityNPCController::StaticClass();
	GetCapsuleComponent()->InitCapsuleSize(34.f,90.f);
	GetMesh()->SetRelativeLocation(FVector(0,0,-90));
	GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0,240,0);
	GetCharacterMovement()->MaxWalkSpeed = 140.f;
	GetCharacterMovement()->MaxAcceleration = 700.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 900.f;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	Threat=CreateDefaultSubobject<UNPCThreatComponent>(TEXT("ThreatPerception"));
	State = CreateDefaultSubobject<UNPCStateComponent>(TEXT("NPCState"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> NPCMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
	if (NPCMesh.Succeeded()) { GetMesh()->SetSkeletalMesh(NPCMesh.Object); }
 if(FPackageName::DoesPackageExist(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_Character_NPC")))
 {
  static ConstructorHelpers::FClassFinder<UAnimInstance> Presentation(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_Character_NPC"));
  if(Presentation.Succeeded()) GetMesh()->SetAnimInstanceClass(Presentation.Class);
 }
	IdleAnimation = Idle.Object;
	WalkAnimation = Walk.Object;
}
void ANeonCityNPCCharacter::InitializePedestrian(FName Id, UNPCData* Profile, const TArray<FVector>& Points, int32 Seed, APedestrianSpawnPoint* SpawnPoint)
{
	NPCId = Id; Data = Profile; Route = Points; BehaviorSeed = Seed; SourceSpawnPoint=SpawnPoint;
 CombatAI->Configure(GetNPCData()->CombatProfile,Seed);
 // Spawn points may assign the stable identity after BeginPlay.
 if(HasActorBegunPlay()) if(auto* Appearance=FindComponentByClass<UCharacterAppearanceComponent>()) Appearance->RefreshAppearance();
}
APedestrianSpawnPoint* ANeonCityNPCCharacter::GetSpawnPoint() const { return SourceSpawnPoint.Get(); }
const UNPCData* ANeonCityNPCCharacter::GetNPCData() const { return Data ? Data.Get() : GetDefault<UNPCData>(); }
void ANeonCityNPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	const float Speed = GetNPCData()->WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeed = FMath::IsFinite(Speed) ? FMath::Clamp(Speed,60.f,220.f) : 140.f;
	if(!Cast<UCharacterPresentationAnimInstance>(GetMesh()->GetAnimInstance())) GetMesh()->PlayAnimation(IdleAnimation, true);
}
void ANeonCityNPCCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 if(Cast<UCharacterPresentationAnimInstance>(GetMesh()->GetAnimInstance())) return;
	const float Speed = GetVelocity().Size2D();
	const bool bWalk = Speed > 8.f;
	if (bWalk != bPlayingWalk)
	{
		bPlayingWalk = bWalk;
		GetMesh()->PlayAnimation(bWalk ? WalkAnimation : IdleAnimation, true);
	}
	if (bWalk && GetMesh()->GetSingleNodeInstance())
	{
		GetMesh()->GetSingleNodeInstance()->SetPlayRate(FMath::Clamp(Speed/150.f,0.4f,2.f));
	}
}

bool ANeonCityNPCCharacter::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ANeonCityNPCCharacter::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityNPCCharacter::GetCombatState_Implementation() const { return Combat->GetCombatState(); }

bool ANeonCityNPCCharacter::CanSuspendPopulation() const
{
 if(Threat && !Threat->CanSuspendPopulation()) return false;
 if(CombatAI && CombatAI->ControlsLocomotion()) return false;
 if(BodyState && !BodyState->CanAct()) return false;
 if(auto* Impact=FindComponentByClass<UCharacterImpactComponent>();Impact && Impact->GetState()!=ECharacterImpactState::Normal) return false;
 const auto S=State->GetState();
 return S==ENeonCityNPCState::Idle || S==ENeonCityNPCState::Walk || S==ENeonCityNPCState::Return;
}

bool ANeonCityNPCCharacter::UsesPopulationBehaviourBudget() const
{
 // Incapacitated bodies retain their identity, physics and collision lifecycle.
 // They are not twenty permanently reserved pedestrian behaviour slots.
 return !bPopulationDormant && (!BodyState || BodyState->CanAct());
}

void ANeonCityNPCCharacter::SetPopulationDormant(bool Dormant)
{
 if(Dormant==bPopulationDormant || (Dormant && !CanSuspendPopulation())) return;
 if(!Dormant && bSavedCollision)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(PedestrianWakeClearance),false,this);
  const auto* Capsule=GetCapsuleComponent();
  if(GetWorld()->OverlapBlockingTestByChannel(GetActorLocation(),GetActorQuat(),ECC_Pawn,
   FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2),Q)) return;
 }
 bPopulationDormant=Dormant;
 if(Dormant)
 {
  bSavedActorTick=IsActorTickEnabled();bSavedCollision=GetActorEnableCollision();bSavedHidden=IsHidden();
  SuspendedController=GetController();bSavedControllerTick=SuspendedController.IsValid() && SuspendedController->IsActorTickEnabled();
  TInlineComponentArray<UActorComponent*> Components(this);
  if(SuspendedController.IsValid())
  {
   TInlineComponentArray<UActorComponent*> ControllerComponents(SuspendedController.Get());
   Components.Append(ControllerComponents);
  }
  for(auto* C:Components) {SuspendedTicks.Add({C,C->IsComponentTickEnabled()});C->SetComponentTickEnabled(false);}
  SetActorTickEnabled(false);if(SuspendedController.IsValid()) SuspendedController->SetActorTickEnabled(false);
  SetActorEnableCollision(false);SetActorHiddenInGame(true);
 }
 else
 {
  SetActorHiddenInGame(bSavedHidden);SetActorEnableCollision(bSavedCollision);SetActorTickEnabled(bSavedActorTick);
  for(const auto& S:SuspendedTicks) if(S.Component.IsValid()) S.Component->SetComponentTickEnabled(S.Enabled);
  if(SuspendedController.IsValid() && GetController()==SuspendedController.Get()) SuspendedController->SetActorTickEnabled(bSavedControllerTick);
  SuspendedTicks.Reset();SuspendedController.Reset();
 }
}
