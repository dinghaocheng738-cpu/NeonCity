#include "World/Waterways/NeonCityShipLock.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace NeonCityLock
{
const TCHAR* Mesh=TEXT("/Game/NeonCity/Art/WestLake/Waterway/");
constexpr float SwingDeg=84.f,SettleSeconds=2.f,SettleSpeed=150.f;
UStaticMesh* Load(const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s%s.%s"),Mesh,Name,Name));}
}
using namespace NeonCityLock;

bool ANeonCityLockWater::QueryWaterSurface_Implementation(FVector Point,FWaterSurfaceSample& Result) const
{
 if(!Super::QueryWaterSurface_Implementation(Point,Result))return false;
 Result.SurfacePoint.Z=Level;
 const FVector Local=GetActorTransform().InverseTransformPosition(Point);
 Result.InsideVolume=Local.Z>=-Extent.Z && Point.Z<=Level;return true;
}

ANeonCityShipLock::ANeonCityShipLock()
{
 PrimaryActorTick.bCanEverTick=true;
 Root=CreateDefaultSubobject<USceneComponent>(TEXT("LockRoot"));SetRootComponent(Root);
 for(int32 I=0;I<4;++I)
 {
  auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GateLeaf%d"),I));C->SetupAttachment(Root);
  C->SetMobility(EComponentMobility::Movable);C->SetCollisionProfileName(TEXT("BlockAll"));C->SetCanEverAffectNavigation(false);Leaves.Add(C);
 }
 Surface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChamberSurface"));Surface->SetupAttachment(Root);
 Surface->SetMobility(EComponentMobility::Movable);Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);Surface->SetCastShadow(false);
 Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("LockSign"));Sign->SetupAttachment(Root);
 Sign->SetHorizontalAlignment(EHTA_Center);Sign->SetWorldSize(70);Sign->SetTextRenderColor(FColor(255,190,90));
 Horn=CreateDefaultSubobject<UAudioComponent>(TEXT("LockHorn"));Horn->SetupAttachment(Root);Horn->bAutoActivate=false;
}

void ANeonCityShipLock::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 for(UStaticMeshComponent* Leaf:Leaves)Leaf->SetStaticMesh(Load(TEXT("SM_WL_LockGateLeaf")));
 Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
 if(auto* Lake=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/WestLake/Materials/MI_WL_Lake.MI_WL_Lake")))Surface->SetMaterial(0,Lake);
 Surface->SetRelativeScale3D(FVector(ChamberLength/100.f,ChamberWidth/100.f,1));
 Sign->SetRelativeLocationAndRotation(FVector(-ChamberLength*.5f-200,-ChamberWidth*.5f-250,900),FRotator(0,180,0));
 if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Art/WeaponFX/Audio/SW_NC_LockHorn.SW_NC_LockHorn")))Horn->SetSound(Sound);
 Level=UpperLevel;Upper=1;Lower=0;Present();
}

void ANeonCityShipLock::BeginPlay()
{
 Super::BeginPlay();
 OnConstruction(GetActorTransform());
 Level=At==ESide::Upper?UpperLevel:LowerLevel;
 FActorSpawnParameters P;P.Owner=this;P.bDeferConstruction=true;
 const FVector Centre=GetActorTransform().TransformPosition(FVector(0,0,0));
 const FTransform T(GetActorRotation(),FVector(Centre.X,Centre.Y,FloorZ+1000));
 Water=GetWorld()->SpawnActor<ANeonCityLockWater>(ANeonCityLockWater::StaticClass(),T,P);
 if(Water)
 {
  Water->Extent=FVector(ChamberLength*.5f,ChamberWidth*.5f,1000);Water->bContainedPoolWater=true;Water->BasinPriority=10;Water->Level=Level;
  Water->FinishSpawning(T);
 }
 Present();
}

void ANeonCityShipLock::EndPlay(EEndPlayReason::Type Reason)
{
 if(Water)Water->Destroy();Water=nullptr;Super::EndPlay(Reason);
}

void ANeonCityShipLock::Survey(float Delta,bool& bSettledInChamber,ESide& Wants,bool& bChamberEmpty,bool& bCallUpper,bool& bCallLower)
{
 bSettledInChamber=false;bChamberEmpty=true;bCallUpper=bCallLower=false;bool bAnySettled=false;
 const FTransform T=GetActorTransform();const float Half=ChamberLength*.5f;
 for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It)
 {
  const FVector L=T.InverseTransformPosition(It->GetActorLocation());
  if(FMath::Abs(L.Y)>ChamberWidth)continue;
  const FVector V=T.InverseTransformVectorNoScale(It->GetVelocity());
  const float Length=It->GetProfile()->HullExtent.X;
  if(FMath::Abs(L.X)<Half)
  {
   bChamberEmpty=false;
   // wholly inside and stopped: lock it through toward where it points
   if(FMath::Abs(L.X)+Length<Half+150 && V.Size2D()<SettleSpeed)
   {bAnySettled=true;Wants=T.InverseTransformVectorNoScale(It->GetActorForwardVector()).X>0?ESide::Lower:ESide::Upper;}
   continue;
  }
  // beyond a gate, close to it or heading for it
  const bool bNear=FMath::Abs(L.X)-Half<Length+4000;
  if(L.X<0 && -L.X-Half<CallRange && (bNear || V.X>30))bCallUpper=true;
  if(L.X>0 && L.X-Half<CallRange && (bNear || V.X<-30))bCallLower=true;
 }
 Settled=bAnySettled?Settled+Delta:0.f;
 bSettledInChamber=Settled>SettleSeconds;
}

