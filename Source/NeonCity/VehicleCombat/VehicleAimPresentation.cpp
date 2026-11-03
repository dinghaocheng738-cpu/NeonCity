#include "VehicleCombat/VehicleAimPresentation.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "Combat/CombatComponent.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/LockOnSystem.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Combat/CombatCollision.h"
#include "Combat/DamageInterface.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Volume.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

namespace NeonCityAim
{
const FLinearColor Safe(.85f,.88f,.9f,.55f),Armed(.42f,1.f,.55f,.95f),Hot(1.f,.25f,.18f,1.f),Seek(1.f,.82f,.25f,.95f),Dim(.42f,1.f,.55f,.38f);
constexpr float Step=.04f;                 // trajectory prediction step (s)

void Line(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,TArray<FVector2D> Points,const FLinearColor& C,float Width)
{ FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width); }

void Circle(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D At,float R,const FLinearColor& C,float Width,int32 Dashes=0)
{
 const int32 N=FMath::Clamp(int32(R*.6f),24,96);
 if(Dashes<=0){TArray<FVector2D> P;for(int32 I=0;I<=N;++I){const float A=2*PI*I/N;P.Add(At+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}Line(Out,Layer,G,P,C,Width);return;}
 for(int32 D=0;D<Dashes;++D)
 {
  TArray<FVector2D> P;const float A0=2*PI*D/Dashes,A1=A0+PI/Dashes;
  for(int32 I=0;I<=6;++I){const float A=FMath::Lerp(A0,A1,I/6.f);P.Add(At+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}
  Line(Out,Layer,G,P,C,Width);
 }
}

void Text(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D At,const FString& S,const FLinearColor& C,int32 Size=11)
{
 const FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle("Bold",Size);
 // Dark drop shadow keeps the read-out legible over bright sky and water.
 FSlateDrawElement::MakeText(Out,Layer,G.ToPaintGeometry(FVector2f(400,30),FSlateLayoutTransform(FVector2f(At+FVector2D(1,1)))),S,Font,ESlateDrawEffect::None,FLinearColor(0,0,0,C.A*.8f));
 FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2f(400,30),FSlateLayoutTransform(FVector2f(At))),S,Font,ESlateDrawEffect::None,C);
}

/** Corner brackets of a box, pulled in by Inset (0 = on the box). */
void Brackets(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D Min,FVector2D Max,float Inset,const FLinearColor& C,float Width)
{
 const FVector2D Grow(Inset,Inset);Min-=Grow;Max+=Grow;const float L=FMath::Max(8.f,FMath::Min(Max.X-Min.X,Max.Y-Min.Y)*.28f);
 for(const FVector2D& K:{Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y)})
 {
  const float SX=K.X==Min.X?1.f:-1.f,SY=K.Y==Min.Y?1.f:-1.f;
  Line(Out,Layer,G,{K+FVector2D(SX*L,0),K,K+FVector2D(0,SY*L)},C,Width);
 }
}
}
using namespace NeonCityAim;

