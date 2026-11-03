#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Adventure/MarineSchool.h"
#include "OceanWildlifeCatalog.generated.h"
class UFishData;

USTRUCT(BlueprintType)
struct FOceanWildlifeProfile
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TObjectPtr<UFishData> Species;
 UPROPERTY(EditAnywhere) EMarineBehaviour Behaviour=EMarineBehaviour::School;
 UPROPERTY(EditAnywhere) FVector2D SeabedDepthMetres=FVector2D(1,30);
 UPROPERTY(EditAnywhere) int32 GroupSize=8;
 UPROPERTY(EditAnywhere) float Density=1;
 UPROPERTY(EditAnywhere) float Speed=45;
 UPROPERTY(EditAnywhere) float Clearance=40;
 UPROPERTY(EditAnywhere) float Frequency=2;
 UPROPERTY(EditAnywhere) float Amplitude=.035f;
 UPROPERTY(EditAnywhere) bool bReefAffinity=false;
 UPROPERTY(EditAnywhere) bool bNearBottom=false;
 UPROPERTY(EditAnywhere) bool bCapturable=true;
 UPROPERTY(EditAnywhere) bool bSideways=false;
};

/** The native models, fishing profiles and spatial population use one species ID. */
UCLASS(BlueprintType)
class NEONCITY_API UOceanWildlifeCatalog : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) TArray<FOceanWildlifeProfile> Profiles;
 UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UStaticMesh>> ReefRocks;
 UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UStaticMesh>> ReefCorals;
 UPROPERTY(EditAnywhere) int32 ActiveSchoolBudget=24;
 UPROPERTY(EditAnywhere) int32 ActiveCreatureBudget=260;
 UPROPERTY(EditAnywhere) float ZoneSize=4200;
};
