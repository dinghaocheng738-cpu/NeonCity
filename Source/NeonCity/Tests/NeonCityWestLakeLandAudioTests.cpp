#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "World/Audio/NeonCityLandSoundscape.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityFootstepComponent.h"
#include "Vehicles/Marine/NeonCityLamborghini63.h"

namespace NeonCityWestLakeLandAudio
{
/** West Lake land sound: the player is set down around the map and walks a few steps at each place. The beds must
 * follow the place and the hour (traffic on a CBD avenue, cicadas and birdsong in Liulang park by day, crickets
 * there at night, frogs by the lotus beds at night, lapping along the shore), birds must sing from the trees, the
 * Jingci bell must ring, and the footsteps must match the ground (asphalt, grass, stone paving, the yacht's teak). */
class FTour:public IAutomationLatentCommand
{
public:
 explicit FTour(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>300){Test->AddError(FString::Printf(TEXT("Land audio tour timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  ACharacter* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<5)return false;
  if(!Scape.IsValid())
  {
   for(TActorIterator<ANeonCityLandSoundscape> It(W);It;++It){Scape=*It;break;}
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(!It->ActorHasTag(TEXT("WL.Leisure"))){Boat=*It;break;}      // the player's own 63, not the boat club's
   if(!Test->TestNotNull(TEXT("the West Lake land soundscape is in the map"),Scape.Get()))return true;
   Steps=P->FindComponentByClass<UNeonCityFootstepComponent>();
   if(!Test->TestNotNull(TEXT("the player has footsteps"),Steps))return true;
  }
  const double E=FPlatformTime::Seconds()-At;auto* S=Scape.Get();
  auto Walk=[&](){P->AddMovementInput(FRotator(0,PC->GetControlRotation().Yaw,0).Vector(),.55f);if(!Steps->GetLastSurface().IsNone())Trodden.Add(Steps->GetLastSurface());};
  auto Trod=[&](const TCHAR* What){const bool bYes=Trodden.Contains(FName(What));Trodden.Reset();return bYes;};
  switch(Stage)
  {
  case 0: Hour(W,11);Put(P,Find(W,FVector(-35000,46950,0),TEXT("asphalt")));return Next(1);                                  // CBD avenue, late morning
  case 1:
   if(E<3.5)return false;Walk();if(E<5.5)return false;
   Report(TEXT("CBD avenue"));
   Test->TestTrue(FString::Printf(TEXT("traffic on the CBD avenue (city %.2f, bed %.2f)"),S->SampleCity(P->GetActorLocation()),S->GetBedLevel(1)),S->GetBedLevel(1)>.45f);
   Test->TestTrue(TEXT("no cicadas over the traffic"),S->GetBedLevel(2)<.35f);
   Test->TestTrue(TEXT("asphalt underfoot on the avenue"),Trod(TEXT("asphalt")));
   Birds=S->GetOneShotCount();Put(P,Find(W,FVector(-4000,-35000,0),TEXT("grass")));return Next(2);                // Liulang park lawn
  case 2:
   if(E<3.5)return false;Walk();if(E<12)return false;
   Report(TEXT("Liulang park"));
   Test->TestTrue(FString::Printf(TEXT("cicadas in the park (%.2f)"),S->GetBedLevel(2)),S->GetBedLevel(2)>.3f);
   Test->TestTrue(FString::Printf(TEXT("quiet of traffic (%.2f)"),S->GetBedLevel(1)),S->GetBedLevel(1)<.3f);
   Test->TestTrue(FString::Printf(TEXT("birds sing from the trees (%d calls)"),S->GetOneShotCount()-Birds),S->GetOneShotCount()-Birds>=2);
   Test->TestTrue(TEXT("grass underfoot in the park"),Trod(TEXT("grass")));
   Hour(W,22.5f);return Next(3);
  case 3:
   if(E<4)return false;
   Report(TEXT("Liulang park at night"));
   Test->TestTrue(FString::Printf(TEXT("crickets at night (%.2f)"),S->GetBedLevel(4)),S->GetBedLevel(4)>.3f);
   Test->TestTrue(FString::Printf(TEXT("cicadas quiet at night (%.2f)"),S->GetBedLevel(2)),S->GetBedLevel(2)<.2f);
   if(S->LotusBeds.Num())Put(P,S->LotusBeds[0]+FVector(0,0,800));
   return Next(4);
  case 4:
   if(E<4)return false;
   Report(TEXT("lotus beds at night"));
   Test->TestTrue(FString::Printf(TEXT("frogs among the lotus at night (%.2f)"),S->GetBedLevel(5)),S->GetBedLevel(5)>.4f);
   Hour(W,11);Put(P,FVector(-4900,-35000,0));return Next(5);                                          // stone path
  case 5:
   if(E<1.5)return false;Walk();if(E<3.5)return false;
   Test->TestTrue(TEXT("stone paving underfoot on the path"),Trod(TEXT("stone")));
   if(Boat.IsValid())Put(P,Boat->GetActorTransform().TransformPosition(FVector(-515,0,102)),true);      // the 63's cockpit
   return Next(6);
  case 6:
   if(E<1.5)return false;Walk();if(E<3)return false;
   Report(TEXT("yacht deck"));
   Test->TestTrue(TEXT("teak underfoot on the yacht"),Trod(TEXT("wood")));
   Test->TestTrue(FString::Printf(TEXT("water lapping alongside the quay (%.2f)"),S->GetBedLevel(6)),S->GetBedLevel(6)>.25f);
   Strokes=S->GetBellStrokes();S->RingBell(2);return Next(7);
  case 7:
   if(E<10)return false;
   Test->TestEqual(TEXT("the Jingci bell rings two strokes"),S->GetBellStrokes()-Strokes,2);
   Test->TestTrue(FString::Printf(TEXT("footsteps sounded on the tour (%d)"),Steps->GetStepCount()),Steps->GetStepCount()>=10);
   return true;
  default: return true;
  }
 }
private:
 bool Next(int32 N){Stage=N;At=FPlatformTime::Seconds();return false;}
 void Hour(UWorld* W,float H){for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(H);}}
 void Put(ACharacter* P,FVector To,bool bExactZ=false)
 {
  if(!bExactZ)
  {
   FHitResult G;FCollisionQueryParams Q(TEXT("LandAudioPut"),true,P);
   if(P->GetWorld()->LineTraceSingleByChannel(G,To+FVector(0,0,20000),To-FVector(0,0,20000),ECC_Visibility,Q))To=G.ImpactPoint;
  }
  P->GetCharacterMovement()->StopMovementImmediately();Trodden.Reset();
  P->SetActorLocation(To+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
 }
 FVector Find(UWorld* W,FVector Near,FName Surface)
 {
  // a spot of this ground near the place: the footstep probe decides
  for(int32 R=0;R<20;++R)for(int32 K=0;K<12;++K)
  {
   const FVector P=Near+FVector(FMath::Cos(K*PI/6),FMath::Sin(K*PI/6),0)*(R*300.f);FHitResult G;FCollisionQueryParams Q(TEXT("LandAudioLawn"),true);
   if(W->LineTraceSingleByChannel(G,P+FVector(0,0,20000),P-FVector(0,0,20000),ECC_Visibility,Q))
   {
    const FName Here=Steps->ProbeSurface(G.ImpactPoint);Seen.FindOrAdd(Steps->GetLastGround())++;
    if(G.ImpactNormal.Z>.95f && Here==Surface)return G.ImpactPoint;
   }
  }
  for(const auto& KV:Seen)Test->AddInfo(FString::Printf(TEXT("   no %s near: saw %s x%d"),*Surface.ToString(),*KV.Key,KV.Value));
  return Near;
 }
 void Report(const TCHAR* Where)
 {
  auto* S=Scape.Get();
  Test->AddInfo(FString::Printf(TEXT("%s: wind %.2f city %.2f cicadas %.2f birds %.2f crickets %.2f frogs %.2f lap %.2f | one-shots %d steps %d on %s"),Where,
   S->GetBedLevel(0),S->GetBedLevel(1),S->GetBedLevel(2),S->GetBedLevel(3),S->GetBedLevel(4),S->GetBedLevel(5),S->GetBedLevel(6),S->GetOneShotCount(),Steps->GetStepCount(),*Steps->GetLastSurface().ToString()));
  Test->AddInfo(FString::Printf(TEXT("   underfoot: %s"),*Steps->GetLastGround()));
 }
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0,Birds=0,Strokes=0;
 TMap<FString,int32> Seen;TSet<FName> Trodden;
 TWeakObjectPtr<ANeonCityLandSoundscape> Scape;TWeakObjectPtr<ANeonCityLamborghini63> Boat;UNeonCityFootstepComponent* Steps=nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeLandAudioPIE,"NeonCity.WestLake.LandAudioPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeLandAudioPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeLandAudio::FTour(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