int32 UVehicleReticleWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
 Layer=Super::NativePaint(Args,G,Culling,Out,Layer,Style,bParentEnabled);
 const FVehicleReticleState& S=State;
 if(!S.bVisible)return Layer;
 const int32 L=Layer+1;
 // boot-up: the sight draws in from 1.6x size and fades up over the moment after V
 const float Boot=FMath::Clamp(S.Boot,0.f,1.f),Grow=FMath::Lerp(1.6f,1.f,FMath::InterpEaseOut(0.f,1.f,Boot,2.f));
 auto Fade=[Boot](FLinearColor C){C.A*=Boot;return C;};
 const FLinearColor Main=Fade(S.bOnTarget?Hot:Armed),Faint=Fade(Dim);
 const FVector2D C=S.Center;
 // boresight: where the camera looks - a fine cross with a gap
 for(const FVector2D& D:{FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)})Line(Out,L,G,{C+D*6*Grow,C+D*16*Grow},Faint,1.5f);
 if(Boot<1)Text(Out,L,G,C+FVector2D(-70,-90),TEXT("WEAPONS HOT"),Fade(Armed),14);
 // impact pipper (gun / rockets): where the round will really strike, with a range ring
 if(S.bImpactOnScreen && S.Mode<2)
 {
  const FVector2D P=S.Impact;const float R=(S.Mode==0?17.f:22.f)*Grow;
  if(FVector2D::Distance(P,C)>24)Line(Out,L,G,{C,P},FLinearColor(Main.R,Main.G,Main.B,.22f*Boot),1.f);
  if(S.Mode==0){Circle(Out,L,G,P,R,Main,2.f);Line(Out,L,G,{P-FVector2D(2,0),P+FVector2D(2,0)},Main,4.f);}
  else Line(Out,L,G,{P+FVector2D(0,-R),P+FVector2D(R,0),P+FVector2D(0,R),P+FVector2D(-R,0),P+FVector2D(0,-R)},Main,2.f);
  // range ring: the arc's length shows range out to 2 km
  const float Frac=FMath::Clamp(S.RangeMeters/2000.f,0.f,1.f);TArray<FVector2D> Arc;
  for(int32 I=0;I<=40;++I){const float A=-HALF_PI+2*PI*Frac*I/40.f;Arc.Add(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*(R+7));}
  if(Frac>0)Line(Out,L,G,Arc,Main,2.5f);
  Text(Out,L,G,P+FVector2D(R+14,-9),S.RangeMeters>0?FString::Printf(TEXT("%.0f%s"),S.RangeMeters,S.bWater?TEXT("  WATER"):TEXT("")):FString(TEXT("---")),Main,11);
  if(S.HitMarker>0)for(const FVector2D& D:{FVector2D(1,1),FVector2D(-1,1),FVector2D(1,-1),FVector2D(-1,-1)})
   Line(Out,L+2,G,{P+D*10,P+D*22},FLinearColor(1,1,1,S.HitMarker),2.5f);
 }
 // guided missile: the seeker's field, the designated target, lock
 if(S.Mode==2)
 {
  const float F=FMath::Max(40.f,S.SeekerRadius*.72f)*Grow;
  Brackets(Out,L,G,C-FVector2D(F,F),C+FVector2D(F,F),0,Faint,1.5f);
  if(S.bSeeker && S.bCandidate)
  {
   const FVector2D M=(S.BoxMin+S.BoxMax)*.5f;
   if(S.bLocked)
   {
    const float R=FMath::Max(20.f,(S.BoxMax-S.BoxMin).GetMax()*.62f);
    Line(Out,L,G,{M+FVector2D(0,-R),M+FVector2D(R,0),M+FVector2D(0,R),M+FVector2D(-R,0),M+FVector2D(0,-R)},Fade(Hot),2.5f);
    Brackets(Out,L,G,S.BoxMin,S.BoxMax,2,Fade(Hot),2.5f);
    Text(Out,L,G,FVector2D(S.BoxMin.X,S.BoxMax.Y+8),FString::Printf(TEXT("%s  %.0f m   LMB FIRE"),S.bPointLock?TEXT("LASE LOCK"):TEXT("LOCK"),S.RangeMeters),Fade(Hot),12);
   }
   else
   {
    Brackets(Out,L,G,S.BoxMin,S.BoxMax,FMath::Lerp(70.f,4.f,S.LockProgress),Fade(Seek),2.f);
    Text(Out,L,G,FVector2D(S.BoxMin.X,S.BoxMax.Y+8),FString::Printf(TEXT("TRACK %.0f%%"),S.LockProgress*100),Fade(Seek),11);
   }
  }
  else if(S.bSeeker)Text(Out,L,G,C+FVector2D(-34,F+10),S.bLost?TEXT("LOCK LOST"):TEXT("SEARCH"),Fade(Seek),11);
  else Text(Out,L,G,C+FVector2D(-70,F+10),TEXT("HOLD RMB TO DESIGNATE"),Faint,10);
 }
 // own rocket / missile in flight, with its time to impact
 if(S.bRoundOnScreen)
 {
  const FVector2D R=S.Round;
  Circle(Out,L,G,R,9,Fade(Seek),2.f);Line(Out,L,G,{R-FVector2D(0,14),R-FVector2D(0,9)},Fade(Seek),2.f);
  Text(Out,L,G,R+FVector2D(13,-8),S.RoundSeconds>0?FString::Printf(TEXT("TOF %.1f"),S.RoundSeconds):FString::Printf(TEXT("%.0f m"),S.RoundMeters),Fade(Seek),10);
 }
 // stores panel along the bottom: every station, the selected one bright; missiles drawn on their rails
 const FVector2D Base=FVector2D(C.X,C.Y*2-118);
 const float W=150.f;const float X0=Base.X-W*S.Stations.Num()*.5f;
 for(int32 I=0;I<S.Stations.Num();++I)
 {
  const bool bSel=I==S.SelectedStation;const FLinearColor Col=bSel?Main:Faint;
  const FVector2D A(X0+I*W+8,Base.Y);
  Brackets(Out,L,G,A,A+FVector2D(W-16,34),0,Col,bSel?2.f:1.f);
  Text(Out,L,G,A+FVector2D(10,8),S.Stations[I],Col,bSel?12:11);
 }
 // the Hellfires still on the rack: four small rounds above the missile station's box
 for(int32 I=0;I<S.Stations.Num() && S.RailMissiles>=0;++I)if(S.Stations[I].StartsWith(TEXT("MSL")))
  for(int32 K=0;K<4;++K)
  {
   const FVector2D P(X0+I*W+W*.5f-34+K*18,Base.Y-22);const bool bOn=K<S.RailMissiles;const FLinearColor Col=I==S.SelectedStation?Main:Faint;
   Line(Out,L,G,{P+FVector2D(0,16),P+FVector2D(0,-4),P+FVector2D(3,-10),P+FVector2D(6,-4),P+FVector2D(6,16)},bOn?Col:FLinearColor(Col.R,Col.G,Col.B,Col.A*.35f),bOn?2.5f:1.f);
  }
 Text(Out,L,G,FVector2D(X0+8,Base.Y-58),S.Weapon,Main,13);
 Text(Out,L,G,FVector2D(X0+W*S.Stations.Num()-150,Base.Y-58),S.Ammo,Main,13);
 return L+3;
}

