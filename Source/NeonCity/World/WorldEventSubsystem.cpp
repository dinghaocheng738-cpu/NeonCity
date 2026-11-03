#include "World/WorldEventSubsystem.h"
#include "Engine/World.h"
FGuid UWorldEventSubsystem::Publish(ENeonCityWorldEventType Type,FVector Location,AActor* SourceActor,AController* Instigator,float Magnitude,FName ContextID)
{
 if(!GetWorld() || GetWorld()->GetNetMode()==NM_Client || Location.ContainsNaN() || !FMath::IsFinite(Magnitude)) return FGuid();
 if((SourceActor && SourceActor->GetWorld()!=GetWorld()) || (Instigator && Instigator->GetWorld()!=GetWorld())) return FGuid();
 FNeonCityWorldEvent Event; Event.EventId=FGuid::NewGuid(); Event.Type=Type; Event.Location=Location; Event.SourceActor=SourceActor;
 Event.Instigator=Instigator; Event.Magnitude=FMath::Clamp(Magnitude,0.f,100000.f); Event.ContextID=ContextID;
 Event.Timestamp=GetWorld()->GetTimeSeconds();
 if(Recent.Num()>=Capacity) Recent.RemoveAt(0);
 Recent.Add(Event); ++EventCount; OnWorldEvent.Broadcast(Event); return Event.EventId;
}
int32 UWorldEventSubsystem::CountOfType(ENeonCityWorldEventType Type) const
{ int32 Count=0; for(const auto& Event:Recent) if(Event.Type==Type) ++Count; return Count; }
void UWorldEventSubsystem::ResetForLoad() { Recent.Reset(); EventCount=0; }