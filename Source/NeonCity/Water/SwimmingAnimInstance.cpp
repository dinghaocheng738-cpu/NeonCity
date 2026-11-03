#include "Water/SwimmingAnimInstance.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/SwimmingComponent.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"

void USwimmingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const auto* Character=Cast<ACharacter>(TryGetPawnOwner());
    const auto* Move=Character?Character->GetCharacterMovement():nullptr;
    const auto* Body=Character?Character->FindComponentByClass<UDamageableCharacterComponent>():nullptr;
    const auto* Water=Character?Character->FindComponentByClass<UWaterInteractionComponent>():nullptr;
    bNativeSwimming=Move && Move->IsSwimming() && (!Body || Body->CanAct());
    const FVector Velocity=Move?Move->Velocity:FVector::ZeroVector;
    // Pose choice uses body depth, not animated head depth: otherwise selecting
    // a diving pose lowers the head and keeps its own condition true forever.
    // Oxygen still uses the actual filtered head in WaterInteractionComponent.
    const auto* Swimming=Character?Character->FindComponentByClass<USwimmingComponent>():nullptr;
    const bool bSurface=Swimming && Swimming->IsFollowingSurface();
    const bool bUnder=(Swimming && Swimming->HasDiveIntent()) || (!bSurface && Water && Character && Water->GetSample().DepthAt(Character->GetActorLocation())>
        Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.2f);
    const float Delta=FMath::Clamp(DeltaSeconds,0.f,.1f);
    const auto* Kit=Character?Character->FindComponentByClass<UDivingComponent>():nullptr;
    const bool ShortPush=Swimming && Swimming->IsSeafloorPushOff();
    DiveAlpha=FMath::FInterpTo(DiveAlpha,Kit && Kit->IsEquipped() && bUnder && !ShortPush?1.f:0.f,Delta,5.f);
    WaterAlpha=FMath::FInterpConstantTo(WaterAlpha,bNativeSwimming?1.f:0.f,Delta,4.f);
    MovingAlpha=FMath::FInterpTo(MovingAlpha,ShortPush?0.f:FMath::Clamp(Velocity.Size2D()/120.f,0.f,1.f),Delta,5.f);
    UnderwaterAlpha=FMath::FInterpTo(UnderwaterAlpha,bUnder && !ShortPush?1.f:0.f,Delta,5.f);
    AscendAlpha=FMath::FInterpTo(AscendAlpha,bUnder && !ShortPush?FMath::Clamp((Velocity.Z-20.f)/100.f,0.f,1.f):0.f,Delta,5.f);
    // Following a falling wave is surface treading, not an intentional dive.
    // A dive pose lowers the animated mouth and would otherwise falsely consume oxygen.
    DescendAlpha=FMath::FInterpTo(DescendAlpha,bUnder && !ShortPush?FMath::Clamp((-Velocity.Z-20.f)/100.f,0.f,1.f):0.f,Delta,5.f);
    SwimPlayRate=FMath::Clamp(.80f+Velocity.Size()/600.f,.8f,1.3f);
    VisibleSwimState=!bNativeSwimming?FName(NAME_None):FName(DescendAlpha>.45f?TEXT("Descend"):AscendAlpha>.45f?TEXT("Ascend")
        :bUnder?TEXT("Underwater"):MovingAlpha>.35f?TEXT("Forward"):TEXT("Tread"));
    const bool Floor=Swimming && Swimming->IsSeafloorStanding();
    const bool Equipped=Kit && Kit->IsEquipped() && (!Body || Body->CanAct()) && (bNativeSwimming || Floor);
    EquipmentWaterAlpha=FMath::FInterpConstantTo(EquipmentWaterAlpha,Equipped?1.f:0.f,Delta,3.2f);
    const float Yaw=Character?Character->GetActorRotation().Yaw:0;
    const float Turn=DeltaSeconds>0?FMath::FindDeltaAngleDegrees(PreviousYaw,Yaw)/DeltaSeconds:0;
    PreviousYaw=Yaw;SmoothedTurn=FMath::FInterpTo(SmoothedTurn,FMath::Clamp(Turn,-180.f,180.f),Delta,4.f);
    const float Speed=Velocity.Size2D();
    // Current and wave velocity do not imply intentional forward swimming.
    const bool Driving=Move && Move->GetCurrentAcceleration().Size2D()>12;
    SurfaceDrive=FMath::FInterpConstantTo(SurfaceDrive,Driving?1.f:0.f,Delta,Driving?3.f:2.5f);
    if(!bNativeSwimming || bUnder)bSurfaceTravel=false;
    else if(SurfaceDrive>.6f)bSurfaceTravel=true;
    else if(SurfaceDrive<.2f)bSurfaceTravel=false;
    OceanPose=Floor?(Speed>8?10:9):ShortPush && Swimming->GetPushOffPoseWeight()>.3f?11
      :!bUnder?(bSurfaceTravel?1:0):Velocity.Z>45?7:Velocity.Z<-45?8
      :SmoothedTurn<-35?5:SmoothedTurn>35?6:Speed>125?4:Speed>80?3:Speed>28?2:19;
    const auto* Equipment=Character?Character->FindComponentByClass<UDiveEquipmentComponent>():nullptr;
    if(Equipped && bUnder && Equipment && Equipment->IsSpeargunDeployed())
    {
        const int32 Hand=Equipment->GetHandPose();
        OceanPose=Floor?(Speed>8?21:20):Hand==3?15:Hand==2?14:Equipment->IsAiming()?13:12;
    }
    if(Equipped && bUnder && Equipment && Equipment->IsDPVDeployed())OceanPose=Floor?(Speed>8?18:17):16;
    OceanPoseRate=!bUnder && !Floor?(bSurfaceTravel?1.f:.92f):Floor?FMath::Clamp(Speed/78.f,.45f,1.3f):FMath::Clamp(.8f+Speed/520.f,.8f,1.4f);
    if(bNativeSwimming && !bUnder)VisibleSwimState=bSurfaceTravel?TEXT("Forward"):TEXT("Tread");
    if(Equipped && bUnder)
    {
        static const FName Names[]={TEXT("Tread"),TEXT("SurfaceForward"),TEXT("Slow"),TEXT("Forward"),TEXT("Fast"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Ascend"),TEXT("Descend"),TEXT("FloorStand"),TEXT("FloorWalk"),TEXT("PushOff"),TEXT("SpeargunHold"),TEXT("SpearAim"),TEXT("SpearUse"),TEXT("Recover"),TEXT("DPVHold"),TEXT("FloorDPVStand"),TEXT("FloorDPVWalk"),TEXT("Neutral"),TEXT("FloorSpearStand"),TEXT("FloorSpearWalk")};
        VisibleSwimState=Names[OceanPose];
    }
    if(OceanPose!=PreviousOceanPose){PoseStarted=GetWorld()->GetTimeSeconds();PreviousOceanPose=OceanPose;}
    OceanActionTime=GetWorld()->GetTimeSeconds()-PoseStarted;
}
