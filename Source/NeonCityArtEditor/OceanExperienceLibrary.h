#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OceanExperienceLibrary.generated.h"
class UNiagaraSystem;
class UMaterialInterface;
class USkeletalMesh;
UCLASS()
class NEONCITYARTEDITOR_API UOceanExperienceLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UNiagaraSystem* CreateRegulatorExhaust(UMaterialInterface* Material);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UNiagaraSystem* CreateBowContactSpray(UMaterialInterface* Material);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UNiagaraSystem* CreateMarineSprayLayer(UMaterialInterface* Material,int32 Kind);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UNiagaraSystem* CreateDPVWash(UMaterialInterface* Material);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static UNiagaraSystem* CreateMarineSnow(UMaterialInterface* Material);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool AuthorRegulatorSocket(USkeletalMesh* Suit);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Ocean") static bool ConfigureDrysuitLods(USkeletalMesh* Suit);
};
