#pragma once
#include "CoreMinimal.h"
#include "Fishing/FishingFightModel.h"
#include "FishingSessionModel.generated.h"

UENUM(BlueprintType)
enum class ENeonCityFishingState : uint8 { Ready, Casting, Waiting, Bite, Fighting, Caught, Escaped, Cancelled, Windup };
UENUM(BlueprintType)
enum class ENeonCityFishingFailure : uint8 { None, EarlyHook, MissedBite, BrokenLine, FishEscaped, Interrupted };

/** Input-driven, frame-independent session rules; no Actor, rewards, timers or persistence side effects. */
struct NEONCITY_API FNeonCityFishingSession
{
 ENeonCityFishingState State=ENeonCityFishingState::Ready;
 ENeonCityFishingFailure Failure=ENeonCityFishingFailure::None;
 float Elapsed=0;
 float Tension=.25f;
 float Progress=0;
 float WaitSeconds=4;
 float BiteSeconds=2.25f;
 float RemainingLineM=0;
 float FishStamina=1;
 float Surge=0;
 float CastPower=0,CastFlightSeconds=1.2f;
 float WindupDuration=1.4f,BackSwingRemaining=0,LaunchDelay=.35f;
 bool PerfectCast=false;
 static constexpr float PerfectLow=69.f/84.f;
 static constexpr float PerfectHigh=80.f/84.f;
 bool Reeling=false;
 bool Struggling=false;
 void Reset(int32 Seed,float WeightKg);
 void Reset(int32 Seed,const FNeonCityFishFightConfig& Config);
 bool SetEncounter(int32 Seed,const FNeonCityFishFightConfig& Config,float Delay);
 void Press();
 void Release(bool AnimationReady=false);
 void Step(float DeltaSeconds);
 void Cancel();
 bool IsTerminal() const { return State==ENeonCityFishingState::Caught || State==ENeonCityFishingState::Escaped || State==ENeonCityFishingState::Cancelled; }
private:
 FNeonCityFishFight Fight;
 FNeonCityFishFightConfig FightConfig;
 uint32 FightSeed=1;
 void Transition(ENeonCityFishingState Next);
 void Escape(ENeonCityFishingFailure Why);
};
