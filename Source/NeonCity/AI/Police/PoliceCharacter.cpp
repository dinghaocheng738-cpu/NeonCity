#include "AI/Police/PoliceCharacter.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatAIData.h"
#include "Wanted/SearchSystem.h"
#include "Physics/CharacterImpactComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "AI/Police/PoliceController.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "Animation/AnimSequence.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
APoliceCharacter::APoliceCharacter()
{
 CreateDefaultSubobject<UCharacterImpactComponent>(TEXT("PhysicalReaction"));
 CombatAI=CreateDefaultSubobject<UNeonCityCombatAIComponent>(TEXT("CombatAI"));
 PrimaryActorTick.bCanEverTick=true;
 CreateDefaultSubobject<UCombatTargetComponent>(TEXT("CombatIdentity"))->Faction=ENeonCityFaction::Police;
 AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
 AIControllerClass=APoliceController::StaticClass();
 CreateDefaultSubobject<UCharacterWeaponPresentationComponent>(TEXT("WeaponPresentation"));
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
 BodyState=CreateDefaultSubobject<UDamageableCharacterComponent>(TEXT("BodyState"));
 DamageFeedback=CreateDefaultSubobject<UCharacterDamageFeedbackComponent>(TEXT("DamageFeedback"));
 Sensor=CreateDefaultSubobject<UPoliceSensorComponent>(TEXT("PoliceVision"));
 GetCapsuleComponent()->InitCapsuleSize(34,90);
 GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));
 GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 bUseControllerRotationYaw=false;
 GetCharacterMovement()->bOrientRotationToMovement=true;
 GetCharacterMovement()->RotationRate=FRotator(0,300,0);
 GetCharacterMovement()->MaxWalkSpeed=150;
 GetCharacterMovement()->bEnablePhysicsInteraction=false;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> OfficerMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
 if (OfficerMesh.Succeeded()) GetMesh()->SetSkeletalMesh(OfficerMesh.Object);
 if(FPackageName::DoesPackageExist(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_Character_NPC")))
 {
  static ConstructorHelpers::FClassFinder<UAnimInstance> Presentation(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_Character_NPC"));
  if(Presentation.Succeeded()) GetMesh()->SetAnimInstanceClass(Presentation.Class);
 }
 Idle=IdleAsset.Object; Walk=WalkAsset.Object;
 UTextRenderComponent* Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("OfficerLabel"));
 Label->SetupAttachment(GetRootComponent()); Label->SetRelativeLocation(FVector(0,0,115));
 Label->SetText(FText::FromString(TEXT("NEON CITY POLICE"))); Label->SetTextRenderColor(FColor(70,160,255));
 Label->SetWorldSize(14); Label->SetHorizontalAlignment(EHTA_Center);
}
void APoliceCharacter::BeginPlay()
{
 Super::BeginPlay(); if(!Cast<UCharacterPresentationAnimInstance>(GetMesh()->GetAnimInstance())) GetMesh()->PlayAnimation(Idle,true);
 auto* Profile=NewObject<UCombatAIData>(this); Profile->Role=ENeonCityCombatRole::Security; Profile->CanFight=true;
 Profile->Faction=ENeonCityFaction::Police; Profile->RequiresArmedThreat=true; Profile->PlayerResponseOnly=true; Profile->AllowChase=false;
 Profile->ReactionTime=1; Profile->Accuracy=.45f; Profile->MaximumCombatSeconds=45;
 CombatAI->Configure(Profile,GetUniqueID());
}
EPoliceForceState APoliceCharacter::GetUseOfForce() const
{
 if(CombatAI->ControlsLocomotion()) return CombatAI->GetState()==ENeonCityCombatAIState::AcquireTarget?EPoliceForceState::Threatened:EPoliceForceState::Combat;
 const auto* S=GetWorld()->GetSubsystem<USearchSystem>();
 return !bResponseEnabled || !S->HasLocation()?EPoliceForceState::Observe:S->HasVisualContact()?EPoliceForceState::Pursue:EPoliceForceState::Investigate;
}
void APoliceCharacter::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(Cast<UCharacterPresentationAnimInstance>(GetMesh()->GetAnimInstance())) return;
 const float Speed=GetVelocity().Size2D(); const bool bNow=Speed>8;
 if (bNow!=bWalking) { bWalking=bNow; GetMesh()->PlayAnimation(bNow?Walk:Idle,true); }
 if (bNow && GetMesh()->GetSingleNodeInstance()) GetMesh()->GetSingleNodeInstance()->SetPlayRate(FMath::Clamp(Speed/150.f,0.4f,2.f));
}
bool APoliceCharacter::CanRespond() const { return !Health->IsDead() && BodyState->CanAct(); }
bool APoliceCharacter::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float APoliceCharacter::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState APoliceCharacter::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
