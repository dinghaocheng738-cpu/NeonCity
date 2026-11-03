#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CoastalWaveData.generated.h"

class UTexture2D;

/** Cooked Tidewater uniform profiles plus native coastal bathymetry.
 * Source profile bytes preserve the audited 224-byte layout exactly. */
UCLASS(BlueprintType)
class NEONCITY_API UCoastalWaveData : public UDataAsset
{
 GENERATED_BODY()
public:
 UCoastalWaveData();
 UPROPERTY(EditDefaultsOnly) TArray<uint8> CalmProfile;
 UPROPERTY(EditDefaultsOnly) TArray<uint8> LightProfile;
 UPROPERTY(EditDefaultsOnly) TArray<uint8> ModerateProfile;
 UPROPERTY(VisibleAnywhere) FVector2D GridMinimum=FVector2D(-220000,-297500);
 UPROPERTY(VisibleAnywhere) FVector2D GridMaximum=FVector2D(220000,-2500);
 UPROPERTY(VisibleAnywhere) int32 GridSize=0;
 UPROPERTY(VisibleAnywhere) TArray<float> DepthCm;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTexture2D> DepthTexture;
 UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> IslandHeight;
 UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> IslandShore;
 UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> IslandNormal;
 UPROPERTY(EditDefaultsOnly) float SeaLevel=-130;
 bool IsValidField() const;
 float GetDepth(FVector2D Point,FVector2D* Gradient=nullptr) const;
 const TArray<uint8>& GetProfile(uint8 State)const{return State==0?CalmProfile:State==1?LightProfile:ModerateProfile;}
};