UVehicleAimPresentationComponent::UVehicleAimPresentationComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PostPhysics; }

bool UVehicleAimPresentationComponent::PredictImpact(UWorld* World,const AActor* Shooter,const FVector& Start,const FVector& Velocity,float GravityZ,float MaxTime,float MaxRange,FVector& OutPoint,FHitResult& OutHit,bool& bOutWater)
{
 bOutWater=false;OutHit=FHitResult();OutPoint=Start;
 if(!World || Velocity.IsNearlyZero())return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleAimPredict),false,Shooter);
 if(Shooter){TArray<AActor*> Attached;Shooter->GetAttachedActors(Attached,true,true);Q.AddIgnoredActors(Attached);}
 auto* Water=World->GetSubsystem<UMarineWorldSubsystem>();
 FVector P=Start,V=Velocity;float Travelled=0;
 // No drop (guided missile): the flight is one straight segment, split only to find the water surface.
 const float Dt=FMath::IsNearlyZero(GravityZ)?FMath::Max(Step,1500.f/FMath::Max(1.f,float(Velocity.Size()))):Step;
 for(float T=0;T<MaxTime && Travelled<MaxRange;T+=Dt)
 {
  const FVector Next=P+V*Dt+FVector(0,0,.5f*GravityZ*Dt*Dt);V.Z+=GravityZ*Dt;
  // the water surface has no collision: if this step goes under it, only the part above can strike something
  float Surface=0;FVector Current;FVector End=Next;bool bEntry=false;
  if(Water && Water->SampleWater(Next,Surface,Current) && Next.Z<Surface && P.Z>=Surface)
  {End=FMath::Lerp(P,Next,(P.Z-Surface)/FMath::Max(1.f,float(P.Z-Next.Z)));bEntry=true;}
  if(World->LineTraceSingleByChannel(OutHit,P,End,NeonCityCombat::WeaponTrace,Q))
  {
   OutPoint=OutHit.ImpactPoint;
   // the lake's water volume answers the trace at its surface: that is a strike on the water
   if(Cast<AVolume>(OutHit.GetActor())){OutHit=FHitResult();bOutWater=true;}
   return true;
  }
  if(bEntry){OutHit=FHitResult();OutPoint=End;bOutWater=true;return true;}
  Travelled+=FVector::Distance(P,Next);P=Next;
 }
 OutPoint=P;return false;
}

