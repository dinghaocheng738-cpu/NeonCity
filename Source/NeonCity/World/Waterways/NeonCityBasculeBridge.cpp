#include "World/Waterways/NeonCityBasculeBridge.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundBase.h"

namespace NeonCityBascule
{
const TCHAR* Mesh=TEXT("/Game/NeonCity/Art/WestLake/Waterway/");
constexpr float WarnSeconds=4.f,ArmSeconds=2.5f,QuietSeconds=4.f,BlinkHz=1.4f,MaxDeckWait=15.f;
UStaticMesh* Load(const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s%s.%s"),Mesh,Name,Name));}
}
using namespace NeonCityBascule;

ANeonCityBasculeBridge::ANeonCityBasculeBridge()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0.f;
 Root=CreateDefaultSubobject<USceneComponent>(TEXT("BridgeRoot"));SetRootComponent(Root);
 auto Part=[this](const TCHAR* Name)
 {
  auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(Root);C->SetMobility(EComponentMobility::Movable);
  C->SetCollisionProfileName(TEXT("BlockAll"));C->SetCanEverAffectNavigation(false);return C;
 };
 LeafA=Part(TEXT("LeafA"));LeafB=Part(TEXT("LeafB"));PierA=Part(TEXT("PierA"));PierB=Part(TEXT("PierB"));
 for(int32 I=0;I<4;++I)
 {
  Barriers.Add(Part(*FString::Printf(TEXT("Barrier%d"),I)));
  auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Signal%d"),I));L->SetupAttachment(Root);
  L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(0);L->SetLightColor(FLinearColor(1,.08f,.04f));L->SetAttenuationRadius(900);L->SetCastShadows(false);
  Signals.Add(L);
 }
 DeckZone=CreateDefaultSubobject<UBoxComponent>(TEXT("DeckZone"));DeckZone->SetupAttachment(Root);
 DeckZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Bell=CreateDefaultSubobject<UAudioComponent>(TEXT("BridgeBell"));Bell->SetupAttachment(Root);Bell->bAutoActivate=false;
}

void ANeonCityBasculeBridge::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 // canal along X, road along Y; leaf A hinges on the -Y bank, leaf B (turned round) on the +Y bank
 UStaticMesh* Leaf=LeafMesh?LeafMesh.Get():Load(TEXT("SM_WL_BridgeLeaf"));UStaticMesh* Pier=PierMesh?PierMesh.Get():Load(TEXT("SM_WL_BridgePier"));
 LeafA->SetStaticMesh(Leaf);LeafB->SetStaticMesh(Leaf);PierA->SetStaticMesh(Pier);PierB->SetStaticMesh(Pier);
 // the pier mesh runs +y from its hinge line (Blender -y, mirrored on import): A turns round to stand on the -y bank
 PierA->SetRelativeLocationAndRotation(FVector(0,-Span*.5f,0),FRotator(0,180,0));
 PierB->SetRelativeLocationAndRotation(FVector(0,Span*.5f,0),FRotator::ZeroRotator);
 // the meshes are modelled at ModelWidth (road bridge: Hubin Road's 26 m, 16 m carriageway and 5 m footways)
 const FVector Width(DeckWidth/FMath::Max(ModelWidth,1.f),1,1);
 for(UStaticMeshComponent* C:{LeafA.Get(),LeafB.Get(),PierA.Get(),PierB.Get()})C->SetRelativeScale3D(Width);
 for(int32 I=0;I<4;++I)
 {
  // one arm per carriageway on each approach, pivots at the kerb, lying across the lane when down
  const float SideY=(I<2?-1.f:1.f)*(Span*.5f+700),Kerb=(I%2?1.f:-1.f)*(DeckWidth*.5f-120);
  Barriers[I]->SetStaticMesh(Load(TEXT("SM_WL_BridgeBarrier")));
  Barriers[I]->SetRelativeLocation(FVector(Kerb,SideY,0));
  Signals[I]->SetRelativeLocation(FVector(Kerb,SideY,180));
 }
 DeckZone->SetRelativeLocation(FVector(0,0,250));DeckZone->SetBoxExtent(FVector(DeckWidth*.5f,Span*.5f+250,250));
 if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Art/WeaponFX/Audio/SW_NC_BridgeBell.SW_NC_BridgeBell")))Bell->SetSound(Sound);
 Present();
}

void ANeonCityBasculeBridge::BeginPlay()
{
 Super::BeginPlay();OnConstruction(GetActorTransform());Open=0;Arms=0;State=ENeonCityBridgeState::Closed;Present();
}

