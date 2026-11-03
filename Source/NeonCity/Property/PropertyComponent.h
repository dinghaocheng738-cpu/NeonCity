#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PropertyComponent.generated.h"
class UNeonCityPropertyData;

/** Composition boundary for world properties; supplies physical spawn and garage locations. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UPropertyComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPropertyComponent();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") TObjectPtr<UNeonCityPropertyData> Data;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") FTransform HomeSpawnLocal=FTransform(FRotator(0,-90,0),FVector(170,510,99));
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") TArray<FVector> GarageSlotsLocal;
 UFUNCTION(BlueprintPure,Category="Property") FName GetPropertyID() const;
 UFUNCTION(BlueprintPure,Category="Property") FTransform GetHomeSpawnTransform() const;
 FVector GetGarageLocation(int32 Index) const;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
};
