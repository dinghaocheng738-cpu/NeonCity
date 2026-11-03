#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityLandSoundscape.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;

/** The West Lake's land ambience, one per map (placed by LandRebuild/LandAudio_WL.py from the map data).
 *  Looping beds blend by where the listener is and the hour: city traffic over the CBD streets, summer cicadas and
 *  a daytime bird chorus in the parks, crickets at night, lotus-pond frogs by the lotus beds, wind over the open
 *  lake and water lapping along the shore. Spatial one-shots: birds singing from the trees around the listener
 *  (black-naped orioles at Liulang, 'Orioles Singing in the Willows'), car horns off the avenues, and the Jingci
 *  Temple bell across the lake on the hour ('Evening Bell at Nanping Hill', nine strokes at 18:00).
 *  City weight comes from a coarse grid baked from road / pavement / building density; water from the marine
 *  water query; time of day from the map clock and the lighting subsystem. */
UCLASS()
class NEONCITY_API ANeonCityLandSoundscape : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityLandSoundscape();
 virtual void Tick(float Delta) override;
 /** City density grid (0..255), row-major, GridSize.X columns along world X from GridOrigin, Cell cm a side. */
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") TArray<uint8> CityGrid;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") FVector2D GridOrigin=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") FIntPoint GridSize=FIntPoint::ZeroValue;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") float Cell=2500.f;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") FVector TempleBell=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") TArray<FVector> OrioleGroves;
 UPROPERTY(EditAnywhere,Category="NeonCity|Audio") TArray<FVector> LotusBeds;
 /** 0..1 bed levels now (wind, city, cicadas, birds, crickets, frogs, lapping), for tests and debugging. */
 float GetBedLevel(int32 Index) const { return Beds.IsValidIndex(Index)?Level[Index]:0.f; }
 static constexpr int32 BedCount=7;
 float SampleCity(const FVector& At) const;
 int32 GetOneShotCount() const { return OneShots; }
 int32 GetBellStrokes() const { return BellStrokes; }
 /** Rings the temple bell now (Strokes times, 8 s apart). */
 void RingBell(int32 Strokes);
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 bool Load();
 float WaterAround(const FVector& At) const;
 void PlayAt(USoundBase* Sound,const FVector& At,float Volume,float Pitch,USoundAttenuation* Attenuation);
 USoundAttenuation* MakeAttenuation(float Inner,float Falloff);
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Beds;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Birds;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Orioles;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Horns;
 UPROPERTY(Transient) TObjectPtr<USoundBase> Bell;
 UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BirdFalloff;
 UPROPERTY(Transient) TObjectPtr<USoundAttenuation> HornFalloff;
 UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BellFalloff;
 float Level[BedCount]={};
 float BirdWait=2,HornWait=6,BellQueue=0,BellGap=0,WaterCache=0,WaterAge=1;
 int32 OneShots=0,BellStrokes=0,LastBellHour=-1;
 bool bLoaded=false;
};