bool ANeonCityBasculeBridge::VesselCalls() const
{
 const FTransform T=GetActorTransform();
 for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It)
 {
  // only a vessel that cannot pass under the closed span calls the bridge
  if(It->GetAirDraft()<=Clearance)continue;
  const FVector Local=T.InverseTransformPosition(It->GetActorLocation());
  if(FMath::Abs(Local.Y)>Span || FMath::Abs(Local.X)>CallRange)continue;
  const float Length=It->GetProfile()->HullExtent.X;
  if(FMath::Abs(Local.X)<Length+6000)return true;                              // under the span or waiting at it: keep it up
  const FVector V=T.InverseTransformVectorNoScale(It->GetVelocity());
  if(V.X*-FMath::Sign(Local.X)>30)return true;                                   // coming this way
 }
 return false;
}

bool ANeonCityBasculeBridge::DeckClear() const
{
 TArray<FOverlapResult> Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(BridgeDeck),false,this);
 GetWorld()->OverlapMultiByObjectType(Hits,DeckZone->GetComponentLocation(),DeckZone->GetComponentQuat(),
  FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),FCollisionShape::MakeBox(DeckZone->GetScaledBoxExtent()),Q);
 for(const auto& H:Hits)if(Cast<APawn>(H.GetActor()) && !Cast<ANeonCityMarineVehicleBase>(H.GetActor()))return false;
 return true;
}

void ANeonCityBasculeBridge::Tick(float Delta)
{
 Super::Tick(Delta);
 StateTime+=Delta;
 const bool bCalled=bForced || VesselCalls();
 switch(State)
 {
  case ENeonCityBridgeState::Closed:
   if(bCalled){State=ENeonCityBridgeState::Warning;StateTime=0;if(Bell->Sound)Bell->Play();}
   break;
  case ENeonCityBridgeState::Warning:
   // lights and bell, the arms come down, then the deck must be empty of cars and people
   if(StateTime>WarnSeconds && Arms>=1.f && (DeckClear() || StateTime>WarnSeconds+MaxDeckWait)){State=ENeonCityBridgeState::Opening;StateTime=0;}
   break;
  case ENeonCityBridgeState::Opening:
   Open=FMath::Min(1.f,Open+Delta/TravelSeconds);if(Open>=1.f){State=ENeonCityBridgeState::Open;StateTime=0;QuietTime=0;}
   break;
  case ENeonCityBridgeState::Open:
   QuietTime=bCalled?0.f:QuietTime+Delta;
   if(QuietTime>QuietSeconds){State=ENeonCityBridgeState::Closing;StateTime=0;bForced=false;}
   break;
  case ENeonCityBridgeState::Closing:
   Open=FMath::Max(0.f,Open-Delta/TravelSeconds);
   if(bCalled && !bForced){State=ENeonCityBridgeState::Opening;StateTime=0;break;}               // another vessel: back up
   if(Open<=0.f){State=ENeonCityBridgeState::Closed;StateTime=0;if(Bell->IsPlaying())Bell->FadeOut(1.f,0.f);}
   break;
 }
 const bool bArmsDown=State!=ENeonCityBridgeState::Closed;
 Arms=FMath::FInterpConstantTo(Arms,bArmsDown?1.f:0.f,Delta,1.f/ArmSeconds);
 Blink+=Delta;
 Present();
}

void ANeonCityBasculeBridge::Present()
{
 // leaves swing up about their hinge lines (along X): the leaf mesh reaches -y from its hinge and positive
 // roll lifts -y, so B (on the +y bank) is unrotated and A is B turned round
 const float Angle=OpenAngle*FMath::SmoothStep(0.f,1.f,Open);
 LeafA->SetRelativeLocationAndRotation(FVector(0,-Span*.5f,0),FRotator(0,180,Angle));
 LeafB->SetRelativeLocationAndRotation(FVector(0,Span*.5f,0),FRotator(0,0,Angle));
 for(int32 I=0;I<Barriers.Num();++I)
 {
  // the arm pivots up from across the lane (down) to vertical; it blocks only while down
  const float Pitch=90.f*(1-FMath::SmoothStep(0.f,1.f,Arms));
  Barriers[I]->SetRelativeRotation(FRotator(Pitch,I%2?180.f:0.f,0));
  Barriers[I]->SetCollisionEnabled(Arms>.9f?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  const bool bOn=State!=ENeonCityBridgeState::Closed && FMath::Fmod(Blink*BlinkHz+(I%2)*.5f,1.f)<.5f;
  Signals[I]->SetIntensity(bOn?6000.f:0.f);
 }
}
