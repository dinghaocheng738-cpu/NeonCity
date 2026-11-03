#include "Adventure/MarineSchool.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Fishing/FishData.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"

AMarineSchool::AMarineSchool()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickInterval=.5f;
 Bodies=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MarineInstances"));
 SetRootComponent(Bodies);
 Bodies->SetMobility(EComponentMobility::Movable);
 Bodies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Bodies->SetCollisionResponseToAllChannels(ECR_Ignore);
 Bodies->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Bodies->SetCanEverAffectNavigation(false);
 Bodies->SetCastShadow(false);
 Bodies->bAffectDistanceFieldLighting=false;
 Bodies->NumCustomDataFloats=9;
 Bodies->SetCullDistances(3000,4200);
}

void AMarineSchool::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);RebuildSchool();
}
void AMarineSchool::RebuildSchool()
{
 Bodies->ClearInstances();Creatures.Reset();Random.Initialize(Seed);
 Bodies->SetStaticMesh(CreatureMesh);
 // Coastal blue-channel transmission is under 0.8% at 42 m. Keep distant
 // ecology simulated at its normal throttle, but stop drawing invisible fish.
 Bodies->SetCullDistances(3000,4200);
 if(!CreatureMesh)return;
 for(const FTransform& Start:Starts)
 {
  FCreature F;
  F.Position=Start.GetLocation();F.Rotation=Start.GetRotation();F.Scale=Start.GetScale3D().X;
  if(FloorResolution>1 && FloorHeights.Num()==FloorResolution*FloorResolution)
  {
   const float Bottom=FloorAt(F.Position)+FloorClearance*(Behaviour==EMarineBehaviour::Benthic?F.Scale:1.f);
   const float Ceiling=MeanSeaLevel-GetActorLocation().Z-SurfaceClearance;
   if(Bottom<Ceiling)F.Position.Z=Behaviour==EMarineBehaviour::Benthic?Bottom:FMath::Clamp(float(F.Position.Z),Bottom,Ceiling);
  }
  F.Velocity=(bSideways?F.Rotation.GetRightVector():F.Rotation.GetForwardVector())*CruiseSpeed;F.Phase=Random.FRandRange(0,2*PI);
  const int32 I=Creatures.Add(F);
  Bodies->AddInstance(Start);
  WritePresentation(I,0,0,false);
 }
 Bodies->MarkRenderStateDirty();
}
void AMarineSchool::BeginPlay()
{
 Super::BeginPlay();RebuildSchool();PickGoal();
 GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->RegisterSchool(this);
}
void AMarineSchool::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* Ecology=GetWorld()->GetSubsystem<UMarineEcologySubsystem>())Ecology->UnregisterSchool(this);
 Super::EndPlay(Reason);
}
float AMarineSchool::FloorAt(FVector P) const
{
 if(FloorResolution<2 || FloorHeights.Num()!=FloorResolution*FloorResolution || FloorExtent<=0)
  return -100000; // Invalid authoring is rejected by the placement audit, never fabricated as flat ground.
 const float U=FMath::Clamp((P.X/FloorExtent*.5f+.5f)*(FloorResolution-1),0.f,FloorResolution-1.001f);
 const float V=FMath::Clamp((P.Y/FloorExtent*.5f+.5f)*(FloorResolution-1),0.f,FloorResolution-1.001f);
 const int32 X=FMath::FloorToInt(U),Y=FMath::FloorToInt(V);
 return FMath::Lerp(FMath::Lerp(FloorHeights[Y*FloorResolution+X],FloorHeights[Y*FloorResolution+X+1],U-X),
  FMath::Lerp(FloorHeights[(Y+1)*FloorResolution+X],FloorHeights[(Y+1)*FloorResolution+X+1],U-X),V-Y);
}
void AMarineSchool::PickGoal()
{
 GoalClock=Random.FRandRange(4,9);
 const bool Close=Behaviour==EMarineBehaviour::Hover || Behaviour==EMarineBehaviour::Benthic;
 const float Angle=Random.FRandRange(0,2*PI),Radius=Random.FRandRange(.1f,Close?.4f:.82f)*HomeRadius;
 Goal=FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
 const float Floor=FloorAt(Goal),Ceiling=MeanSeaLevel-GetActorLocation().Z-SurfaceClearance;
 if(Floor+FloorClearance>=Ceiling){Goal=FVector::ZeroVector;GoalClock=1;return;}
 if(Behaviour==EMarineBehaviour::Benthic)Goal.Z=Floor+FloorClearance;
 else if(Behaviour==EMarineBehaviour::Forage)Goal.Z=FMath::Min(Floor+FloorClearance+Random.FRandRange(15,80),Ceiling);
 else Goal.Z=FMath::Clamp(Random.FRandRange(-90,90),Floor+FloorClearance,Ceiling);
 if(Behaviour==EMarineBehaviour::Forage || Behaviour==EMarineBehaviour::Benthic)PauseClock=Random.FRandRange(.5f,2.4f);
}
void AMarineSchool::WritePresentation(int32 I,float Time,float Window,bool bMoving)
{
 const FCreature& F=Creatures[I];
 const FTransform Transform(F.Rotation,F.Position,FVector(F.bAlive && !F.bLanded?F.Scale:0));
 Bodies->UpdateInstanceTransform(I,Transform,false,false,true);
 const FVector LocalVelocity=bMoving?F.Rotation.UnrotateVector(F.Velocity)/FMath::Max(.1f,F.Scale):FVector::ZeroVector;
 const float Values[]={F.Death.Active()?F.Death.TailPhase:F.Phase,F.Death.Active()?0.f:SwimFrequency*(.75f+F.Velocity.Size()/FMath::Max(CruiseSpeed,1.f)*.25f),
  SwimAmplitude*(F.Death.Active()?F.Death.TailStrength:1.f),Behaviour==EMarineBehaviour::Benthic?FMath::Clamp(F.Velocity.Size()/FMath::Max(CruiseSpeed,1.f),0.f,1.f):F.TurnBend,
  Time,static_cast<float>(LocalVelocity.X),static_cast<float>(LocalVelocity.Y),static_cast<float>(LocalVelocity.Z),Window};
 for(int32 Slot=0;Slot<9;++Slot)Bodies->SetCustomDataValue(I,Slot,Values[Slot],false);
}
void AMarineSchool::Tick(float Delta)
{
 Super::Tick(Delta);
 if(Creatures.IsEmpty() || !FMath::IsFinite(Delta) || Delta<=0)return;
 const auto* PC=GetWorld()->GetFirstPlayerController();
 const auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
 const FVector Eye=Camera?Camera->GetCameraLocation():FVector(1.e9);
 const float Distance=FVector::Dist(Eye,GetActorLocation());
 const bool Active=Distance<HomeRadius+7000;
 const float Time=GetWorld()->GetTimeSeconds();
 // Corpses remain independent of the weapon and of the school's normal AI.
 // Absolute age also advances when a distant school is throttled.
 for(int32 I=0;I<Creatures.Num();++I)
 {
  auto& F=Creatures[I];if(!F.bAlive || !F.Death.Active())continue;
  const FVector WorldAt=GetActorTransform().TransformPosition(F.Position);
  float Surface=MeanSeaLevel;FVector Current=FVector::ZeroVector;
  GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(WorldAt,Surface,Current);
  F.Death.Step(Time,Delta,GetActorTransform().InverseTransformVectorNoScale(Current)*.25f,
   FloorAt(F.Position)+FMath::Max(3.f,FloorClearance*.25f),Surface-GetActorLocation().Z-8,F.Position,F.Rotation);
  F.Velocity=F.Death.Velocity;
  if(F.Death.Expired(FVector::Dist(Eye,WorldAt),F.HookTicket.IsValid())){F.bAlive=false;F.HookTicket.Invalidate();}
  WritePresentation(I,Time,0,false);Bodies->MarkRenderStateDirty();
 }
 if(!Active)
 {
  if(bActive)
  {
   Bodies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   for(int32 I=0;I<Creatures.Num();++I)WritePresentation(I,Time,0,false);
   Bodies->MarkRenderStateDirty();
  }
  bActive=false;SetActorTickInterval(.5f);return;
 }
 const bool Near=Distance<HomeRadius+2500;
 const float Interval=Near?.05f:.125f;
 SetActorTickInterval(Interval);
 const float Step=bActive?FMath::Min(Delta,.15f):Interval;
 bActive=true;
 Bodies->SetCollisionEnabled(Near?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 GoalClock-=Step;PauseClock=FMath::Max(0.f,PauseClock-Step);
 if(GoalClock<=0)PickGoal();
 const APawn* Player=PC?PC->GetPawn():nullptr;
 const FVector Threat=GetActorTransform().InverseTransformPosition(Player?Player->GetActorLocation():Eye);
 const float ThreatRange=110+FMath::Clamp(((Player?Player->GetVelocity().Size():0)-70)*.8f,0.f,380.f);
 const float CellWidth=FMath::Max(Spacing*4,100.f);
 auto Cell=[CellWidth](FVector P){return FIntVector(FMath::FloorToInt(P.X/CellWidth),FMath::FloorToInt(P.Y/CellWidth),FMath::FloorToInt(P.Z/CellWidth));};
 TMap<FIntVector,TArray<int32>> Cells;
 for(int32 I=0;I<Creatures.Num();++I)if(IsAlive(I))Cells.FindOrAdd(Cell(Creatures[I].Position)).Add(I);
 // Snapshot avoids update-order bias: every neighbour belongs to the same simulation time.
 const TArray<FCreature> Snapshot=Creatures;
 for(int32 I=0;I<Creatures.Num();++I)
 {
  FCreature& F=Creatures[I];if(!F.bAlive || F.Death.Active())continue;
  if(F.bLanded){WritePresentation(I,Time,0,false);continue;}
  if(F.bHooked)
  {
   const FVector Tow=F.HookTarget-F.Position;
   F.Velocity=Tow/Step;F.Position=F.HookTarget;
   if(!Tow.IsNearlyZero())F.Rotation=Tow.Rotation().Quaternion();
   WritePresentation(I,Time,0,false);continue;
  }
  FVector Separation=FVector::ZeroVector,Alignment=FVector::ZeroVector,Cohesion=FVector::ZeroVector;
  int32 Count=0,Inspected=0;
  const FIntVector C=Cell(F.Position);
  for(int32 Z=-1;Z<=1 && Inspected<24;++Z)for(int32 Y=-1;Y<=1 && Inspected<24;++Y)for(int32 X=-1;X<=1 && Inspected<24;++X)
  {
   const auto* Bucket=Cells.Find(C+FIntVector(X,Y,Z));if(!Bucket)continue;
   for(int32 J:*Bucket)
   {
    if(J==I)continue;
    if(++Inspected>24)break;
    const FCreature& N=Snapshot[J];const FVector Away=F.Position-N.Position;const float D2=Away.SizeSquared();
    if(D2<1 || D2>CellWidth*CellWidth)continue;
    if(D2<Spacing*Spacing)Separation+=Away/FMath::Max(D2,1.f)*Spacing;
    Alignment+=N.Velocity.GetSafeNormal();Cohesion+=N.Position;++Count;
   }
  }
  FVector Direction=((F.HookTicket.IsValid()?F.HookTarget:Goal)-F.Position).GetSafeNormal();
  const bool Schooling=Behaviour==EMarineBehaviour::School;
  if(Count && !F.HookTicket.IsValid())Direction+=(Alignment/Count)*(Schooling?1.35f:.18f)+(Cohesion/Count-F.Position)/CellWidth*(Schooling?.9f:.2f);
  Direction+=Separation*3;
  const FVector Away=F.Position-Threat;
  F.Alarm=FMath::Max(0.f,F.Alarm-Step*.7f);
  if(Away.SizeSquared()<FMath::Square(ThreatRange)){F.Alarm=1;F.AlarmSource=Threat;}
  if(F.Alarm>.05f)Direction+=(F.Position-F.AlarmSource).GetSafeNormal()*F.Alarm*5;
  if(F.Position.Size2D()>HomeRadius && !F.HookTicket.IsValid())Direction-=F.Position.GetSafeNormal2D()*3;
  const FVector LookAhead=F.Position+F.Velocity*.75f;
  const float Floor=FloorAt(LookAhead),Ceiling=MeanSeaLevel-GetActorLocation().Z-SurfaceClearance;
  if(LookAhead.Z<Floor+FloorClearance+35)Direction.Z+=2;
  if(LookAhead.Z>Ceiling-35)Direction.Z-=2;
  for(const FVector4& O:Obstacles)
  {
   const FVector Offset=LookAhead-FVector(O.X,O.Y,O.Z);
   const float Safety=O.W+Spacing*.5f;
   if(Offset.SizeSquared()<Safety*Safety)Direction+=Offset.GetSafeNormal()*4;
  }
  if(Behaviour==EMarineBehaviour::Benthic)Direction.Z=0;
  else
  {
   const float MaxPitch=F.Alarm>.1f?45.f:Behaviour==EMarineBehaviour::Cruise?18.f:32.f;
   const FVector Horizontal=Direction.GetSafeNormal2D(UE_SMALL_NUMBER,F.Rotation.GetForwardVector().GetSafeNormal2D());
   const float Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Direction.Z,Direction.Size2D())),-MaxPitch,MaxPitch);
   Direction=FRotator(Pitch,Horizontal.Rotation().Yaw,0).Vector();
  }
  const float Rest=PauseClock>0 && F.Alarm<.1f && !F.HookTicket.IsValid()?.18f:1.f;
  const float DesiredSpeed=CruiseSpeed*(1+F.Alarm*2.2f)*Rest*(F.HookTicket.IsValid()?1.6f:1.f);
  const float Length=Species?(Species->SizeCm.X+Species->SizeCm.Y)*.5f*F.Scale:40;
  const float TurnRate=Behaviour==EMarineBehaviour::Cruise?FMath::Clamp(5000.f/FMath::Max(80.f,Length),16.f,45.f):65.f;
  const FVector PreviousHeading=bSideways?F.Rotation.GetRightVector():F.Rotation.GetForwardVector();
  const FVector Heading=FMath::VInterpNormalRotationTo(PreviousHeading,Direction.GetSafeNormal(),Step,
   F.Alarm>.1f?FMath::Min(160.f,TurnRate*2):TurnRate);
  const float Bend=FVector::CrossProduct(F.Rotation.GetForwardVector(),Heading).Z/FMath::Max(.01f,Step)*.045f;
  F.TurnBend=FMath::FInterpTo(F.TurnBend,FMath::Clamp(Bend,-.08f,.08f),Step,4.f);
  F.Velocity=FMath::VInterpConstantTo(F.Velocity,Heading*DesiredSpeed,Step,CruiseSpeed*2.5f);
  FVector Next=F.Position+F.Velocity*Step;
  const float Bottom=FloorAt(Next)+FloorClearance*(Behaviour==EMarineBehaviour::Benthic?F.Scale:1.f);
  bool Clear=Bottom<Ceiling && FMath::Abs(Next.X)<FloorExtent && FMath::Abs(Next.Y)<FloorExtent;
  for(const FVector4& O:Obstacles)if(FVector::DistSquared(Next,FVector(O.X,O.Y,O.Z))<FMath::Square(O.W))Clear=false;
  if(Clear)
  {
   Next.Z=Behaviour==EMarineBehaviour::Benthic?Bottom:FMath::Clamp(Next.Z,Bottom,Ceiling);
   F.Position=Next;
  }
  else F.Velocity=FVector(-F.Velocity.Y,F.Velocity.X,F.Velocity.Z)*.5f;
  if(Behaviour==EMarineBehaviour::Benthic)
  {
   constexpr float Span=20;
   const FVector Normal(-(FloorAt(F.Position+FVector(Span,0,0))-FloorAt(F.Position-FVector(Span,0,0)))/(2*Span),
    -(FloorAt(F.Position+FVector(0,Span,0))-FloorAt(F.Position-FVector(0,Span,0)))/(2*Span),1);
   F.Rotation=FRotationMatrix::MakeFromXZ(Heading,Normal.GetSafeNormal()).ToQuat()*FQuat(FVector::UpVector,bSideways?-PI*.5f:0.f);
  }
  else F.Rotation=Heading.Rotation().Quaternion();
  F.Phase=FMath::Fmod(F.Phase+Step*2*PI*SwimFrequency*(.75f+F.Velocity.Size()/FMath::Max(CruiseSpeed,1.f)*.25f),2*PI);
  WritePresentation(I,Time,Interval,true);
 }
 Bodies->MarkRenderStateDirty();
}
bool AMarineSchool::IsAlive(int32 I)const{return IsPresent(I) && !Creatures[I].Death.Active();}
FVector AMarineSchool::CreaturePosition(int32 I)const{return IsPresent(I)?GetActorTransform().TransformPosition(Creatures[I].Position):GetActorLocation();}
int32 AMarineSchool::LivingCount()const
{int32 Count=0;for(const auto& F:Creatures)Count+=F.bAlive && !F.Death.Active()?1:0;return Count;}
bool AMarineSchool::HasReservedCreatures()const
{for(const auto& F:Creatures)if(F.bAlive && (F.HookTicket.IsValid() || F.Death.Active()))return true;return false;}
TSet<int32> AMarineSchool::GetDepletedIndices()const
{TSet<int32> Result;for(int32 I=0;I<Creatures.Num();++I)if(!IsAlive(I))Result.Add(I);return Result;}
void AMarineSchool::ApplyDepletedIndices(const TSet<int32>& Indices)
{for(int32 I:Indices)if(Creatures.IsValidIndex(I)){Creatures[I].bAlive=false;WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);}Bodies->MarkRenderStateDirty();}
bool AMarineSchool::NearestCreature(const FVector& P,float& DistanceSquared,FVector& Result)const
{
 bool Found=false;
 for(int32 I=0;I<Creatures.Num();++I)if(IsAlive(I))
 {
  const FVector Position=CreaturePosition(I);const float D=FVector::DistSquared(P,Position);
  if(D<DistanceSquared){DistanceSquared=D;Result=Position;Found=true;}
 }
 return Found;
}
bool AMarineSchool::TryCapture(int32 I,UNeonCityFishingSubsystem* Inventory)
{
 if(!IsAlive(I) || Creatures[I].HookTicket.IsValid() || !bCapturable || !Species || !Inventory || !Inventory->HasPouchSpace())return false;
 FNeonCityCaughtFish Fish;Fish.FishID=FGuid::NewGuid();Fish.SpeciesID=Species->SpeciesID;
 Fish.WeightKg=FMath::Lerp(Species->WeightKg.X,Species->WeightKg.Y,FMath::Clamp((Creatures[I].Scale-.8f)/.4f,0.f,1.f));
 Fish.SizeCm=(Species->SizeCm.X+Species->SizeCm.Y)*.5f*Creatures[I].Scale;
 Fish.CaughtAt=FDateTime::UtcNow();Fish.FishingPointID=HabitatID;
 if(!Inventory->StoreCatch(Fish))return false;
 Creatures[I].bAlive=false;WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;
}
bool AMarineSchool::CanHook(int32 I) const
{return HasAuthority() && bCapturable && IsAlive(I) && !Creatures[I].HookTicket.IsValid() && Species && Species->IsValidProfile() && Behaviour!=EMarineBehaviour::Benthic;}
void AMarineSchool::StartleAt(FVector WorldPoint,float Radius)
{
 const FVector Point=GetActorTransform().InverseTransformPosition(WorldPoint);
 for(auto& F:Creatures)if(F.bAlive && !F.Death.Active() && FVector::DistSquared(F.Position,Point)<FMath::Square(Radius)){F.Alarm=1;F.AlarmSource=Point;}
}
bool AMarineSchool::ReserveHook(int32 I,FGuid Ticket,FNeonCityCaughtFish& Catch)
{
 if(!Ticket.IsValid() || !CanHook(I))return false;
 auto& F=Creatures[I];F.HookTicket=Ticket;F.HookTarget=F.Position;
 Catch=FNeonCityCaughtFish();Catch.FishID=Ticket;Catch.SpeciesID=Species->SpeciesID;
 Catch.WeightKg=FMath::Lerp(Species->WeightKg.X,Species->WeightKg.Y,FMath::Clamp((F.Scale-.8f)/.4f,0.f,1.f));
 Catch.SizeCm=FMath::Clamp(float((Species->SizeCm.X+Species->SizeCm.Y)*.5f*F.Scale),float(Species->SizeCm.X),float(Species->SizeCm.Y));
 Catch.FishingPointID=HabitatID;return true;
}
bool AMarineSchool::MoveHook(int32 I,FGuid Ticket,FVector Target,bool Hooked,bool Landed)
{
 if(!IsPresent(I) || Creatures[I].HookTicket!=Ticket || !Ticket.IsValid() || Target.ContainsNaN())return false;
 if(Creatures[I].Death.Active() && (!Landed || !Creatures[I].Death.Collectible()))return false;
 auto& F=Creatures[I];F.HookTarget=GetActorTransform().InverseTransformPosition(Target);F.bHooked=Hooked;F.bLanded=Landed;
 WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;
}
bool AMarineSchool::FinishHook(int32 I,FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch,bool Keep)
{
 if(!IsPresent(I) || Creatures[I].HookTicket!=Ticket || !Ticket.IsValid())return false;
 auto& F=Creatures[I];
 if(Keep)
 {
  if(!F.bLanded || (F.Death.Active() && !F.Death.Collectible()) || !Inventory || Catch.FishID!=Ticket || Catch.SpeciesID!=Species->SpeciesID)return false;
  F.bAlive=false;
  if(!Inventory->StoreCatch(Catch)){F.bAlive=true;return false;}
 }
 F.HookTicket.Invalidate();F.bHooked=F.bLanded=false;F.Alarm=1;
 if(F.Death.Active())
 {WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;}
 // A released catch returns underwater at its last line position, then swims home.
 F.Position.X=FMath::Clamp(F.Position.X,double(-FloorExtent+50),double(FloorExtent-50));
 F.Position.Y=FMath::Clamp(F.Position.Y,double(-FloorExtent+50),double(FloorExtent-50));
 F.Position.Z=FMath::Clamp(F.Position.Z,double(FloorAt(F.Position)+FloorClearance),double(MeanSeaLevel-GetActorLocation().Z-SurfaceClearance));
 WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;
}

