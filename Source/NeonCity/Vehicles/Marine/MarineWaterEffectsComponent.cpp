#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Water/CoastalOceanSurface.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Vehicles/Marine/ReferenceBoatSpray.generated.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/DirectionalLightComponent.h"

namespace TransomWash
{
 // At the yacht's measured 47km/h cruise, the former80ms births were over1m
 // apart and faded in for another1m. Keep the existing48 surface stamps, with
 // overlapping births and a short lifetime instead of extending a white trail.
 constexpr float IntervalSeconds=.055f;
 constexpr float LifetimeSeconds=1.2f;
 constexpr float FadeInSeconds=.025f;
 constexpr float WidthScale=1.15f;
}

UMarineWaterEffectsComponent::UMarineWaterEffectsComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 FoamMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/VerticalSlice/Effects/M_NC205C_MarineFoam.M_NC205C_MarineFoam")));
}
void UMarineWaterEffectsComponent::BeginPlay()
{
 Super::BeginPlay();
 for(TActorIterator<ACoastalOceanSurface> It(GetWorld());It;++It){Ocean=*It;break;}
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){DayNight=*It;break;}
 if(GetWorld()->GetNetMode()==NM_DedicatedServer)return;
 // Phase 20.5c: the vessel's cosmetic effect is a stern trail only. Keep the
 // old licensed spray sources for provenance, but never load/spawn bow or
 // hull-side emitters. Ocean breaking/shore foam and hull forces are separate.
 auto* Material=FoamMaterial.LoadSynchronous();
 auto* Quad=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
 if(!Material || !Quad)return;
 Particles=NewObject<UInstancedStaticMeshComponent>(GetOwner(),TEXT("MarineWaterParticles"));
 Particles->SetMobility(EComponentMobility::Movable);Particles->SetStaticMesh(Quad);Particles->SetMaterial(0,Material);
 Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision);Particles->SetCanEverAffectNavigation(false);
 Particles->SetCastShadow(false);Particles->bReceivesDecals=false;Particles->NumCustomDataFloats=1;
 GetOwner()->AddInstanceComponent(Particles);Particles->SetupAttachment(GetOwner()->GetRootComponent());
 // Like world-space particles, existing foam must stay behind as the hull moves.
 Particles->SetAbsolute(true,true,true);Particles->SetWorldTransform(FTransform::Identity);
 Pool.SetNum(72);Transforms.Init(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),72);
 for(const auto& Transform:Transforms)Particles->AddInstance(Transform,true);
 Particles->RegisterComponent();Particles->SetVisibility(false);LastPosition=GetOwner()->GetActorLocation();
}
void UMarineWaterEffectsComponent::Clear()
{
 for(auto& P:Pool)P.Life=0;
 for(auto& N:SprayBursts)if(IsValid(N))N->DestroyComponent();SprayBursts.Reset();
 for(auto& C:Contacts)C=FContactState();
 WakeClock=SprayClock=WashCarry=ImpactBurst=TransomClock=0;
 if(Particles)Particles->SetVisibility(false);
 bVisible=false;
}
void UMarineWaterEffectsComponent::ConfigureTransomWash(FVector LocalAnchor,float HalfBeamCm)
{
 if(LocalAnchor.ContainsNaN() || !FMath::IsFinite(HalfBeamCm) || HalfBeamCm<30 || HalfBeamCm>500)return;
 TransomAnchor=LocalAnchor;TransomHalfBeamCm=HalfBeamCm;bTransomWash=true;
}
void UMarineWaterEffectsComponent::Emit(bool Spray,FVector Position,FVector Velocity,float Size,float Angle,bool Transom)
{
 int32& Cursor=Spray?SprayCursor:WakeCursor;
 auto& P=Pool[Cursor];P.Position=Position;P.Velocity=Velocity;P.Size=Size;P.Angle=Angle;P.Age=0;P.Life=Spray?.4f:Transom?TransomWash::LifetimeSeconds:2.3f;P.bSpray=Spray;P.bTransom=Transom;
 Cursor=Spray?(Cursor==71?48:Cursor+1):(Cursor==47?0:Cursor+1);
}
void UMarineWaterEffectsComponent::UpdateTransomWash(const FMarineDriveTelemetry& State,FVector Forward,FVector Right,float Delta)
{
 if(!bTransomWash || !State.Afloat || !State.EngineRunning || State.Throttle<.04f)
 {TransomClock=0;return;}
 TransomClock+=Delta;if(TransomClock<TransomWash::IntervalSeconds)return;
 TransomClock=FMath::Fmod(TransomClock,TransomWash::IntervalSeconds);
 auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();
 const FVector Anchor=GetOwner()->GetActorTransform().TransformPosition(TransomAnchor);
 const float Activity=FMath::Sqrt(State.Throttle)*(.35f+.65f*FMath::SmoothStep(1.f,14.f,FMath::Abs(State.ForwardSpeedKmh)));
 for(float Side:{-1.f,1.f})
 {
  const float Phase=FMath::Sin(float(TransomEmissions)*2.399963f);
  FVector Point=Anchor+Right*(Side*TransomHalfBeamCm*.29f+Phase*12);FWaterSurfaceSample Sample;
  if(!Water->QueryWater(Point,Sample))continue;
  Point.Z=Sample.SurfacePoint.Z+4;
  // Two short submerged propeller races. World-fixed, flat and wave-aligned;
  // no billboards, bow emitter, new particles, or per-frame object allocation.
  Emit(false,Point,Sample.Flow+Right*(Side*18+Phase*7)-Forward*25,
   FMath::Clamp(TransomHalfBeamCm/240.f,.4f,1.1f)*Activity,
   GetOwner()->GetActorRotation().Yaw+Side*(5+Phase*3),true);
  ++TransomEmissions;
 }
}
int32 UMarineWaterEffectsComponent::GetActiveWakeCount() const
{ int32 Count=0;for(const auto& P:Pool)if(P.Life>0 && !P.bSpray)++Count;return Count; }
int32 UMarineWaterEffectsComponent::GetActiveSprayCount() const
{ int32 Count=0;for(const auto& P:Pool)if(P.Life>0 && P.bSpray)++Count;
 for(const auto& N:SprayBursts)if(IsValid(N) && N->IsActive()){bool Valid;Count+=N->GetVariableInt(TEXT("User.DropCount"),Valid);}return Count; }
