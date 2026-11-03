#include "Presentation/TransportOccupantAnimInstance.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UTransportOccupantAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);
 const auto* Owner=GetOwningActor();const auto* Mesh=GetSkelMeshComponent();
 const auto* Seat=Owner?Owner->FindComponentByClass<UTransportOccupantComponent>():nullptr;
 if(!Seat || !Mesh)return;
 const FTransform Relative=Mesh->GetRelativeTransform();
 auto Convert=[&](FVector Point){return Relative.InverseTransformPosition(Point);};
 LeftHand=Convert(Seat->LeftHandTarget);RightHand=Convert(Seat->RightHandTarget);
 LeftFoot=Convert(Seat->LeftFootTarget);RightFoot=Convert(Seat->RightFootTarget);
 LeftElbow=Convert(Seat->SeatHipLocation+FVector(5,-70,30));
 RightElbow=Convert(Seat->SeatHipLocation+FVector(5,70,30));
 LeftKnee=Convert(Seat->SeatHipLocation+FVector(120,-20,10));
 RightKnee=Convert(Seat->SeatHipLocation+FVector(120,20,10));
}
