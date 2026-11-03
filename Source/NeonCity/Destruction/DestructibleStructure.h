#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Destruction/DestructionInterfaces.h"
#include "Explosion/ExplosionResponseInterface.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "DestructibleStructure.generated.h"
class UStaticMeshComponent;
/** Bounded localized building destruction: a stable shell plus authored breakable surfaces. */
UCLASS()
class NEONCITY_API ADestructibleStructure : public AActor, public ILocalizedDestructionInterface, public IExplosionResponseInterface
{
 GENERATED_BODY()
public:
 ADestructibleStructure();
 /** Shell scale in cm. The shell is never destructible; only authored parts are. */
 void Configure(FName InStructureID,FVector ShellExtent,bool bGlassStorefront=false);
 /** One authored breakable surface in structure-local space. */
 ANeonCityPhysicsProp* AddPart(ECityPhysicsProp Type,FVector LocalLocation,FRotator LocalRotation,float Health=100);
 virtual int32 ApplyLocalizedDestruction_Implementation(FVector Center,float Radius,float Damage,AActor* Instigator) override;
 virtual int32 GetIntactSurfaceCount_Implementation() const override;
 virtual bool ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event) override;
 UStaticMeshComponent* GetShell() const { return Shell; }
 const TArray<TObjectPtr<ANeonCityPhysicsProp>>& GetParts() const { return Parts; }
 FName GetStructureID() const { return StructureID; }
 int32 GetLocalizedBreakCount() const { return LocalizedBreaks; }
 int32 GetLocalizedDamageCount() const { return LocalizedDamages; }
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Shell;
 UPROPERTY() TArray<TObjectPtr<ANeonCityPhysicsProp>> Parts;
 FName StructureID=TEXT("NeonCity.Structure");
 int32 LocalizedBreaks=0,LocalizedDamages=0;
};