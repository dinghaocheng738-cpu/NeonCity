#include "Fishing/FishingSessionModel.h"
void FNeonCityFishingSession::Transition(ENeonCityFishingState Next)
{ State=Next;Elapsed=0; }
void FNeonCityFishingSession::Escape(ENeonCityFishingFailure Why)
{ Failure=Why;Reeling=false;Transition(ENeonCityFishingState::Escaped); }
void FNeonCityFishingSession::Reset(int32 Seed,float WeightKg)
{
 FNeonCityFishFightConfig Config;Config.WeightKg=WeightKg;Config.SpeciesMaximumKg=FMath::Max(1.2,double(WeightKg));
 Reset(Seed,Config);
}
void FNeonCityFishingSession::Reset(int32 Seed,const FNeonCityFishFightConfig& Config)
{
 *this=FNeonCityFishingSession();FRandomStream Random(Seed);
 FightConfig=Config;FightSeed=uint32(Seed);WaitSeconds=Random.FRandRange(3.f,7.f);
 RemainingLineM=float(Config.DistanceM);
 if(!Config.IsValid())Cancel();
}
void FNeonCityFishingSession::Press()
{
 if(State==ENeonCityFishingState::Ready){CastPower=0;PerfectCast=false;Transition(ENeonCityFishingState::Windup);}
 else if(State==ENeonCityFishingState::Waiting)Escape(ENeonCityFishingFailure::EarlyHook);
 else if(State==ENeonCityFishingState::Bite)
 { if(Fight.Reset(FightConfig,FightSeed)) { Transition(ENeonCityFishingState::Fighting);Reeling=true; } else Cancel(); }
 else if(State==ENeonCityFishingState::Fighting)Reeling=true;
}
void FNeonCityFishingSession::Release(bool AnimationReady)
{
 Reeling=false;
 if(State==ENeonCityFishingState::Windup)
 {
  PerfectCast=AnimationReady && CastPower>=PerfectLow && CastPower<=PerfectHigh;
  BackSwingRemaining=FMath::Max(0.f,(PerfectLow-CastPower)*WindupDuration);
  LaunchDelay=BackSwingRemaining+.35f;
  CastFlightSeconds=FMath::Lerp(.85f,1.55f,CastPower)+(PerfectCast?.12f:0.f);
  Transition(ENeonCityFishingState::Casting);
 }
}
bool FNeonCityFishingSession::SetEncounter(int32 Seed,const FNeonCityFishFightConfig& Config,float Delay)
{
 if((State!=ENeonCityFishingState::Ready && State!=ENeonCityFishingState::Windup && State!=ENeonCityFishingState::Casting && State!=ENeonCityFishingState::Waiting)
  || !Config.IsValid() || !FMath::IsFinite(Delay) || Delay<0)return false;
 FightConfig=Config;FightSeed=uint32(Seed);RemainingLineM=Config.DistanceM;WaitSeconds=Elapsed+Delay;return true;
}
void FNeonCityFishingSession::Cancel()
{ if(!IsTerminal()) { Reeling=false;Failure=ENeonCityFishingFailure::Interrupted;Transition(ENeonCityFishingState::Cancelled); } }
void FNeonCityFishingSession::Step(float DeltaSeconds)
{
 if(!FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0 || IsTerminal() || State==ENeonCityFishingState::Ready)return;
 // A hitch cannot skip an entire bite window or finish the fight in one update.
 const float Delta=FMath::Min(DeltaSeconds,.1f);Elapsed+=Delta;
 if(State==ENeonCityFishingState::Windup)CastPower=FMath::Min(1.f,Elapsed/WindupDuration);
 else if(State==ENeonCityFishingState::Casting && Elapsed>=CastFlightSeconds+LaunchDelay)Transition(ENeonCityFishingState::Waiting);
 else if(State==ENeonCityFishingState::Waiting && Elapsed>=WaitSeconds)Transition(ENeonCityFishingState::Bite);
 else if(State==ENeonCityFishingState::Bite && Elapsed>=BiteSeconds)Escape(ENeonCityFishingFailure::MissedBite);
 else if(State==ENeonCityFishingState::Fighting)
 {
  const auto Result=Fight.Step(Delta,Reeling);
  Tension=float(Fight.Tension);Surge=float(Fight.Surge);Struggling=Surge>.6f;
  RemainingLineM=float(Fight.DistanceM);FishStamina=float(Fight.Stamina);
  Progress=float(FMath::Clamp((FightConfig.DistanceM-Fight.DistanceM)/FMath::Max(.01,FightConfig.DistanceM-1.2),0.,1.));
  if(Result==ENeonCityFishFightResult::Snapped)Escape(ENeonCityFishingFailure::BrokenLine);
  else if(Result==ENeonCityFishFightResult::Escaped)Escape(ENeonCityFishingFailure::FishEscaped);
  else if(Result==ENeonCityFishFightResult::Caught) { Progress=1;Reeling=false;Transition(ENeonCityFishingState::Caught); }
  else if(Result==ENeonCityFishFightResult::Invalid)Cancel();
 }
}
