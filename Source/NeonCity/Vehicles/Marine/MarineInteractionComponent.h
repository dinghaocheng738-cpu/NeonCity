#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MarineInteractionComponent.generated.h"
class ACharacter;
class APlayerController;
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UMarineInteractionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UMarineInteractionComponent();
 bool CanEnter(AActor* Actor) const;
 bool Enter(AActor* Actor);
 bool Exit();
 /** Move the seated driver straight into another boat (no landing in between): the swan yacht's tender. */
 bool HandOver(UMarineInteractionComponent* To);
 ACharacter* GetDriver() const;
 bool FindExitTransform(FTransform& Transform,bool ShoreOnly=false) const;
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 void Restore(const FTransform& Transform,bool Possess);
 TWeakObjectPtr<ACharacter> Driver;
 TWeakObjectPtr<APlayerController> DriverController;
 FTransform EntryTransform;
 bool bHidden=false,bCollision=true,bTransition=false,bVolumeUpdates=true;
};
