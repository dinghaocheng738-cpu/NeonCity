#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NeonCityMapData.generated.h"
class UTexture2D;

USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityMapPlace
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PlaceID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Name;
 /** home, landmark, temple, scenic, area */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName Kind;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D Location=FVector2D::ZeroVector;
};

/** Surface classes of the route grid (LakeConcept/wl_map.py). */
enum class ENeonCityMapCell:uint8 { Blocked=0, Road=1, Path=2, Open=3, Rough=4 };

/** A painted top-down map of an area with its walk / drive route grid and named places
 * (authored by Tools/Art/Phase205cUpgrade/LandRebuild/MapAsset_WL.py). World units are centimetres. */
UCLASS(BlueprintType)
class NEONCITY_API UNeonCityMapData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Title;
 /** North (world +Y) up, east (world -X) right. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UTexture2D> Texture;
 /** World XY covered by the texture (cm): X runs right-to-left, Y bottom-to-top. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D WorldMin=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D WorldMax=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere) FVector2D GridOrigin=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere) float GridCell=200;
 UPROPERTY(EditAnywhere) int32 GridWidth=0;
 UPROPERTY(EditAnywhere) int32 GridHeight=0;
 /** Row-major run-length pairs: value, count, value, count ... */
 UPROPERTY(EditAnywhere) TArray<int32> GridRuns;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FNeonCityMapPlace> Places;

 bool Contains(const FVector& World) const;
 /** World -> 0..1 texture UV. */
 FVector2D ToUV(const FVector& World) const;
 FVector ToWorld(const FVector2D& UV,double Z=0) const;
 /** Decoded grid, built once (bounded: at most 4M cells). */
 const TArray<uint8>& GetGrid() const;
private:
 mutable TArray<uint8> Decoded;
};

/** Grid route search, stateless, safe on a worker thread (the grid is decoded on the game thread first). */
struct NEONCITY_API FNeonCityMapRouter
{
 /** A* over the grid, at most MaxExpand cells; Driving keeps to roads where it can. Returns a world polyline
  * (string-pulled, Z left at the origin's height) from Origin to Goal, or false when no connected way exists. */
 static bool FindRoute(const UNeonCityMapData& Map,const FVector& Origin,const FVector& Goal,bool Driving,TArray<FVector>& Route,int32 MaxExpand=2000000);
};