void ANeonCityShipLock::Begin(ESide Target)
{
 Goal=Target;Phase=ENeonCityLockPhase::ClosingGate;
 if(Horn->Sound)Horn->Play();
}

void ANeonCityShipLock::Tick(float Delta)
{
 Super::Tick(Delta);
 bool bSettled,bEmpty,bCallUpper,bCallLower;ESide Wants=At;
 Survey(Delta,bSettled,Wants,bEmpty,bCallUpper,bCallLower);
 switch(Phase)
 {
  case ENeonCityLockPhase::Ready:
   if(bSettled && Wants!=At)Begin(Wants);
   else if(bEmpty && At==ESide::Upper && bCallLower && !bCallUpper)Begin(ESide::Lower);
   else if(bEmpty && At==ESide::Lower && bCallUpper && !bCallLower)Begin(ESide::Upper);
   break;
  case ENeonCityLockPhase::ClosingGate:
  {
   float& Gate=At==ESide::Upper?Upper:Lower;
   Gate=FMath::Max(0.f,Gate-Delta/GateSeconds);
   if(Gate<=0.f)Phase=Goal==ESide::Upper?ENeonCityLockPhase::Filling:ENeonCityLockPhase::Draining;
   break;
  }
  case ENeonCityLockPhase::Filling:
  case ENeonCityLockPhase::Draining:
  {
   const float Target=Goal==ESide::Upper?UpperLevel:LowerLevel;
   Level=FMath::FInterpConstantTo(Level,Target,Delta,FMath::Abs(UpperLevel-LowerLevel)/LevelSeconds);
   if(FMath::IsNearlyEqual(Level,Target,.5f)){Level=Target;Phase=ENeonCityLockPhase::OpeningGate;}
   break;
  }
  case ENeonCityLockPhase::OpeningGate:
  {
   float& Gate=Goal==ESide::Upper?Upper:Lower;
   Gate=FMath::Min(1.f,Gate+Delta/GateSeconds);
   if(Gate>=1.f){At=Goal;Phase=ENeonCityLockPhase::Ready;}
   break;
  }
 }
 if(Water)Water->Level=Level;
 Present();
}

void ANeonCityShipLock::Present()
{
 const float Half=ChamberLength*.5f,W=ChamberWidth*.5f;
 // the leaf mesh reaches -Y from its heel post (Blender +y, mirrored on import): leaf A on the -Y wall is turned
 // round to reach +Y, B on the +Y wall is unrotated. Open, they fold back against the walls: the upper pair
 // into the canal side (-X), the lower pair out to sea (+X), clear of the chamber
 auto Pose=[&](int32 I,float GateX,float Openness,float SwingSign)
 {
  const float S=FMath::SmoothStep(0.f,1.f,Openness)*SwingDeg*SwingSign;
  Leaves[I]->SetRelativeLocationAndRotation(FVector(GateX,-W,FloorZ-GetActorLocation().Z),FRotator(0,180+S,0));
  Leaves[I+1]->SetRelativeLocationAndRotation(FVector(GateX,W,FloorZ-GetActorLocation().Z),FRotator(0,-S,0));
  for(int32 K:{I,I+1})Leaves[K]->SetCollisionEnabled(Openness<.98f?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 };
 Pose(0,-Half,Upper,1.f);Pose(2,Half,Lower,-1.f);
 Surface->SetRelativeLocation(FVector(0,0,Level-GetActorLocation().Z));
 const TCHAR* Text=Phase==ENeonCityLockPhase::Ready?(At==ESide::Upper?TEXT("LOCK  |  LAKE LEVEL  |  ENTER"):TEXT("LOCK  |  SEA LEVEL  |  ENTER"))
  :Phase==ENeonCityLockPhase::ClosingGate?TEXT("LOCK  |  GATES CLOSING"):Phase==ENeonCityLockPhase::OpeningGate?TEXT("LOCK  |  GATES OPENING")
  :Phase==ENeonCityLockPhase::Filling?TEXT("LOCK  |  FILLING"):TEXT("LOCK  |  DRAINING");
 Sign->SetText(FText::FromString(Text));
}
