#include "World/Audio/NeonCityLandSoundscape.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace NeonCityLandAudio
{
const TCHAR* TW=TEXT("/Game/NeonCity/Art/Tidewater/Audio/");
const TCHAR* WL=TEXT("/Game/NeonCity/Art/WestLake/Audio/");
USoundBase* Load(const TCHAR* Dir,const FString& Name)
{ return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("%s%s.%s"),Dir,*Name,*Name),nullptr,LOAD_NoWarn|LOAD_Quiet); }
// bed order: wind, city, cicadas, birds (day chorus), crickets, frogs, lapping
const TCHAR* BedDirs[ANeonCityLandSoundscape::BedCount]={TW,WL,WL,TW,TW,WL,TW};
const TCHAR* BedNames[ANeonCityLandSoundscape::BedCount]={TEXT("SW_NCTW_wind"),TEXT("SW_WL_CityBed"),TEXT("SW_WL_Cicadas"),TEXT("SW_NCTW_birds_dawn"),
 TEXT("SW_NCTW_crickets"),TEXT("SW_WL_Frogs"),TEXT("SW_NCTW_pier_lap")};
const float BedGain[ANeonCityLandSoundscape::BedCount]={.55f,.6f,.32f,.35f,.45f,.55f,.6f};
}
using namespace NeonCityLandAudio;

ANeonCityLandSoundscape::ANeonCityLandSoundscape()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 SetActorEnableCollision(false);
}

USoundAttenuation* ANeonCityLandSoundscape::MakeAttenuation(float Inner,float Falloff)
{
 auto* A=NewObject<USoundAttenuation>(this);
 A->Attenuation.bAttenuate=true;A->Attenuation.bSpatialize=true;A->Attenuation.AttenuationShape=EAttenuationShape::Sphere;
 A->Attenuation.AttenuationShapeExtents=FVector(Inner,0,0);A->Attenuation.FalloffDistance=Falloff;
 A->Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;
 A->Attenuation.bAttenuateWithLPF=true;A->Attenuation.LPFRadiusMin=Inner;A->Attenuation.LPFRadiusMax=Inner+Falloff;
 return A;
}

bool ANeonCityLandSoundscape::Load()
{
 if(bLoaded)return Beds.Num()==BedCount;
 bLoaded=true;
 for(int32 I=0;I<BedCount;++I)
 {
  USoundBase* S=NeonCityLandAudio::Load(BedDirs[I],BedNames[I]);
  if(auto* Wave=Cast<USoundWave>(S))Wave->bLooping=true;
  auto* C=NewObject<UAudioComponent>(this);C->bAutoActivate=false;C->bAutoDestroy=false;C->bAllowSpatialization=false;
  C->bIsUISound=false;C->SetSound(S);C->SetupAttachment(GetRootComponent());C->RegisterComponent();C->SetVolumeMultiplier(0.f);
  Beds.Add(C);
 }
 for(int32 K=1;K<=13;++K)if(auto* S=NeonCityLandAudio::Load(TW,FString::Printf(TEXT("SW_NCTW_bird_forest_%02d"),K)))Birds.Add(S);
 for(int32 K=1;K<=5;++K)if(auto* S=NeonCityLandAudio::Load(TW,FString::Printf(TEXT("SW_NCTW_bird_dove_%02d"),K)))Birds.Add(S);
 for(int32 K=1;K<=4;++K)if(auto* S=NeonCityLandAudio::Load(WL,FString::Printf(TEXT("SW_WL_Oriole_%02d"),K)))Orioles.Add(S);
 for(int32 K=1;K<=4;++K)if(auto* S=NeonCityLandAudio::Load(WL,FString::Printf(TEXT("SW_WL_Horn_%02d"),K)))Horns.Add(S);
 Bell=NeonCityLandAudio::Load(WL,TEXT("SW_WL_TempleBell"));
 BirdFalloff=MakeAttenuation(400,5000);HornFalloff=MakeAttenuation(1500,25000);BellFalloff=MakeAttenuation(6000,250000);
 return true;
}

