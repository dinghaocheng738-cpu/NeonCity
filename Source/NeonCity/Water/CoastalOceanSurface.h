#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoastalOceanSurface.generated.h"
class UCoastalWaveData;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UMaterialInterface;
class UTidewaterOceanTexture;
class UPrimitiveComponent;
class UTexture2D;
class UTextureRenderTarget2D;
class ANeonCityDayNightCycle;
struct FCoastalOceanRuntime;
struct FTidewaterWaterSample;
#if WITH_DEV_AUTOMATION_TESTS
struct FNeonCityOceanLeaseDiagnostic
{
 // 0=no ready surface, 1=no lease, 2=invalid, 3=outside footprint,
 // 4=expired (>1 s), 5=usable. Diagnostic reads never refresh the lease.
 int32 State=0;
 double IssuedAge=0,FootprintMetres=0;
};
struct FNeonCityOceanInstanceDiagnostic
{
 int32 TrackedNodes=0,RenderedInstances=0;
 uint64 ChangedSelections=0,OrderOnlySelections=0,TopologyUpdates=0,AddedNodes=0,RemovedNodes=0;
};
#endif

UENUM(BlueprintType)
enum class ECoastalSeaState:uint8 { Calm, Light, Moderate };

/** Shared Tidewater FFT/WaterQuery/CDLOD instance. Physics uses bounded asynchronous samples. */
UCLASS()
class NEONCITY_API ACoastalOceanSurface:public AActor
{
 GENERATED_BODY()
public:
 ACoastalOceanSurface();
 virtual void Tick(float DeltaSeconds) override;
 UFUNCTION(BlueprintCallable,Category="Ocean") void SetSeaState(ECoastalSeaState State);
 UFUNCTION(BlueprintPure,Category="Ocean") ECoastalSeaState GetSeaState()const{return SeaState;}
 const UCoastalWaveData* GetField()const{return Field;}
 /** Optional authored bootstrap field, accepted only before BeginPlay. */
 bool InitializeField(UCoastalWaveData* InField,UMaterialInterface* InMaterial=nullptr);
 float GetStrength()const{return Strength;}
 double GetWaveSeconds()const{return WaveSeconds;}
 bool IsReady()const{return bReady;}
 void Sample(FVector Point,float& Height,FVector& Normal,FVector& Velocity)const;
 // Stable sample leases: wake particles cannot evict physical hull queries.
 bool RegisterSamples(UPrimitiveComponent* Body,const TArray<FVector>& LocalPoints);
 void UnregisterSamples(UPrimitiveComponent* Body);
 bool SampleBody(UPrimitiveComponent* Body,int32 Index,FVector Point,float& Height,FVector& Normal,FVector& Velocity,FTidewaterWaterSample* Snapshot=nullptr)const;
#if WITH_DEV_AUTOMATION_TESTS
 FNeonCityOceanLeaseDiagnostic GetBodyLeaseDiagnostic(UPrimitiveComponent* Body,int32 Index,FVector Point)const;
 FNeonCityOceanInstanceDiagnostic GetSurfaceInstanceDiagnostic()const;
#endif
 uint64 GetQuerySequence()const;
 int32 GetSurfaceNodeCount()const;
 bool HasPhysicalWakeFor(const AActor* Boat)const;
 UTextureRenderTarget2D* GetFineCaustics()const{return CausticsFine;}
 UTextureRenderTarget2D* GetBroadCaustics()const{return CausticsBroad;}
 FVector2f GetCausticTileSizes()const{return CausticTileSizes;}
protected:
 virtual void BeginPlay()override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Surface;
 UPROPERTY() TObjectPtr<UCoastalWaveData> Field;
 UPROPERTY() TObjectPtr<UStaticMesh> GridAsset;
 UPROPERTY() TObjectPtr<UMaterialInterface> WaterMaterial;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
 UPROPERTY() TObjectPtr<UTidewaterOceanTexture> Displacement;
 UPROPERTY() TObjectPtr<UTidewaterOceanTexture> Derivatives;
 UPROPERTY() TObjectPtr<UTexture2D> WakeHull;
 UPROPERTY() TObjectPtr<UTexture2D> WakePiles;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> WakeDisplay;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> WakeAeration;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> WakeNear;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CausticsFine;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CausticsBroad;
 TWeakObjectPtr<ANeonCityDayNightCycle> DayNight;
 FVector2f CausticTileSizes=FVector2f(1,1);
 UPROPERTY(EditAnywhere) ECoastalSeaState SeaState=ECoastalSeaState::Light;
 TSharedPtr<FCoastalOceanRuntime,ESPMode::ThreadSafe> Runtime;
 TArray<TWeakObjectPtr<AActor>> HiddenLegacySurfaces;
 float Strength=0;
 double WaveSeconds=0;
 bool bReady=false;
};