void UVehicleAimPresentationComponent::BeginPlay()
{
 Super::BeginPlay();
 Combat=GetOwner()->FindComponentByClass<UVehicleCombatComponent>();
 LockTone=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Art/WeaponFX/Audio/SW_NC_LockTone.SW_NC_LockTone"));
 LockedTone=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Art/WeaponFX/Audio/SW_NC_LockedTone.SW_NC_LockedTone"));
 // Laser designator: a thin glowing beam and its spot, world-placed (the airframe's sway never bends it).
 auto Make=[this](const TCHAR* Name,const TCHAR* Mesh,const TCHAR* Material)
 {
  auto* C=NewObject<UStaticMeshComponent>(GetOwner(),Name);
  C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Mesh));C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);C->SetGenerateOverlapEvents(false);
  C->SetupAttachment(GetOwner()->GetRootComponent());C->SetUsingAbsoluteLocation(true);C->SetUsingAbsoluteRotation(true);C->SetUsingAbsoluteScale(true);
  C->SetVisibility(false);C->RegisterComponent();return C;
 };
 Beam=Make(TEXT("AimLaserBeam"),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),TEXT("/Game/NeonCity/Art/WeaponFX/MI_NC_AimLaser.MI_NC_AimLaser"));
 Spot=Make(TEXT("AimLaserSpot"),TEXT("/Engine/BasicShapes/Sphere.Sphere"),TEXT("/Game/NeonCity/Art/WeaponFX/MI_NC_AimDot.MI_NC_AimDot"));
}

void UVehicleAimPresentationComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Widget)Widget->RemoveFromParent();Widget=nullptr;
 Super::EndPlay(Reason);
}

bool UVehicleAimPresentationComponent::IsLaserVisible() const { return Beam && Beam->IsVisible(); }

void UVehicleAimPresentationComponent::Hide()
{
 if(Widget){Widget->RemoveFromParent();Widget=nullptr;}
 UpdateWorldMarks(false,FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector);
 Last=FVehicleReticleState();Viewer.Reset();
}

void UVehicleAimPresentationComponent::UpdateWorldMarks(bool bShow,const FVector& From,const FVector& To,const FVector& Eye)
{
 if(!Beam || !Spot)return;
 Beam->SetVisibility(bShow);Spot->SetVisibility(bShow);
 if(!bShow)return;
 const FVector D=To-From;const float Length=D.Size();
 if(Length<10){Beam->SetVisibility(false);Spot->SetVisibility(false);return;}
 // Thicken with distance from the eye so the beam reads as a line, not a hairline that shimmers away.
 const float Radius=FMath::Clamp(FVector::Dist(Eye,(From+To)*.5f)*.0009f,.6f,6.f);
 Beam->SetWorldLocationAndRotation((From+To)*.5f,FRotationMatrix::MakeFromZ(D/Length).Rotator());
 Beam->SetWorldScale3D(FVector(Radius/50,Radius/50,Length/100));
 const float SpotSize=FMath::Clamp(FVector::Dist(Eye,To)*.004f,8.f,160.f);
 Spot->SetWorldLocation(To);Spot->SetWorldScale3D(FVector(SpotSize/100));
}

void UVehicleAimPresentationComponent::UpdateTones(float Delta,const FVehicleReticleState& S)
{
 ToneTimer-=Delta;
 if(!S.bSeeker || !S.bCandidate){ToneTimer=0;return;}
 if(ToneTimer>0)return;
 if(S.bLocked){if(LockedTone)UGameplayStatics::PlaySound2D(this,LockedTone,.55f);ToneTimer=.45f;}
 else {if(LockTone)UGameplayStatics::PlaySound2D(this,LockTone,.5f,FMath::Lerp(.9f,1.25f,S.LockProgress));ToneTimer=FMath::Lerp(.32f,.1f,S.LockProgress);}
}

void UVehicleAimPresentationComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* Pawn=Cast<APawn>(GetOwner());
 auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 auto* C=Combat.Get();
 auto* Manager=C?C->GetManager():nullptr;
 auto* W=Manager?Manager->GetWeapon():nullptr;
 if(!PC || !PC->IsLocalController() || !C || !W || !W->GetVehicleData()){bWasArmed=false;if(Widget || Viewer.IsValid() || IsLaserVisible())Hide();return;}
 // the sight exists only while the weapons are armed (V); arming plays a tone and boots the sight up
 if(!C->IsArmed()){bWasArmed=false;if(Widget || Viewer.IsValid() || IsLaserVisible())Hide();return;}
 if(!bWasArmed){bWasArmed=true;ArmedAt=GetWorld()->GetTimeSeconds();if(LockTone)UGameplayStatics::PlaySound2D(this,LockTone,.6f,.7f);}
 if(!Widget || Viewer.Get()!=PC)
 {
  if(Widget)Widget->RemoveFromParent();
  Widget=CreateWidget<UVehicleReticleWidget>(PC);if(!Widget)return;
  Widget->SetVisibility(ESlateVisibility::HitTestInvisible);Widget->AddToViewport(4);Viewer=PC;
 }
 const UVehicleWeaponData& D=*W->GetVehicleData();
 FVehicleReticleState S;S.bVisible=true;S.bArmed=C->IsArmed();S.Time=GetWorld()->GetTimeSeconds();
 S.Boot=FMath::Clamp(float(S.Time-ArmedAt)/.35f,0.f,1.f);
 S.Mode=D.WeaponType==EVehicleWeaponType::Missile?2:D.WeaponType==EVehicleWeaponType::Rocket?1:0;
 S.bPointLock=C->IsDesignatingPoint();
 // stores: every station's rounds, the selected one, Hellfires left on the rails
 const auto& Mounts=Manager->GetMounts();
 for(int32 I=0;I<Mounts.Num();++I)
 {
  const AVehicleMountedWeapon* MW=Mounts[I].Combat?Cast<AVehicleMountedWeapon>(Mounts[I].Combat->GetWeapon()):nullptr;const UVehicleWeaponData* MD=MW?MW->GetVehicleData():nullptr;if(!MD)continue;
  const TCHAR* Tag=MD->WeaponType==EVehicleWeaponType::Missile?TEXT("MSL"):MD->WeaponType==EVehicleWeaponType::Rocket?TEXT("RKT"):MD->WeaponType==EVehicleWeaponType::Cannon?TEXT("CAN"):TEXT("GUN");
  if(MW==W)S.SelectedStation=S.Stations.Num();
  S.Stations.Add(FString::Printf(TEXT("%s  %d / %d"),Tag,MW->GetAmmo(),MW->GetReserveAmmo()));
  if(const AVehicleMissileLauncher* Rack=Cast<AVehicleMissileLauncher>(MW))S.RailMissiles=Rack->GetMissilesOnRails();
 }
 const float Scale=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(PC));
 S.Center=UWidgetLayoutLibrary::GetViewportSize(PC)/Scale*.5f;
 // Predicted strike: the round leaves the muzzle along the clamped aim with the airframe's velocity.
 const FVector Start=W->GetMuzzleLocation();
 const FVector Dir=Manager->GetPoint()?Manager->GetPoint()->ClampAim(C->GetAimDirection()):C->GetAimDirection();
 const bool bRound=D.ProjectileType==EVehicleProjectileType::Bullet;
 const FVector V=Dir*D.MuzzleVelocity+(bRound?Pawn->GetVelocity().GetClampedToMaxSize(20000):FVector::ZeroVector);
 const float G=GetWorld()->GetGravityZ()*(bRound?ABulletProjectile::BallisticGravityScale(D):D.ProjectileType==EVehicleProjectileType::Rocket?.12f:0.f);
 FHitResult Hit;bool bWater=false;
 const bool bHit=PredictImpact(GetWorld(),Pawn,Start,V,G,D.Lifetime,D.Range,AimPoint,Hit,bWater);
 S.bWater=bWater;S.RangeMeters=bHit?FVector::Dist(Start,AimPoint)/100:0;
 S.bOnTarget=bHit && !bWater && Hit.GetActor() && Hit.GetActor()->Implements<UDamageInterface>() && UCombatTargetComponent::IsAliveTarget(Hit.GetActor());
 FVector2D P;
 S.bImpactOnScreen=UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,AimPoint,P,true);S.Impact=P;
 const FString Kind=D.WeaponType==EVehicleWeaponType::Missile?TEXT("MSL"):D.WeaponType==EVehicleWeaponType::Rocket?TEXT("RKT"):D.WeaponType==EVehicleWeaponType::Cannon?TEXT("CANNON"):TEXT("GUN");
 S.Weapon=FString::Printf(TEXT("%s  ARMED"),*Kind);
 S.Ammo=W->IsReloading()?FString(TEXT("RELOADING")):FString::Printf(TEXT("%d / %d"),W->GetAmmo(),W->GetReserveAmmo());
 // Seeker: only for the guided missile, armed, while the lock (RMB) is held.
 if(auto* Lock=C->GetLockSystem();Lock && D.WeaponType==EVehicleWeaponType::Missile && C->IsArmed() && C->IsLockRequested())
 {
  S.bSeeker=true;S.LockProgress=Lock->GetProgress();S.bLocked=Lock->State==EVehicleLockState::Locked;S.bLost=Lock->State==EVehicleLockState::Lost;
  const float Fov=PC->PlayerCameraManager?PC->PlayerCameraManager->GetFOVAngle():90.f;
  S.SeekerRadius=FMath::Tan(FMath::DegreesToRadians(Lock->AngleLimit))/FMath::Tan(FMath::DegreesToRadians(Fov*.5f))*S.Center.X;
  if(AActor* T=Lock->GetCandidate())
  {
   FVector O,E;T->GetActorBounds(true,O,E);
   if(E.IsNearlyZero()){O=T->GetActorLocation();E=FVector(90);}   // the laser spot: a small box on it
   FVector2D Min(1e9,1e9),Max(-1e9,-1e9);int32 Seen=0;
   for(int32 I=0;I<8;++I)
   {
    const FVector K=O+E*FVector(I&1?1:-1,I&2?1:-1,I&4?1:-1);FVector2D Q;
    if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,K,Q,true)){Min=FVector2D::Min(Min,Q);Max=FVector2D::Max(Max,Q);++Seen;}
   }
   if(Seen>0)
   {
    const FVector2D M=(Min+Max)*.5f,Half=FVector2D::Max((Max-Min)*.5f,FVector2D(18,18));
    S.bCandidate=true;S.BoxMin=M-Half;S.BoxMax=M+Half;
   }
  }
 }
 // Own rocket / missile still flying: keep a mark on it.
 if(auto* Round=Cast<ARocketProjectile>(W->GetLastProjectile());Round && !Round->IsActorBeingDestroyed())
 {
  FVector2D Q;S.bRoundOnScreen=UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Round->GetActorLocation(),Q,true);S.Round=Q;
  S.RoundMeters=FVector::Dist(Round->GetActorLocation(),Start)/100;
  // time to impact: what remains to the aim point (or the lock) at the round's speed
  const AActor* Goal=C->GetLockSystem()?C->GetLockSystem()->GetCandidate():nullptr;
  const FVector To=Goal && D.WeaponType==EVehicleWeaponType::Missile?Goal->GetActorLocation():AimPoint;
  const float Speed=Round->GetVelocity().Size();S.RoundSeconds=Speed>100?FVector::Dist(Round->GetActorLocation(),To)/Speed:0;
 }
 if(auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>())
  S.HitMarker=FMath::Clamp(1-float(S.Time-Effects->GetLastConfirmedHit(PC))/.35f,0.f,1.f);
 Widget->SetState(S);Last=S;
 UpdateTones(Delta,S);
 FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);
 // the laser designates for the guided missile while RMB is held; guns and rockets need none
 const bool bDesignating=S.Mode==2 && C->IsLockRequested();
 FVector LaserTo=AimPoint;
 if(bDesignating && S.bCandidate)if(AActor* T=C->GetLockSystem()->GetCandidate())
 {
  // the beam stops on whatever stands between the mount and the target (a tree, a wall)
  LaserTo=T->GetActorLocation();FHitResult Block;FCollisionQueryParams Q(SCENE_QUERY_STAT(AimLaser),false,Pawn);Q.AddIgnoredActor(W);
  if(GetWorld()->LineTraceSingleByChannel(Block,Start,LaserTo,ECC_Visibility,Q) && Block.GetActor()!=T)LaserTo=Block.ImpactPoint;
 }
 UpdateWorldMarks(bDesignating,Start,LaserTo,Eye);
}
