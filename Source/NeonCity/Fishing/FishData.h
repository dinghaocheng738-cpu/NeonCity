#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FishData.generated.h"
class UStaticMesh;

UENUM(BlueprintType)
enum class ENeonCityFishRarity : uint8 { Common, Uncommon, Rare };
UENUM(BlueprintType)
enum class ENeonCityFishTime : uint8 { Any, Day, Dusk, Night };
UENUM(BlueprintType)
enum class ENeonCityFishStorage : uint8 { Pouch, Aquarium, Pen, Grill };

UENUM(BlueprintType)
enum class ENeonCityGrillStage : uint8 { Idle, Opening, Igniting, Placing, CookingFirst, Turning, CookingSecond, Plating, Ready, Closing };

/** Fictional coastal species. Assets define presentation and game ecology independently of catches. */
UCLASS(BlueprintType)
class NEONCITY_API UFishData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName SpeciesID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Name;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityFishRarity Rarity=ENeonCityFishRarity::Common;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName Habitat=TEXT("Coastal");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityFishTime TimePreference=ENeonCityFishTime::Any;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName WeatherPreference=TEXT("Any");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D WeightKg=FVector2D(.25,1.5);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D SizeCm=FVector2D(15,35);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 ValuePerKg=30;
 // Source fight parameters; independent from cosmetic rarity or inventory state.
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float FightStrength=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0.1",ClampMax="600")) float FightStaminaSeconds=4;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool SuitableForAquarium=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool SuitableForPen=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool SaltwaterSpecies=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> Mesh;
 bool IsValidProfile() const;
 float EncounterWeight(FName AtHabitat,float TimeOfDay,FName Weather) const;
};

USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityCaughtFish
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid FishID;
 UPROPERTY(SaveGame,BlueprintReadOnly) FName SpeciesID;
 UPROPERTY(SaveGame,BlueprintReadOnly) float WeightKg=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) float SizeCm=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) FDateTime CaughtAt;
 UPROPERTY(SaveGame,BlueprintReadOnly) FName FishingPointID;
 UPROPERTY(SaveGame,BlueprintReadOnly) ENeonCityFishStorage Storage=ENeonCityFishStorage::Pouch;
 UPROPERTY(SaveGame,BlueprintReadOnly) FName ContainerID;
 // Additive V1 fields. A freshly landed catch remains alive until cooking or an
 // explicit mortality transition; moving containers never generates a new fish.
 UPROPERTY(SaveGame,BlueprintReadOnly) bool bAlive=true;
 UPROPERTY(SaveGame,BlueprintReadOnly) ENeonCityGrillStage GrillStage=ENeonCityGrillStage::Idle;
 UPROPERTY(SaveGame,BlueprintReadOnly) float GrillStageSeconds=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) float CookFraction=0;
};

USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityFishMeal
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid MealID;
 UPROPERTY(SaveGame,BlueprintReadOnly) FName RecipeID=TEXT("Food.GrilledFish");
 UPROPERTY(SaveGame,BlueprintReadOnly) FName SpeciesID;
 UPROPERTY(SaveGame,BlueprintReadOnly) FGuid SourceFishID;
 UPROPERTY(SaveGame,BlueprintReadOnly) float SourceWeightKg=0;
 UPROPERTY(SaveGame,BlueprintReadOnly) float SourceSizeCm=0;
};

USTRUCT()
struct NEONCITY_API FNeonCityFishingSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) TArray<FNeonCityCaughtFish> Fish;
 UPROPERTY(SaveGame) TArray<FNeonCityFishMeal> Meals;
};
