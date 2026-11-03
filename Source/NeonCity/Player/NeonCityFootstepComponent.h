#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeonCityFootstepComponent.generated.h"

class USoundBase;
class USoundAttenuation;
class ACharacter;

/** The walker's footsteps: a step every stride (longer when running), the sound chosen from what is underfoot -
 *  grass / soil, stone paving and granite, asphalt, timber decks and the yacht's teak, carpet below decks, sand,
 *  shallow water - plus a heavier footfall on landing. The surface is read from the material under the foot
 *  (keyword table), so new ground needs no setup. Steps by the island's own soundscape take precedence there. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityFootstepComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityFootstepComponent();
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 int32 GetStepCount() const { return Steps; }
 FName GetLastSurface() const { return LastSurface; }
 /** What the last probe hit (actor / component: material), for debugging. */
 const FString& GetLastGround() const { return LastGround; }
 /** Surface under a point (for tests and other systems): grass, stone, asphalt, wood, carpet, sand, water. */
 FName ProbeSurface(const FVector& Foot) const;
protected:
 virtual void BeginPlay() override;
private:
 UFUNCTION() void OnLanded(const FHitResult& Hit);
 void Step(float Volume);
 bool Load();
 struct FBank{TArray<TObjectPtr<USoundBase>> Clips;float Volume=1,Pitch=1;};
 TMap<FName,FBank> Banks;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Held;
 UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Falloff;
 TWeakObjectPtr<ACharacter> Walker;
 bool OnIsland() const;
 TArray<TWeakObjectPtr<class UNeonCityReferenceSoundscapeComponent>> Islands;
 float Travelled=0;int32 Steps=0;bool bLoaded=false;FName LastSurface;mutable FString LastGround;
};
