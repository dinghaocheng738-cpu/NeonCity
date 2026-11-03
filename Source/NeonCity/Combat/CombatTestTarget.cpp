#include "Combat/CombatTestTarget.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
ACombatTestTarget::ACombatTestTarget()
{
	Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetBody")); SetRootComponent(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral"));
	Body->SetStaticMesh(Cube.Object); Body->SetMaterial(0,Paint.Object);
	Body->SetRelativeScale3D(FVector(.3,1.2,1.8)); Body->SetCollisionProfileName(TEXT("BlockAll"));
	Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetLabel")); Label->SetupAttachment(Body);
	Label->SetAbsolute(false,false,true); Label->SetRelativeLocation(FVector(65,0,70));
	Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(24);
}
void ACombatTestTarget::BeginPlay()
{
	Super::BeginPlay(); Health->OnHealthChanged.AddDynamic(this,&ACombatTestTarget::UpdateHealth);
	UpdateHealth(Health->GetCurrentHealth(),Health->GetMaxHealth());
}
void ACombatTestTarget::UpdateHealth(float Current,float Maximum)
{
	Label->SetText(FText::FromString(FString::Printf(TEXT("COMBAT TEST\nHP %.0f / %.0f%s"),Current,Maximum,Health->IsDead() ? TEXT("\nDEAD") : TEXT(""))));
	Label->SetTextRenderColor(Health->IsDead() ? FColor::Red : Current<Maximum ? FColor::Yellow : FColor::White);
}
bool ACombatTestTarget::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ACombatTestTarget::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ACombatTestTarget::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
