#include "Adventure/NacreRoofComponent.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Vehicles/VehicleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/AsyncInitBodyHelper.h"
#include "Chaos/Collision/CollisionConstraintFlags.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundWave.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"

// IDs and solver are immutable. Commands retain this native state, never a
// component/proxy pointer; Installed is accessed only on the physics thread.
struct FNacreRoofCollisionPair
{
 Chaos::FPhysicsSolver* Solver=nullptr;
 Chaos::FUniqueIdx Chassis,Panel;
 FDelegateHandle UnregisteredHandle;
 TAtomic<bool> RemovalQueued{false};
 bool Installed=false;
};

namespace NacreRoof
{
 const FString Base=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Nacre/");
 float Phase(float Seconds,float Start,float End)
 { const float T=FMath::Clamp((Seconds-Start)/(End-Start),0.f,1.f);return T*T*(3-2*T); }
 template<class T> T* Asset(const FString& Path)
 { return FPackageName::DoesPackageExist(Path)?LoadObject<T>(nullptr,*Path):nullptr; }
 void RemoveContactPair(const TSharedRef<FNacreRoofCollisionPair,ESPMode::ThreadSafe>& Pair)
 {
  if(Pair->RemovalQueued.Exchange(true))return;
  Pair->Solver->EnqueueCommandImmediate([Pair]()
  {
   if(!Pair->Installed)return;
   auto* Chassis=Pair->Solver->GetParticleProxy_PT(Pair->Chassis);
   auto* Panel=Pair->Solver->GetParticleProxy_PT(Pair->Panel);
   if(Chassis && Panel)
    Pair->Solver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().RemoveIgnoreCollisions(Chassis->GetHandle_LowLevel(),Panel->GetHandle_LowLevel());
   Pair->Installed=false;
  });
 }
}
UNacreRoofComponent::UNacreRoofComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;
 auto Part=[this](const TCHAR* Name)
 { auto* P=CreateDefaultSubobject<UStaticMeshComponent>(Name);P->SetupAttachment(this);P->SetMobility(EComponentMobility::Movable);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetCanEverAffectNavigation(false);return P; };
 FrontPanel=Part(TEXT("NacreRoofFront"));RearPanel=Part(TEXT("NacreRoofRear"));BayCover=Part(TEXT("NacreRoofBayCover"));
 LeftWindow=Part(TEXT("NacreWindowLeft"));RightWindow=Part(TEXT("NacreWindowRight"));
 LeftLatch=Part(TEXT("NacreLatchLeft"));RightLatch=Part(TEXT("NacreLatchRight"));
 LeftCarriage=Part(TEXT("NacreRoofCarriageLeft"));RightCarriage=Part(TEXT("NacreRoofCarriageRight"));
 for(int32 I=0;I<4;++I){SwingArms.Add(Part(*FString::Printf(TEXT("RoofSwingArm%d"),I)));PhysicalCarriers.Add(Part(*FString::Printf(TEXT("RoofPhysicalCarrier%d"),I)));}
 FixedGuides=Part(TEXT("RoofFixedGuides"));RoofMounts=Part(TEXT("RoofPhysicalMounts"));
 Motor=CreateDefaultSubobject<UAudioComponent>(TEXT("NacreRoofMotor"));Motor->SetupAttachment(this);Motor->bAutoActivate=false;
 LatchAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("NacreRoofLatchAudio"));LatchAudio->SetupAttachment(this);LatchAudio->bAutoActivate=false;
}
void UNacreRoofComponent::OnRegister()
{
 Super::OnRegister();
 if(IsTemplate() || !GetWorld())return;
 for(auto* Part:{FrontPanel.Get(),RearPanel.Get(),BayCover.Get(),LeftWindow.Get(),RightWindow.Get(),LeftLatch.Get(),RightLatch.Get(),LeftCarriage.Get(),RightCarriage.Get()})
  if(Part && !Part->IsRegistered())Part->RegisterComponentWithWorld(GetWorld());
 for(auto* Audio:{Motor.Get(),LatchAudio.Get()})if(Audio && !Audio->IsRegistered())Audio->RegisterComponentWithWorld(GetWorld());
 for(auto* Part:{FixedGuides.Get(),RoofMounts.Get()})if(!Part->IsRegistered())Part->RegisterComponentWithWorld(GetWorld());
 for(const auto& Parts:{SwingArms,PhysicalCarriers})for(const auto& Part:Parts)if(!Part->IsRegistered())Part->RegisterComponentWithWorld(GetWorld());
 ConfigureChassisContactFilter();
}
void UNacreRoofComponent::OnUnregister()
{
 ClearChassisContactFilter();
 if(auto* Car=Cast<AIslandSupercar>(GetOwner()))Car->GetMesh()->OnComponentPhysicsStateChanged.RemoveDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 FrontPanel->OnComponentPhysicsStateChanged.RemoveDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 Super::OnUnregister();
}
void UNacreRoofComponent::Configure(bool Enabled,UIslandSupercarPresentationData* Presentation)
{
 // Mesh/collision changes can recreate the panel body. Release our exact old
 // pair before changing it; never clear all chassis ignores (wheels use them).
 ClearChassisContactFilter();
 PresentationProfile=Presentation && Presentation->ResolveRequiredAssets()?Presentation:nullptr;
 // Fixed-roof presentation deliberately has no leaves/roof links. An editor
 // flag mismatch must never index that absent mechanism or enable its input.
 bEnabled=Enabled && (!PresentationProfile || PresentationProfile->bRequiresRetractableRoof);SetComponentTickEnabled(IsCycling());SetVisibility(bEnabled,true);
 FrontPanel->SetCollisionEnabled(bEnabled && PresentationProfile?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 FrontPanel->SetCollisionResponseToAllChannels(ECR_Block);
 FrontPanel->SetGenerateOverlapEvents(false);
 UpdateMotionInterlock();
 if(!bEnabled){Motor->Stop();return;}
 if(PresentationProfile)
 {
  FrontPanel->SetStaticMesh(PresentationProfile->Roof.LoadSynchronous());BayCover->SetStaticMesh(PresentationProfile->Tonneau.LoadSynchronous());
  LeftWindow->SetStaticMesh(PresentationProfile->Backlight.LoadSynchronous());
  LeftLatch->SetStaticMesh(PresentationProfile->Leaves[0].Mesh.LoadSynchronous());RightLatch->SetStaticMesh(PresentationProfile->Leaves[1].Mesh.LoadSynchronous());
  FixedGuides->SetStaticMesh(PresentationProfile->FixedGuides.LoadSynchronous());RoofMounts->SetStaticMesh(PresentationProfile->RoofMounts.LoadSynchronous());
  for(auto* P:{RearPanel.Get(),RightWindow.Get(),LeftCarriage.Get(),RightCarriage.Get()}){P->SetStaticMesh(nullptr);P->SetVisibility(false);}
  for(int32 I=0;I<4;++I){SwingArms[I]->SetStaticMesh(PresentationProfile->Arm.LoadSynchronous());PhysicalCarriers[I]->SetStaticMesh(PresentationProfile->Carrier.LoadSynchronous());}
 }
 else
 {
 const TPair<UStaticMeshComponent*,const TCHAR*> Parts[]={
 {FrontPanel,TEXT("SM_Nacre_RoofFront")},{RearPanel,TEXT("SM_Nacre_RoofRear")},{BayCover,TEXT("SM_Nacre_BayCover")},
 {LeftWindow,TEXT("SM_Nacre_WindowLeft")},{RightWindow,TEXT("SM_Nacre_WindowRight")},
 {LeftLatch,TEXT("SM_Nacre_Latch")},{RightLatch,TEXT("SM_Nacre_Latch")},
 {LeftCarriage,TEXT("SM_Nacre_Carriage")},{RightCarriage,TEXT("SM_Nacre_Carriage")}};
 for(const auto& P:Parts)
 {
  P.Key->SetStaticMesh(NacreRoof::Asset<UStaticMesh>(NacreRoof::Base+P.Value));
  if(GetOwner() && GetOwner()->HasActorBegunPlay() && !P.Key->IsRegistered())P.Key->RegisterComponent();
 }
 for(auto* P:{FixedGuides.Get(),RoofMounts.Get()}){P->SetStaticMesh(nullptr);P->SetVisibility(false);}
 for(const auto& Extra:{SwingArms,PhysicalCarriers})for(const auto& P:Extra){P->SetStaticMesh(nullptr);P->SetVisibility(false);}
 }
 Motor->SetSound(NacreRoof::Asset<USoundWave>(NacreRoof::Base+TEXT("Audio/SW_NacreRoofMotor")));
 LatchAudio->SetSound(NacreRoof::Asset<USoundWave>(NacreRoof::Base+TEXT("Audio/SW_NacreRoofLatch")));
 FSoundAttenuationSettings Att;Att.bAttenuate=true;Att.bSpatialize=true;Att.AttenuationShapeExtents=FVector(130);Att.FalloffDistance=1300;
 Motor->AdjustAttenuation(Att);LatchAudio->AdjustAttenuation(Att);Motor->SetVolumeMultiplier(.32f);LatchAudio->SetVolumeMultiplier(.42f);
 Motor->SetRelativeLocation(FVector(-145,0,65));LatchAudio->SetRelativeLocation(FVector(48,0,103));ApplyPose();
 ConfigureChassisContactFilter();
}
void UNacreRoofComponent::ConfigureChassisContactFilter()
{
 auto* Car=Cast<AIslandSupercar>(GetOwner());
 if(!bEnabled || !PresentationProfile || !Car || !Car->HasActorBegunPlay() || bContactFilterEndingPlay)
 {
  ClearChassisContactFilter();return;
 }
 Car->GetMesh()->OnComponentPhysicsStateChanged.AddUniqueDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 FrontPanel->OnComponentPhysicsStateChanged.AddUniqueDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 auto* ChassisBody=Car->GetMesh()->GetBodyInstance(TEXT("Root"));
 auto* PanelBody=FrontPanel->GetBodyInstance();
 auto* Chassis=ChassisBody?ChassisBody->GetPhysicsActor():nullptr;
 auto* Panel=PanelBody?PanelBody->GetPhysicsActor():nullptr;
 auto* Solver=Chassis?Chassis->GetSolver<Chaos::FPhysicsSolver>():nullptr;
 const auto* ManagerCVar=IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.EnableCollisionManager"));
 const bool NativeSupported=ManagerCVar && ManagerCVar->GetBool() && Solver && Panel && Panel->GetSolver<Chaos::FPhysicsSolver>()==Solver
  && Chassis->GetParticle_LowLevel()->CastToRigidParticle() && Panel->GetParticle_LowLevel()->CastToRigidParticle();
 if(NativeSupported)
 {
  const auto ChassisID=Chassis->GetGameThreadAPI().UniqueIdx(),PanelID=Panel->GetGameThreadAPI().UniqueIdx();
  if(NativeChassisContactFilter && !NativeChassisContactFilter->RemovalQueued.Load()
   && NativeChassisContactFilter->Solver==Solver && NativeChassisContactFilter->Chassis==ChassisID && NativeChassisContactFilter->Panel==PanelID)return;
  ClearChassisContactFilter();
  auto Pair=MakeShared<FNacreRoofCollisionPair,ESPMode::ThreadSafe>();
  Pair->Solver=Solver;Pair->Chassis=ChassisID;Pair->Panel=PanelID;
  NativeChassisContactFilter=Pair;
  {
   // UnregisterObject holds this same lock while broadcasting. Async body
   // destruction may invoke the callback on a worker, despite its GT name.
   Chaos::Private::FChaosAsyncInitBodyWriteScopeLock Lock(Solver->GetExternalDataLock_External());
   const TWeakPtr<FNacreRoofCollisionPair,ESPMode::ThreadSafe> WeakPair=Pair;
   Pair->UnregisteredHandle=Solver->AddParticleUnregisteredGTCallback(FSolverParticleUnregisteredGT::FDelegate::CreateLambda([WeakPair](Chaos::FSingleParticlePhysicsProxy*,Chaos::FUniqueIdx ID)
   {
    if(auto LivePair=WeakPair.Pin())if(ID==LivePair->Chassis || ID==LivePair->Panel)NacreRoof::RemoveContactPair(LivePair.ToSharedRef());
   }));
  }
  // A collision-only joint still enters Chaos's constraint/island path and
  // prevents these initially awake parked cars settling. A native ignore pair
  // preserves the same self-contact exclusion without a joint or sleep edits.
  Solver->EnqueueCommandImmediate([Pair]()
  {
   if(Pair->RemovalQueued.Load())return;
   auto* ChassisProxy=Pair->Solver->GetParticleProxy_PT(Pair->Chassis);
   auto* PanelProxy=Pair->Solver->GetParticleProxy_PT(Pair->Panel);
   auto* ChassisParticle=ChassisProxy?ChassisProxy->GetHandle_LowLevel():nullptr;
   auto* PanelParticle=PanelProxy?PanelProxy->GetHandle_LowLevel():nullptr;
   if(ChassisParticle && PanelParticle)
   {
    Pair->Solver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().AddIgnoreCollisions(ChassisParticle,PanelParticle);
    Pair->Installed=true;
   }
  });
 }
 else
 {
  // Preserve the existing collision contract if this solver/body combination
  // cannot use native pair filtering. External Vehicle/Pawn/world still block.
  if(NativeChassisContactFilter)ClearChassisContactFilter();
  if(!ChassisContactFilter)
  {
   ChassisContactFilter=NewObject<UPhysicsConstraintComponent>(GetOwner(),TEXT("RoofChassisContactFilter"),RF_Transient);
   ChassisContactFilter->SetLinearXLimit(LCM_Free,0);
   ChassisContactFilter->SetLinearYLimit(LCM_Free,0);
   ChassisContactFilter->SetLinearZLimit(LCM_Free,0);
   ChassisContactFilter->SetAngularSwing1Limit(ACM_Free,0);
   ChassisContactFilter->SetAngularSwing2Limit(ACM_Free,0);
   ChassisContactFilter->SetAngularTwistLimit(ACM_Free,0);
   ChassisContactFilter->SetProjectionEnabled(false);
   ChassisContactFilter->SetDisableCollision(true);
   ChassisContactFilter->RegisterComponent();
  }
  ChassisContactFilter->SetConstrainedComponents(Car->GetMesh(),TEXT("Root"),FrontPanel,NAME_None);
 }
}
void UNacreRoofComponent::ClearChassisContactFilter()
{
 if(NativeChassisContactFilter)
 {
  auto Pair=MoveTemp(NativeChassisContactFilter);
  {
   Chaos::Private::FChaosAsyncInitBodyWriteScopeLock Lock(Pair->Solver->GetExternalDataLock_External());
   Pair->Solver->RemoveParticleUnregisteredGTCallback(Pair->UnregisteredHandle);
   // Queue before releasing the unregister lock, so async destruction cannot
   // pass the removed listener and retire either proxy ahead of our removal.
   NacreRoof::RemoveContactPair(Pair.ToSharedRef());
  }
 }
 if(ChassisContactFilter){ChassisContactFilter->DestroyComponent();ChassisContactFilter=nullptr;}
}
void UNacreRoofComponent::HandleContactPhysicsStateChanged(UPrimitiveComponent*,EComponentPhysicsStateChange StateChange)
{
 if(StateChange==EComponentPhysicsStateChange::Created)ConfigureChassisContactFilter();
}
void UNacreRoofComponent::BeginPlay(){Super::BeginPlay();bContactFilterEndingPlay=false;Configure(bEnabled,PresentationProfile);}
void UNacreRoofComponent::UpdateMotionInterlock()
{
 if(auto* Car=Cast<AIslandSupercar>(GetOwner()))Car->GetDrivingComponent()->SetMechanicalMotionInterlock(IsCycling());
}
void UNacreRoofComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 bContactFilterEndingPlay=true;ClearChassisContactFilter();
 if(auto* Car=Cast<AIslandSupercar>(GetOwner()))Car->GetMesh()->OnComponentPhysicsStateChanged.RemoveDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 FrontPanel->OnComponentPhysicsStateChanged.RemoveDynamic(this,&UNacreRoofComponent::HandleContactPhysicsStateChanged);
 if(auto* Car=Cast<AIslandSupercar>(GetOwner()))Car->GetDrivingComponent()->SetMechanicalMotionInterlock(false);
 Motor->Stop();Super::EndPlay(Reason);
}
float UNacreRoofComponent::GetCycleDuration() const{return PresentationProfile?PresentationProfile->CycleSeconds:CycleSeconds;}
float UNacreRoofComponent::GetOperationSpeedKmh() const
{
 const auto* Car=Cast<AIslandSupercar>(GetOwner());
 return Car?float(Car->GetMesh()->GetPhysicsLinearVelocity().Size()*.036):MaximumOperationSpeedKmh+1.f;
}
void UNacreRoofComponent::Notify(const FText& Message) const
{
 if(const auto* Car=Cast<AIslandSupercar>(GetOwner()))if(auto* PC=Cast<APlayerController>(Car->GetController()))
  if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ShowNotice(Message);
}
bool UNacreRoofComponent::RequestToggle()
{
 auto* Car=Cast<AIslandSupercar>(GetOwner());if(!bEnabled || !Car || !Car->IsPlayerControlled())return false;
 const float Speed=GetOperationSpeedKmh();
 if(!FMath::IsFinite(Speed) || FMath::Abs(Speed)>(PresentationProfile?PresentationProfile->SafeOperationSpeedKmh:MaximumOperationSpeedKmh))
 {Notify(NSLOCTEXT("NeonCity","NacreRoofSpeed","Roof: slow below 8 km/h and stop safely."));return false;}
 bTargetOpen=!bTargetOpen;bUnsafeNotice=false;
 SetComponentTickEnabled(IsCycling());
 UpdateMotionInterlock();
 Notify(bTargetOpen?NSLOCTEXT("NeonCity","NacreRoofOpening","Roof opening · R to reverse · brakes held"):
 NSLOCTEXT("NeonCity","NacreRoofClosing","Roof closing · R to reverse · brakes held"));
 if(LatchAudio->Sound)LatchAudio->Play();return true;
}
void UNacreRoofComponent::RestoreState(float SavedProgress,bool SavedTargetOpen)
{
 Motor->Stop();LatchAudio->Stop();
 Progress=bEnabled?FMath::Clamp(FMath::IsFinite(SavedProgress)?SavedProgress:0.f,0.f,1.f):0.f;bTargetOpen=bEnabled && SavedTargetOpen;
 bWasCycling=false;bUnsafeNotice=false;UpdateMotionInterlock();ApplyPose();
 // ResetVehicleState recreates the chassis body before persistence restores
 // the roof. Rebind the collision pair to that new native physics body.
 ConfigureChassisContactFilter();
 SetComponentTickEnabled(IsCycling());
}
FText UNacreRoofComponent::GetStatus() const
{
 if(IsCycling() && bUnsafeNotice)return NSLOCTEXT("NeonCity","NacreRoofPausedHUD","Roof paused: speed unsafe · brakes held");
 if(IsCycling())return FText::Format(NSLOCTEXT("NeonCity","NacreRoofWorking","Roof {0}% · R reverse"),FText::AsNumber(FMath::RoundToInt(Progress*100)));
 return bTargetOpen?NSLOCTEXT("NeonCity","NacreRoofOpen","Roof open · R close"):NSLOCTEXT("NeonCity","NacreRoofClosed","Roof closed · R open");
}
void UNacreRoofComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Dt,Type,Function);auto* Car=Cast<AIslandSupercar>(GetOwner());if(!bEnabled || !Car)return;
 const bool Cycling=IsCycling();
 if(Cycling)
 {
  // The drive consumer holds brakes even if input/cruise updates later. The
  // interlock persists through an unsafe-speed pause, exit and restored motion.
  const float Speed=GetOperationSpeedKmh();
  if(FMath::IsFinite(Speed) && FMath::Abs(Speed)<=(PresentationProfile?PresentationProfile->SafeOperationSpeedKmh:MaximumOperationSpeedKmh))
  {
   bUnsafeNotice=false;
   Progress=FMath::FInterpConstantTo(Progress,bTargetOpen?1.f:0.f,Dt,1/GetCycleDuration());ApplyPose();
   if(Motor->Sound && !Motor->IsPlaying())Motor->Play();
  }
  else
  {
   Motor->Stop();if(!bUnsafeNotice){Notify(NSLOCTEXT("NeonCity","NacreRoofPaused","Roof paused until speed is safe."));bUnsafeNotice=true;}
  }
 }
 if(Cycling && !IsCycling())
 {
  Motor->Stop();if(LatchAudio->Sound)LatchAudio->Play();Notify(GetStatus());
 }
 bWasCycling=IsCycling();
 UpdateMotionInterlock();
 if(!IsCycling())SetComponentTickEnabled(false);
}
void UNacreRoofComponent::ApplyPose()
{
 if(!bEnabled)return;
 if(PresentationProfile)
 {
  const auto& P=*PresentationProfile;
  const FVector Shift=P.PanelTranslationCm(Progress);FrontPanel->SetRelativeLocationAndRotation(Shift,FRotator::ZeroRotator);
  RoofMounts->SetRelativeLocationAndRotation(Shift,FRotator::ZeroRotator);FixedGuides->SetRelativeTransform(FTransform::Identity);
  const float Cover=P.CoverPeakDegrees*P.Phase(Progress,P.CoverOpenPhase)*(1-P.Phase(Progress,P.CoverClosePhase));
  const FQuat CoverRotation(FVector::RightVector,FMath::DegreesToRadians(-Cover));
  BayCover->SetRelativeLocationAndRotation(P.CoverPivotCm,CoverRotation);
  LeftWindow->SetRelativeLocationAndRotation(FVector(0,0,-P.BacklightDropCm*P.Phase(Progress,P.BacklightLowerPhase)+P.BacklightAeroReturnCm*P.Phase(Progress,P.BacklightAeroPhase)),FRotator::ZeroRotator);
  for(int32 I=0;I<4;++I)
  {
   const FVector Base=P.Anchors[I].BodyPinCm+FVector(0,0,P.CarrierHeightCm(Progress));
   PhysicalCarriers[I]->SetRelativeLocationAndRotation(Base,FRotator::ZeroRotator);
   SwingArms[I]->SetRelativeLocationAndRotation(Base,FQuat(FVector::RightVector,FMath::DegreesToRadians(P.ArmAngleDegrees(Progress))));
  }
  for(int32 I=0;I<2;++I)
  {
   const auto& Leaf=P.Leaves[I];const float Angle=Leaf.MaximumAngleDegrees*P.Phase(Progress,Leaf.OpeningPhase)*(1-P.Phase(Progress,Leaf.ClosingPhase));
   auto* Part=I?RightLatch.Get():LeftLatch.Get();
   Part->SetRelativeLocationAndRotation(P.CoverPivotCm+CoverRotation.RotateVector(Leaf.CoverLocalHingeCm),CoverRotation*FQuat(FVector::ForwardVector,FMath::DegreesToRadians(Angle)));
  }
  return;
 }
 const float S=Progress*CycleSeconds;
 const float Unlatch=NacreRoof::Phase(S,0,.6f),Window=NacreRoof::Phase(S,.6f,1.4f);
 const float Cover=NacreRoof::Phase(S,1.4f,2.4f)*(1-NacreRoof::Phase(S,6.6f,7.6f));
 const float Lift=NacreRoof::Phase(S,2.4f,2.85f),Nest=NacreRoof::Phase(S,2.85f,4.2f),Slide=NacreRoof::Phase(S,4.2f,5.7f),Lower=NacreRoof::Phase(S,5.7f,6.6f);
 FrontPanel->SetRelativeLocation(FVector(15-70*Nest-80*Slide,0,103-40*Lower));
 RearPanel->SetRelativeLocation(FVector(-55-80*Slide,0,103+10*Lift-40*Lower));
 BayCover->SetRelativeLocationAndRotation(FVector(-193,0,81),FRotator(70*Cover,0,0));
 LeftWindow->SetRelativeLocation(FVector(0,0,-49*Window));RightWindow->SetRelativeLocation(FVector(0,0,-49*Window));
 LeftLatch->SetRelativeLocationAndRotation(FVector(50,-57,102),FRotator(0,90*Unlatch*(1-NacreRoof::Phase(S,6.6f,7.6f)),0));
 RightLatch->SetRelativeLocationAndRotation(FVector(50,57,102),FRotator(0,-90*Unlatch*(1-NacreRoof::Phase(S,6.6f,7.6f)),0));
 LeftCarriage->SetRelativeLocation(FVector(-55-80*Slide,-61,103+10*Lift-40*Lower));
 RightCarriage->SetRelativeLocation(FVector(-55-80*Slide,61,103+10*Lift-40*Lower));
}
