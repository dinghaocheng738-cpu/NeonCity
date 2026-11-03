#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandGarageDoor.generated.h"
class UStaticMeshComponent;
/** Ten real articulated leaves travel on vertical, curved and ceiling tracks.
 * Normal collision stays active throughout; occupancy reverses closing. */
UCLASS()
class NEONCITY_API AIslandGarageDoor : public AActor
{
 GENERATED_BODY()
public:
 AIslandGarageDoor();
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Panels;
 UPROPERTY(EditAnywhere) float Width=600;
 UPROPERTY(EditAnywhere) float Height=300;
 UPROPERTY(EditAnywhere) float TrackRadius=40;
 UPROPERTY(EditAnywhere) float TravelSeconds=4;
 void Toggle();
 float GetOpenFraction()const{return OpenFraction;}
 bool IsTargetOpen()const{return bTargetOpen;}
 bool IsObstructed()const{return bObstructed;}
 /** What last stopped a leaf (actor / component), for diagnostics. */
 const FString& GetLastBlocker()const{return LastBlocker;}
 void PanelPose(int32 Index,float Fraction,FVector& Location,FRotator& Rotation)const;
 virtual void Tick(float Delta)override;
 virtual void OnConstruction(const FTransform& Transform)override;
protected:
 virtual void BeginPlay()override;
private:
 bool CanMoveTo(float Fraction)const;
 void Present();
 float OpenFraction=1;
 bool bTargetOpen=true,bObstructed=false;
 mutable FString LastBlocker;
};
