#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FullArtReviewSubsystem.generated.h"

/** Explicit, disposable art review harness. Never enabled in ordinary play. */
UCLASS()
class NEONCITY_API UFullArtReviewSubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UFullArtReviewSubsystem,STATGROUP_Tickables); }
 virtual void Tick(float DeltaTime) override;
private:
 void GoToStop();
 int32 Stop=0,Lighting=0;
 bool Initialized=false;
 float NextHint=0;
};