void ANeonCityLandSoundscape::BeginPlay()
{
 Super::BeginPlay();
 if(!GetWorld() || !GetWorld()->IsGameWorld() || GetNetMode()==NM_DedicatedServer){SetActorTickEnabled(false);return;}
 Load();
 for(UAudioComponent* B:Beds)if(B && B->Sound)B->Play(FMath::FRand()*10.f);
}

void ANeonCityLandSoundscape::EndPlay(const EEndPlayReason::Type Reason)
{
 for(UAudioComponent* B:Beds)if(B)B->Stop();
 Super::EndPlay(Reason);
}

float ANeonCityLandSoundscape::SampleCity(const FVector& At) const
{
 if(GridSize.X<2 || GridSize.Y<2 || CityGrid.Num()!=GridSize.X*GridSize.Y || Cell<=0)return 0.f;
 const float U=(At.X-GridOrigin.X)/Cell,V=(At.Y-GridOrigin.Y)/Cell;
 const int32 X0=FMath::Clamp(FMath::FloorToInt(U),0,GridSize.X-2),Y0=FMath::Clamp(FMath::FloorToInt(V),0,GridSize.Y-2);
 const float FX=FMath::Clamp(U-X0,0.f,1.f),FY=FMath::Clamp(V-Y0,0.f,1.f);
 auto G=[&](int32 X,int32 Y){return CityGrid[Y*GridSize.X+X]/255.f;};
 return FMath::Lerp(FMath::Lerp(G(X0,Y0),G(X0+1,Y0),FX),FMath::Lerp(G(X0,Y0+1),G(X0+1,Y0+1),FX),FY);
}

float ANeonCityLandSoundscape::WaterAround(const FVector& At) const
{
 // share of a 30 m ring (and the spot itself) that is open water: 0 inland, ~0.5 on the shore, 1 out on the lake
 const auto* Marine=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();if(!Marine)return 0.f;
 int32 Wet=0;
 // open water where the water surface stands above the ground (the query alone answers for the whole basin)
 FCollisionQueryParams Q(SCENE_QUERY_STAT(LandSoundscapeWater),false);
 if(APlayerController* PC=GetWorld()->GetFirstPlayerController())if(APawn* Pawn=PC->GetPawn())Q.AddIgnoredActor(Pawn);
 auto IsWet=[&](const FVector& P)
 {
  float H;FVector Current;if(!Marine->SampleWater(P,H,Current))return false;
  FHitResult G;
  return !GetWorld()->LineTraceSingleByChannel(G,FVector(P.X,P.Y,H+3000),FVector(P.X,P.Y,H-20000),ECC_Visibility,Q) || G.ImpactPoint.Z<H-20;
 };
 for(int32 K=0;K<8;++K)
 {
  const FVector P=At+FVector(FMath::Cos(K*PI/4),FMath::Sin(K*PI/4),0)*3000;
  if(IsWet(P))++Wet;
 }
 if(IsWet(At))Wet+=2;
 return Wet/10.f;
}

void ANeonCityLandSoundscape::PlayAt(USoundBase* Sound,const FVector& At,float Volume,float Pitch,USoundAttenuation* Attenuation)
{
 if(!Sound)return;
 UGameplayStatics::PlaySoundAtLocation(this,Sound,At,FRotator::ZeroRotator,Volume,Pitch,0.f,Attenuation);
 ++OneShots;
}

void ANeonCityLandSoundscape::RingBell(int32 Strokes)
{
 BellQueue+=FMath::Max(0,Strokes);BellGap=0;
}

