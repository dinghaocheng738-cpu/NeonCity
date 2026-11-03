#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Adventure/NacreRoofComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"

URoadCruiseComponent::URoadCruiseComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
ANeonCityBaseVehicle* URoadCruiseComponent::Vehicle() const {return Cast<ANeonCityBaseVehicle>(GetOwner());}
bool URoadCruiseComponent::IsAvailable() const
{const auto* V=Vehicle();return V && V->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::SportsCar;}
void URoadCruiseComponent::Notify(const FString& Text)
{
 auto* V=Vehicle();auto* PC=V?Cast<APlayerController>(V->GetController()):nullptr;
 if(auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr)Nav->ShowNotice(FText::FromString(Text));
}
void URoadCruiseComponent::Toggle()
{
 if(bActive){Cancel(TEXT("Driver switched off"));return;}
 auto* V=Vehicle();if(!IsAvailable() || !V->IsPlayerControlled())return;
 auto* PC=Cast<APlayerController>(V->GetController());auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr;
 if(auto* Roof=V->FindComponentByClass<UNacreRoofComponent>();Roof && Roof->IsCycling())
 {Reason=TEXT("Roof is moving; cruise unavailable");Notify(Reason);return;}
 if(!Nav || V->GetDrivingComponent()->GetSpeedKmh()<-1 || V->GetDrivingComponent()->GetEngineState()!=ENeonCityEngineState::Running)
 {Reason=TEXT("Cruise requires forward drive and an available road");Notify(Reason);return;}
 Nav->RefreshRoadGuide();const auto& Destination=Nav->GetDestination();
 FVector Goal=Destination.MovingTarget.IsValid()?Destination.MovingTarget->GetActorLocation():Destination.Location;
 const FVector* GoalPtr=Destination.UseRoadRoute && !Destination.DestinationID.IsNone()?&Goal:nullptr;
 if(!Nav->GetRoadGuide().FindDriveRoute(V->GetActorLocation(),V->GetActorForwardVector(),GoalPtr,Route,RoadSpeedLimitKmh,bLooped,&PointSpeedLimits))
 {Reason=GoalPtr?TEXT("No connected road to destination; cruise refused"):TEXT("Align with an authored road before enabling cruise");Notify(Reason);return;}
 Distances.Reset();Distances.Add(0);for(int32 I=1;I<Route.Num();++I)Distances.Add(Distances.Last()+FVector::Distance(Route[I-1],Route[I]));
 if(Distances.Last()<350){Reason=TEXT("Road ends too close to enable cruise");Notify(Reason);return;}
 if(!MeasureAxles()){Reason=TEXT("Wheel geometry unavailable; cruise refused");Notify(Reason);return;}
 bActive=true;bLaneFault=false;ProgressCm=0;Segment=0;PlayerDrive=PlayerBrake=0;SpeedIntegral=AppliedSteering=0;HazardCountdown=0;
 NavigationRevision=Nav->GetDriveDestinationRevision();
 RequestedSpeedKmh=FMath::Clamp(FMath::Max(V->GetDrivingComponent()->GetSpeedKmh(),PreferredSpeedKmh),0.f,RoadSpeedLimitKmh);
 Reason=TEXT("Following road");Notify(TEXT("Cruise on: W/S speed, A/D take over, C off; Shift+S reverse"));
}
bool URoadCruiseComponent::MeasureAxles()
{
 const auto* V=Vehicle();const auto* M=V?V->GetChaosMovement():nullptr;const auto* Rig=V?V->GetMesh()->GetSkeletalMeshAsset():nullptr;
 if(!M || !Rig || M->WheelSetups.Num()<4)return false;
 const auto& Skeleton=Rig->GetRefSkeleton();FVector Rest[4];
 for(int32 I=0;I<4;++I)
 {
  const auto& Setup=M->WheelSetups[I];int32 Bone=Skeleton.FindBoneIndex(Setup.BoneName);
  if(Bone==INDEX_NONE || !Setup.WheelClass)return false;
  FTransform Pose=Skeleton.GetRefBonePose()[Bone];while((Bone=Skeleton.GetParentIndex(Bone))!=INDEX_NONE)Pose*=Skeleton.GetRefBonePose()[Bone];
  Rest[I]=Pose.GetLocation()+Setup.AdditionalOffset+Setup.WheelClass.GetDefaultObject()->Offset;
 }
 RearAxleLocal=(Rest[2]+Rest[3])*.5;WheelbaseCm=FVector::Dist2D((Rest[0]+Rest[1])*.5,RearAxleLocal);
 // Use the physical chassis, rather than the old92cm half-width which was
 // narrower than Aster. Cosmetic panels/glass never inflate this envelope.
 SafetyHalfWidthCm=FMath::Max(M->ChassisWidth*.5f,90.f);
 SafetyHalfLengthCm=WheelbaseCm*.5f;
 if(const auto* Physics=V->GetMesh()->GetPhysicsAsset())
 {
  const FBox Bounds=Physics->CalcAABB(V->GetMesh(),FTransform::Identity);
  if(Bounds.IsValid)
  {
   const FTransform MeshToActor=V->GetMesh()->GetComponentTransform().GetRelativeTransform(V->GetActorTransform());
   const FBox ActorBounds=Bounds.TransformBy(MeshToActor);
   SafetyHalfWidthCm=FMath::Max(SafetyHalfWidthCm,float(FMath::Max(FMath::Abs(ActorBounds.Min.Y),FMath::Abs(ActorBounds.Max.Y))));
   SafetyHalfLengthCm=FMath::Max(SafetyHalfLengthCm,float(FMath::Max(FMath::Abs(ActorBounds.Min.X),FMath::Abs(ActorBounds.Max.X))));
  }
 }
 SafetyHalfWidthCm+=15;
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseDiagnostic")))UE_LOG(LogTemp,Display,TEXT("Cruise measured physical envelope: halfWidth=%.2f halfLength=%.2f wheelbase=%.2f"),SafetyHalfWidthCm,SafetyHalfLengthCm,WheelbaseCm);
 return !RearAxleLocal.ContainsNaN() && WheelbaseCm>=180 && WheelbaseCm<=400;
}
void URoadCruiseComponent::Cancel(const FString& Cause)
{
 const bool WasActive=bActive;bActive=false;PlayerDrive=PlayerBrake=0;SpeedIntegral=AppliedSteering=0;Reason=Cause;
 if(auto* V=Vehicle()){V->GetDrivingComponent()->SetDriveInput(0);V->GetDrivingComponent()->SetSteerInput(0);V->GetDrivingComponent()->SetBrakeInput(0);}
 if(WasActive)Notify(FString(TEXT("Cruise off: "))+Cause);
}
bool URoadCruiseComponent::HandlePlayerDrive(float Axis)
{
 if(!bActive)return false;
 auto* PC=Vehicle()?Cast<APlayerController>(Vehicle()->GetController()):nullptr;
 if(Axis<-.01f && PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)))
 {Cancel(TEXT("Driver requested reverse"));return false;}
 PlayerDrive=FMath::Clamp(Axis,-1.f,1.f);return true;
}
bool URoadCruiseComponent::HandlePlayerBrake(float Axis)
{if(!bActive)return false;PlayerBrake=FMath::Clamp(Axis,0.f,1.f);return true;}
void URoadCruiseComponent::PlayerSteeringTakeover(){if(bActive)Cancel(TEXT("Manual steering"));}
FText URoadCruiseComponent::GetStatusText() const
{
 if(!IsAvailable())return FText::GetEmpty();
 return FText::FromString(bActive?FString::Printf(TEXT("C Cruise ON | target %.0f / safe %.0f km/h | %s\nW/S adjust (stop waits), A/D take over, Shift+S reverse"),RequestedSpeedKmh,SafeSpeedKmh,*Reason):TEXT("C Cruise | W/S drive | A/D steer | Space brake"));
}
FVector URoadCruiseComponent::PointAt(float Distance) const
{
 if(Route.Num()<2)return FVector::ZeroVector;
 const float D=FMath::Clamp(Distance,0.f,Distances.Last());
 int32 I=FMath::Clamp(Segment,0,Route.Num()-2);
 while(I>0 && Distances[I]>D)--I;while(I<Route.Num()-2 && Distances[I+1]<D)++I;
 return FMath::Lerp(Route[I],Route[I+1],(D-Distances[I])/FMath::Max(Distances[I+1]-Distances[I],1.f));
}
bool URoadCruiseComponent::UpdateProgress(const FVector& Position)
{
 float Best=TNumericLimits<float>::Max(),Candidate=ProgressCm;int32 Chosen=Segment;FVector Projected;
 const float TravelWindow=FMath::Max(1400.f,FMath::Abs(Vehicle()->GetChaosMovement()->GetForwardSpeed())*.4f+600.f);
 for(int32 I=FMath::Max(0,Segment-3);I<Route.Num()-1 && Distances[I]<=ProgressCm+TravelWindow;++I)
 {
  const FVector AB=Route[I+1]-Route[I];const float Alpha=FMath::Clamp(float(FVector::DotProduct(Position-Route[I],AB)/FMath::Max(AB.SizeSquared(),1.)),0.f,1.f);
  const FVector P=Route[I]+AB*Alpha;const float D=FVector::DistSquared(Position,P);
  if(D<Best){Best=D;Chosen=I;Candidate=Distances[I]+float(AB.Size())*Alpha;Projected=P;}
 }
 LaneErrorCm=FVector::Dist2D(Position,Projected);
 if(Best==TNumericLimits<float>::Max() || LaneErrorCm>330 || FMath::Abs(Position.Z-Projected.Z)>160)return false;
 Segment=Chosen;ProgressCm=FMath::Max(ProgressCm-50,Candidate);
 if(bLooped && ProgressCm>Distances.Last()-150){Segment=0;ProgressCm=0;}
 return true;
}
float URoadCruiseComponent::PreviewSafeSpeed(float SpeedCm) const
{
 float Cap=RoadSpeedLimitKmh;const float Horizon=FMath::Clamp(SpeedCm*SpeedCm/600.f+SpeedCm*.4f+1500.f,1500.f,30000.f);
 // A conservative 3.2 m/s² lateral envelope, with advance service braking.
 for(float D=0;D<Horizon;D+=250)
 {
  int32 SpeedIndex=Segment;while(SpeedIndex<Distances.Num()-1 && Distances[SpeedIndex+1]<ProgressCm+D)++SpeedIndex;
  if(PointSpeedLimits.IsValidIndex(SpeedIndex))
  {
   const float LocalCap=PointSpeedLimits[SpeedIndex]/.036f;
   Cap=FMath::Min(Cap,FMath::Sqrt(LocalCap*LocalCap+640.f*D)*.036f);
  }
  const FVector A=PointAt(ProgressCm+D),B=PointAt(ProgressCm+D+250),C=PointAt(ProgressCm+D+500);
  const FVector2D U(B-A),V(C-B);const float Cross=FMath::Abs(U.X*V.Y-U.Y*V.X);
  if(Cross<1)continue;
  const float Radius=U.Size()*V.Size()*FVector2D(C-A).Size()/(2*Cross);
  const float CurveSpeed=FMath::Sqrt(320.f*FMath::Max(Radius,100.f));
  const float Reachable=FMath::Sqrt(CurveSpeed*CurveSpeed+640.f*D)*.036f;
  Cap=FMath::Min(Cap,Reachable);
 }
 if(!bLooped)Cap=FMath::Min(Cap,FMath::Sqrt(500.f*FMath::Max(0.f,Distances.Last()-ProgressCm-280.f))*.036f);
 return FMath::Max(Cap,0.f);
}
float URoadCruiseComponent::FindHazardDistance(float HorizonCm)
{
 auto* V=Vehicle();FCollisionQueryParams Params(SCENE_QUERY_STAT(RoadCruiseSafety),false,V);
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_Vehicle);
 // Start at the authored road height. A chassis-origin offset lifted the
 // first sweep and the old57cm lower edge missed wheel/chassis obstacles.
 // The8..132cm envelope clears the road while including low35cm barriers.
 FVector Previous=PointAt(ProgressCm)+FVector(0,0,70);
 for(float D=350;D<=HorizonCm;D+=250)
 {
  const FVector RoadPoint=PointAt(ProgressCm+D);const FVector P=RoadPoint+FVector(0,0,70);
  const FQuat Rotation=(P-Previous).Rotation().Quaternion();TArray<FHitResult> Hits;
  GetWorld()->SweepMultiByObjectType(Hits,Previous,P,Rotation,Objects,FCollisionShape::MakeBox(FVector(SafetyHalfLengthCm,SafetyHalfWidthCm,62)),Params);
  // Object queries also find overlap-only water/interaction volumes. Only
  // components that really block this chassis are driving obstructions.
  for(const auto& H:Hits)if(H.bBlockingHit && H.GetActor()!=V && H.GetComponent()
   && H.GetComponent()->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block && H.ImpactNormal.Z<.65f)
  {
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseDiagnostic")) && GetWorld()->GetTimeSeconds()>LastHazardLog+1)
   {LastHazardLog=GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Warning,TEXT("Cruise actual obstruction: car=%s progress=%.1f ahead=%.1f actor=%s component=%s location=%s impact=%s normal=%s"),*V->VehicleID.ToString(),ProgressCm,D,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*P.ToString(),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString());}
   return FMath::Max(0.f,D-250.f);
  }
  FHitResult Support;if(!GetWorld()->LineTraceSingleByChannel(Support,RoadPoint+FVector(0,0,100),RoadPoint-FVector(0,0,170),ECC_Visibility,Params)
   || Support.ImpactNormal.Z<.55f || FMath::Abs(Support.ImpactPoint.Z-RoadPoint.Z)>65)
  {
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseDiagnostic")) && GetWorld()->GetTimeSeconds()>LastHazardLog+1)
   {LastHazardLog=GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Warning,TEXT("Cruise actual missing support: progress=%.1f ahead=%.1f road=%s actor=%s component=%s impact=%s normal=%s"),ProgressCm,D,*RoadPoint.ToString(),*GetNameSafe(Support.GetActor()),*GetNameSafe(Support.GetComponent()),*Support.ImpactPoint.ToString(),*Support.ImpactNormal.ToString());}
   return FMath::Max(0.f,D-350.f);
  }
  Previous=P;if(!bLooped && ProgressCm+D>=Distances.Last()-100)break;
 }
 return TNumericLimits<float>::Max();
}
void URoadCruiseComponent::Hold(const FString& Cause,float Delta)
{
 Reason=Cause;SafeSpeedKmh=0;AppliedSteering=FMath::FInterpConstantTo(AppliedSteering,0.f,Delta,2.f);
 auto* D=Vehicle()->GetDrivingComponent();D->SetDriveInput(0);D->SetBrakeInput(1);D->SetSteerInput(AppliedSteering);SpeedIntegral=0;
}
void URoadCruiseComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);if(!bActive)return;auto* V=Vehicle();if(!V)return;
 if(!V->IsPlayerControlled() || V->GetDrivingComponent()->GetEngineState()!=ENeonCityEngineState::Running)
 {Cancel(TEXT("Driver left or vehicle unavailable"));return;}
 if(auto* Roof=V->FindComponentByClass<UNacreRoofComponent>();Roof && Roof->IsCycling())
 {Cancel(TEXT("Roof is moving"));V->GetDrivingComponent()->SetBrakeInput(1);return;}
 auto* PC=Cast<APlayerController>(V->GetController());
 auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr;
 if(!Nav || Nav->GetDriveDestinationRevision()!=NavigationRevision)
 {Cancel(TEXT("Navigation changed; toggle C to accept the new route"));V->GetDrivingComponent()->SetBrakeInput(1);return;}
 const float Speed=V->GetChaosMovement()->GetForwardSpeed();if(Speed<-80){Cancel(TEXT("Vehicle reversed"));return;}
 RequestedSpeedKmh=FMath::Clamp(RequestedSpeedKmh+PlayerDrive*(PlayerDrive>=0?12.f:22.f)*Delta,0.f,RoadSpeedLimitKmh);
 if(PlayerBrake>.01f)RequestedSpeedKmh=FMath::Max(0.f,RequestedSpeedKmh-30.f*PlayerBrake*Delta);
 const float PreviousProgress=ProgressCm;
 if(bLaneFault || !UpdateProgress(V->GetActorLocation())){bLaneFault=true;Hold(TEXT("Lane lost; align safely and toggle C to resume"),Delta);return;}
 // The cached query is anchored at its previous progress. Consume the gap
 // while moving; otherwise a high-speed car gets up to12% second free travel.
 if(CachedHazardDistance<TNumericLimits<float>::Max())CachedHazardDistance=FMath::Max(0.f,CachedHazardDistance-FMath::Max(0.f,ProgressCm-PreviousProgress));
 SafeSpeedKmh=PreviewSafeSpeed(FMath::Max(Speed,0.f));HazardCountdown-=Delta;
 if(HazardCountdown<=0){CachedHazardDistance=FindHazardDistance(FMath::Clamp(Speed*Speed/600.f+FMath::Max(650.f,Speed*.4f)+SafetyHalfLengthCm,900.f,30000.f));HazardCountdown=.12f;}
 if(LaneErrorCm>45)SafeSpeedKmh=FMath::Min(SafeSpeedKmh,FMath::GetMappedRangeValueClamped(FVector2D(45,90),FVector2D(40,8),LaneErrorCm));
 if(CachedHazardDistance<TNumericLimits<float>::Max())
 {SafeSpeedKmh=FMath::Min(SafeSpeedKmh,FMath::Sqrt(600.f*FMath::Max(0.f,CachedHazardDistance-450.f))*.036f);Reason=TEXT("Obstacle or missing road: braking");}
 else Reason=RequestedSpeedKmh<.2f?TEXT("Stopped; W resumes original lane"):SafeSpeedKmh<RequestedSpeedKmh-.5f?TEXT("Bend / approach speed limit"):TEXT("Following road");
 const float Desired=FMath::Min(RequestedSpeedKmh,SafeSpeedKmh),Error=Desired-FMath::Max(Speed*.036f,0.f);
 float Brake=FMath::Clamp(-Error/10.f,0.f,1.f);
 if(Desired<.2f){Brake=1.f;SpeedIntegral=0;}else SpeedIntegral=FMath::Clamp(SpeedIntegral+Error*.012f*Delta,0.f,.45f);
 const float Throttle=Brake>.01f?0.f:FMath::Clamp(Error*.07f+SpeedIntegral+.12f,0.f,1.f);
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseDiagnostic")) && GetWorld()->GetTimeSeconds()>LastControlLog+1)
 {
  LastControlLog=GetWorld()->GetTimeSeconds();auto* CM=V->GetChaosMovement();
  UE_LOG(LogTemp,Display,TEXT("Cruise control diagnostic: p=%s progress=%.1f remaining=%.1f segment=%d request=%.2f safe=%.2f speed=%.3f desired=%.2f drive=%.3f brake=%.3f chaosThrottle=%.3f chaosBrake=%.3f gear=%d/%d rpm=%.1f lane=%.1f reason=%s"),*V->GetActorLocation().ToString(),ProgressCm,Distances.Last()-ProgressCm,Segment,RequestedSpeedKmh,SafeSpeedKmh,Speed*.036f,Desired,Throttle,Brake,CM->GetThrottleInput(),CM->GetBrakeInput(),CM->GetCurrentGear(),CM->GetTargetGear(),CM->GetEngineRotationSpeed(),LaneErrorCm,*Reason);
 }
 const float Lookahead=FMath::Clamp(300.f+FMath::Max(Speed,0.f)*.65f,350.f,1400.f);
 // UE5.8's legacy Wheel.Location is not refreshed by this vehicle output path.
 // Use the same authored bone rest position + offsets that Chaos uses to build
 // its suspension, transformed by the live physical chassis each frame.
 const FVector RearAxle=V->GetMesh()->GetComponentTransform().TransformPosition(RearAxleLocal);
 const FVector Local=V->GetActorTransform().InverseTransformVectorNoScale(PointAt(ProgressCm+Lookahead)-RearAxle);
 auto* M=V->GetChaosMovement();const float BaseAngle=M->Wheels.Num()>0 && M->Wheels[0]?M->Wheels[0]->MaxSteerAngle:38.f;
 const float AngleRatio=M->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(FMath::Abs(Speed)*.0223694f,1.f);
 // Chaos's AngleRatio mode reduces one front wheel. Pure pursuit requests
 // the axle's mean angle, so account for that native steering geometry.
 const float AxleRatio=M->SteeringSetup.SteeringType==ESteeringType::AngleRatio?
  .5f*(1.f+FMath::Clamp(M->SteeringSetup.AngleRatio,0.f,1.f)):1.f;
 const float PursuitAngle=FMath::RadiansToDegrees(FMath::Atan2(2.f*WheelbaseCm*Local.Y,FMath::Max(Local.X*Local.X+Local.Y*Local.Y,1.f)));
 // Pursuit alone can settle outside a tight mountain bend. Correct the live
 // signed lane offset rather than progressively rubbing its safety barrier.
 const float CrossTrack=V->GetActorTransform().InverseTransformVectorNoScale(PointAt(ProgressCm)-V->GetActorLocation()).Y;
 const float LaneCorrection=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(CrossTrack*.75f,FMath::Max(Speed,0.f)+300.f)),-8.f,8.f);
 const float DesiredAngle=PursuitAngle+LaneCorrection;
 const float TargetSteer=FMath::Clamp(DesiredAngle/FMath::Max(BaseAngle*AngleRatio*AxleRatio,2.f),-1.f,1.f);
 AppliedSteering=FMath::FInterpConstantTo(AppliedSteering,TargetSteer,Delta,2.5f);
 auto* Driving=V->GetDrivingComponent();Driving->SetDriveInput(Throttle);Driving->SetBrakeInput(FMath::Max(Brake,PlayerBrake));Driving->SetSteerInput(AppliedSteering);
}
