#pragma once
#include "CoreMinimal.h"
#include "Police/SuspectDescriptor.h"
class AActor;
class UWorld;
namespace NeonCityObservation
{
 bool Visible(UWorld* World,AActor* Observer,FVector Eye,FVector Forward,float Range,float HalfAngle,AActor* Candidate);
 FSuspectDescriptor Capture(AActor* Candidate,EPoliceEvidenceSource Source);
 bool Matches(AActor* Candidate,const FSuspectDescriptor& Known);
 // Reads the local camera solely to reject unsafe/pop-in spawns. Never provides a pursuit destination.
 bool SafeDeployment(UWorld* World,FVector Position,float MinimumDistance=2000);
}