void ANeonCityLandSoundscape::Tick(float Delta)
{
 Super::Tick(Delta);
 if(!Load())return;
 APlayerController* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 FVector Ear;FRotator View;PC->GetPlayerViewPoint(Ear,View);
 const auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>();
 const float Night=Lighting?Lighting->GetNightAmount():0.f,Day=1-Night;
 WaterAge+=Delta;if(WaterAge>.5f){WaterAge=0;WaterCache=WaterAround(Ear);}
 const float Water=WaterCache,Land=1-Water,Shore=FMath::Clamp(4*Water*(1-Water),0.f,1.f);
 const float City=SampleCity(Ear);
 float Lotus=0;for(const FVector& P:LotusBeds)Lotus=FMath::Max(Lotus,1-FMath::SmoothStep(4000.f,15000.f,float(FVector::Dist2D(P,Ear))));
 // bed targets (wind, city, cicadas, birds, crickets, frogs, lapping)
 const float Target[BedCount]={
  .25f+.5f*Water,
  City*(1-.45f*Night),
  (1-City)*Land*Day*Day,
  (1-City*.8f)*Land*Day*.8f,
  (1-City*.7f)*Land*Night,
  Lotus*(.3f+.7f*Night),
  Shore*(Ear.Z<1500.f?1.f:.3f)};
 for(int32 I=0;I<BedCount;++I)
 {
  Level[I]=FMath::FInterpConstantTo(Level[I],FMath::Clamp(Target[I],0.f,1.f),Delta,.6f);
  if(Beds[I])Beds[I]->SetVolumeMultiplier(FMath::Max(.0001f,Level[I]*BedGain[I]));
 }
 // birds in the trees around the listener: by day, away from the CBD; orioles in the willow groves
 BirdWait-=Delta;
 if(BirdWait<=0)
 {
  const float Green=(1-City)*Land*Day;
  BirdWait=FMath::Lerp(9.f,2.f,Green)*FMath::FRandRange(.6f,1.4f);
  if(Green>.2f && (Birds.Num() || Orioles.Num()))
  {
   float Grove=0;for(const FVector& P:OrioleGroves)Grove=FMath::Max(Grove,1-FMath::SmoothStep(15000.f,40000.f,float(FVector::Dist2D(P,Ear))));
   const bool bOriole=Orioles.Num() && FMath::FRand()<.15f+.6f*Grove;
   const auto& Pool=bOriole?Orioles:Birds;
   if(Pool.Num())
   {
    const FVector At=Ear+FVector(FMath::VRand().GetSafeNormal2D()*FMath::FRandRange(1200.f,4000.f))+FVector(0,0,FMath::FRandRange(500.f,1100.f));
    PlayAt(Pool[FMath::RandHelper(Pool.Num())],At,FMath::FRandRange(.45f,.8f)*Green,FMath::FRandRange(.93f,1.07f),BirdFalloff);
   }
  }
 }
 // horns off the avenues
 HornWait-=Delta;
 if(HornWait<=0)
 {
  HornWait=FMath::FRandRange(7.f,22.f);
  if(City>.3f && Horns.Num() && FMath::FRand()<City*(1-.6f*Night))
  {
   const FVector At=Ear+FVector(FMath::VRand().GetSafeNormal2D()*FMath::FRandRange(5000.f,16000.f))+FVector(0,0,150);
   PlayAt(Horns[FMath::RandHelper(Horns.Num())],At,FMath::FRandRange(.35f,.7f),FMath::FRandRange(.95f,1.05f),HornFalloff);
  }
 }
 // Jingci Temple bell: three strokes on the hour from 06:00 to 21:00, nine at 18:00 (the evening bell)
 int32 Hour=-1;
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){Hour=FMath::FloorToInt(It->GetTimeOfDay());break;}
 if(Hour>=0 && Hour!=LastBellHour)
 {
  if(LastBellHour>=0 && Hour>=6 && Hour<=21)RingBell(Hour==18?9:3);
  LastBellHour=Hour;
 }
 if(BellQueue>0)
 {
  BellGap-=Delta;
  if(BellGap<=0)
  {
   BellGap=8.f;--BellQueue;++BellStrokes;
   PlayAt(Bell,TempleBell.IsNearlyZero()?Ear+FVector(50000,0,2000):TempleBell,1.f,FMath::FRandRange(.98f,1.02f),BellFalloff);
  }
 }
}