bool AMarineSchool::ReserveSpear(int32 I,FGuid Ticket,FVector Hit,FVector Direction,FNeonCityCaughtFish& Catch)
{
 if(!HasAuthority() || !IsPresent(I) || !Ticket.IsValid() || !bCapturable || !Species || Behaviour==EMarineBehaviour::Benthic)return false;
 auto& F=Creatures[I];if(F.HookTicket.IsValid())return false;
 if(F.Death.Active())
 {
  if(!F.Death.Collectible())return false;
  F.HookTicket=Ticket;Catch=FNeonCityCaughtFish();Catch.FishID=Ticket;Catch.SpeciesID=Species->SpeciesID;
  Catch.SizeCm=FMath::Clamp(float((Species->SizeCm.X+Species->SizeCm.Y)*.5f*F.Scale),float(Species->SizeCm.X),float(Species->SizeCm.Y));
  Catch.WeightKg=FMath::Lerp(Species->WeightKg.X,Species->WeightKg.Y,FMath::Clamp((F.Scale-.8f)/.4f,0.f,1.f));Catch.FishingPointID=HabitatID;
 }
 else if(!ReserveHook(I,Ticket,Catch))return false;
 const auto T=GetActorTransform();
 if(!F.Death.Active())F.Death.Begin(GetWorld()->GetTimeSeconds(),F.Position,F.Rotation,F.Velocity,
  T.InverseTransformPosition(Hit),T.InverseTransformVectorNoScale(Direction),Catch.SizeCm,GetTypeHash(Species->SpeciesID));
 else F.Death.Attach(F.Position,F.Rotation,T.InverseTransformPosition(Hit),T.InverseTransformVectorNoScale(Direction),Catch.SizeCm);
 F.bHooked=false;F.bLanded=false;F.TurnBend=0;
 WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;
}
bool AMarineSchool::GetSpearAttachment(int32 I,FGuid Ticket,FTransform& Pose,bool& Collectible) const
{
 if(!IsPresent(I) || !Ticket.IsValid())return false;const auto& F=Creatures[I];
 if(F.HookTicket!=Ticket || !F.Death.Active())return false;
 Pose=F.Death.Attachment(F.Position,F.Rotation)*GetActorTransform();Collectible=F.Death.Collectible();return true;
}
bool AMarineSchool::PullSpear(int32 I,FGuid Ticket,FVector Delta)
{
 FTransform Pose;bool Ready=false;if(!GetSpearAttachment(I,Ticket,Pose,Ready) || !Ready || Delta.ContainsNaN())return false;
 auto& F=Creatures[I];F.Position+=GetActorTransform().InverseTransformVectorNoScale(Delta);F.Death.Velocity*=.8f;
 WritePresentation(I,GetWorld()->GetTimeSeconds(),0,false);Bodies->MarkRenderStateDirty();return true;
}
EMarineSpearLife AMarineSchool::GetSpearLife(int32 I) const
{return Creatures.IsValidIndex(I)?Creatures[I].Death.State:EMarineSpearLife::Alive;}
