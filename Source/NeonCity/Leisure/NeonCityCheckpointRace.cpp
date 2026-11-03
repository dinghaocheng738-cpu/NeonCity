#include "Leisure/NeonCityCheckpointRace.h"
#include "Leisure/NeonCityActivityHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace NeonCityRace
{
const FName SilkSlot(TEXT("LanternSilk"));
// silk glow (M_WL_Simple EmissiveStrength) and the gate light, by state: next, the one after, passed / later
const float GlowNext=14.f,GlowAfter=3.f,GlowIdle=.6f;
const float LightNext=9000.f,LightAfter=1800.f;     // lumens
const FLinearColor LanternRed(1.f,.28f,.12f);
}
using namespace NeonCityRace;

ANeonCityCheckpointRace::ANeonCityCheckpointRace()
{
 Vehicle=ENeonCityLeisureVehicle::Boat;OpenFromHour=19;OpenToHour=5;MaximumSeconds=480;RewardPreview=1200;
 Accent=FLinearColor(1.f,.32f,.14f);
 MarkerMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_LanternBuoy.SM_WL_LanternBuoy")));
}
int32 ANeonCityCheckpointRace::MedalFor(float Seconds) const
{
 if(Seconds<=0)return 0;
 return Seconds<=GoldSeconds?3:Seconds<=SilverSeconds?2:Seconds<=BronzeSeconds?1:0;
}
bool ANeonCityCheckpointRace::Crosses(int32 Index,const FVector& From,const FVector& To) const
{
 if(!Gates.IsValidIndex(Index))return false;
 const FNeonCityRaceGate& G=Gates[Index];
 const FVector Fwd=FRotator(0,G.Yaw,0).Vector(),Side=FRotator(0,G.Yaw+90,0).Vector();
 const double A=FVector::DotProduct(From-G.Location,Fwd),B=FVector::DotProduct(To-G.Location,Fwd);
 if(!(A<0 && B>=0))return false;
 const double T=A/(A-B);const FVector P=FMath::Lerp(From,To,T);
 return FMath::Abs(FVector::DotProduct(P-G.Location,Side))<=G.HalfWidth && FMath::Abs(P.Z-G.Location.Z)<=G.HalfHeight;
}
FString ANeonCityCheckpointRace::DescribeRules() const
{
 FString S=Description.ToString();
 S+=FString::Printf(TEXT("\n\n%d gates · Gold %s ($%lld) · Silver %s ($%lld) · Bronze %s ($%lld)"),Gates.Num(),
  *FormatTime(GoldSeconds),GoldReward,*FormatTime(SilverSeconds),SilverReward,*FormatTime(BronzeSeconds),BronzeReward);
 if(Progress.State==1)S+=Progress.ActivityStage==0?TEXT("\n\nGo to the start and stop there to begin the countdown."):TEXT("\n\nRace in progress.");
 return S;
}
void ANeonCityCheckpointRace::OnAccepted(APlayerController* Player)
{
 SpawnMarkers();LightGates();bHasLast=false;Countdown=0;Splits.Reset();
 Guide(StartLocation,FText::Format(FText::FromString(TEXT("{0} start")),Title));
 const TCHAR* Need=Vehicle==ENeonCityLeisureVehicle::Boat?TEXT("Bring a boat"):Vehicle==ENeonCityLeisureVehicle::Aircraft?TEXT("Fly"):TEXT("Come");
 Notify(FText::FromString(FString::Printf(TEXT("%s to the start marked on your map and stop there."),Need)),7);
}
void ANeonCityCheckpointRace::SpawnMarkers()
{
 UStaticMesh* Mesh=MarkerMesh.LoadSynchronous();
 for(int32 I=0;I<Gates.Num();++I)
 {
  const FNeonCityRaceGate& G=Gates[I];const FVector Side=FRotator(0,G.Yaw+90,0).Vector();
  if(!G.bBridgeArch && Mesh)for(int32 S=-1;S<=1;S+=2)
  {
   auto* M=NewObject<UStaticMeshComponent>(this);M->SetStaticMesh(Mesh);M->SetMobility(EComponentMobility::Movable);
   M->SetWorldLocationAndRotation(G.Location+Side*(S*G.HalfWidth)+FVector(0,0,MarkerZ),FRotator(0,G.Yaw,0));
   M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->ComponentTags.Add(FName(*FString::FromInt(I)));
   M->RegisterComponent();
   const int32 Slot=M->GetMaterialIndex(SilkSlot);
   if(Slot!=INDEX_NONE)M->CreateDynamicMaterialInstance(Slot);
   Markers.Add(M);
  }
  auto* L=NewObject<UPointLightComponent>(this);L->SetMobility(EComponentMobility::Movable);
  L->SetWorldLocation(G.Location+FVector(0,0,G.bBridgeArch?260:180));L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(0);
  L->SetAttenuationRadius(G.bBridgeArch?1400:2200);L->SetLightColor(LanternRed);L->SetCastShadows(false);L->SetSourceRadius(40);
  L->RegisterComponent();GateLights.Add(L);
 }
}
void ANeonCityCheckpointRace::LightGates()
{
 const int32 Next=Progress.ActivityCheckpoint;
 for(UStaticMeshComponent* M:Markers)if(M)
 {
  const int32 I=M->ComponentTags.Num()?FCString::Atoi(*M->ComponentTags[0].ToString()):-1;
  const float Glow=I==Next?GlowNext:I==Next+1?GlowAfter:GlowIdle*(I<Next?.3f:1.f);
  const int32 Slot=M->GetMaterialIndex(SilkSlot);
  if(auto* D=Slot!=INDEX_NONE?Cast<UMaterialInstanceDynamic>(M->GetMaterial(Slot)):nullptr)D->SetScalarParameterValue(TEXT("EmissiveStrength"),Glow);
 }
 for(int32 I=0;I<GateLights.Num();++I)if(GateLights[I])
  GateLights[I]->SetIntensity(I==Next?LightNext:I==Next+1?LightAfter:0.f);
}
void ANeonCityCheckpointRace::ClearRun()
{
 for(UStaticMeshComponent* M:Markers)if(M)M->DestroyComponent();Markers.Reset();
 for(UPointLightComponent* L:GateLights)if(L)L->DestroyComponent();GateLights.Reset();
 Countdown=0;bHasLast=false;
}
FText ANeonCityCheckpointRace::CheckFailure(APlayerController* Player,APawn* Pawn) const
{
 const FText Base=Super::CheckFailure(Player,Pawn);if(!Base.IsEmpty())return Base;
 if(Progress.ActivityStage==2 && Vehicle!=ENeonCityLeisureVehicle::Any && Classify(Pawn)!=Vehicle && VehicleMissingSeconds>6)
  return FText::FromString(TEXT("You left the race boat."));
 return FText::GetEmpty();
}
void ANeonCityCheckpointRace::ShowRaceHUD(float Speed)
{
 FNeonCityActivityHUDState S;S.Title=Title;S.Accent=Accent;
 const int32 Next=Progress.ActivityCheckpoint;
 S.Clock=FText::FromString(FormatTime(Progress.ActivitySeconds));
 FString Sub=FString::Printf(TEXT("Gate %d / %d"),FMath::Min(Next+1,Gates.Num()),Gates.Num());
 if(Gates.IsValidIndex(Next) && !Gates[Next].Label.IsEmpty())Sub+=TEXT("  ·  ")+Gates[Next].Label.ToString();
 S.Sub=FText::FromString(Sub);
 if(SplitShow>0)S.Lines.Add(FText::FromString(FString::Printf(TEXT("Split %s%.1f s vs best"),SplitDelta<=0?TEXT("-"):TEXT("+"),FMath::Abs(SplitDelta))));
 const int32 Medal=MedalFor(Progress.ActivitySeconds);
 const TCHAR* Pace=Medal==3?TEXT("on gold pace"):Medal==2?TEXT("silver time"):Medal==1?TEXT("bronze time"):TEXT("outside the medals");
 S.Lines.Add(FText::FromString(FString::Printf(TEXT("Gold %s · Silver %s · Bronze %s  (%s)"),*FormatTime(GoldSeconds),*FormatTime(SilverSeconds),*FormatTime(BronzeSeconds),Pace)));
 S.Lines.Add(FText::FromString(FString::Printf(TEXT("%.0f km/h%s"),Speed,Progress.ActivityBestSeconds>0?*FString::Printf(TEXT("  ·  best %s"),*FormatTime(Progress.ActivityBestSeconds)):TEXT(""))));
 S.Progress=Gates.Num()?float(Next)/Gates.Num():-1.f;
 ShowHUD(S);
}
void ANeonCityCheckpointRace::TickActivity(float Delta,APlayerController* Player,APawn* Pawn)
{
 const bool RightVehicle=Vehicle==ENeonCityLeisureVehicle::Any || Classify(Pawn)==Vehicle;
 VehicleMissingSeconds=RightVehicle?0.f:VehicleMissingSeconds+Delta;
 const FVector At=Pawn->GetActorLocation();const float Speed=Pawn->GetVelocity().Size()*.036f;
 auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>();
 if(Progress.ActivityStage==0)
 {
  const bool Near=FVector::Dist2D(At,StartLocation)<=StartRadius;
  if(Near && RightVehicle && Speed<=StartSpeedKmh)
  {
   SetStage(1);Countdown=3.99f;LastBeep=4;
   Notify(FText::FromString(TEXT("Hold position... the race starts on GO.")),4);
  }
  else if(Near && !RightVehicle && VehicleMissingSeconds>0 && FMath::Fmod(VehicleMissingSeconds,8.f)<Delta)
   Notify(FText::FromString(TEXT("This race needs a boat. The boat club's Lamborghini 63 is moored at the Hubin revetment.")),6);
  return;
 }
 if(Progress.ActivityStage==1)
 {
  if(!RightVehicle || FVector::Dist2D(At,StartLocation)>StartRadius*1.6f)
  {SetStage(0);Notify(FText::FromString(TEXT("Countdown stopped. Return to the start.")));return;}
  Countdown-=Delta;const int32 Shown=FMath::CeilToInt(Countdown);
  if(Shown<LastBeep && Shown>0){LastBeep=Shown;PlayCue(TEXT("Beep"));if(HUD)HUD->Banner(FText::AsNumber(Shown),FText::GetEmpty(),FLinearColor::White,.9f);}
  if(Countdown<=0)
  {
   PlayCue(TEXT("Go"));if(HUD)HUD->Banner(FText::FromString(TEXT("GO!")),Title,Accent,1.2f);
   Progress.ActivitySeconds=0;Progress.ActivityCheckpoint=0;Splits.Reset();bHasLast=false;SetStage(2);LightGates();
   if(Gates.Num())Guide(Gates[0].Location,Gates[0].Label.IsEmpty()?Title:Gates[0].Label);
  }
  FNeonCityActivityHUDState S;S.Title=Title;S.Accent=Accent;S.Clock=FText::FromString(FormatTime(0));S.Sub=FText::FromString(TEXT("Get ready"));
  ShowHUD(S);return;
 }
 // racing
 Progress.ActivitySeconds+=Delta;SplitShow=FMath::Max(0.f,SplitShow-Delta);
 if(bHasLast && RightVehicle && Crosses(Progress.ActivityCheckpoint,LastLocation,At))
 {
  const int32 Passed=Progress.ActivityCheckpoint;Splits.Add(Progress.ActivitySeconds);
  if(BestSplits.IsValidIndex(Passed)){SplitDelta=Progress.ActivitySeconds-BestSplits[Passed];SplitShow=4;}
  ++Progress.ActivityCheckpoint;
  if(Progress.ActivityCheckpoint>=Gates.Num())
  {
   const float T=Progress.ActivitySeconds;const int32 Medal=MedalFor(T),Before=MedalFor(Progress.ActivityBestSeconds);
   const int64 Prize=Medal==3?GoldReward:Medal==2?SilverReward:Medal==1?BronzeReward:0;
   const int64 Pay=Medal>Before?Prize:Prize/5;
   if(Progress.ActivityBestSeconds<=0 || T<Progress.ActivityBestSeconds)BestSplits=Splits;
   const bool Record=Progress.ActivityBestSeconds<=0 || T<Progress.ActivityBestSeconds;
   const TCHAR* Names[]={TEXT("FINISHED"),TEXT("BRONZE"),TEXT("SILVER"),TEXT("GOLD")};
   const FString Detail=FString::Printf(TEXT("%s%s  ·  rank %d of %d  ·  $%lld"),*FormatTime(T),Record?TEXT("  new best"):TEXT(""),RankOf(T),Rivals.Num()+1,Pay);
   Complete(Pay,T,FText::FromString(Names[Medal]),FText::FromString(Detail));
   return;
  }
  PlayCue(TEXT("Gate"),.8f);LightGates();
  const auto& G=Gates[Progress.ActivityCheckpoint];Guide(G.Location,G.Label.IsEmpty()?Title:G.Label);
 }
 LastLocation=At;bHasLast=true;
 ShowRaceHUD(Speed);
}
