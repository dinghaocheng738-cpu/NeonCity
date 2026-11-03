#pragma once
#include "CoreMinimal.h"

enum class EMarineSpearLife : uint8 { Alive, HitReaction, LosingControl, DeathMotion, DeadDrift, Collectible };

/** Only struck animals use this bounded, non-ragdoll simulation. Units are cm/s. */
struct FMarineSpearDeath
{
 EMarineSpearLife State=EMarineSpearLife::Alive;
 double Started=0;
 float Age=0,Duration=3.2f,RollSign=1,RollDegrees=0,TailPhase=0,TailStrength=1,TerminalZ=-6;
 FVector Velocity=FVector::ZeroVector,HitOffset=FVector::ZeroVector,LocalSpear=FVector::ForwardVector;
 FQuat RestRotation=FQuat::Identity;
 bool Active() const { return State!=EMarineSpearLife::Alive; }
 bool Collectible() const { return State==EMarineSpearLife::Collectible; }
 void Attach(FVector Position,FQuat Rotation,FVector Hit,FVector Direction,float Length)
 {
  HitOffset=Rotation.UnrotateVector(Hit-Position).GetClampedToMaxSize(FMath::Max(8.f,Length*.55f));
  LocalSpear=Rotation.UnrotateVector(Direction.GetSafeNormal());
 }
 void Begin(double Now,FVector Position,FQuat Rotation,FVector Travel,FVector Hit,FVector Direction,float Length,uint32 SpeciesSeed)
 {
  Started=Now;Age=0;State=EMarineSpearLife::HitReaction;RestRotation=Rotation;
  Attach(Position,Rotation,Hit,Direction,Length);
  RollSign=FMath::Abs(HitOffset.Y)>.5f?FMath::Sign(HitOffset.Y):(LocalSpear.Y>=0?1.f:-1.f);
  Duration=FMath::Clamp(2.8f+Length*.003f,2.9f,3.8f);
  TerminalZ=SpeciesSeed%5==0?2.f:-4.f-float(SpeciesSeed%9);
  Velocity=Travel.GetClampedToMaxSize(120)+Direction.GetSafeNormal()*FMath::Clamp(Length*.3f,18.f,65.f);
  TailStrength=1.5f;TailPhase=0;RollDegrees=0;
 }
 void Step(double Now,float Delta,FVector Current,float Floor,float Surface,FVector& Position,FQuat& Rotation)
 {
  Age=FMath::Max(0.f,float(Now-Started));
  State=Age<.25f?EMarineSpearLife::HitReaction:Age<.95f?EMarineSpearLife::LosingControl:
   Age<Duration-.65f?EMarineSpearLife::DeathMotion:Age<Duration?EMarineSpearLife::DeadDrift:EMarineSpearLife::Collectible;
  const float Effort=1-FMath::SmoothStep(.25f,Duration-.65f,Age);
  const float Twitch=FMath::Sin(Age*23)*FMath::Exp(-Age*1.9f)+.3f*FMath::Sin(Age*39)*FMath::Exp(-Age*2.8f);
  TailStrength=Effort*(.25f+1.15f*FMath::Abs(Twitch));
  // Distinct from normal periodic swimming: irregular twitches fade to rest.
  TailPhase+=FMath::Min(Delta,.4f)*(3+13*FMath::Abs(Twitch))*Effort;
  for(float Remaining=FMath::Min(Delta,.5f);Remaining>UE_SMALL_NUMBER;)
  {
   const float Dt=FMath::Min(Remaining,1.f/60);Remaining-=Dt;
   const FVector Desired=Current.GetClampedToMaxSize(25)+FVector(0,0,TerminalZ);
   const FVector Burst=RestRotation.GetForwardVector()*(Age<.45f?65.f*Effort:0.f);
   Velocity+=(Desired-Velocity)*(.9f+.003f*Velocity.Size())*Dt+Burst*Dt;
   Position+=Velocity*Dt;
   if(Position.Z<Floor){Position.Z=Floor;Velocity.Z=FMath::Max(0.,Velocity.Z);Velocity.X*=.97;Velocity.Y*=.97;}
   if(Position.Z>Surface){Position.Z=Surface;Velocity.Z=FMath::Min(0.,Velocity.Z);}
  }
  RollDegrees=RollSign*(68*FMath::SmoothStep(.15f,Duration-.4f,Age)+Twitch*13);
  Rotation=RestRotation*FRotator(Twitch*5,RollSign*Twitch*9,RollDegrees).Quaternion();
 }
 FTransform Attachment(FVector Position,FQuat Rotation) const
 { return FTransform((Rotation.RotateVector(LocalSpear)).Rotation(),Position+Rotation.RotateVector(HitOffset)); }
 bool Expired(float Distance,bool Reserved) const
 { return Age>90 || (!Reserved && Age>12 && Distance>5500); }
};