void UMarineWaterEffectsComponent::UpdateContactSpray(const FMarineDriveTelemetry& State,FVector Extent,float Delta)
{
 SprayBursts.RemoveAllSwap([](const auto& N){return !IsValid(N) || !N->IsActive();});
 auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetOwner());
 if(!ContactSpray || !Boat || !Boat->GetProfile() || !State.Afloat || Delta<=0)return;
 const FTransform Transform=Boat->GetActorTransform();auto* Body=Cast<UPrimitiveComponent>(Boat->GetRootComponent());
 if(!Body)return;
 const FVector Scale=Boat->GetProfile()->bReferenceDisplacementHull?FVector::OneVector:FVector(Extent.X/390.f,Extent.Y/125.f,1);
 const float Step=FMath::Clamp(Delta,1.f/240,.05f);
 auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();
 const float Speed=State.SpeedKmh/3.6f;
 SmoothedAcceleration=FMath::FInterpTo(SmoothedAcceleration,(Speed-LastSpraySpeed)/Step,Step,4);LastSpraySpeed=Speed;
 ImpactBurst*=FMath::Exp(-Step/.15f);
 SprayClock+=Step;const bool EmitNow=SprayClock>=.12f;
 if(EmitNow)SprayClock=FMath::Fmod(SprayClock,.12f);
 // Contact model adapted from Tidewater BoatSpray.js, MIT (notice in
 // Docs/ThirdParty/Tidewater-LICENSE.txt). Keep each side's strongest contact,
 // rather than placing symmetric permanent emitters at the bow.
 struct FContactFrame { FVector At,Normal,Velocity;float Height=0,Demand=0,Impact=0,Response=0,Rate=0,Vn=0; };
 FContactFrame Frame[6];int32 Best[2]={-1,-1};float DemandSum=0,SideDemand[2]={0,0};
 for(int32 I=0;I<6;++I)
 {
  auto& C=Contacts[I];const auto& Source=NeonCityReferenceSpray::Contacts[I];
  const FVector Local=NeonCityReferenceBoat::ToUEPolar((Source.A+Source.B)*.5)*100*Scale;
  const FVector Mid=Transform.TransformPosition(Local);float Height;FVector Flow;
  if(!Water->SampleWater(Mid,Height,Flow)){C=FContactState();continue;}
  const float Depth=(Height-Mid.Z)/100;
  const float Rate=C.Primed?(Depth-C.Depth)/Step:0;
  C.Rate=FMath::Lerp(C.Rate,Rate,FMath::Min(1.f,Step/.03f));C.Depth=Depth;C.Primed=true;
  const float Wet=FMath::SmoothStep(-.35f,-.08f,Depth)*(1-FMath::SmoothStep(.7f,1.1f,Depth));
  // Normal follows the scaled waterline, and point speed includes pitch/roll/yaw.
  const FVector Normal=Transform.TransformVectorNoScale(NeonCityReferenceBoat::ToUEPolar(Source.Normal)/Scale).GetSafeNormal2D();
  const FVector Velocity=(Body->GetPhysicsLinearVelocityAtPoint(Mid)-Flow)/100;
  const float Vn=FMath::Max(0.f,float(Velocity.Dot(Normal)));
  const float RV=FMath::Min(30.f,FMath::Max(0.f,Vn-.4f)*1.6f);
  const float RI=FMath::Min(30.f,FMath::Max(0.f,C.Rate-.6f)*1.2f);
  const float DV=RV*Wet,DI=RI*3*Wet*(.15f+.85f*FMath::SmoothStep(2.f,6.f,Speed));
  auto& F=Frame[I];F.At=Mid;F.Normal=Normal;F.Velocity=Velocity;F.Height=Height;F.Vn=Vn;F.Rate=RI;
  F.Demand=(DV+DI)*Source.Length*FMath::Max(Scale.X,Scale.Y);
  F.Impact=DI/FMath::Max(.0001f,DV+DI);F.Response=FMath::Lerp(RV,RI,F.Impact);DemandSum+=F.Demand;SideDemand[I/3]+=F.Demand;
  const int32 Side=I/3;if(F.Demand>.001f && (Best[Side]<0 || F.Demand>Frame[Best[Side]].Demand))Best[Side]=I;
  if(I%3==0 && Wet>.3f && Speed>2.5f)ImpactBurst=FMath::Max(ImpactBurst,FMath::Clamp((C.Rate-1.8f)*.4f+Speed*.02f,0.f,1.f));
 }
 const float Budget=FMath::Min(1.f,900.f/FMath::Max(1.f,DemandSum*300.f));
 const float Making=FMath::SmoothStep(3.f,6.f,Speed);
 const auto HalfBeam=[](double Z,double Y)
 {
  using namespace NeonCityReferenceSpray;
  const double Sheer=FMath::Lerp(Shape_ySheerAft,Shape_ySheerStem,FMath::Clamp((Z-Shape_zAft)/(Shape_zStem-Shape_zAft),0.,1.));
  const double F=FMath::Clamp(Y/FMath::Max(Sheer,.1),0.,1.);
  const double Shoulder=FMath::Lerp(Shape_wlShoulder,Shape_zShoulder,F),Stem=FMath::Lerp(Shape_wlStem,Shape_zStem,F);
  const double HB=FMath::Lerp(Shape_wlHalfBeam,Shape_halfBeam,F);
  if(Z>=Stem)return 0.;const double E=FMath::Clamp((Z-Shoulder)/FMath::Max(Stem-Shoulder,.01),0.,1.);
  return HB*FMath::Sqrt(FMath::Max(1-E*E,.02));
 };
 const auto Spawn=[&](int32 Kind,int32 Count,FVector At,FVector Launch,float Height)
 {
  if(Count<=0 || SprayBursts.Num()>=64 || !SprayLayers.IsValidIndex(Kind) || !SprayLayers[Kind])return;
  if(auto* N=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),SprayLayers[Kind],At,FRotator::ZeroRotator,
    FVector::OneVector,true,false,ENCPoolMethod::None,true))
  {
   N->SetVariableFloat(TEXT("User.LaunchSpeed"),Launch.Size()*100);
   N->SetVariableVec3(TEXT("User.LaunchDirection"),Launch.GetSafeNormal());
   N->SetVariableInt(TEXT("User.DropCount"),FMath::Min(48,Count));N->SetVariableFloat(TEXT("User.SurfaceZ"),Height);
   const auto* Sun=DayNight.IsValid()?DayNight->GetSun():nullptr;
   const FVector SunDirection=Sun?-Sun->GetForwardVector():FVector::UpVector;
   const FLinearColor SunRadiance=Sun?Sun->GetLightColor()*FMath::Max(0.f,Sun->Intensity):FLinearColor::Black;
   N->SetVariableVec3(TEXT("User.SpraySunDirection"),SunDirection);
   N->SetVariableVec3(TEXT("User.SpraySunRadiance"),SunDirection.Z>0?FVector(SunRadiance.R,SunRadiance.G,SunRadiance.B):FVector::ZeroVector);
   N->Activate(true);SprayBursts.Add(N);++TotalContactBursts;++LayerBursts[Kind];
  }
 };
 for(int32 I=0;I<6;++I)
 {
  auto& F=Frame[I];auto& C=Contacts[I];if(F.Demand<.001f)continue;
  const float N=F.Demand*300*Step*Budget*(1+ImpactBurst);
  const bool Primary=Best[I/3]==I;
  const float Shares[]={1.f,Primary?(.04f*F.Impact+.12f*ImpactBurst)*Making:0.f,
   Primary?.3f*FMath::SmoothStep(1.f,5.f,F.Response):0.f,
   Primary?(.10f+.12f*F.Impact*FMath::SmoothStep(1.5f,5.f,F.Rate)+.12f*ImpactBurst)*Making:0.f,
   Primary?.055f*Making*(1-ImpactBurst*.5f):0.f};
  FVector Launch=(F.Velocity-F.Normal*F.Velocity.Dot(F.Normal))*.6f;
  Launch.Z=F.Velocity.Z*.3f+F.Response*FMath::Lerp(.55f,1.15f,F.Impact)+ImpactBurst*2.5f;
  Launch+=F.Normal*(.8f*F.Response+.4f+ImpactBurst);
  const auto& Source=NeonCityReferenceSpray::Contacts[I];
  // Match BoatSpray._halfBeam at the actual elevated birth height. A constant
  // offset from the waterline placed many old births inside the flared bow.
  const double Rise=FMath::Min(.5,double(F.Vn*F.Vn)/(2*9.81)*.5);
  const double LocalY=FMath::Clamp((F.Height+Rise*100-Transform.GetLocation().Z)/100.,-.3,.9);
  const double LocalZ=FMath::Lerp(Source.A.Z,Source.B.Z,FMath::FRandRange(.1,.9));
  const double Side=I<3?1.:-1.;
  FVector At=Transform.TransformPosition(NeonCityReferenceBoat::ToUEPolar(FVector(Side*(HalfBeam(LocalZ,LocalY)+.05),LocalY,LocalZ))*100*Scale);
  At.Z=F.Height+Rise*100+3;
  for(int32 Kind=4;Kind>=0;--Kind)
  {
   // Source sheets/ligaments/whitewater collect the whole side's emission at
   // its strongest wetted contact. Using only that contact's N lost ~2/3.
   const float Requested=Kind==0?N:SideDemand[I/3]*300*Step*Budget*(1+ImpactBurst);
   C.Carry[Kind]=FMath::Min(48.f,C.Carry[Kind]+Requested*Shares[Kind]);
   if(EmitNow && C.Carry[Kind]>=1)
   {const int32 Count=FMath::FloorToInt(C.Carry[Kind]);C.Carry[Kind]-=Count;Spawn(Kind,Count,At,Launch*(Kind==1?.6f:1.f),F.Height);}
  }
 }
 // Accelerating propeller race: a short churn near the stern, never a fixed
 // pair of smoke ribbons. The spectral wake carries its surface trail onward.
 if(State.EngineRunning && State.Throttle>.06f)
 {
  const float Activity=FMath::Sqrt(State.Throttle)*(.3f+.7f*FMath::SmoothStep(1.f,8.f,Speed))*(1+FMath::Clamp(SmoothedAcceleration*.12f,0.f,.6f));
  WashCarry+=Activity*24*Step;
  if(EmitNow && WashCarry>=1)
  {
   FVector At=Transform.GetLocation()-Transform.GetUnitAxis(EAxis::X)*Extent.X*.9f;float Height;FVector Flow;
   if(Water->SampleWater(At,Height,Flow))
   {At.Z=Height+7;Spawn(3,FMath::FloorToInt(WashCarry),At,Body->GetPhysicsLinearVelocity()/100*.45f-Transform.GetUnitAxis(EAxis::X)*1.8f+FVector(0,0,.7f),Height);}
   WashCarry=FMath::Fmod(WashCarry,1.f);
  }
 }
}
void UMarineWaterEffectsComponent::UpdateEffects(const FMarineDriveTelemetry& State,FVector Extent,float Delta)
{
 if(!Particles)return;
 const FVector Position=GetOwner()->GetActorLocation();
 auto* PC=GetWorld()->GetFirstPlayerController();auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
 if(!Camera || FVector::DistSquared(Position,Camera->GetCameraLocation())>FMath::Square(10000.f)) { Clear();LastPosition=Position;return; }
 if(FVector::DistSquared(Position,LastPosition)>FMath::Square(2000.f))Clear();
 LastPosition=Position;
 const float Step=FMath::Clamp(Delta,0.f,.25f);
 float Height=0;FVector Flow;
 const bool Water=State.Afloat && GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Position,Height,Flow);
 if(!bVisible && !(Water && (State.SpeedKmh>4 || (bTransomWash && State.EngineRunning && State.Throttle>.04f))))return;
 const FVector Forward=GetOwner()->GetActorForwardVector().GetSafeNormal2D(),Right=FVector::CrossProduct(FVector::UpVector,Forward);
 const bool PhysicalWake=Ocean.IsValid() && Ocean->HasPhysicalWakeFor(GetOwner());
 if(PhysicalWake)for(auto& P:Pool)if(!P.bSpray && !P.bTransom)P.Life=0;
 UpdateTransomWash(State,Forward,Right,Step);
 const auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetOwner());
 const bool FoamWake=!Boat || !Boat->GetProfile() || Boat->GetProfile()->bFoamWake;
 if(Water && FoamWake && State.ForwardSpeedKmh>4 && !PhysicalWake && !bTransomWash)
 {
  WakeClock+=Step;
  if(WakeClock>=.1f)
  {
   WakeClock=FMath::Fmod(WakeClock,.1f);
   const float Direction=State.ForwardSpeedKmh>=0?1.f:-1.f;
   for(float Side:{-1.f,1.f})
   {
    FVector P=Position-Forward*Extent.X*.85f*Direction+Right*Side*Extent.Y*.65f;P.Z=Height+3;
    // Overlapping, lengthwise ribbons; circular expanding stamps read as smoke.
    Emit(false,P,Flow+Right*Side*25,1.1f,GetOwner()->GetActorRotation().Yaw-Side*12*Direction);
   }
  }
 }
 else WakeClock=0;
 SprayClock=0;
 int32 Active=0;
 for(int32 I=0;I<Pool.Num();++I)
 {
  auto& P=Pool[I];float Opacity=0;
  if(P.Life>0)
  {
   P.Age+=Step;
   if(P.Age>=P.Life)P.Life=0;
   else
   {
    ++Active;P.Position+=P.Velocity*Step;FVector WaveNormal=FVector::UpVector;
    if(!P.bSpray)
    {
     // Foam remains on the same displaced surface the hull samples. The bounded
     // 48-ribbon pool performs no traces or GPU readbacks.
     FWaterSurfaceSample Sample;
     if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(P.Position,Sample))
     {P.Position.Z=Sample.SurfacePoint.Z+3;WaveNormal=Sample.Normal;}
    }
    if(P.bSpray)P.Velocity.Z-=650*Step;
    const float Fraction=P.Age/P.Life;
    Opacity=(P.bSpray?.16f:P.bTransom?.68f:.24f)*FMath::Min(1.f,P.Age/(P.bTransom?TransomWash::FadeInSeconds:.08f))*(1-Fraction);
    const float Size=P.Size*(1+Fraction*(P.bSpray?.5f:.8f));
    FQuat Rotation=P.bSpray?FRotationMatrix::MakeFromZ(Camera->GetCameraLocation()-P.Position).ToQuat():FRotator(0,P.Angle,0).Quaternion();
    if(!P.bSpray)Rotation=FRotationMatrix::MakeFromZX(WaveNormal,FRotator(0,P.Angle,0).Vector()).ToQuat();
    Transforms[I]=FTransform(Rotation,P.Position,FVector(Size*(P.bSpray?1.f:3.2f),Size*(P.bSpray?1.f:P.bTransom?TransomWash::WidthScale:.65f),1));
   }
  }
  if(P.Life<=0)Transforms[I]=FTransform(FQuat::Identity,Position,FVector::ZeroVector);
  Particles->SetCustomDataValue(I,0,Opacity,false);
 }
 if(Active || bVisible)Particles->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
 if(bool(Active)!=bVisible) { bVisible=Active>0;Particles->SetVisibility(bVisible); }
}
void UMarineWaterEffectsComponent::EndPlay(EEndPlayReason::Type Reason)
{
 Clear();
 if(Particles) { Particles->DestroyComponent();Particles=nullptr; }
 Pool.Reset();Transforms.Reset();Super::EndPlay(Reason);
}
