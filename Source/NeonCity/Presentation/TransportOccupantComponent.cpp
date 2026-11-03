#include "Presentation/TransportOccupantComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
#include "Misc/PackageName.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

UTransportOccupantComponent::UTransportOccupantComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Driving(TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Driving_Loop.AN_NC205C_Driving_Loop"));
 SeatedAnimation=Driving.Object;
 if(FPackageName::DoesPackageExist(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_TransportOccupant")))
 {
  static ConstructorHelpers::FClassFinder<UAnimInstance> Seated(TEXT("/Game/NeonCity/Animations/VerticalSlice/ABP_NC_TransportOccupant"));
  SeatedAnimClass=Seated.Class;
 }
}
ACharacter* UTransportOccupantComponent::GetRepresentedDriver() const { return Driver.Get(); }
bool UTransportOccupantComponent::ShowDriver(ACharacter* Character)
{
 if(!bPresentDriver || !IsValid(Character) || Character->GetWorld()!=GetWorld() || !GetOwner()->GetRootComponent()
  || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset() || !SeatedAnimation
  || !SeatedAnimation->GetSkeleton()
  || !SeatedAnimation->GetSkeleton()->IsCompatibleMesh(Character->GetMesh()->GetSkeletalMeshAsset())) return false;
 if(!DriverMesh)
 {
  DriverMesh=NewObject<USkeletalMeshComponent>(GetOwner(),MakeUniqueObjectName(GetOwner(),USkeletalMeshComponent::StaticClass(),TEXT("VisibleOccupant")));
  GetOwner()->AddInstanceComponent(DriverMesh);DriverMesh->SetupAttachment(GetOwner()->GetRootComponent());
  DriverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  DriverMesh->SetCanEverAffectNavigation(false);DriverMesh->SetGenerateOverlapEvents(false);
  DriverMesh->CanCharacterStepUpOn=ECB_No;
  DriverMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
  DriverMesh->RegisterComponent();
 }
 Driver=Character; DriverMesh->SetOwnerNoSee(false); DriverMesh->SetSkeletalMesh(Character->GetMesh()->GetSkeletalMeshAsset());
 DriverMesh->EmptyOverrideMaterials();
 for(int32 Index=0;Index<Character->GetMesh()->GetNumMaterials();++Index)
  DriverMesh->SetMaterial(Index,Character->GetMesh()->GetMaterial(Index));
 DriverMesh->SetRelativeTransform(FTransform(FRotator(SeatReclineDegrees,SeatYaw,0).Quaternion()*FRotator(0,-90,0).Quaternion()));
 DriverMesh->SetComponentTickEnabled(true);DriverMesh->SetHiddenInGame(false);DriverMesh->SetVisibility(true);
 DriverMesh->PlayAnimation(SeatedAnimation,true);
 // Evaluate once to align the authored pelvis to this vehicle's physical seat.
 // Animation thereafter uses the normal skeletal component tick and attachment.
 DriverMesh->TickAnimation(0,false);DriverMesh->RefreshBoneTransforms();
 const FVector Hip=DriverMesh->GetSocketTransform(TEXT("pelvis"),RTS_Component).GetLocation();
 DriverMesh->SetRelativeLocation(SeatHipLocation-DriverMesh->GetRelativeRotation().RotateVector(Hip));
 if(SeatedAnimClass) DriverMesh->SetAnimInstanceClass(SeatedAnimClass);
 return true;
}
void UTransportOccupantComponent::HideDriver()
{
 Driver.Reset();
 if(DriverMesh)
 {
  DriverMesh->SetHiddenInGame(true);DriverMesh->SetComponentTickEnabled(false);
  DriverMesh->SetSkeletalMesh(nullptr);
 }
}
void UTransportOccupantComponent::EndPlay(EEndPlayReason::Type Reason)
{ HideDriver();Super::EndPlay(Reason); }
